/*
 Program to provide interface 3to the observatory dome shutter. 
 Also publishes to MQTT topic /skybadger/device/<hostname> on current state
 Hosts REST web interface on port 80 returning application/json strings
 
 To do:
 
 Done: PCF8574 library added to support shutter motors - needs testing.
fix all .begin errors to represent their status as to whether present on the bus . 
 
 Layout:
 Pin 13 to DHT data 
 GPIO 4,2 to SDA
 GPIO 5,0 to SCL 
 All 3.3v logic. 
  
*/
#include "ProjectDefs.h"
#include "DebugSerial.h"
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

//Ntp dependencies - available from v2.4
#include <time.h>
#include <sys/time.h>
#include <coredecls.h>
time_t now; //use as 'gmtime(&now);'

ESP8266WiFiMulti wifiMulti; //Allows the use of multiple SSID wireless APs during development.

//Strings
#include <SkybadgerStrings.h>
char* myHostname     = "espDSH01";
char* thisID         = "espDSH01";

WiFiClient espClient;
PubSubClient client(espClient);
bool callbackFlag = false;

//Detect changes of shutter states on pins
int event = 0;
int lastShutterState = 0;

// Create an instance of the server
// specify the port to listen on as an argument
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

//Hardware device system functions - reset/restart etc
EspClass device;
ETSTimer timer, timeoutTimer;
volatile bool newDataFlag = false;
volatile bool timeoutFlag = false;
bool timerSet = false;
void onTimer(void);
void onTimeoutTimer(void);

//Dome shutter control via I2C Port Expander PCF8574
#include "DomeShutter.h"
PCF8574 shutter( 0x51, Wire );
domeHWControl_t domeHW;

//Shutter variables
bool shutterPresent = false;
enum shutterState shutterStatus = SHUTTER_CLOSED;
int altitude = 0 ; //degrees of opening - currently no means to measure other than time of operation

#include "Skybadger_common_funcs.h"
#include "ShutterHandlers.h"

void setup_wifi()
{
  int zz = 0;
  WiFi.hostname( myHostname );
  WiFi.mode(WIFI_STA);
  
  WiFi.begin(ssid1, password1);
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
  Serial.begin( 115200, SERIAL_8N1, SERIAL_TX_ONLY);
  Serial.println(F("ESP starting."));
  
  //Start NTP client
  configTime(TZ_SEC, DST_SEC, timeServer1, timeServer2, timeServer3 );
  
  //Setup variables from EEPROM
  //TODO
  
  //Pins mode and direction setup for i2c on ESP8266-01
  pinMode(0, OUTPUT);
  pinMode(2, OUTPUT);
  //GPIO 3 (RX) swap the pin to a GPIO.
  pinMode(3, OUTPUT);
  
  //I2C setup SDA pin 0, SCL pin 2 on ESP-01
  //I2C setup SDA pin 5, SCL pin 4 on ESP-12
  Wire.begin(0, 2);
  Wire.setClock(100000 );//100KHz target rate

  Serial.println("Setting up shutter controls");
  //bit 0 LED 
  //bit 1,2 are limit switch inputs with weak pullups  is the lock relay enable, 
  //bit 2 is unused
  //bit 3 and 4 are motor control ouputs
  //bit 5 is unused.
  //bit 7,6 
  shutter.begin(0b0000110);
  shutterPresent = true;
  Serial.println("Shutter control initialised");

  // Connect to wifi 
  setup_wifi();                   
  
  //Open a connection to MQTT
  client.setServer( mqtt_server, 1883 );
  client.connect( thisID, pubsubUserID, pubsubUserPwd ); 
  //Create a callback that causes this device to read the local status for data to publish.
  client.setCallback( callback );
  client.subscribe( inTopic );
  Serial.println("MQTT subscription initialised");
  
  //Setup webserver handler functions
  httpUpdater.setup( &server );
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound); 
  server.on("/shutter", HTTP_GET, handleShutterStatusGet );
  server.on("/shutter", HTTP_PUT, handleShutterStatusPut );
  //server.on("/shutter/reset", HTTP_PUT, handleShutterResetPut );
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
  
  Serial.println( "Setup complete" );
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
     shutter.write( PIN_LOCK, domeHW.latchRelay = 0);
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
  
  if( WiFi.status() != WL_CONNECTED)
  {
      device.restart();
  }

  if( newDataFlag == true ) 
  {
    //toggle LED
    shutter.write( PIN_LED, domeHW.led = (domeHW.led == 1 )? 0 : 1 );

#ifndef _TESTING    
    if ( shutterPresent )
    {
      //Adopt the view that the shutter at stopped at target position is actually SHUTTER_OPEN, even if openSensor isn't triggered
      //Update the sensors
      int in = shutter.read8() & 0b00111110;
      event = lastShutterState ^ in;
      lastShutterState = in;
      
      domeHW.led          = ( in & 0b01);
      domeHW.closedSensor = ((in & 0b010)     >> 1);
      domeHW.openSensor   = ((in & 0b0100)    >> 2);
      domeHW.latchRelay   = ((in & 0b01000)   >> 3);
      domeHW.motorEn      = ((in & 0b010000)  >> 4);
      domeHW.motorDirn    = ((in & 0b0100000) >> 5);

      if ( shutterStatus != SHUTTER_IDLE )
      {  
          handleShutter();
      }
    }
#else
    Serial.println(".");
#endif
    newDataFlag = false;
  }  

  //Webserver handling.
  server.handleClient();
 
  //MQTT housekeeping.
  if ( client.connected() ) 
  {
    if ( callbackFlag )
    {
      publishHealth();
      ;;//STUFF
      callbackFlag = false;
    }
    //if( event > 0 ) 
    //   publishShutterEvent( event );

    client.loop();
  }
  else 
  {
    reconnectNB();
    client.subscribe(inTopic);
  }     
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
void publishShutterEvent( int event )
{
  String output;
  String outTopic;
  String timestamp = "";
   
  //checkTime();
  getTimeAsString2( timestamp );

  //publish to our device topic(s)
  DynamicJsonBuffer jsonBuffer(256);
  JsonObject& root = jsonBuffer.createObject();

  switch (event) 
  {
   case 0x01://led
      break;
   case 0x02://closed sensor
      break;
   case 0x04://open sensor
      break;
   case 0x08://latch relay
      break;
   case 0x10://motor activity
      break;
   case 0x20:
   case 0x40:
   case 0x80:
   default:
      break;
  }
  root["time"] = timestamp;
  root["device"] = "Shutter";
  root["event"] = event;
  
  root.printTo(output);
  outTopic = outFnTopic;
  outTopic.concat( myHostname );
  
  if ( client.publish( outTopic.c_str(), output.c_str(), true ) )
    Serial.printf( "Publish success! topic: %s: message: %s\n", outTopic.c_str(), output.c_str() ); 
  else
    Serial.printf( "Publish failed! topic: %s: message: %s \n", outTopic.c_str(), output.c_str() ); 
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
  output = "Listening";
  outTopic = outHealthTopic;
  outTopic.concat( myHostname );
  
  if ( client.publish( outTopic.c_str(), output.c_str(), true ) )
    Serial.printf( "Publish success! topic: %s: message: %s\n", outTopic.c_str(), output.c_str() ); 
  else
    Serial.printf( "Publish failed! topic: %s: message: %s \n", outTopic.c_str(), output.c_str() ); 
}
