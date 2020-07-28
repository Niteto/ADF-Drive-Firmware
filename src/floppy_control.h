/*
   This Module handles all the low level access to the drive, reading, writing,
   moving the head, i think you get it
*/
#ifndef FLOPPY_CONTROL_H /* include guards */
#define FLOPPY_CONTROL_H

#include <Arduino.h>
//#include "vars_n_defs.h"
#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

   extern int _index, _drivesel, _motorA, _motorB, _dir, _step, _writedata, _writeen, _track0, _wprot, _readdata, _side, _diskChange;
   extern volatile boolean motor;
   extern uint32_t motorTime;
   struct tfloppyPos
   {
      byte track;
      byte side;
      byte dir;
   };

   extern struct tfloppyPos floppyPos;
   extern int densMode;
#ifdef hdmode
   extern boolean getDensity;
#else
extern boolean getDensity;
#endif

   /* wait for index hole */
   void waitForIndex();

   /* starts the motor if it isnt running yet */
   void motorOn();

   /* stops motor */
   void motorOff();

   void driveSelect();

   void driveDeselect();

   /* selects travel direction of head, dir = 0 outwards to track 0, dir !=0 inwards to track 79 */
   void setDir(int dir);

   /* selects side to read/write, side = 0  upper side, side != 0 lower side */
   void setSide(int side);

   /* steps one track into the direction selected by setDir() */
   void step1();

   /* move head to track 0 */
   int seek0();

   /* moves head to physical track (0-xx) */
   int gotoTrack(int track);

   /* moves head to logical amiga track (0-159) */
   void gotoLogicTrack(int track);

   /* prints some stuff, mostly for debugging disk signals */
   void printStatus();

   boolean writeProtect();

   /* checks if disk has changed / is inserted, do one step and check status again because the signal gets updated after one step
   returns 1 = disk is in drive, 0 = do disk in drive */
   int diskChange();

   void diskChangeIRQ();

   void fluxCounter();

   uint32_t measureRPM();

   /* measures time for one rotation of the disk, returns milliseconds and updates the fluxCounter for detecting HD formatted disks */
   int countFluxes();

   /* tries to detect how the teensy is connected to the drive by stepping the head to track0, if track0 switches to low we have a hit.
   there are two different schematics at the moment: breadboard and pcb */
   int hardwareVersion();

   /* sets up the registers according to the hardware version */
   void registerSetup(int version);

#ifdef __cplusplus
}
#endif

#endif /* FLOPPY_CONTROL_H */