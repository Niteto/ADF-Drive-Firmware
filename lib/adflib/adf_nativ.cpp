/*
 * adf_nativ.c
 *
 * $Id$
 *
 *  This file is part of ADFLib.
 *
 *  ADFLib is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  ADFLib is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Foobar; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include "adf_defines.h"
#include <string.h>
#include"adf_str.h"
#include"adf_nativ.h"
#include"adf_err.h"
#include <Arduino.h>
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
extern struct Env adfEnv;
extern boolean getDensity;
extern int currentTrack;
extern int diskChange();
extern void motorOn();
extern int seek0();
extern void gotoLogicTrack(int track);
extern int64_t readTrack(boolean silent, boolean compOnly);
extern byte* ptrSector(int index);
extern int writeWithVerify(int tTrack, int wRetries);
extern int logTrack;
extern boolean writeProtect();
extern int densMode;

static boolean modified = false;
#ifdef __cplusplus
}
#endif
/*
 * myInitDevice
 *
 * must fill 'dev->size'
 */
RETCODE myInitDevice(struct Device* dev, char* name,BOOL ro)
{
    struct nativeDevice* nDev;

    nDev = (struct nativeDevice*)dev->nativeDev;

    nDev = (struct nativeDevice*)malloc(sizeof(struct nativeDevice));
    if (!nDev) {
        (*adfEnv.eFct)("myInitDevice : malloc");
        return RC_ERROR;
    }
    dev->nativeDev = nDev;
    if (!writeProtect())
        /* check if device is writable, if not, force readOnly to TRUE */
        dev->readOnly = FALSE;
    else
        /* mount device as read only */
        dev->readOnly = TRUE;

    dev->size = 0;
	
    getDensity = true;
    currentTrack = -1;
    if (diskChange() == 1) {
      motorOn();
      seek0();
	  if (densMode == 1)
		dev->size = 160*22*512;
	  else
		dev->size = 160*11*512;
	  return RC_OK;
    } else
      return RC_ERROR;
}


/*
 * myReadSector
 *
 */
RETCODE myReadSector(struct Device *dev, int32_t n, int size, uint8_t* buf)
{
	int track = n / dev->sectors;
	int sector = n % dev->sectors;
#ifdef debugadf
	Serial.printf("[ADFlib]Read Track: %d Sector: %d ",track, sector);
#endif
	if (track!=logTrack) {
#ifdef debugadf
      Serial.println("from Disk.");
#endif
	  if (modified) {
		  writeWithVerify(logTrack,6);
#ifdef debugadf
		  Serial.printf("[ADFlib]ReadSector: write track %d ", logTrack);
#endif
	  }
	  modified = false;
	  gotoLogicTrack(track);
	  uint64_t __attribute__((unused)) error64;
	  error64 = readTrack(true,false);
#ifdef debugadf
	  if (error64 !=0) Serial.println("Read failed");
#endif
	} else {
#ifdef debugadf
		Serial.println("from Cache.");
#endif
	}
	if (size+(sector*512)>512*dev->sectors) {
		Serial.println("[ADFlib]PANIC: myReadSector size too large!");
		delay(1000);
		while(1);
	}
	memcpy(buf, ptrSector(sector),size);
    return RC_OK;   
}


/*
 * myWriteSector
 *
 */
RETCODE myWriteSector(struct Device *dev, int32_t n, int size, uint8_t* buf)
{
	int track = n / dev->sectors;
	int sector = n % dev->sectors;
	if (track!=logTrack) {
	  if (modified) {
#ifdef debugadf
		  Serial.printf("\n[ADFlib]WriteSector: track %d written\n", logTrack);
#endif
		  writeWithVerify(logTrack,15);
	  }
	  modified = false;
	  gotoLogicTrack(track);
	  readTrack(true,false);
	} else {
#ifdef debugadf
	    Serial.printf("%d:%d ",track, sector);
#endif
	}
	if (size+(sector*512)>512*dev->sectors) {
		Serial.println("[ADFlib]PANIC: myWriteSector size too large!");
		delay(1000);
		while(1);
	}
	memcpy(ptrSector(sector), buf, size);
	modified = true;
    return RC_OK;
}


/*
 * myReleaseDevice
 *
 * free native device
 */
RETCODE myReleaseDevice(struct Device *dev)
{
	if (modified) writeWithVerify(logTrack,6);
	modified = false;
    struct nativeDevice* nDev;

    nDev = (struct nativeDevice*)dev->nativeDev;

	free(nDev);

    return RC_OK;
}

/*
 * flushToDisk
 *
 * if last track was modified, write it to disk.
 */
RETCODE flushToDisk()
{
	if (modified){
#ifdef debugadf
	  Serial.printf("[ADFlib]flushToDisk: write track %d ", logTrack);
      Serial.println("to Disk.");
#endif
	  writeWithVerify(logTrack,6);
	}
	modified = false;
    return RC_OK;
}


/*
 * adfInitNativeFct
 *
 */
void adfInitNativeFct()
{
    struct nativeFunctions *nFct;

    nFct = (struct nativeFunctions*)adfEnv.nativeFct;

    nFct->adfInitDevice = myInitDevice ;
    nFct->adfNativeReadSector = myReadSector ;
    nFct->adfNativeWriteSector = myWriteSector ;
    nFct->adfReleaseDevice = myReleaseDevice ;
    nFct->adfIsDevNative = myIsDevNative;
}


/*
 * myIsDevNative
 *
 */
BOOL myIsDevNative(char *devName)
{
  //  return (strncmp(devName,"DF0:",4)==0);
  return true;
}
/*##########################################################################*/
