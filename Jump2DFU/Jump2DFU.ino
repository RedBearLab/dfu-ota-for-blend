
#include <SPI.h>
#include <boards.h>
#include <RBL_nRF8001.h>
#include <avr/wdt.h>

void setup()
{  
  ble_reset(4);
  ble_begin();
}

void loop()
{
  if ( ble_available() )
  {
    if(0xFF == ble_read())
    {
        *(uint16_t *)0x0800 = 0x7777;
        wdt_enable(WDTO_15MS);
        while(1);
    }
  }
  
  ble_do_events();
}

