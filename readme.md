"ESP8266_Shutter Dome shutter controller over WiFi connection" 
This application runs on the ESP8266-02 wifi-enabled SoC device to drive the shutter components of an observatory shutter meeting the ASCOM standard specifications.
This app responds to calls from an ASCOM Dome device http://github.com/skybadger/ESP8266_AscomDome and manages the local state of local switches and motors on the rotating section. 
In my arrangement, Node-red flows are used to listen for and graph the updated readings in the dashboard UI. 
The unit is setup for I2C operation to an PCF8574 i2C 8-bit io expander and so is expecting to see SCL on GIO0 and SDA on GPIO2. 
Serial Tx for debug output is available at 115200 baud (no parity, 8-bits) on the Tx pin at 3.3v TTL only .
The device will reboot on loss of wifi connection until regained.
The device will auto reconnect to the MQTT server.

Dependencies:
Arduino 1.6, 
ESP8266 V2.4+ 
Arduino MQTT client (https://pubsubclient.knolleary.net/api.html)
Arduino JSON library (pre v6) 
My general helper functions 

Testing
Access by serial port - Tx only is available from device at 115,200 baud at 3.3v. THis provides debug output .
Wifi is used for MQTT reporting, OTA updates (<hostname/update> and servicing REST web requests
Use http://ESPDSHXX where XX= 00 to 99 to connect. 

Use:
I use mine to source a dashboard via hosting the mqtt server in node-red. It runs off a solar-panel supply in my observatory dome. 

ToDo:
Finish the hardware motor interface and complete testing. 