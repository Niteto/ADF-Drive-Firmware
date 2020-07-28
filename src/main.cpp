/*
   ADF-Copy/Drive - Reads and writes Amiga floppy disks
   Copyright (C) 2016-2019 Dominik Tonn (nick@niteto.de)

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
   encodeMFM is partly based upon floppySectorMfmEncode from FLOPPY.C which is part of the Win Fellow Project
   Authors: Petter Schau, Torsten Enderling (carfesh@gmx.net)
   Copyright (C) 1991, 1992, 1996 Free Software Foundation, Inc
   released under the GNU General Public License
*/

/*
   decodesMFM is partly based upon DecodeSectorData and DecodeLongword functions from AFR.C,
   written by Marco Veneri Copyright (C) 1997 and released as public domain
*/

/*
   MTP is based on MTP.h
   MTP.h - Teensy MTP Responder library
   Copyright (C) 2017 Fredrik Hubinette <hubbe@hubbe.net>
   With updates from MichaelMC, YoongHM
   see MTPADF.h for details
*/

/*
   This program uses a modified version of ADFLib
   http://lclevy.free.fr/adflib/

   ADF Library. (C) 1997-2002 Laurent Clevy
   ADFLib is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   ADFLib is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Foobar; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/* compile with following arduino ide settings:
    USB Type: MTP Disk (Experimental) + Serial
    CPU Speed: 96 Mhz (Overclock)
    Optimize: smallest code
*/

#include <Arduino.h>
#include "mk20dx128.h"
#include "vars_n_defs.h" //moved many defines and variables
#include "floppy_rw.h"
#include "floppy_control.h"
#include "adflib_helper.h"
#include "MTPADF.h"
#include "utility_functions.h"
#include "adf_util.h"

#ifdef __cplusplus
extern "C"
{
#endif
  /*
   Setup for the Watchdog Timer to 8 seconds
*/
  void startup_early_hook()
  {
    WDOG_TOVALL = 8000;
    WDOG_TOVALH = 0;
    WDOG_PRESC = 0;
    WDOG_STCTRLH = (WDOG_STCTRLH_ALLOWUPDATE | WDOG_STCTRLH_WDOGEN);
  }
#ifdef __cplusplus
}
#endif

#include "TimeLib.h"
#include "adf_nativ.h"

/*
   execute command
*/
void doCommand()
{
  inputString.replace((char)10, (char)0);
  inputString.replace((char)13, (char)0);
  cmd = inputString;
  param = inputString;
  if (inputString.indexOf(" ") > 0)
  {
    cmd.remove(inputString.indexOf(" "), inputString.length());
    param.remove(0, inputString.indexOf(" "));
  }
  else
  {
    cmd = inputString;
    param = "";
  }
  cmd.trim();
  param.trim();
  //    Serial.printf("cmd: '%s' param: '%s'\n",cmd.c_str(),param.c_str());
  //    Serial.println(cmd);
  //    Serial.println(param);
  if (cmd == "help")
  {
    Serial.print("-----------------------------------------------------------------------------\n");
    Serial.printf("ADF-Drive %s - Copyright (C) 2016-2020 Dominik Tonn (nick@niteto.de)\n", Version);
    Serial.print("-----------------------------------------------------------------------------\n");
    Serial.print("read <n> - read logical track #n\n");
    Serial.print("write <n> - write logical track #n\n");
    Serial.print("erase <n> - erase logical track #n\n");
    Serial.print("testwrite <n> - write logical track #n filled with 0-255\n");
    Serial.print("print - prints amiga track with header\n");
    Serial.print("error - returns encoded error in ascii\n");
    Serial.print("exterr - returns a detailed error message in ascii\n");
    Serial.print("weak - returns retry number for last read in binary format\n");
    Serial.print("get <n> - reads track #n silent\n");
    Serial.print("put <n> - writes track #n silent\n");
    Serial.print("download - download track to pc without headers in binary\n");
    Serial.print("upload - upload track to mcu without headers in binary\n");
    Serial.print("init - goto track 0\n");
    Serial.print("hist - prints histogram of track in ascii\n");
    Serial.print("flux - returns histogram of track in binary\n");
    Serial.print("index - prints index signal timing in ascii\n");
    Serial.print("name - reads track 80 an returns disklabel in ascii\n");
    Serial.print("dskcng - returns disk change signal in binary\n");
    Serial.print("dens - returns density type of inserted disk in ascii\n");
    Serial.print("info - prints state of various floppy signals in ascii\n");
    Serial.print("enc - encodes data track into mfm\n");
    Serial.print("dec - decodes raw mfm into data track\n");
    Serial.print("log - prints logical track / tracknumber extracted from sectors\n");
    Serial.print("showsettings - shows current floppy access timings\n");
    Serial.print("getsettings - gets current floppy access timings\n");
    Serial.print("setsettings - sets timing values for floppy access\n");
    Serial.print("resetsettings - resets all timing values to default\n");
    Serial.print("storesettings - stores current values in eeprom\n");
    Serial.print("restoresettings - restores timing values from eeprom\n");
    Serial.print("diskinfo - show info about the current disk\n");
    Serial.print("bitmap - dumps rootblockbitmap in binary\n");
  }
  if (cmd == "setsettings")
  {
    sscanf(param.c_str(), "%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
           &settings.motorSpinupDelay, &settings.motorSpindownDelay, &settings.driveSelectDelay,
           &settings.driveDeselectDelay, &settings.setDirDelay, &settings.setSideDelay,
           &settings.stepDelay, &settings.stepSettleDelay, &settings.gotoTrackSettle,
           &settings.sdRetries, &settings.hdRetries, &settings.config,
           &settings.reserved3, &settings.reserved4, &settings.reserved5,
           &settings.reserved6, &settings.reserved7);
    Serial.println("OK");
  }
  if (cmd == "getsettings")
  {
    Serial.printf("%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu\n",
                  settings.motorSpinupDelay, settings.motorSpindownDelay, settings.driveSelectDelay,
                  settings.driveDeselectDelay, settings.setDirDelay, settings.setSideDelay,
                  settings.stepDelay, settings.stepSettleDelay, settings.gotoTrackSettle,
                  settings.sdRetries, settings.hdRetries, settings.config,
                  settings.reserved3, settings.reserved4, settings.reserved5,
                  settings.reserved6, settings.reserved7);
    Serial.println("OK");
  }
  if (cmd == "showsettings")
  {
    printTimings(settings);
    Serial.print("MTP is ");
    if (mtpOn)
      Serial.println("ON");
    else
      Serial.println("OFF");
    Serial.println("OK");
  }
  if (cmd == "resetsettings")
  {
    setDefaultTimings();
    Serial.println("OK");
  }
  if (cmd == "cleareeprom")
  {
    for (int i = 0; i < 80; i++)
    {
      EEPROM.write(i, 0xff);
    }
    Serial.println("OK");
  }
  if (cmd == "storesettings")
  {
    storeTimings();
    Serial.println("OK");
  }
  if (cmd == "restoresettings")
  {
    if (restoreTimings() == 0)
      Serial.println("OK");
    else
      Serial.println("Checksum Error");
  }
  if (cmd == "read")
  {
    Serial.printf("Reading Track %d\n", param.toInt());
    doRead(param.toInt(), false);
    Serial.printf("Track: %d Side: %d Dir: %d CurrentTrack: %d LogicalTrack: %d\n",
                  floppyPos.track, floppyPos.side, floppyPos.dir, currentTrack, logTrack);
  }

  if (cmd == "read81")
  {
    Serial.printf("Reading Track %d\n", param.toInt());
    doRead(param.toInt(), true);
    Serial.printf("Track: %d Side: %d Dir: %d CurrentTrack: %d LogicalTrack: %d\n",
                  floppyPos.track, floppyPos.side, floppyPos.dir, currentTrack, logTrack);
  }

  if (cmd == "write")
  {
    busy(true);
    Serial.printf("Writing Track %d\n", param.toInt());
    gotoLogicTrack(param.toInt());
    errors = writeTrack(preErase);
    if (errors != -1)
    {
      Serial.println("OK");
    }
    else
    {
      Serial.println("Write failed!");
    }
  }

  if (cmd == "erase")
  {
    busy(true);
    uint32_t tTrans = transTimeDD;
    int temp = 0;
    int tTrack = 0;
    sscanf(param.c_str(), "%d %d", &tTrack, &temp);
    tTrans = temp / 2.08;
    //    Serial.printf("Erasing Track: %d tTime: ", tTrack);
    //    Serial.println(tTrans);
    gotoLogicTrack(tTrack);
    errors = eraseTrack(tTrans);
    //    Serial.println(writeBitCnt);
    if (errors != -1)
    {
      Serial.println("OK");
    }
    else
    {
      Serial.println("Erase failed!");
    }
  }
  if (cmd == "edisk")
  {
    busy(true);
    Serial.println("Erasing Disk.");
    errors = eraseDisk(400/2.08);
    Serial.println(writeBitCnt);
    if (errors != -1)
    {
      Serial.println("OK");
    }
    else
    {
      Serial.println("Erase failed!");
    }
  }
  if (cmd == "ttest")
  {
    Serial.printf("Testing Track %d\n", param.toInt());
    gotoLogicTrack(param.toInt());
    for (int i = 60; i <= 120; i = i + 2)
    {
      eraseTrack(i);
      countFluxes();
      Serial.print(i);
      Serial.printf(" write: %6d read: %6d diff: %5d\n", writeBitCnt, fluxCount, writeBitCnt - fluxCount);
    }
  }
  if (cmd == "probe")
  {
    gotoLogicTrack(param.toInt());
    Serial.println(probeTrack(false));
  }
  if (cmd == "vprobe")
  {
    gotoLogicTrack(param.toInt());
    Serial.println(probeTrack(true));
  }
  if (cmd == "preerase")
  {
    preErase = true;
    Serial.println("preerase on");
  }
  if (cmd == "noerase")
  {
    preErase = false;
    Serial.println("preerase off");
  }
  if (cmd == "indexalign")
  {
    if (param.toInt() == 0)
      indexAligned = false;
    else
      indexAligned = true;
  }

  if (cmd == "put")
  {
    busy(true);
    trackTemp = param.toInt();
    floppyTrackMfmEncode(trackTemp, (byte *)track, stream);
    gotoLogicTrack(trackTemp);
    errors = writeTrack(preErase);
    if (errors == 0)
    {
      Serial.println("OK");
    }
    else
    {
      Serial.println("ERROR");
    }
  }

  if (cmd == "print")
  {
    printTrack();
    Serial.println("OK");
  }

  if (cmd == "raw")
  {
    for (uint32_t i = 0; i < streamLen; i++)
    {
      if (i % 32 == 0)
      {
        Serial.println();
      }
      Serial.printf("%.2x", stream[i]);
      if (i % 2 == 1)
        Serial.print(" ");
    }
    Serial.println("\nOK");
  }

  if (cmd == "getstream")
  {
    uint32_t tZeit = millis();
    for (int i = 0; i < 10; i++)
    {
      Serial.write(stream, 20480);
    }
    Serial.printf("\nTrans took %dms\n", millis() - tZeit);
  }
  if (cmd == "capf")
  {
    busy(true);
    captureFlux(false, param.toInt());
    Serial.printf("Long Bits: %d Overflows: %d\n", hist[0], hist[1]);
  }
  if (cmd == "getcells")
  {
    busy(true);
    captureFlux(true, param.toInt());
  }

  if (cmd == "getindex")
  {
    getIndexes(param.toInt());
  }

  if (cmd == "gettrans")
  {
    getTransCount();
  }
  if (cmd == "getpktcnt")
  {
    getPacketCnt();
  }

  if (cmd == "dump")
  {
    byte tByte = 0;
    for (uint32_t i = 0; i < streamLen; i = i + 2)
    {
      if ((i % 128 == 0) && (tByte != 0))
      {
        Serial.println();
      }
      tByte = (stream[i] & 0x55) | ((stream[i + 1] & 0x55) << 1);
      if (tByte != 0)
      {
        Serial.print(byte2char(tByte));
      }
    }
    Serial.println("\nOK");
  }

  if (cmd == "error")
  {
    Serial.print(errorD | errorH);
    Serial.println();
  }

  if (cmd == "exterr")
  {
    printExtError();
  }

  if (cmd == "weak")
  {
    Serial.write(weakTracks[logTrack]);
  }

  if (cmd == "get")
  {
    gotoLogicTrack(param.toInt());
    readTrack(true, false);
  }

  if (cmd == "verify")
  {
    gotoLogicTrack(param.toInt());
    readTrack(false, true);
  }

  if (cmd == "goto")
  {
    busy(true);
    gotoLogicTrack(param.toInt());
  }

  if (cmd == "download")
  {
    downloadTrack();
  }

  if (cmd == "upload")
  {
    busy(true);
    uploadTrack();
    Serial.println("OK");
  }

  if (cmd == "tw")
  {
    busy(true);
    motorOn();
    for (int i = 0; i < sectors; i++)
    {

      fillSector(i, true);
    }
    trackTemp = param.toInt();
    floppyTrackMfmEncode(trackTemp, (byte *)track, stream);
    gotoLogicTrack(trackTemp);
    writeTrack(false);
    retries = 6;
    delay(500);
    if (readTrack(false, false) != -1)
    {
      Serial.printf("Sectors found: %d  Errors found: ", sectorCnt);
      Serial.print(errorD | errorH);
      Serial.println();
      Serial.println("OK");
    }
    else
    {
      Serial.println("Read failed!");
    }
    retries = settings.sdRetries;
  }

  if (cmd == "hist")
  {
    analyseHist(false);
    printHist(128);
    Serial.println("OK");
  }

  if (cmd == "flux")
  {
    analyseHist(true);
    printFlux();
  }

  if (cmd == "index")
  {
    if (diskChange() == 1)
      Serial.printf("%d microseconds\nOK\n", measureRPM());
    else
      Serial.println("NO DISK");
  }

#ifdef hdmode
  if (cmd == "getmode")
  {
    getDensity = true;
    motorOn();
    if (densMode == HD)
    {
      Serial.println("HD");
    }
    else
    {
      Serial.println("DD");
    }
  }
#endif

  if (cmd == "name")
  {
    volumeName = getName();
    Serial.println(volumeName);
  }

  if (cmd == "diskinfo")
  {
    diskInfo();
    Serial.println("OK");
  }

  if (cmd == "di")
  {
    diskInfoOld();
    Serial.println("OK");
  }

  if (cmd == "bitmap")
  {
    dumpBitmap();
    Serial.println("OK");
  }

  if (cmd == "dskcng")
  {
    Serial.println(diskChange());
  }

#ifdef hdmode
  if (cmd == "setmode")
  {
    if (param == "hd")
    {
      getDensity = false;
      setMode(HD);
      Serial.println("HD");
    }
    else
    {
      getDensity = false;
      setMode(DD);
      Serial.println("DD");
    }
  }
#endif

  if (cmd == "ver")
  {
    Serial.printf("ADF-Drive/Copy Controller Firmware %s", Version);
    if (hDet == 0)
      Serial.println(" Breadboard");
    if (hDet == 1)
      Serial.println(" V3/4/6 PCB");
    if (hDet == 2)
      Serial.println(" ESP32");
    if (hDet == -1)
      Serial.println(" Unknown Hardware");
  }

  if (cmd == "enc")
  {
    busy(true);
    trackTemp = param.toInt();
    Serial.printf("Encoding Track %d to MFM\n", trackTemp);
    uint32_t tZeit = micros();
    floppyTrackMfmEncode(trackTemp, (byte *)track, stream);
    tZeit = micros() - tZeit;
    Serial.printf("done in %d micros\n", tZeit);
  }

  if (cmd == "dec")
  {
    busy(true);
    Serial.println("Decode MFM");
    decodeTrack(false, false, false);
    Serial.println("done");
  }

  if (cmd == "log")
  {
    Serial.println("Tracking Info");
    for (int i = 0; i < 160; i++)
    {
      if (i != trackLog[i])
      {
        Serial.printf("Track: %d Real Track: %d\n", i, trackLog[i]);
      }
    }
  }

  if (cmd == "mfm")
  {
    Serial.println(mfmByte(param.toInt(), 0), HEX);
  }

  if (mtpOn)
  {
    if (cmd == "event")
    {
      int x, y = 0;
      sscanf(param.c_str(), "%x %d", &x, &y);
      Serial.printf("x: %#.4x y: %d\n", x, y);
      mtpd.sendEvent(x, y);
    }
    if (cmd == "busy")
    {
      busy(true);
    }
    if (cmd == "nobusy")
    {
      busy(false);
    }
  }
  if (cmd == "nomtp")
  {
    mtpOn = false;
  }
  if (cmd == "mtp")
  {
    if ((settings.config & 0x01) == 1)
      mtpOn = true;
    else
      mtpOn = false;
  }

  if (cmd == "retry")
  {
    Serial.printf("Old retry value: %d\n", retries);
    retries = param.toInt();
    Serial.printf("New retry value: %d\n", retries);
  }

  if (cmd == "ana")
  {
    busy(true);
    analyseTrack(false, param.toInt());
    printHist(-1);
  }

  if (cmd == "diag")
  {
    busy(true);
    if (diskChange() == 1)
      Serial.printf("%d microseconds\nOK\n", measureRPM());
    else
      Serial.println("NO DISK");

  }

  if (cmd == "format")
  {
    busy(true);
    uint32_t tZeit = millis();
    int fErr = 0;
    if (param == "quick")
      fErr = formatDisk(true, true);
    else
      fErr = formatDisk(false, false);
    tZeit = millis() - tZeit;
    if (fErr == 0)
      Serial.printf("Format done in %d seconds\n", tZeit / 1000);
    else
      Serial.printf("Format failed in %d seconds\n", tZeit / 1000);
    busy(false);
  }

  if (cmd == "getdate")
  {
    struct DateTime timeTemp;
    timeTemp = adfGiveCurrentTime();
    Serial.printf("adfTime: %02d:%02d:%02d %02d.%02d.%04d\n",
                  timeTemp.hour,
                  timeTemp.min,
                  timeTemp.sec,
                  timeTemp.day,
                  timeTemp.mon,
                  timeTemp.year + 1900);
  }

  if (cmd == "setdate")
  {
    struct DateTime timeTemp;
    timeTemp = adfGiveCurrentTime();
    Serial.printf("adfTime: %02d:%02d:%02d %02d.%02d.%04d\n",
                  timeTemp.hour,
                  timeTemp.min,
                  timeTemp.sec,
                  timeTemp.day,
                  timeTemp.mon,
                  timeTemp.year + 1900);
  }

  if (cmd == "init")
  {
    busy(true);
#ifdef hdmode
    getDensity = true;
#endif
    currentTrack = -1;
    if (diskChange() == 1)
    {
      motorOn();
      seek0();
    }
    errors = 0;
  }

  if (cmd == "rst")
  {
    delay(1000);
    CPU_RESTART
  }
  if (cmd == "info")
  {
    printStatus();
    Serial.print("Pin0: ");
    Serial.println(digitalRead(0));
    for (int i = 0; i < 94; i++)
      if (NVIC_IS_ENABLED(i))
        Serial.printf("Irq: %d Prio: %d Enabled: %d\n", i, NVIC_GET_PRIORITY(i), 1);
//Serial.printf("FreeStack: %d\n", FreeStack());
#include "kinetis.h"
    // extern unsigned long _stext;
    extern unsigned long _etext;
    extern unsigned long _sdata;
    extern unsigned long _edata;
    extern unsigned long _sbss;
    extern unsigned long _ebss;
    extern unsigned long _estack;
    // Serial.printf("_stext:  %d\n", _stext);
    Serial.printf("_etext:  0x%.8lX\n", &_etext);
    Serial.printf("_sdata:  0x%.8lX\n", &_sdata);
    Serial.printf("_edata:  0x%.8lX\n", &_edata);
    Serial.printf("_sbss:   0x%.8lX\n", &_sbss);
    Serial.printf("_ebss:   0x%.8lX\n", &_ebss);
    Serial.printf("_estack: 0x%.8lX\n", &_estack);
    Serial.printf("stream:  0x%.8lX\n", stream);
    Serial.printf("end:     0x%.8lX\n", stream + streamSizeHD);
    Serial.printf("flux:    0x%.8lX\n", flux);

#ifdef USEBITBAND
    Serial.printf("bitband: 0x%.8lX\n", streamBitband);
#endif //USEBITBAND
  }
}

/*
   Setup Teensy, Hardware and other stuff
*/
void setup()
{
  pinMode(0, INPUT_PULLUP);
  if ((restoreTimings() == -1) || (digitalRead(0) == 0))
    storeDefaultTimings(); //restore default timings when pin 0 is pulled low
  hDet = -1;
  failureCode = hardError;
  blinkCode = failureCode;
#ifdef errLed
  startErrorLed();
#endif
  while (hDet == -1)
  { // No drive detected? Blink and loop
    watchdogReset();
    delay(1000);
    hDet = hardwareVersion(); // check if something changed
    if (hDet != -1)
    {
      registerSetup(hDet);
      failureCode = 0;
      blinkCode = failureCode;
    }
  }
  if (hDet != -1)
  {
    pinMode(_index, INPUT_PULLUP);
    pinMode(_motorB, OUTPUT);
    digitalWrite(_motorB, HIGH);
    pinMode(_motorA, OUTPUT);
    digitalWrite(_motorA, HIGH);
    pinMode(_drivesel, OUTPUT);
    digitalWrite(_drivesel, LOW);
    pinMode(_dir, OUTPUT);
    digitalWrite(_dir, HIGH);
    pinMode(_step, OUTPUT);
    digitalWrite(_step, HIGH);
    pinModeFast(_writedata, OUTPUT);
    digitalWrite(_writedata, HIGH);
    pinMode(_writeen, OUTPUT);
    digitalWrite(_writeen, HIGH);
    pinMode(_track0, INPUT_PULLUP);
    pinMode(_wprot, INPUT_PULLUP);
    pinMode(_side, OUTPUT);
    digitalWrite(_side, HIGH);
    pinMode(_diskChange, INPUT_PULLUP);
  }
  Serial.begin(4608000);
  delay(100);
#ifdef debug
  delay(5000);
#endif // debug
  inputString.reserve(200);
  extError = NO_ERR;
  cmd.reserve(20);
  param.reserve(20);
  Serial.println("---------------------------------------------------------------------------------------");
  Serial.printf("ADF-Drive/Copy %s %s - Copyright (C) 2016-2019 Dominik Tonn (nick@niteto.de)\n", Version, __DATE__);
  Serial.printf("released under GNU General Public License, for details see http://www.gnu.org/licenses/)\n");
  Serial.printf("---------------------------------------------------------------------------------------\n");
  Serial.printf("type help for commands\n");
#ifdef USEBITBAND
#ifdef FIXEDMEMORY
  stream = (byte *)0x20008000 - (streamSizeHD+16);
  streamBitband = (int *)BITBAND_ADDR(*stream, 0);
  flux = (uint16_t *)stream;
#else
  byte *tempMem;
  tempMem = (byte *)malloc(1);

  uint32_t foo = 0x20000000 - (long)tempMem;
  free(tempMem);
  tempMem = (byte *)malloc(foo);

  // workaround to prevent the compiler from optimizing away the malloc(foo)
  // for aligning the stream memory to bitband memory space
  // you might get a warning that preventCompilerOptimize is never used.
  volatile uint32_t preventCompilerOptimize __attribute__((unused));
  preventCompilerOptimize = (long)&tempMem[foo];
  // end of workaround
  stream = (byte *)malloc(streamSizeHD - 10);
  streamBitband = (int *)BITBAND_ADDR(*stream, 0);
#ifdef debug
  printPtr(tempMem, "tempMem");
  printPtr(stream, "stream");
#endif //debug

  if (stream == 0)
  {
    Serial.println("Out of memory");
    failureCode = memError;
    blinkCode = memError;

    while (1)
    { // No memory for Bitband -> blink forever
      Serial.println("Out of memory");
      watchdogReset();
      delay(1000);
    }
  }
  free(tempMem);
#endif //USEBITBAND
#endif //FIXEDMEMORY
  initRead();
  floppyPos.dir = 0;
  floppyPos.side = 0;
  floppyPos.track = 0;
  if (mtpOn)
  {
    queuedEvent[0] = 0;
  }
  // noSD = true;
  //  noSD = !SD.begin(10, SPI_QUARTER_SPEED);
  attachInterrupt(_diskChange, diskChangeIRQ, FALLING);
}

/*
   Main loop for doing everything
   Check Serial, handle MTP requests, detect inserted Disk
*/
void loop()
{
  if (Serial.available() != 0)
  {
    getCommand();
    doCommand();
  }
  if ((millis() - motorTime) > 5000)
  {
    if (motor)
    {
      if (ADFisOpen)
        flushToDisk();
      motorOff();
    }
  }
  if (!motor)
  {
    if ((millis() - diskTick) > 500)
    {
      driveSelect();
      driveDeselect();
      if (noDisk)
      {
        if (diskChange() == 1)
        {
          failureCode = 0;
          countFluxes();
          if (fluxCount < 160000)
          {
            badDisk = false;
            if (mtpOn)
            {
              mtpd.sendEvent(MTP_EVENT_STORE_ADDED, FDid);
              mtpd.sendEvent(MTP_EVENT_STORE_ADDED, FSid);
            }
          }
          else
          {
            badDisk = true;
            if (mtpOn)
            {
              mtpd.sendEvent(MTP_EVENT_STORE_ADDED, FDid);
            }
          }
          dchgActive = true;
        }
        //     } else {
      }
      diskTick = millis();
    }
  }
  if (mtpOn)
  {
    mtpd.loop();
    if (fail)
    {
#ifdef errLed
      blinkCode = failureCode;
#endif
      fail = false;
    }
    if (queuedEvent[0] != 0)
    {
      mtpd.sendEvent(queuedEvent[0], queuedEvent[1]);
      queuedEvent[0] = 0;
    }
  }
  watchdogReset();
}

int main()
{
  setup();
  while (1)
    loop();
}
