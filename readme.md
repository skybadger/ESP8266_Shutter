<h1>ESP8266_Shutter Dome shutter controller over WiFi connection</h1>
<p>This application runs on the ESP8266-02 wifi-enabled SoC device to drive the shutter components of an observatory shutter meeting the ASCOM standard specifications.
This app responds to calls from an ASCOM Dome device http://github.com/skybadger/ESP8266_AscomDome and manages the local state of local switches and motors on the rotating section. </p>
<p>In my arrangement, Node-red flows are used to listen for and graph the updated readings in the dashboard UI. </p>
<p>The unit is setup for I2C operation to an PCF8574 i2C 8-bit io expander and so is expecting to see SCL on GPIO0 and SDA on GPIO2. </p>
<p>Serial Tx for debug output is available at 115200 baud (no parity, 8-bits) on the Tx pin at 3.3v TTL only .<br/>
The device will reboot on loss of wifi connection until regained.<br/>
The device will auto reconnect to the MQTT server.</p>

<h3>Dependencies:</h3>
<ul>
  <li>Arduino IDE 1.6-1.86 </li>
  <li>ESP8266 V2.2+ </li>
  <li>Arduino MQTT client (https://pubsubclient.knolleary.net/api.html)</li>
  <li>Arduino JSON library (pre v6) </li>
  </ul>

<p>This also has a dependency on the library of general ASCOM and ALPACA helper functions, including WiFi string settings : <a href="https://ww.github.com/skybadger/something"> ALPACA_COMMON</a></p>

<h3>Testing</h3>
<p>Access by serial port - Tx only is available from device at 115,200 baud at 3.3v. THis provides debug output on the serial port at TTL levels</p>
<p>Wifi is used for MQTT reporting, OTA updates (<hostname/update> and servicing REST web requests </p>
<p>Use http://ESPDSHXX where XX = is a device specific number 00 to 99 to connect. USe the setup web handler commands to update internal settings. </p>
<p>Use http://espdshxx/status for a status response. Use any url to the above to get a response outlining valid web commmands to use. </p>
<p>Use http://espdshxx/setup to access the setup page </p>
 
<h3>Use:</h3>
<p>I use mine to source a dashboard via hosting the mqtt server in node-red. It runs off a solar-panel supply in my observatory dome. </p>

<h3>ToDo:</h3>
<p>Finish the hardware motor interface and complete testing. - COMPLETE !</p> 
<p>Finish the setup web pages </p> 
