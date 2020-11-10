/*
 Program to provide interface to the observatory dome shutter. 
 Publishes to MQTT topic /skybadger/device/<hostname> to update on device health
 Publishes to MQTT topic /skybadger/function/<hostname> to update on device status on major events.
 Subscribes to MQTT topic /skybadger/device/heartbeat to callback with device health
 Hosts REST web interface on port 80 returning application/json strings
 supports remote debug interface in place of serial - access using telnet 
 Suports remote update from web browser - use <hostname>/update 
 No Eeprom support required- there is really no data to maintain yet. 
 The two switches when not connected to the board are pulled up by local 47K pullups so a normally closed switch to gnd will show closed on both 
 the open and closed dwitches 
 The motor drive outputs are pulled to gnd by 10K pulldowns so they don't energise unless actively driven.
  
 Done:
 PCF8574 library added to support shutter motors - needs testing.
 Added 8871 driver support by refactoring motor and latch controls into their own functions. 
 Moved now to BTS9760 driver board. 
 
 To do:
 PCG8574 library not apparently functioning. 
 fix all .begin errors to represent their status as to whether present on the bus . 
 
 Layout:
 GPIO 4,2 to SDA
 GPIO 5,0 to SCL 
 All 3.3v logic. 
*/
#include "ProjectDefs.h"
#include "DebugSerial.h"
#include "AlpacaErrorConsts.h"
#include <esp8266_peri.h> //register map and access
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <PubSubClient.h> //https://pubsubclient.knolleary.net/api.html
#include <EEPROM.h>
#include <EEPROMAnything.h>
#include <Wire.h>         //https://playground.arduino.cc/Main/WireLibraryDetailedReference
#include <Time.h>         //Look at https://github.com/PaulStoffregen/Time for a more useful internal timebase library
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ArduinoJson.h>
#include <GDBStub.h> //Debugging stub for GDB
#include "RemoteDebug.h"  //https://github.com/JoaoLopesF/RemoteDebug

//Ntp dependencies - available from v2.4
#include <time.h>
#include <sys/time.h>
#include <coredecls.h>
time_t now; //use as 'gmtime(&now);'

ESP8266WiFiMulti wifiMulti; //Allows the use of multiple SSID wireless APs during development.

//Strings
#include <SkybadgerStrings.h>
char* myHostname     = "espDSH00";
char* thisID         = "espDSH00";

WiFiClient espClient;
PubSubClient client(espClient);
bool callbackFlag = false;
bool timerSet = false;

//Detect changes of shutter states on pins
int event = 0;
int lastShutterState = 0;

// Create an instance of the server
// specify the port to listen on as an argument
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

//Create a remote debug object
RemoteDebug Debug;

//Hardware device system functions - reset/restart etc
EspClass device;
ETSTimer timer, timeoutTimer;
volatile bool newDataFlag = false;
volatile bool timeoutFlag = false;

void onTimer(void);
void onTimeoutTimer(void);

//Dome shutter control via I2C Port Expander PCF8574AP
#include "DomeShutter.h"
PCF8574 shutter;
domeHWControl_t domeHW;

//Shutter variables
bool shutterPresent = false;
enum shutterState shutterStatus = SHUTTER_IDLE;
enum shutterState targetStatus = SHUTTER_IDLE;
int altitude = 0 ; //degrees of opening - currently no means to measure other than time of operation
int targetAltitude = 0;

#include "Skybadger_common_funcs.h"
#include "ShutterHandlers.h"

void setup_wifi()
{
  int zz = 0;
  WiFi.mode(WIFI_STA);
  WiFi.hostname( myHostname );  

  WiFi.begin(ssid1, password1  );
  Serial.print("Searching for WiFi..");
  
  while (WiFi.status() != WL_CONNECTED) 
  {
     delay(500);
     Serial.print(".");
   if (zz++ >= 400) 
    {
      device.restart();
    }    
  }

  Serial.println("WiFi connected");
  Serial.printf("SSID: %s, Signal strength %i dBm \n\r", WiFi.SSID().c_str(), WiFi.RSSI() );
  Serial.printf("Hostname: %s\n\r",       WiFi.hostname().c_str() );
  Serial.printf("IP address: %s\n\r",     WiFi.localIP().toString().c_str() );
  Serial.printf("DNS address 0: %s\n\r",  WiFi.dnsIP(0).toString().c_str() );
  Serial.printf("DNS address 1: %s\n\r",  WiFi.dnsIP(1).toString().c_str() );
  delay(5000);

  //Setup sleep parameters
  //wifi_set_sleep_type(LIGHT_SLEEP_T);

  //WiFi.mode(WIFI_NONE_SLEEP);
  wifi_set_sleep_type(NONE_SLEEP_T);
}

void setup()
{
  String outbuf;
  int error = 0;
  //Minimise serial to one pin only. 
  //Serial.begin( 115200 );
  Serial.begin( 115200, SERIAL_8N1, SERIAL_TX_ONLY);
  //  Serial.println
  Serial.println("ESP starting.");
  //gdbstub_init();
  delay(2000); 
  
  //Start NTP client
  configTime(TZ_SEC, DST_MN, timeServer1, timeServer2, timeServer3 );
  
  //Setup variables from EEPROM
  //TODO when required
  
  //Pins mode and direction setup for i2c on ESP8266-01
  pinMode(0, OUTPUT);
  pinMode(2, OUTPUT);
  //GPIO 3 (RX) swap the pin to a GPIO.
  //Could use as output to pwm the motor speed or to behave as an /INT input for detecting switch change.
  pinMode(3, OUTPUT);
  
  //I2C setup SDA pin 0, SCL pin 2 on ESP-01
  //I2C setup SDA pin 5, SCL pin 4 on ESP-12
  Wire.begin(0, 2);
  Wire.setClock(100000 );//100KHz target rate
    
  // Connect to wifi 
  setup_wifi();     

  //Debugging over telnet setup
  // Initialize the server (telnet or web socket) of RemoteDebug
  Debug.begin( WiFi.hostname().c_str(), Debug.INFO );
  Debug.setSerialEnabled(true);//until set otherwise
  // OR
  //Debug.begin(HOST_NAME, startingDebugLevel );
  // Options
  //Debug.setResetCmdEnabled(true); // Enable the reset command
  // Debug.showProfiler(true); // To show profiler - time between messages of Debug
  
  //Open a connection to MQTT
  client.setServer( mqtt_server, 1883 );
  client.connect( thisID, pubsubUserID, pubsubUserPwd ); 
  //Create a callback that causes this device to read the local status for data to publish.
  client.setCallback( callback );
  client.subscribe( inTopic );
  Serial.println("MQTT subscription initialised");
  
  Serial.println("Setting up shutter controls");
  //bit 0 LED 
  //bit 1 is latch relay activation bit.
  //bits 2,3 is unused
  //bit 4,5 are limit switch inputs with weak pullups
  //bit 7 and 6 are motor half-bridge control outputs - both low stopped, one one high - go one way, the other high, go the other way, both high - stopped.

  outbuf = scanI2CBus();
  Serial.println( outbuf.c_str() ) ;

  //Setup i2c to control PCF display
  //shutter = PCF8574( 0x38, Wire );//0x70 is 8-bit address, 0x38 the 7-bit address. Decimal 56
  Serial.println("Starting to configure i2C Expander connection");
  shutter.begin( (const uint8_t) 0x38, Wire, (uint8_t) 0b00111100 );  
  error = shutter.lastError();

  if ( error == PCF8574_I2C_ERROR )
  {
    shutterPresent = false;
    Serial.println("unable to access I2C expander\n");
  }
  else 
  {
    shutterPresent = true;
    shutter.begin(0b00111111);//set to 1s to set for read. Set to 1s for high output. set to 0s for low output input. 
    Serial.println("Found I2C expander\n");
  }
  Serial.printf( "shutter i2c expander init - status: %i\n", error );  
  
  //Setup webserver handler functions
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound); 
  server.on("/shutter", HTTP_GET, handleShutterStatusGet );
  server.on("/status", handleShutterStatusGet );
  server.on("/shutter", HTTP_PUT, handleShutterStatusPut );
  server.on("/shutter/restart", handleShutterRestartPut );
  //Debug functions
  server.on("/shutter/latch", handleLatch );
  server.on("/shutter/motor", handleMotor );
  
  httpUpdater.setup( &server );
  server.begin();
  
  //Setup timers
  //setup interrupt-based 'soft' alarm handler for periodic acquisition of new bearing
  ets_timer_setfn( &timer, onTimer, NULL ); 
  
  //For MQTT Async non blocking reconnect
  ets_timer_setfn( &timeoutTimer, onTimeoutTimer, NULL ); 
  
  //fire timer every 500 msec
  //Set the timer function first
  ets_timer_arm_new( &timer, 500, 1/*repeat*/, 1);
  //ets_timer_arm_new( &timeoutTimer, 2500, 0/*one-shot*/, 1);
  
  Debug.setSerialEnabled(False);
  Serial.println( "Setup complete\n" );
}

//Timer handler for 'soft' 
void onTimer( void * pArg )
{
  newDataFlag = true;
}

//Used to complete timeout actions. 
//Used for MQTT timeout actions as well.
void onTimeoutTimer( void* pArg )
{
  //Read command list and apply.
  if ( !timerSet ) 
  {//Then its not being used by MQTT. it must be this.
    //Turn off the shutter latch relay after timeout. 
     unpowerLatch();
  }

  //reset flag  
  timeoutFlag = true;
}

//Main processing loop
void loop()
{
  String timestamp;
  String output;
  static int loopCount = 0;
  int error = PCF8574_OK;
  static int lastShutterState; 
  
  //@100KHz, an i2c exchange lasts ~ 200 uS
  if( newDataFlag == true ) 
  {
    //toggle LED
    shutter.toggle( PIN_LED );
    domeHW.led = (bool) shutter.read( PIN_LED );  //(domeHW.led == 1 )? 0 : 1 );
    if ( shutter.lastError() != PCF8574_OK )
      debugW( "Toggle: No response" );

#ifndef _TESTING    
    if ( shutterPresent )
    {
      //Adopt the view that the shutter stopped at target position is actually SHUTTER_OPEN, even if openSensor isn't triggered
      //Update the sensors
      uint8_t in = shutter.read8() & 0b11110011;
      event = ( lastShutterState ^ in ) & 0xFD;//mask off the togggle event.
      //event = ( lastShutterState ^ in );
      lastShutterState = in;
      domeHW.led          = ( in & 0b01);
      domeHW.latchRelay   = ((in & 0b010)       >> 1);
      //two unused bits
      domeHW.openSensor   = ((in & 0b010000)    >> 4);
      domeHW.closedSensor = ((in & 0b0100000)   >> 5);
      domeHW.motorEnA     = ((in & 0b01000000)  >> 6);
      domeHW.motorEnB     = ((in & 0b010000000) >> 7);
      debugD("Last shutter state: 0x%x", in );
      
      handleShutter();
    }
#endif
    debugV(".");
    newDataFlag = false;
  }  

  //Webserver handling.
  server.handleClient();
 
  //MQTT housekeeping.
  if ( client.connected() ) 
  {
    client.loop();
    if ( callbackFlag )
    {
      publishHealth();
      callbackFlag = false;
    }
    if( event > 0 ) 
    {
       int eventState = lastShutterState && event;
       //publishShutterEvent( event, eventState );
    }
  }
  else 
  {
    reconnectNB();
    client.subscribe(inTopic);
  }
  
  // Remote debug over WiFi
  Debug.handle();
  // Or
  //debugHandle(); // Equal to SerialDebug  
}
 
/* MQTT callback for subscription and topic.
 * Only respond to valid states ""
 */
void callback(char* topic, byte* payload, unsigned int length) 
{
   callbackFlag = true;
}

/*
This function will publish events to the functional queue
Subscribers will receive this an know there is change of state
*/
void publishShutterEvent( int event, int state )
{
  String output;
  String outTopic;
  String timestamp = "";
  String eventMsg;
   
  //checkTime();
  getTimeAsString2( timestamp );

  //publish to our device topic(s)
  DynamicJsonBuffer jsonBuffer(256);
  JsonObject& root = jsonBuffer.createObject();

  switch (event) 
  {
   case 0x02://latch relay
      eventMsg = "Latch power change";
      break;
   case 0x04:
      break;
   case 0x08:
      break;
   case 0x10://open sensor
      eventMsg = "Shutter Closed switch change";
      break;
   case 0x20://closed sensor
      eventMsg = "Shutter Open switch change";
      break;
   case 0x40://motor activity en_A
      eventMsg = "Shutter Motor opening";
      break;
   case 0x80://motor activity en_B
      eventMsg = "Shutter Motor closing";
      break;
   case 0x01://led
      break;
   default:
      break;
  }
  root["time"] = timestamp;
  root["event"] = eventMsg;
  root["state"] = state;
  root["hostname"] = myHostname;
  
  root.printTo(output);
  ///skybadger/function/<device type>/{time, state, eventMsg, <hostname>}
  outTopic = outFnTopic;
  outTopic.concat(F("Shutter"));
  
  if ( client.publish( outTopic.c_str(), output.c_str(), true ) )
    debugV( "Publish success! topic: %s: message: %s\n", outTopic.c_str(), output.c_str() ); 
  else
    debugW( "Publish failed! topic: %s: message: %s \n", outTopic.c_str(), output.c_str() ); 
}

void publishHealth(void )
{
  String output;
  String outTopic;
  String timestamp = "";
   
  //checkTime();
  getTimeAsString2( timestamp );

  //publish to our device topic(s)
  DynamicJsonBuffer jsonBuffer(256);
  JsonObject& root = jsonBuffer.createObject();

  root["time"] = timestamp;
  root["hostname"] = myHostname;
  output = "Listening";
  outTopic = outHealthTopic;
  outTopic.concat( myHostname );
  
  if ( client.publish( outTopic.c_str(), output.c_str(), true ) )
    /*Serial.printf*/debugI( "Publish success! topic: %s: message: %s\n", outTopic.c_str(), output.c_str() ); 
  else
    /*Serial.printf*/debugW( "Publish failed! topic: %s: message: %s \n", outTopic.c_str(), output.c_str() ); 
}
