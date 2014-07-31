
#include <SPI.h>
#include <boards.h>
#include <RBL_nRF8001.h>
#include <avr/wdt.h>

void setup()
{  
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);

  ble_set_name("Jump2DFU");
  ble_reset(4);
  ble_begin();
}

void loop()
{
  if ( 3 == ble_available() )
  {
    if('D' == ble_read() && 'F' == ble_read() && 'U' == ble_read())
    {
        *(uint16_t *)0x0800 = 0x7777;
        wdt_enable(WDTO_15MS);
        while(1);
    }
  }
  
  ble_do_events();
  
  digitalWrite(13, HIGH);
  delay(500);
  digitalWrite(13, LOW);
  delay(500);
}

