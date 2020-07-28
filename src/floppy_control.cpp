/*
   This Module handles all the low level access to the drive, reading, writing,
   moving the head, i think you get it
*/
#include <Arduino.h>
#include "vars_n_defs.h"
#include "floppy_control.h"
//#include "MTPADF.h"
#include "utility_functions.h"
#include "adflib_helper.h"
#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

  int _index, _drivesel, _motorA, _motorB, _dir, _step, _writedata, _writeen, _track0, _wprot, _readdata, _side, _diskChange;
  boolean shugartDrive = false;
  volatile boolean motor = false;
  uint32_t motorTime = 0;
  struct tfloppyPos floppyPos;
  int densMode = DD;
#ifdef hdmode
  boolean getDensity = true;
#else
boolean getDensity = false;
#endif

  /*
   wait for index hole
*/
  void waitForIndex()
  {
    motorOn();
    while (digitalRead(_index) == 1)
      ;
    while (digitalRead(_index) == 0)
      ;
  }

  /*
   starts the motor if it isnt running yet
*/
  void motorOn()
  {
    motorTime = millis();
    driveSelect();
    if (motor == false)
    {
      digitalWriteFast(_motorB, LOW);
      if (!shugartDrive)
        digitalWriteFast(_motorA, LOW);
      motor = true;
      delay(settings.motorSpinupDelay); // more than plenty of time to spinup motor
      if (getDensity)
        densityDetect();
    }
  }

  /*
   stops motor
*/
  void motorOff()
  {
    driveSelect();
    motor = false;
    digitalWriteFast(_motorB, HIGH);
    if (!shugartDrive)
      digitalWriteFast(_motorA, LOW);
    delayMicroseconds(settings.motorSpindownDelay);
    driveDeselect();
  }

  void driveSelect()
  {
#ifdef debug
    //if (!motor) Serial.println("driveSelect");
    digitalWrite(13, HIGH);
#endif
    digitalWriteFast(_drivesel, LOW);
    delayMicroseconds(settings.driveSelectDelay);
  }

  void driveDeselect()
  {
#ifdef debug
    //if (!motor) Serial.println("driveDeselect");
    digitalWrite(13, LOW);
#endif
    digitalWriteFast(_drivesel, HIGH);
    delayMicroseconds(settings.driveDeselectDelay);
  }

  /*
   selects travel direction of head
   dir = 0   outwards to track 0
   dir !=0 inwards to track 79
*/
  void setDir(int dir)
  {
    floppyPos.dir = dir;
    if (dir == 0)
    {
      digitalWriteFast(_dir, HIGH);
    }
    else
    {
      digitalWriteFast(_dir, LOW);
    }
    delayMicroseconds(settings.setDirDelay);
  }

  /*
   selects side to read/write
   side = 0  upper side
   side != 0 lower side
*/
  void setSide(int side)
  {
    if (side == 0)
    {
      digitalWriteFast(_side, HIGH);
    }
    else
    {
      digitalWriteFast(_side, LOW);
    }
    delayMicroseconds(settings.setSideDelay);
    floppyPos.side = side;
  }

  /*
   steps one track into the direction selected by setDir()
*/
  void step1()
  {
    digitalWriteFast(_step, LOW);
    delayMicroseconds(settings.stepDelay);
    digitalWriteFast(_step, HIGH);
    delay(settings.stepSettleDelay);
    if (floppyPos.dir == 0)
    {
      floppyPos.track--;
    }
    else
    {
      floppyPos.track++;
    }
  }

  /*
   move head to track 0
*/
  int seek0()
  {
    int trkCnt = 0;
    setDir(0);
    while (digitalRead(_track0) == 1)
    {
      step1();
      trkCnt++;
      if (trkCnt > 85)
      {
        extError = TRACK0_ERR;
        return -1;
      }
    }
    currentTrack = 0;
    floppyPos.track = 0;
    extError = NO_ERR;
    return 0;
  }

  /*
   moves head to physical track (0-xx)
*/
  int gotoTrack(int track)
  {
    motorOn();
    if (track == 0)
    {
      currentTrack = -1;
    }
    int steps = 0;
    if (track < 0)
      return -1;
    if (currentTrack == -1)
    {
      if (seek0() == -1)
      {
        return -1;
      }
    }
    if (track == currentTrack)
    {
      return 0;
    }
    if (track < currentTrack)
    {
      setDir(0);
      steps = currentTrack - track;
      currentTrack = track;
    }
    else
    {
      setDir(1);
      steps = track - currentTrack;
      currentTrack = track;
    }
    for (int i = 0; i < steps; i++)
    {
      step1();
    }
    return 0;
  }

  /*
   moves head to logical amiga track (0-159)
*/
  void gotoLogicTrack(int track)
  {
    if (track > 164)
      track = 164;
    logTrack = track;
    setSide(track % 2);
    gotoTrack(track / 2);
    delay(settings.gotoTrackSettle);
  }

  /*
   prints some stuff, mostly for debugging disk signals
*/
  void printStatus()
  {
    driveSelect();
    setDir(0);
    step1();
    seek0();
    int dsel;
    switch (_drivesel)
    {
    case 20:
      dsel = 0;
      break;
    case 16:
      dsel = 1;
      break;
    case 21:
      dsel = 2;
      break;
    default:
      dsel = -1;
      break;
    }
    Serial.printf("/Trk0: %s /WProt: %s /Disk Change: %d \nTrack: %d Side: %d Dir: %d DriveSel: DS%d Shugart/IBM Bus: %s\n",
                  !(digitalRead(_track0)) ? "true" : "false", !(digitalRead(_wprot)) ? "ON" : "OFF", (digitalRead(_diskChange)),
                  floppyPos.track, floppyPos.side, floppyPos.dir, dsel, shugartDrive ? "Shugart" : "IBM?");
    Serial.printf("Density: %s getDensity: %s preErase: %s\nWriteSize: %d StreamLen: %d \nMotor: %d IndexAlign: %s\n",
                  densMode ? "HD" : "DD", getDensity ? "true" : "false", preErase ? "true" : "false", writeSize, streamLen, motor, indexAligned ? "true" : "false");
  }

  boolean writeProtect()
  {
    if (digitalRead(_wprot) == 0)
      wProt = true;
    else
      wProt = false;
    return wProt;
  }

  /*
   checks if disk has changed / is inserted, do one step and check status again because the signal gets
   updated after one step
   returns 1 = disk is in drive, 0 = do disk in drive
*/
  int diskChange() // returns if a disk is inserted: 0 = no disk, 1 = disk inserted
  {
    int return_value = 0;
    driveSelect();
    if (digitalRead(_diskChange) == 1)
    {
      if (writeProtect())
        wProt = true;
      else
        wProt = false;
      return_value = 1;
    }
    else
    {
      seek0();
      setDir(0);
      step1();
      if (writeProtect())
        wProt = true;
      else
        wProt = false;
      return_value = digitalRead(_diskChange);
    }
    if (return_value)
      noDisk = false;
    else
      noDisk = true;
    if (noDisk)
      driveDeselect();
    return return_value;
  }

  void diskChangeIRQ()
  {
    if (!dchgActive)
      return;
    //  digitalWrite(13, HIGH);
    if (motor == true)
      motorOff();
#ifdef hdmode
    getDensity = true;
#endif
    noDisk = true;
    if (mtpOn)
    {
      if (ADFisOpen)
      {
        mtpd.sendEvent(MTP_EVENT_STORE_REMOVED, FDid);
        mtpd.sendEvent(MTP_EVENT_STORE_REMOVED, FSid);
        closeDrive();
      }
    }
    dchgActive = false;
  }

  void isrRPM()
  {
    if (indexCount > 9)
      return;
    indexes[indexCount] = micros();
    indexCount++;
  }

  /*
   measures the rotational speed of the drive in microseconds
*/
  uint32_t measureRPM()
  {
    for (int i = 0; i < 10; i++)
      indexes[i] = 0;
    indexCount = 0;
    motorOn();
    attachInterrupt(_index, isrRPM, FALLING);
    delay(1800);
    detachInterrupt(_index);
    uint32_t timeRPM = 0;
    for (int i = 0; i < indexCount - 1; i++)
    {
      timeRPM = timeRPM + (indexes[i + 1] - indexes[i]);
    }
    timeRPM /= indexCount - 1;
    RPMtime = timeRPM;
    return timeRPM;
  }

  void fluxCounter()
  {
    fluxCount++;
  }

  /*
   counts the transitions of one revolution (200ms)
*/
  int countFluxes()
  {
    motorOn();
    attachInterrupt(_readdata, fluxCounter, FALLING);
    fluxCount = 0;
    //    delayMicroseconds(RPMtime);
    delay(RPMtime / 1000);
    detachInterrupt(_readdata);
    return fluxCount;
  }
/*
   tries to detect how the teensy is connected to the drive by
   stepping the head to track0, if track0 switches to low we have a hit.
   there are two different schematics at the moment: breadboard and pcb
*/
#define pcbVer 1
#define breadboardVer 0
  int hardwareVersion()
  {
    watchdogReset();
    /*
IBM PC Bus
10	/MOTEA	-->	Motor Enable A
12	/DRVSB	-->	Drive Sel B
14	/DRVSA	-->	Drive Sel A
16	/MOTEB	-->	Motor Enable B

Shugart Bus
10	/DS0	-->	Device Select 0
12	/DS1	-->	Drive Sel B
14	/DS2	-->	Device Select 2
16	/MTRON	-->	Motor On
*/

// check for pcb v3/v4/v6 pinout version, v1&v2 were prototypes and v5 was never released
#define DS0 20 //driveSelect 0 FloppyBus pin 10
#define DS1 16 //driveSelect 1 FloppyBus pin 12
#define DS2 21 //driveSelect 2 FloppyBus pin 14
#define DriveDirection 18
#define DriveStep 19
#define DriveTrack0 7
    pinMode(DS0, OUTPUT);                   //drivesel
    pinMode(DS1, INPUT_PULLUP);             //drivesel
    pinMode(DS2, INPUT_PULLUP);             //drivesel
    digitalWriteFast(DS0, LOW);             //select drive
    pinMode(DriveDirection, OUTPUT);        //direction
    digitalWriteFast(DriveDirection, HIGH); //-> track 0
    pinMode(DriveStep, OUTPUT);             // set step pin to output
    pinMode(DriveTrack0, INPUT_PULLUP);     // track 0 as input
    for (int i = 0; i < 85; i++)
    { // move head to track 0
      digitalWriteFast(DriveStep, LOW);
      delayMicroseconds(settings.stepDelay);
      digitalWriteFast(DriveStep, HIGH);
      delay(settings.stepSettleDelay);
      if (digitalRead(DriveTrack0) == 0)
      {
        _drivesel = DS0;
        shugartDrive = true;
        return pcbVer; // track 0? -> hardware rev pcb
      }
    }
    pinMode(DS0, INPUT_PULLUP); // switch previous used pins to input
    pinMode(DS1, OUTPUT);       //drivesel
    digitalWriteFast(DS1, LOW); //select drive
    for (int i = 0; i < 85; i++)
    { // move head to track 0
      digitalWriteFast(DriveStep, LOW);
      delayMicroseconds(settings.stepDelay);
      digitalWriteFast(DriveStep, HIGH);
      delay(settings.stepSettleDelay);
      if (digitalRead(DriveTrack0) == 0)
      {
        _drivesel = DS1;
        return pcbVer; // track 0? -> hardware rev pcb
      }
    }
    pinMode(DS1, INPUT_PULLUP); // switch previous used pins to input
    pinMode(DS2, OUTPUT);       //drivesel
    digitalWriteFast(DS2, LOW); //select drive
    for (int i = 0; i < 85; i++)
    { // move head to track 0
      digitalWriteFast(DriveStep, LOW);
      delayMicroseconds(settings.stepDelay);
      digitalWriteFast(DriveStep, HIGH);
      delay(settings.stepSettleDelay);
      if (digitalRead(DriveTrack0) == 0)
      {
        _drivesel = DS2;
        return pcbVer; // track 0? -> hardware rev pcb
      }
    }

    pinMode(DriveDirection, INPUT);
    pinMode(DriveStep, INPUT);
    pinMode(DriveTrack0, INPUT);

    // check for breadboard pinout version
    pinMode(4, OUTPUT);        //drivesel
    digitalWrite(4, LOW);      //select drive
    pinMode(6, OUTPUT);        //direction
    digitalWriteFast(6, HIGH); //-> track 0
    pinMode(7, OUTPUT);        // set step pin to output
    pinMode(10, INPUT_PULLUP); // track 0 as input
    for (int i = 0; i < 85; i++)
    { // move head to track 0
      digitalWriteFast(7, LOW);
      delayMicroseconds(settings.stepDelay);
      digitalWriteFast(7, HIGH);
      delay(settings.stepSettleDelay);
      if (digitalRead(10) == 0)
        return breadboardVer; // track 0? -> hardware rev breadboard
    }
    pinMode(4, INPUT); // switch previous used pins to input
    pinMode(6, INPUT);
    pinMode(7, INPUT);
    pinMode(10, INPUT);
    return -1; // unknown pinout or drive broken
  }

  /*
   sets up the registers according to the hardware version
*/
  void registerSetup(int version)
  {
    if (version == pcbVer)
    {
      //    _dens = 14;      //2 density select IN
      _index = 15; //8 index OUT
      // _drivesel = 21;  //14 drive select A IN
      //      _drivesel = 20;  //10 drive select A IN
      _motorA = 20; //10 motor A IN
      // _drivesel = 16;  //12 drive select B IN
      _motorB = 17;    //16 motor B on IN
      _dir = 18;       //18 direction IN
      _step = 19;      //20 step IN
      _writedata = 9;  //22 write data IN
      _writeen = 8;    //24 write enable IN
      _track0 = 7;     //26 track 0 OUT
      _wprot = 6;      //28 write protect OUT
      _readdata = 5;   //30 read data OUT (FTM0_CH7) *** do not change this pin ***
      _side = 4;       //32 head select IN
      _diskChange = 3; //34 disk change OUT

      // FlexTimerModule defines for Pin 5
      FTChannelValue = (uint32_t) & (FTM0_C7V); //0x40038048
      FTMx_CnSC = (uint32_t) & (FTM0_C7SC);     //0x40038044
      FTPinMuxPort = (uint32_t) & (PORTD_PCR7); //0x4004C01C
    }
    if (version == breadboardVer)
    {
      //    _dens = 2;      //2 density select IN
      _index = 3;    //8 index OUT
      _drivesel = 4; //12 drive select 1 IN
      _motorB = 5;   //16 motor1 on IN
      _motorA = _motorB;
      _dir = 6;         //18 direction IN
      _step = 7;        //20 step IN
      _writedata = 8;   //22 write data IN
      _writeen = 9;     //24 write enable IN
      _track0 = 10;     //26 track 0 OUT
      _wprot = 11;      //28 write protect OUT
      _readdata = 22;   //30 read data OUT (FTM0_CH0) *** do not change this pin ***
      _side = 14;       //32 head select IN
      _diskChange = 15; //34 disk change OUT

      // FlexTimerModule defines for Pin 22
      FTChannelValue = (uint32_t) & (FTM0_C0V); //0x40038010;
      FTMx_CnSC = (uint32_t) & (FTM0_C0SC);     //0x4003800C;
      FTPinMuxPort = (uint32_t) & (PORTC_PCR1); //0x4004B004;
    }
  }

#ifdef __cplusplus
}
#endif