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

#include <stdbool.h>
#include <avr/io.h>
#include <util/delay.h>

#include <string.h>
#include "hal_aci_tl.h"
#include "hal_platform.h"

static inline void m_aci_event_check (void);
static inline void m_aci_reqn_disable (void);
static inline void m_aci_reqn_enable (void);
static inline void m_spi_init (void);
static inline uint8_t m_spi_readwrite (const uint8_t aci_byte);
static inline void m_aci_spi_transfer (hal_aci_data_t * data_to_send,
    hal_aci_data_t * received_data);

uint8_t need_tx = 0;
uint8_t rx_ready = 0;

static hal_aci_data_t data_to_send;
hal_aci_data_t received_data;

static void m_aci_event_check(void)
{
  // nRF8001 has no event to send or isn't ready to received data
  if (!hal_aci_tl_rdyn())
  {
    // But we have data to send
    if (need_tx)
    {
      // Pull the REQN to low
      m_aci_reqn_enable();
    }

    return;
  }
  // nRF8001 is ready to received data or has event to send
  else
  {
      if(!need_tx)
      {
          // No data to send, just read out event from nRF8001 when transmission begin
          data_to_send.status_byte = 0;
          data_to_send.buffer[0] = 0;
      }
      /* Receive and/or transmit data */
      m_aci_spi_transfer(&data_to_send, &received_data);
      
      need_tx = 0;
      
      /* Check if we received data */
      if (received_data.buffer[0] > 0)
      {
          rx_ready = 1;
      }
  }

  return;
}

static inline void m_aci_reqn_disable (void)
{
  PORTB |= _BV(PB5);
}

static inline void m_aci_reqn_enable (void)
{
  PORTB &= ~_BV(PB5);
}

static void m_aci_spi_transfer (hal_aci_data_t * data_to_send, hal_aci_data_t * received_data)
{
  uint8_t byte_cnt;
  uint8_t byte_sent_cnt;
  uint8_t max_bytes;

  m_aci_reqn_enable();

  /* Send length, receive header */
  byte_sent_cnt = 0;
  received_data->status_byte = m_spi_readwrite(data_to_send->buffer[byte_sent_cnt++]);
  /* Send first byte, receive length from slave */
  received_data->buffer[0] = m_spi_readwrite(data_to_send->buffer[byte_sent_cnt++]);
  if (0 == data_to_send->buffer[0])
  {
    max_bytes = received_data->buffer[0];
  }
  else
  {
    /* Set the maximum to the biggest size. One command byte is already sent */
    max_bytes = (received_data->buffer[0] > (data_to_send->buffer[0] - 1))
                                          ? received_data->buffer[0]
                                          : (data_to_send->buffer[0] - 1);
  }

  if (max_bytes > HAL_ACI_MAX_LENGTH)
  {
    max_bytes = HAL_ACI_MAX_LENGTH;
  }

  /* Transmit/receive the rest of the packet */
  for (byte_cnt = 0; byte_cnt < max_bytes; byte_cnt++)
  {
    received_data->buffer[byte_cnt+1] =  m_spi_readwrite(data_to_send->buffer[byte_sent_cnt++]);
  }

  /* RDYN should follow the REQN line in approx 100ns */
  m_aci_reqn_disable();
}

static void m_spi_init (void)
{
    /* Configure the IO lines */
    /* Set RDYN as input with pull-up */
    DDRB &= ~_BV(PB4);
    PORTB |= _BV(PB4);
    
    /* Set REQN, MOSI & SCK as output */
    DDRB |= _BV(PB2) | _BV(PB1);
    DDRB |= _BV(PB5);
    PORTB |= _BV(PB5);
    
    /* Set MISO as input */
    DDRB &= ~_BV(PB3);
    
    // Set SS as output
    DDRB |= _BV(PB0);
    
    /* Configure SPI registers */
    SPCR |= _BV(SPE) | _BV(DORD) | _BV(MSTR) | _BV(SPI2X) | _BV(SPR0);
}

static uint8_t m_spi_readwrite(const uint8_t aci_byte)
{
  SPDR = aci_byte;
  while(!(SPSR & (1<<SPIF)));
  return SPDR;
}

void hal_aci_tl_init(void)
{
    need_tx = 0;
    rx_ready = 0;

    /* Set up SPI */
    m_spi_init ();
    
    //DDRD |= _BV(PD4);
    //PORTD |= _BV(PD4);
    //PORTD &= ~_BV(PD4);
    //PORTD &= ~_BV(PD4);
    //PORTD &= ~_BV(PD4);
    //PORTD |= _BV(PD4);
}

bool hal_aci_tl_send(hal_aci_data_t *p_aci_cmd)
{
  const uint8_t length = p_aci_cmd->buffer[0];

  if (length > HAL_ACI_MAX_LENGTH)
  {
    return false;
  }
    
  if(need_tx)
  {
    return false;
  }
    
  data_to_send.status_byte = 0;
  memcpy((uint8_t *)&(data_to_send.buffer[0]), (uint8_t *)&p_aci_cmd->buffer[0], length + 1);
    
  need_tx = 1;
  return true;
}

bool hal_aci_tl_event_get(hal_aci_data_t *p_aci_data)
{
  if (!rx_ready)
  {
    m_aci_event_check();
  }

  if(rx_ready)
  {
      memcpy((uint8_t *)p_aci_data, (uint8_t *)&(received_data), sizeof(hal_aci_data_t));
      
      rx_ready = 0;

      return true;
  }

  return false;
}

/* Returns true if the rdyn line is low */
bool hal_aci_tl_rdyn (void)
{
  return !(PINB & _BV(PB4));
}

bool hal_aci_tl_event_peek(hal_aci_data_t *p_aci_data)
{
    m_aci_event_check();
    
    if (rx_ready)
    {
        memcpy((uint8_t *)p_aci_data, (uint8_t *)&(received_data), sizeof(hal_aci_data_t));
        rx_ready = 0;
        return true;
    }
    return false;
}
