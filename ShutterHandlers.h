#if ! defined _DOMESHUTTER_HANDLERS_H_
#define _DOMESHUTTER_HANDLERS_H_

#include "ProjectDefs.h"

//handler definitions
void handleShutterStatusGet( void );
void handleShutterStatusPut( void );
void handleNotFound( void );
void handleRoot( void );
void handleShutterRestartPut( void);

//Private functions
void powerLatch();
void unpowerLatch();
void motorOn( int direction );
void motorOff();
 
 //Motion handler
 bool handleShutter( void );

 void motorOn (int direction ) 
 {
#if defined MOTORDRIVER && ( MOTORDRIVER == DRIVER_4883 || MOTORDRIVER == DRIVER_8771 )
  if( domeHW.motorEnA == 1) 
    shutter.write( PIN_MOTOR_ENA, (uint8_t) ( domeHW.motorEnA = 0 ) );         
  else
    shutter.write( PIN_MOTOR_ENB, (uint8_t) ( domeHW.motorEnB = 0 ) );

#elif defined MOTORDRIVER && MOTORDRIVER == DRIVER_BTS7960
//This driver uses an enable pin and a PWM output pin. We dont have PWM so its stop or go
//ENA is used as enable forward
//ENB is used as enable backward
//PWM L and PWM R are both left enabled. 
    if( direction == DIRECTION_OPEN ) 
    {
      shutter.write( PIN_MOTOR_ENA, (uint8_t) ( domeHW.motorEnA = 1 ) );   
      shutter.write( PIN_MOTOR_ENB, (uint8_t) ( domeHW.motorEnB = 0 ) );
    }
    else
    {
      shutter.write( PIN_MOTOR_ENA, (uint8_t) ( domeHW.motorEnA = 0 ) );   
      shutter.write( PIN_MOTOR_ENB, (uint8_t) ( domeHW.motorEnB = 1 ) );
    }
#endif
  debugV("Motor turned on");
 }

 void motorOff()
 {
#if defined MOTORDRIVER && ( MOTORDRIVER == DRIVER_8871 || MOTORDRIVER == DRIVER_4833 ) 
  shutter.write( PIN_MOTOR_ENA, (uint8_t) ( domeHW.motorEnA = 0 ) );         
  shutter.write( PIN_MOTOR_ENB, (uint8_t) ( domeHW.motorEnB = 0 ) );         
#elif defined MOTORDRIVER && MOTORDRIVER == DRIVER_BTS7960
  shutter.write( PIN_MOTOR_ENA, (uint8_t) ( domeHW.motorEnA = 0 ) );         
  shutter.write( PIN_MOTOR_ENB, (uint8_t) ( domeHW.motorEnB = 0 ) );         
#endif
  debugV("Motor turned off");
 }
/*
 * Functions to encapsulate powering and un-powering the locking latch driven by the solenoid/relay driver
 * Applying power to the latch unlocks a locked latch
 * If the latch is not locked, applying power could do nothing 
 */
void powerLatch()
{
 shutter.write( PIN_LOCK, (uint8_t) ( domeHW.latchRelay = LATCH_POWERED ) );         
 debugV("Latch powered");
}
void unpowerLatch()
{
 shutter.write( PIN_LOCK, (uint8_t) ( domeHW.latchRelay = LATCH_UNPOWERED ) );
 debugV("Latch unpowered");
}

//debugging handlers for internal use. 
 void handleLatch()
 {
    String timeString = "", message = "";
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.createObject();

    debugD("entered");
    root["time"] = getTimeAsString( timeString );

    if( server.hasArg( "latch" )  )
    {
     debugD("latch found");
     if( server.arg("latch").equalsIgnoreCase ("true") )
     {
      powerLatch();
     }
     else
     {
      unpowerLatch();
     }
    }
    /*
    else if ( server.hasArg("plain") ) //means args in body of message not in URL as in a GET
    {
      String arg = server.arg("plain");
      int lastIndex = 0, index = -1;
      do
      {
        index = arg.indexOf( "&" );
        if ( index == -1) //no delimiter found - last or only arg
          value = arg.substr( indexOf( '=' ) , arg.length() - indexOf('=') );
        else //one in a series
          value = arg.substr( indexOf( '=' ) , index - indexOf('=') );
      }
      while ( index != -1 )
    }
    */
    server.send(200, "text/plain", "Latch updated");
 }

 void handleMotor()
 {
    String timeString = "", message = "";
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.createObject();
    bool mDirection = false;
    bool mEnable = false;
    
    debugD("entered");
    root["time"] = getTimeAsString( timeString );

    //USe with care - the limit switches are not engaged... and may not be connected
    if( server.hasArg( "direction" )  )
      mDirection = server.arg("direction").equalsIgnoreCase("true");
    if( server.hasArg ( "enable" ))
      mEnable = server.arg("enable").equalsIgnoreCase("true");

    if ( mEnable  ) 
    {
        if ( mDirection )
          motorOn( true );
        else
          motorOn( false );
    }
    else 
    {
      motorOff();
    }
 
 server.send(200, "text/plain", "Motor updated");
 }

/*
  * Handler for shutter processing  - not a web handler
  * false == nothing done
 */
 bool handleShutter( void )
 {
  String myError;
  String myTopic;

  bool output = false; 
  
  switch ( targetStatus )
  {
    case SHUTTER_OPEN:
    debugD("Handling SHUTTER_OPEN target status 0x%x" , domeHW );
      //compare with current status
      switch ( shutterStatus ) 
      {
        debugV("Shutter current state is %s" , shutterStatusChArr[shutterStatus] );
        case SHUTTER_OPENING:
          //Shutter moving to target altitude
          if ( targetAltitude != SHUTTER_ALTITUDE_DEFAULT && altitude <= targetAltitude )
          {
            //is it there yet ?
            if ( abs( altitude - targetAltitude ) <= SHUTTER_POSITIONING_ERROR_MAX ) 
            {
              motorOff();
              shutterStatus = SHUTTER_IDLE;
            }
          }
          //Detect sensor for shutter opening complete. Its arrived  - fully open
          else if( domeHW.openSensor == SWITCH_DOME_OPEN ) 
          {
              motorOff();
              shutterStatus = SHUTTER_OPEN;
          }
          else
          {
            ;; //else leave motors alone - they should be opening the shutter
          }
          output = true;
          break;            
        case SHUTTER_OPEN:
          //current = target. 
          motorOff();
          targetStatus = SHUTTER_IDLE;
          shutterStatus = SHUTTER_IDLE;
          break;
        case SHUTTER_IDLE:
        //We go to idle when nothing is happening. We might be open or closed or anywhere in between 
        //Seems to be a bit of redundant overlap.. 
          if( domeHW.openSensor == SWITCH_DOME_OPEN ) 
          {
            //We're open
            output = false;
            targetStatus = SHUTTER_IDLE;
          }
          else //TODO - missing case of moving to altitude
          {
            motorOn( DIRECTION_OPEN );
            shutterStatus = SHUTTER_OPENING;//Handle next time around 
          }          
        break;
        case SHUTTER_CLOSED:
        case SHUTTER_CLOSING:
          if( domeHW.openSensor != SWITCH_DOME_OPEN ) 
          {
            motorOff();
            shutterStatus = SHUTTER_IDLE;            
            if( domeHW.closedSensor == SWITCH_DOME_CLOSED ) 
            {
              powerLatch();
              ets_timer_arm_new( &timeoutTimer, 2500, 0/*one-shot*/, 1);                     
            }
            motorOn( DIRECTION_OPEN );
            shutterStatus = SHUTTER_OPENING;
            output = true;
          }
          break;            

        case SHUTTER_ERROR:
        default: 
          output = false;
          break; 
      }
    break;
    case SHUTTER_CLOSED: 
      debugD("Handling SHUTTER_CLOSED target status 0x%x" , domeHW );
      switch ( shutterStatus ) 
      {
        case SHUTTER_CLOSED:
          debugD("Current status : shutter already closed, nothing to do.");
          shutterStatus = SHUTTER_IDLE;
          targetStatus = SHUTTER_IDLE;
          break;
          
        case SHUTTER_CLOSING: //Detect sensor for shutter closing complete. 
          debugD("Current status : shutter closing.");
          if( targetAltitude != SHUTTER_ALTITUDE_DEFAULT && altitude > targetAltitude )
          {
            if( abs( altitude - targetAltitude) < SHUTTER_POSITIONING_ERROR_MAX )
            {
              debugD("Current status : shutter arrived at altitude.");              
              motorOff();
              shutterStatus = SHUTTER_IDLE;
            }
          }
          else if( domeHW.closedSensor == SWITCH_DOME_CLOSED )
          {
              debugD("Current status : shutter arrived closed.");                            
              motorOff();
              shutterStatus = SHUTTER_CLOSED;
          }
          //else leave motors alone - they should be closing the shutter
          output = true;
        break;

        case SHUTTER_OPEN:
          debugD("Current status : OPEN ");                            
          if( domeHW.openSensor == SWITCH_DOME_OPEN )
          {
            debugD("Closing shutter.");                            
            motorOff();
            motorOn( DIRECTION_CLOSE );
            shutterStatus = SHUTTER_CLOSING;
          }
        case SHUTTER_OPENING:
          //If its surprisingly still there. 
          if( domeHW.closedSensor == SWITCH_DOME_CLOSED )
          {
              debugD("Nothing to do - switches indicate closed already. updating status.");                            
              motorOff();
              shutterStatus = SHUTTER_CLOSED;
              shutterStatus = SHUTTER_IDLE;
              targetStatus = SHUTTER_IDLE;
          }
          else if( targetAltitude != SHUTTER_ALTITUDE_DEFAULT && altitude > targetAltitude )
          {
            if( abs( altitude - targetAltitude) < SHUTTER_POSITIONING_ERROR_MAX )
            {
              debugD("Shutter arrived at altitude - halted.");                            
              motorOff();
              shutterStatus = SHUTTER_IDLE;
              targetStatus = SHUTTER_IDLE;
            }
          }
          else //Change the direction to closing
          {
            debugD("Closing shutter.");                            
            motorOff();
            delay(1000);
            shutterStatus = SHUTTER_IDLE;
            motorOn( DIRECTION_CLOSE );
            shutterStatus = SHUTTER_CLOSING;
          }
          //else leave motors alone - they should be closing the shutter
          output = true;
        break;
          
        case SHUTTER_IDLE:
        //We go to idle when nothing is happening. We might be open or closed or anywhere in between 
        //Seems to be a bit of redundant overlap with closed or open.
          if ( domeHW.closedSensor == SWITCH_DOME_CLOSED  ) 
          {          
            debugD("Already closed - updating current status.");                            
            motorOff();
            shutterStatus = SHUTTER_IDLE;
            targetStatus = SHUTTER_IDLE;
            output = true;
          }
          else
          {
            debugD("Closing shutter.");                            
            motorOn( DIRECTION_CLOSE );
            shutterStatus = SHUTTER_CLOSING;//Handle next time around 
          }          
        break;
        case SHUTTER_ERROR:
        default:
          break;
      }    
      break;
    case SHUTTER_IDLE: //Treat as equivalent to abort when received as a command. 
      debugD("Handling target:IDLE, hardware status 0x%x" , domeHW );
      if ( shutterStatus == SHUTTER_OPENING || shutterStatus == SHUTTER_CLOSING )
        debugW("Stopping shutter and moving to idle as requested from %s" , shutterStatusChArr[shutterStatus] );
      motorOff();
      shutterStatus = SHUTTER_IDLE;      
      break;
    
    //there is an error condition. 
    case SHUTTER_ERROR:
    default: 
      //do something to reset the status and tell the world. 
      myTopic = outFnTopic;
      myTopic.concat("shutter");
      //myTopic.concat( myHostname );
      myError = F("Shutter sensor error, one or both shutter sensors are not reflecting reality - closing to reset.");
      client.publish( myTopic.c_str() , myError.c_str() );
      debugV( "Published to %s: %s \n", myTopic.c_str() , myError.c_str() ); 
      
      //turn off motor
      motorOff();

      //Wait a bit.
      delay(1000);
      //Set it to close for safety reasons. 
      debugW("Moving from SHUTTER_ERROR to SHUTTER_CLOSED for safety reasons." );
      targetStatus = SHUTTER_CLOSED;
      motorOn( DIRECTION_CLOSE );
      shutterStatus = SHUTTER_CLOSING;
      output = false;
      break;    
  }    
    
   return output;
 }
  /*
   * Web server handler functions
   */
  void handleNotFound()
  {
  String message = "URL not understood\n";
  message.concat( "Simple read: http://");
  message.concat( myHostname );
  message.concat ( " (GET) \n");
  message.concat( "Status read: http://");
  message.concat( myHostname );
  message.concat ( "/shutter (GET) \n");
  message.concat( "Status update: http://");
  message.concat( myHostname );
  message.concat ( "/shutter?status=open|closed|abort (PUT) \n");
  message.concat( "Status update: http://");
  message.concat( myHostname );
  message.concat ( "/shutter?altitude=0-110 (PUT) \n");
  server.send(404, "text/plain", message);
  }

  //Return shutter status
  void handleShutterStatusGet()
  {
    //enum shutterState  { SHUTTER_OPEN, SHUTTER_CLOSED, SHUTTER_OPENING, SHUTTER_CLOSING, SHUTTER_ERROR, SHUTTER_IDLE }; //ASCOM defined constants.
    String timeString = "", message = "";
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.createObject();

    root["time"]            = getTimeAsString( timeString );
    root["service"]         = F("ASCOM Dome shutter controller");
    root["status string"]   = shutterStatusChArr[ (int) shutterStatus ];
    root["status"]          = (int) shutterStatus;
    root["position"]        = altitude;
    root["latchRelay"]      = (domeHW.latchRelay == 1)? "unlatched":"latched";
    root["led"]             = domeHW.led;
    root["closed sensor"]   = domeHW.closedSensor;
    root["open sensor"]     = domeHW.openSensor;
    root["motor en a"]      = domeHW.motorEnA;
    root["motor en b"]      = domeHW.motorEnB;

    root.printTo(message);
    server.send(200, "application/json", message);
    return;
  }

  /* Update shutter state via PUT
   * Options - 
   *  shutter = open: opens shutter fully after unlock 
   *  shutter = close: close shutter fully and lock
   *  altitude = <number 0-100%> move shutter to altitude - 100 is fully open
   */
  void handleShutterStatusPut()
  {
    String timeString = "", message = "";
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.createObject();

    root["time"] = getTimeAsString( timeString );

    if( server.hasArg( "shutter" )  )
    {
      if( server.hasArg( "open") )
      {
        //update shutter logical state
        targetStatus = SHUTTER_OPENING;
      }
      else if ( server.hasArg( "close") )
      {
        //update shutter logical state
        targetStatus = SHUTTER_CLOSING;
      }        
      else if ( server.hasArg( "abort") )      
      {
        //disable motor
        motorOff();

        //update shutter logical state
        targetStatus = SHUTTER_IDLE;        
      }
    }
    //This needs a sensor implementing on the shutter to measure actual position - inclinometer or gyro ?
    else if( server.hasArg( "altitude" ) )
    {
      int targetPosition = 0; 
     
      targetPosition = server.arg("altitude").toInt();
      if( targetPosition >= SHUTTER_ALTITUDE_MIN && targetPosition <= SHUTTER_ALTITUDE_MAX ) 
      {
        targetAltitude = targetPosition;
        if( altitude > targetAltitude ) 
        {
          motorOn( DIRECTION_CLOSE );
          targetStatus = SHUTTER_CLOSED;
        }
        else
        {
          motorOn( DIRECTION_OPEN );
          targetStatus = SHUTTER_OPEN;
        }
      }
    }
    root["target status"]     = (int) targetStatus;
    root["status"]            = (int) shutterStatus;
    root["interface status"]  =  BIT_PACK_DOMEHW(domeHW);
    root["altitude"]          = (int) targetAltitude;
    root.printTo(message);
    server.send(200, "application/json", message);
    return;  
  }
  
  //Return sensor status
  void handleRoot()
  {
    String timeString = "", message = "";
    char temp[MAX_STRING_LENGTH];
    
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.createObject();

    root["time"]            = getTimeAsString( timeString );
    root["service"]         = F("ASCOM Dome shutter controller");
    root["status string"]   = shutterStatusChArr[ (int) shutterStatus ];
    root["status"]          = (int) shutterStatus;
    root["position"]        = altitude;
    root["latchRelay"]      = (domeHW.latchRelay == LATCH_POWERED )? "unlocked":"locked";
    root["led"]             = domeHW.led;
    root["closed sensor"]   = domeHW.closedSensor;//lower switch
    root["open sensor"]     = domeHW.openSensor;  //upper switch
    root["motor enable a"]  = domeHW.motorEnA;
    root["motor enable b"]  = domeHW.motorEnB;
    
    root.printTo(message);
    server.send(200, "application/json", message);
  }

  void handleShutterRestartPut()
  {
    //Trying to do a redirect to the rebooted host so we pick up from where we left off. 
    server.sendHeader( WiFi.hostname().c_str(), String("/status"), true);
    server.send ( 302, "text/plain", "<!Doctype html><html>Redirecting for restart</html>");
    device.restart();
  }

#endif
