/* Copyright (c) 2014, Nordic Semiconductor ASA
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/** @file
  @brief Implementation of the DFU procedure.
 */

#include "Caterina.h"

#include <string.h>
#include <avr/io.h>
#include <util/delay.h>

#include "lib_aci.h"
#include "dfu.h"

/*****************************************************************************
* Local definitions
*****************************************************************************/

static void dfu_data_pkt_handle (aci_evt_t *aci_evt);
static void dfu_init_pkt_handle (aci_evt_t *aci_evt);
static void dfu_image_size_set (aci_evt_t *aci_evt);
static void dfu_image_validate (aci_evt_t *aci_evt);

static void m_notify (void);
static bool m_send (uint8_t *buff, uint8_t buff_len);
static void m_write_page (uint16_t page, uint8_t *buff);

/*****************************************************************************
* Static Globals
*****************************************************************************/

static aci_state_t *m_aci_state;
static uint8_t      m_dfu_state = ST_ANY;
static uint32_t     m_image_size;
static uint16_t     m_pkt_notif_target;
static uint16_t     m_pkt_notif_target_cnt;
static uint32_t     m_num_of_firmware_bytes_rcvd;
static uint16_t     m_page_address;
static uint8_t      m_page_buff[SPM_PAGESIZE];
static uint8_t      m_page_buff_index;
static uint8_t      m_pipe_array[3];

extern bool RunBootloader;

/*****************************************************************************
* Static Functions
*****************************************************************************/

/* Send receive notification with number of bytes received */
static void m_notify (void)
{
  uint8_t response[] = {OP_CODE_PKT_RCPT_NOTIF,
    0,
    (uint8_t) (m_num_of_firmware_bytes_rcvd >> 0),
    (uint8_t) (m_num_of_firmware_bytes_rcvd >> 8),
    (uint8_t) (m_num_of_firmware_bytes_rcvd >> 16),
    (uint8_t) (m_num_of_firmware_bytes_rcvd >> 24)};

  while(!m_send (response, sizeof(response)));
}

/* Transmit buffer_len number of bytes from buffer to the BLE controller */
static bool m_send (uint8_t *buff, uint8_t buff_len)
{
  bool status;

  // Abort if the notification pipe isn't available
  /*if (!lib_aci_is_pipe_available (m_aci_state, m_pipe_array[1]))
  {
    return false;
  }*/

  // Abort if we don't have the necessary credit to transmit
//  if (m_aci_state->data_credit_available == 0)
//  {
//    return false;
//  }

  // Put the notification message in the queue
  status = lib_aci_send_data(m_pipe_array[1], buff, buff_len);

  // Decrement our credit if we successfully transmitted
  if (status)
  {
    m_aci_state->data_credit_available--;
  }

  TX_LED_TOGGLE();
    
  return status;
}

/* Write the contents of buf to the given flash page */
static void m_write_page (uint16_t page_num, uint8_t *buff)
{
  uint16_t i;
  uint16_t word;
    
  /* Disable timer 1 interrupt - can't afford to process nonessential interrupts while doing SPM tasks */
  TIMSK1 = 0;

  // Erase flash page, then wait while the memory is written
  boot_page_erase(page_num);
  boot_spm_busy_wait();

  // Fill the page buffer
  for (i = 0; i < SPM_PAGESIZE; i += 2)
  {
      // Set up little-endian word.
      word = *buff++;
      word += (*buff++) << 8;

      boot_page_fill(page_num + i, word);
  }

  // Store buffer in flash page, then wait while the memory is written
  boot_page_write(page_num);
  boot_spm_busy_wait();

  // Reenable RWW-section again. We need this if we want to jump back
  // to the application after bootloading.
  boot_rww_enable();
    
  /* Re-enable timer 1 interrupt disabled earlier in this routine */
  TIMSK1 = (1 << OCIE1A);
}

/* Receive a firmware packet, and write it to flash. Also sends receipt
 * notifications if needed
 */
static void dfu_data_pkt_handle (aci_evt_t *aci_evt)
{
  static uint8_t response[] = { OP_CODE_RESPONSE,
                                BLE_DFU_RECEIVE_APP_PROCEDURE,
                                BLE_DFU_RESP_VAL_SUCCESS};

  aci_evt_params_data_received_t *data_received;
  uint8_t bytes_received = aci_evt->len-2;
  uint8_t i;

  // If package notification is enabled, decrement the counter and issue a
  // notification if required
  if (m_pkt_notif_target)
  {
    m_pkt_notif_target_cnt--;

    // Notification is required. Issue notification and reset counter 
    if (m_pkt_notif_target_cnt == 0)
    {
      m_pkt_notif_target_cnt = m_pkt_notif_target;
      m_notify ();
    }
  }

  // Write received data to page buffer. When the buffer is full, write the
  // buffer to flash.
  data_received = (aci_evt_params_data_received_t *) &(aci_evt->params);
  for (i = 0; i < bytes_received; i++)
  {
    m_page_buff[m_page_buff_index++] = data_received->rx_data.aci_data[i];

    if (m_page_buff_index == SPM_PAGESIZE)
    {
      m_page_buff_index = 0;
      m_write_page (m_page_address, m_page_buff);
      m_page_address += SPM_PAGESIZE;
    }
  }

  // Check if we've received the entire firmware image
  m_num_of_firmware_bytes_rcvd += bytes_received;
  if (m_image_size == m_num_of_firmware_bytes_rcvd)
  {
    // Write final page to flash
    m_write_page (m_page_address++, m_page_buff);

    // Send firmware received notification
    while(!m_send (response, sizeof(response)));
  }
}

/* Receive and process an init packet */
static void dfu_init_pkt_handle (aci_evt_t *aci_evt)
{
  static uint8_t response[] = {OP_CODE_RESPONSE,
     BLE_DFU_INIT_PROCEDURE,
     BLE_DFU_RESP_VAL_SUCCESS};

  // Send init received notification
  while(!m_send (response, sizeof(response)));
}

/* Receive and store the firmware image size */
static void dfu_image_size_set (aci_evt_t *aci_evt)
{
  const uint8_t pipe = m_pipe_array[1];
  const uint8_t byte_idx = pipe / 8;
  static uint8_t response[] = {OP_CODE_RESPONSE, BLE_DFU_START_PROCEDURE,
    BLE_DFU_RESP_VAL_SUCCESS};

  // There are two paths into the bootloader. We either got here because
  // there is no application, or we jumped from application.
  // In the latter case, as we haven't received an event from the nRF8001
  // with the pipe statuses, we have to assume that the Control Point
  // TX pipe is open. At this point, that is safe.
  m_aci_state->pipes_open_bitmap[byte_idx] |= (1 << (pipe % 8));
  m_aci_state->pipes_closed_bitmap[byte_idx] &= ~(1 << (pipe % 8));

  m_image_size =
    (uint32_t)aci_evt->params.data_received.rx_data.aci_data[3] << 24 |
    (uint32_t)aci_evt->params.data_received.rx_data.aci_data[2] << 16 |
    (uint32_t)aci_evt->params.data_received.rx_data.aci_data[1] << 8  |
    (uint32_t)aci_evt->params.data_received.rx_data.aci_data[0];
    
    if(m_image_size == 0 || m_image_size > 28672)
    {
        response[2] = BLE_DFU_RESP_VAL_DATA_SIZE;
    }
    else
    {
        m_dfu_state = ST_RDY;
    }

  // Write response
  while(!m_send (response, sizeof(response)));
}

/* Activate the received firmware image */
static void dfu_image_activate (aci_evt_t *aci_evt)
{
  hal_aci_evt_t     aci_data;
  aci_evt_t         *aci_evt_m;

  lib_aci_disconnect(m_aci_state, ACI_REASON_TERMINATE);

  while( !(aci_evt_m->evt_opcode == ACI_EVT_DISCONNECTED) )
  {
      lib_aci_event_get(m_aci_state, &aci_data);
      aci_evt_m = &(aci_data.evt);
  }
    
  RunBootloader = false;
}

/* Validate the received firmware image, and transmit the result */
static void dfu_image_validate (aci_evt_t *aci_evt)
{
  uint8_t response[] = {OP_CODE_RESPONSE,
    BLE_DFU_VALIDATE_PROCEDURE,
    BLE_DFU_RESP_VAL_SUCCESS};

  // Completed successfully
  if (m_num_of_firmware_bytes_rcvd == m_image_size)
  {
    while(!m_send(response, sizeof(response)));
  }

  m_dfu_state = ST_FW_VALID;
}

/*****************************************************************************
* Public API
*****************************************************************************/

/* Initialize the state machine */
void dfu_init (uint8_t *p_pipes)
{
  m_dfu_state = ST_IDLE;
  memcpy(m_pipe_array, p_pipes, 3);
}

/* Update the state machine according to the event in aci_evt */
void dfu_update (aci_state_t *aci_state, aci_evt_t *aci_evt)
{
  const aci_rx_data_t *rx_data = &(aci_evt->params.data_received.rx_data);
  uint8_t event = EV_ANY;
  uint8_t pipe;
    
  RX_LED_TOGGLE();

  m_aci_state = aci_state;
  pipe = rx_data->pipe_number;

  // Incoming data packet
  if (pipe == m_pipe_array[0]) {
    event = DFU_PACKET_RX;
  }
  // Incoming control point
  else if (pipe == m_pipe_array[2]) {
    event = rx_data->aci_data[0];
  }

  // Update the state machine based on the incoming event and current state
  switch (event)
  {
    // Receied data from data pipe
    case DFU_PACKET_RX:
    {
      switch (m_dfu_state)
      {
        case ST_IDLE:
          dfu_image_size_set(aci_evt);
          break;
              
        case ST_RX_INIT_PKT:
          dfu_init_pkt_handle(aci_evt);
          break;
              
        case ST_RX_DATA_PKT:
          dfu_data_pkt_handle(aci_evt);
          break;
      }
      break;
    }
   
    // Received op code from contro point pipe
    /*case OP_CODE_START_DFU:
    {
      m_dfu_state = ST_IDLE;
      break;
    }*/
     
    case OP_CODE_RECEIVE_INIT:
    {
      // Initial parameters
      if (m_dfu_state == ST_RDY)
      {
        m_dfu_state = ST_RX_INIT_PKT;
      }
      break;
    }
          
    case OP_CODE_RECEIVE_FW:
    {
        // Request receive firmware
      if (m_dfu_state == ST_RDY || m_dfu_state == ST_RX_INIT_PKT)
      {
        m_dfu_state = ST_RX_DATA_PKT;
      }
      break;
    }
          
    case OP_CODE_VALIDATE:
    {
        // Validate received firmware
      if (m_dfu_state == ST_RX_DATA_PKT)
      {
        dfu_image_validate(aci_evt);
      }
      break;
    }
          
    case OP_CODE_ACTIVATE_N_RESET:
    {
        // Active received firmware to run sketch
      if (m_dfu_state == ST_FW_VALID)
      {
        dfu_image_activate(aci_evt);
      }
      break;
    }
          
    case OP_CODE_SYS_RESET:
    {
        // Reset nRF8001
        while (!lib_aci_radio_reset(m_aci_state));
        m_dfu_state = ST_IDLE;
        break;
    }
          
    case OP_CODE_PKT_RCPT_NOTIF_REQ:
    {
        // Enable packet receipt
        m_pkt_notif_target =
        (uint16_t)aci_evt->params.data_received.rx_data.aci_data[2] << 8 |
        (uint16_t)aci_evt->params.data_received.rx_data.aci_data[1];
        m_pkt_notif_target_cnt = m_pkt_notif_target;
        break;
    }
  }
}
