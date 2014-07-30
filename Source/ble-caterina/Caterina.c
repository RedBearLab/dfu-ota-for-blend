

#define  INCLUDE_FROM_CATERINA_C
#include "Caterina.h"

#include "BLE/lib_aci.h"
#include "BLE/aci_evts.h"
#include "BLE/dfu.h"
#include "BLE/services.h"
#include "BLE/aci_setup.h"
#include <util/delay.h>

static struct aci_state_t aci_state;
uint8_t pipes[3] = {PIPE_DEVICE_FIRMWARE_UPDATE_BLE_SERVICE_DFU_PACKET_RX, PIPE_DEVICE_FIRMWARE_UPDATE_BLE_SERVICE_DFU_CONTROL_POINT_TX, PIPE_DEVICE_FIRMWARE_UPDATE_BLE_SERVICE_DFU_CONTROL_POINT_RX_ACK_AUTO};
uint16_t conn_timeout = 180;
uint16_t conn_interval = 0x0050;

//static services_pipe_type_mapping_t services_pipe_type_mapping[NUMBER_OF_PIPES] = SERVICES_PIPE_TYPE_MAPPING_CONTENT;
static hal_aci_data_t setup_msgs[NB_SETUP_MESSAGES] PROGMEM = SETUP_MESSAGES_CONTENT;

/** Flag to indicate if the bootloader should be running, or should exit and allow the application code to run
 *  via a watchdog reset. When cleared the bootloader will exit, starting the watchdog and entering an infinite
 *  loop until the AVR restarts and the application runs.
 */
bool RunBootloader = true;

/* Pulse generation counters to keep track of the time remaining for each pulse type */
#define TX_RX_LED_PULSE_PERIOD 100
uint16_t TxLEDPulse = 0; // time remaining for Tx LED pulse
uint16_t RxLEDPulse = 0; // time remaining for Rx LED pulse

/* Bootloader timeout timer */
#define TIMEOUT_PERIOD	8000//4000
uint16_t Timeout = 0;

uint16_t bootKey = 0x7777;    
volatile uint16_t *const bootKeyPtr = (volatile uint16_t *)0x0800;

void StartSketch(void)
{
    CPU_PRESCALE(1);

	cli();
	
	/* Undo TIMER1 setup and clear the count before running the sketch */
	TIMSK1 = 0;
	TCCR1B = 0;
	TCNT1H = 0;		// 16-bit write to TCNT1 requires high byte be written first
	TCNT1L = 0;
	
	/* Relocate the interrupt vector table to the application section */
	MCUCR = (1 << IVCE);
	MCUCR = 0;

	L_LED_OFF();
	//TX_LED_OFF();
	//RX_LED_OFF();

	/* jump to beginning of application space */
	__asm__ volatile("jmp 0x0000");
}

/* Breathing animation on L LED indicates bootloader is running */
uint16_t LLEDPulse;
void LEDPulse(void)
{
	LLEDPulse+=2;
	uint8_t p = LLEDPulse >> 8;
	if (p > 127)
		p = 254-p;  
	p += p;
	if (((uint8_t)LLEDPulse) > p)
		L_LED_OFF();
	else
		L_LED_ON();
}

/** Main program entry point. This routine configures the hardware required by the bootloader, then continuously
 *  runs the bootloader processing routine until it times out or is instructed to exit.
 */
int main(void)
{
	//DDRB |= _BV(PB4); PORTB |= _BV(PB4); PORTB &= ~_BV(PB4); // For debugging

	// Save the value of the boot key memory before it is overwritten 
	uint16_t bootKeyPtrVal = *bootKeyPtr;
	*bootKeyPtr = 0;

	// Check the reason for the reset so we can act accordingly 
	uint8_t  mcusr_state = MCUSR;		// store the initial state of the Status register
	MCUSR = 0;							// clear all reset flags	

	// Watchdog may be configured with a 15 ms period so must disable it before going any further 
	wdt_disable();
	
	/*if (mcusr_state & (1<<EXTRF))
    {
		// External reset -  we should continue to self-programming mode.
	}
    else if ((mcusr_state & (1<<PORF)) && (pgm_read_word(0) != 0xFFFF))
    {
		// After a power-on reset skip the bootloader and jump straight to sketch if one exists.
		StartSketch();
	}
    else if ((mcusr_state & (1<<WDRF)) && (bootKeyPtrVal != bootKey) && (pgm_read_word(0) != 0xFFFF))
    {
		// If it looks like an "accidental" watchdog reset then start the sketch.
		StartSketch();
	}*/

    if (mcusr_state & (1<<EXTRF) || (pgm_read_word(0) == 0xFFFF) || (bootKeyPtrVal == bootKey))
    {
		// External reset -  we should continue to self-programming mode.
	}
    else
    {
		// After a power-on reset skip the bootloader and jump straight to sketch if one exists.
		StartSketch();
	}

	
	// Setup hardware required for the bootloader 
	SetupHardware();

	// Enable global interrupts so that the USB stack can function 
	sei();
	
	//Timeout = 0;
    
    aci_state.aci_setup_info.number_of_pipes    = NUMBER_OF_PIPES;
    aci_state.aci_setup_info.setup_msgs         = setup_msgs;
    aci_state.aci_setup_info.num_setup_msgs     = NB_SETUP_MESSAGES;
    
    lib_aci_init (&aci_state);
    
    dfu_init (pipes);
	
	while (RunBootloader)
	{
        // Time out and start the sketch if one is present
        if (Timeout > TIMEOUT_PERIOD)
        {
            RunBootloader = false;
        }
        
        ble_Task(pipes);
        
        LEDPulse();
	}

	// Jump to beginning of application space to run the sketch - do not reset
    StartSketch();
}

/** Configures all hardware required for the bootloader. */
void SetupHardware(void)
{
	/* Disable watchdog if enabled by bootloader/fuses */
	//MCUSR &= ~(1 << WDRF);
	//wdt_disable();

	/* Disable clock division */
	clock_prescale_set(clock_div_2);

	/* Relocate the interrupt vector table to the bootloader section */
	MCUCR = (1 << IVCE);
	MCUCR = (1 << IVSEL);
	
	LED_SETUP();
	CPU_PRESCALE(1);    //If 0, 1ms timer ISR, if 1, 2ms timer ISR.
	//L_LED_OFF();
	TX_LED_OFF();
	RX_LED_OFF();
	
	/* Initialize TIMER1 to handle bootloader timeout and LED tasks.  
	 * With 16 MHz clock and 1/64 prescaler, timer 1 is clocked at 250 kHz
	 * Our chosen compare match generates an interrupt every 1 ms.
	 * This interrupt is disabled selectively when doing memory reading, erasing,
	 * or writing since SPM has tight timing requirements.
	 */ 
	OCR1AH = 0;
	OCR1AL = 250;
	TIMSK1 = (1 << OCIE1A);					// enable timer 1 output compare A match interrupt
	TCCR1B = ((1 << CS11) | (1 << CS10));	// 1/64 prescaler on timer 1 input
}

//uint16_t ctr = 0;
ISR(TIMER1_COMPA_vect, ISR_BLOCK)
{
	/* Reset counter */
	TCNT1H = 0;
	TCNT1L = 0;
	  
	if (pgm_read_word(0) != 0xFFFF)
		Timeout++;
} 

/* Get and process events from the BLE link. If we detect an event indicating
 * that we are about to receive a new firmware image on BLE we set "ble_mode"
 * to a true value.
 */

void ble_Task(uint8_t *pipes)
{
    hal_aci_evt_t aci_data;
    aci_evt_t *aci_evt;
    uint8_t pipe;
    static bool setup_required = 0;
    
    // Attempt to grab an event from the BLE message queue
    if (!lib_aci_event_get(&aci_state, &aci_data)) {
        return;
    }
    
    aci_evt = &(aci_data.evt);
    
    switch(aci_evt->evt_opcode)
    {
        case ACI_EVT_DEVICE_STARTED:
            
            aci_state.data_credit_total = aci_evt->params.device_started.credit_available;
            
            if(aci_evt->params.device_started.device_mode == ACI_DEVICE_SETUP)
            {
                setup_required = 1;
                
            }
            else if (aci_evt->params.device_started.device_mode == ACI_DEVICE_STANDBY)
            {
                //if (aci_evt->params.device_started.hw_error) {
                    //Magic number used to make sure the HW error event is handled correctly.
                //    _delay_ms (20);
                //}
                //else {
                    lib_aci_connect (conn_timeout, conn_interval);
                //}
            }
            break; // ACI Device Started Event
            
        case ACI_EVT_CMD_RSP:
            if (aci_evt->params.cmd_rsp.cmd_opcode == ACI_CMD_RADIO_RESET)
            {
                if (aci_evt->params.cmd_rsp.cmd_status == ACI_STATUS_SUCCESS)
                {
                    lib_aci_connect (conn_timeout, conn_interval);
                }
            }
            break; // ACI Command Response
        
        case ACI_EVT_CONNECTED:
            Timeout = 0;
            // We should have checked that this is true before we jumped into
            // the bootloader. Hopefully we did.

            aci_state.data_credit_available = aci_state.data_credit_total;
            break;
            
        case ACI_EVT_DATA_CREDIT:
            Timeout = 0;
            aci_state.data_credit_available = aci_state.data_credit_available +
            aci_evt->params.data_credit.credit;
            break;
            
        case ACI_EVT_PIPE_ERROR:
            Timeout = 0;
            // If we received a pipe error, some message got borked.
            // All we can do is update our credit to reflect it

            if (aci_evt->params.pipe_error.error_code != ACI_STATUS_ERROR_PEER_ATT_ERROR)
            {
                aci_state.data_credit_available++;
            }
            break;
            
        case ACI_EVT_DATA_RECEIVED:
            Timeout = 0;
            // If data received is on either of the DFU pipes, we enter DFU mode.
            // We then update the DFU state machine to run the transfer.

            pipe = aci_evt->params.data_received.rx_data.pipe_number;
            if (pipe == pipes[0] || pipe == pipes[2])
            {
                dfu_update(&aci_state, aci_evt);
            }
            break;
            
        case ACI_EVT_DISCONNECTED:
            lib_aci_connect (conn_timeout, conn_interval);
            break;
            
        case ACI_EVT_HW_ERROR:
            lib_aci_connect (conn_timeout, conn_interval);
            break;
            
        default:
            break;
    }
    
    if(setup_required == 1)
    {
        if (SETUP_SUCCESS == do_aci_setup(&aci_state))
        {
            setup_required = 0;
        }
    }
    
    return;
}


