#ifndef _DOME_SHUTTER_H
#define _DOME_SHUTTER_H
#include "PCF8574.h"

//Struct for accessing PCF8574  I2C port expander used as the shutter control interface
typedef struct { //Using bit-packing - I hope - test:compiles at least.
  short int led:1;
  short int latchRelay:1;
  short int closedSensor:1; //Normally closed (0v) microswitch which goes high (3.3v) when shutter closes the switch.
  short int openSensor:1;   //Normally closed (0v) microswitch which goes high (3.3v )when shutter closes the switch.
  short int motorEn:1;      //DC motor half-bridge enable pin
  short int motorDirn:1;    //DC motor haf-bridge direction pin.
  short int nused:2;        //could be as large as 11pins for the 16-bit wide version of the PCF8574
} domeHWControl_t;

#define PIN_LOCK 1
#define PIN_MOTOR_ENABLE  5
#define PIN_MOTOR_DIRN    4
#define PIN_LED           0
#define CCW 0
#define CW  1

//Copied from ASCOM standards - beware of repeat definition.
enum shutterState  { SHUTTER_OPEN=0, SHUTTER_CLOSED, SHUTTER_OPENING, SHUTTER_CLOSING, SHUTTER_ERROR, SHUTTER_IDLE }; //ASCOM defined constants.

//
#endif
