#ifndef _DOME_SHUTTER_H
#define _DOME_SHUTTER_H
#include "PCF8574.h"

//Struct for accessing PCF8574  I2C port expander used as the shutter control interface
typedef struct { //Using bit-packing - I hope - test:compiles at least.
  uint8_t led:1;
  uint8_t latchRelay:1;
  uint8_t nused:2;        //could be as large as 11pins for the 16-bit wide version of the PCF8574
  uint8_t closedSensor:1; //Normally closed (0v) microswitch which goes high (3.3v) when shutter closes the switch.
  uint8_t openSensor:1;   //Normally closed (0v) microswitch which goes high (3.3v )when shutter closes the switch.
  uint8_t motorEnA:1;      //DC motor half-bridge enable pin
  uint8_t motorEnB:1;    //DC motor haf-bridge direction pin.
} domeHWControl_t;
#define BIT_PACK_DOMEHW(a) (a.led | (a.latchRelay <<1) | ( a.closedSensor << 4) | ( a.openSensor << 5) | ( a.motorEnA << 6) | ( a.motorEnB << 7) )

#define PIN_LED        0
#define PIN_LOCK       1
#define PIN_OPENSWITCH 4
#define PIN_CLOSEDSWITCH 5
#define PIN_MOTOR_ENA  6
#define PIN_MOTOR_ENB  7

#define DIRECTION_OPEN 0
#define DIRECTION_CLOSE 1 

#define CCW 0
#define CW  1
//Power the latch to withdrawl the pawl- ie unlock it. 
#define LATCH_POWERED 0
#define LATCH_UNPOWERED 1

//Default switch (NC) state - outputs are low
#define SWITCH_DOME_UNSWITCHED 0
#define SWITCH_DOME_OPEN 1
#define SWITCH_DOME_CLOSED 1

#define SHUTTER_ALTITUDE_MIN 10 //below this its closed!
#define SHUTTER_ALTITUDE_MAX 110
#define SHUTTER_ALTITUDE_DEFAULT 0 //Used to indicate not interested in altitude. 
#define SHUTTER_POSITIONING_ERROR_MAX 5

//define a driver chip model for powering the motor
#define DRIVER_8871 0
#define DRIVER_4833 1
#define DRIVER_BTS7960 2
//then say which one ...
#define MOTORDRIVER DRIVER_BTS7960

//Copied from ASCOM standards - beware of repeat definition.
//The commands we might be asked for are open, close, abort or set shutter to altitude X
enum shutterState  { SHUTTER_OPEN=0, SHUTTER_CLOSED, SHUTTER_OPENING, SHUTTER_CLOSING, SHUTTER_ERROR, SHUTTER_IDLE }; //ASCOM defined constants.
const char* shutterStatusChArr[] = { "SHUTTER_OPEN", "SHUTTER_CLOSED", "SHUTTER_OPENING", "SHUTTER_CLOSING", "SHUTTER_ERROR", "SHUTTER_IDLE" }; 


//
#endif
