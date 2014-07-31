# dfu-ota-for-blend/blend micro #

DFU OTA for Blend and Blend Micro boards.

## Introduction ##

In the "Source" folder, it contains the source code and makefile to generate the hex file of bootloader. To compile the source code correctly, you need to change the folder name from "ble-caterina" to "caterina" and put it into the folder "Arduino\hardware\arduino\bootloaders". So you'd better backup the previous folder of "caterina" fisrt.

In the "Release" folder, it contains a hex file of bootloader we have generated for only Blend Micro. 

In the "Jump2DFU" folder, it contains a sketch and the corresponding hex file running on Blend Micro. The sketch is used to jump to bootloader when Blend Micro received 0xFF from app. The sketch relys on our BLE libraries.

To continue, you should update BLE libraries to latest version.

And the sketch you want to upload it through DFU bootloader, must add the function ble_ reset(uint8_ t reset_ pin) before ble_ begin(). Otherwise, when uploaded it will still advertise the DFU service, unless re-power on Blend Micro.

## Getting Started ##

1. Copy the released hex file, DFU-OTA-BlendMicro.hex, to under the folder "Arduino\hardware\blend\bootloaders\caterina" if you have [added our Blend/Blend Micro boards onto Arduino](http://redbearlab.com/getting-started-blend/). Then rename it with "Caterina-BlendMicro.hex". You'd better backup the previous hex file of Blend Micro.

2. You need an Arduino board with Arduino ISP sketch programmed.  Connect Blend Micro with the Arduino board. More info about Arduino ISP, please refer [here](http://arduino.cc/en/Tutorial/ArduinoISP).

3. Open Arduino IDE, select the target board:**Tools->Board->Blend Micro 3.3v/8MHz** and then burn the bootloader:**Tools->Burn Bootloader**. After burnt the bootloader, disconnect Blend Micro from the Arduino and power Blend Micro with micro USB or the VIN. 

4. Download the nRF Toolbox app on the App Store. Here use ipod for example.

5. Download the "Jump2DFU.hex" via Dropbox, Gmail, QQ or any other app that can store the hex file.

6. Select the downloaded hex file and open it with nRF Toolbox. Click on the "SELECT DEVICE" button to select the device with DFU ability. Then click "Upload".

7. After uploading, the sketch will run. Use our BLE Controller->Simple Chat app to send "DFU" to Blend Micro, it will jump to the DFU bootloader.

8. You can download any other sketchs as Jump2DFU by nRF Toolbox. Just complie the sketch running on Blend Micro to generate the hex file and then follow step 5 and step 6.

9. You can also jump to the DFU bootloader by pressing the on board reset button so that you can select the device on step 6.