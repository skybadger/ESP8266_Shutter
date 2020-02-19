#if ! defined _DOMESHUTTER_HANDLERS_H_
#define _DOMESHUTTER_HANDLERS_H_

//handler definitions
void handleShutterStatusGet( void );
void handleShutterStatusPut( void );
void handleNotFound( void );
void handleRoot( void );

 //Motion handler
 bool handleShutter( void );
 
/*
  * Handler for shutter processing  - not a web handler
  * false == nothing done
 */
 bool handleShutter( void )
 {
  String myError;
  String myTopic;

  bool output = false; 
  if( shutterStatus == SHUTTER_IDLE )
    return output = false;
  
  switch ( shutterStatus )
  {
    case SHUTTER_OPENING://Detect sensor for shutter opening complete. 
      if( domeHW.openSensor ) 
      {
          //turn off motor
          domeHW.motorEn = 0;
          shutter.write( PIN_MOTOR_ENABLE, domeHW.motorEn );          
          //update state
          shutterStatus = SHUTTER_OPEN;
      }
      output = true;
      break;
    case SHUTTER_CLOSING: //Detect sensor for shutter closing complete. 
      if( domeHW.closedSensor )
      {
          //turn off motor
          domeHW.motorEn = 0;
          shutter.write( PIN_MOTOR_ENABLE, domeHW.motorEn );          
          //update state
          shutterStatus = SHUTTER_CLOSED;
      }
      output = true;
      break;
    //Keeps the compiler quiet && check for error conditions
    case SHUTTER_OPEN:
      if ( !domeHW.openSensor ) 
        shutterStatus = SHUTTER_ERROR;
      break;  
    case SHUTTER_CLOSED:
      if ( !domeHW.closedSensor ) 
        shutterStatus = SHUTTER_ERROR;
      break;
       
    case SHUTTER_ERROR:
      //do something to reset the status and tell the world. 
      myTopic = outHealthTopic;
      myTopic.concat( myHostname );
      myError = F("Shutter sensor error, one or both shutter sensors are not reflecting reality - closing to reset.");
      client.publish( myTopic.c_str() , myError.c_str() );
      Serial.printf( "Published to %s: %s \n", myTopic.c_str() , myError.c_str() ); 
      
      //Turn the motor off
      shutter.write( PIN_MOTOR_ENABLE, domeHW.motorEn = 0);
      //Wait a bit.
      delay(5);
      //Set it to close for safety reasons. 
      shutter.write( PIN_MOTOR_DIRN, domeHW.motorDirn = CCW );
      shutter.write( PIN_MOTOR_ENABLE, domeHW.motorEn = 1 );
      shutterStatus = SHUTTER_CLOSING;
      break;
    case SHUTTER_IDLE:
    default:
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
    static const char* shutterStatusChArr[6] = { "SHUTTER_OPEN", "SHUTTER_CLOSED", "SHUTTER_OPENING", "SHUTTER_CLOSING", "SHUTTER_ERROR", "SHUTTER_IDLE" }; 
    String timeString = "", message = "";
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.createObject();

    root["time"]            = getTimeAsString( timeString );
    root["service"]         = F("ASCOM Dome shutter controller");
    root["status string"]   = shutterStatusChArr[ (int) shutterStatus ];
    root["status"]          = (int) shutterStatus;
    root["position"]        = altitude;
    root["latchRelay"]      = (domeHW.latchRelay == 1)? "unlocked":"locked";
    root["led"]             = domeHW.led;
    root["closed sensor"]   = domeHW.closedSensor;
    root["open sensor"]     = domeHW.openSensor;
    root["motor enabled"]   = domeHW.motorEn;
    root["motor direction"] = domeHW.motorDirn;

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
    if( shutterStatus == SHUTTER_OPENING || shutterStatus == SHUTTER_CLOSING )//i.e. not shutter idle or error
    {
      root["error"] = "Can't update state when currently moving";
      root.printTo(message);
      server.send(500, "application/json", message);
      return;
    }

    if( server.hasArg( "shutter" )  )
    {
      if( server.hasArg( "open") )
      {
        //unlock 
        domeHW.latchRelay = 1;
        shutter.write( PIN_LOCK, domeHW.latchRelay );
        //set timeout timer - 2.5 secs - for lock relay
        ets_timer_arm_new( &timeoutTimer, 2500, 0/*one-shot*/, 1);         
        //enable motor
        domeHW.motorDirn = CW;
        shutter.write( PIN_MOTOR_DIRN, (uint8_t) domeHW.motorDirn );
        domeHW.motorEn = 1;
        shutter.write( PIN_MOTOR_ENABLE, (uint8_t) domeHW.motorEn );
        //update shutter logical state
        shutterStatus = SHUTTER_OPENING;
      }
      else if ( server.hasArg( "close") )
      {
          //enable motor
          domeHW.motorDirn = CCW;
          shutter.write( PIN_MOTOR_DIRN, (uint8_t) domeHW.motorDirn );
          domeHW.motorEn = 1;
          shutter.write( PIN_MOTOR_ENABLE, (uint8_t) domeHW.motorEn );
          //update shutter logical state
          shutterStatus = SHUTTER_CLOSING;
      }        
      else if ( server.hasArg( "abort") )      
      {
          //disable motor
          domeHW.motorDirn = CCW;
          shutter.write( PIN_MOTOR_DIRN, (uint8_t) domeHW.motorDirn );
          domeHW.motorEn = 0;
          shutter.write( PIN_MOTOR_ENABLE, (uint8_t) domeHW.motorEn );
          //update shutter logical state
          shutterStatus = SHUTTER_ERROR;        
      }
    }
    else if( server.hasArg( "altitude" ) )
    {
      int targetPosition = server.arg("altitude").toInt();
      if( domeHW.openSensor ^ domeHW.closedSensor ) //then we know its parked at top or bottom 
      {
        ;;//To do  - when it can be measured
      }
    }
  }
  
  //Return sensor status
#define MAX_STRING_LENGTH 25
  void handleRoot()
  {
    String timeString = "", message = "";
    char temp[MAX_STRING_LENGTH];
    
    DynamicJsonBuffer jsonBuffer(256);
    JsonObject& root = jsonBuffer.createObject();

    sprintf( temp, "0b%b", domeHW );
    root["time"] = getTimeAsString( timeString );
    root["status"] = temp;
    root.printTo(message);
    server.send(200, "application/json", message);
  }
#endif
