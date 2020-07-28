/*
   Various defines and variables for ADF-Drive
*/
#include <Arduino.h>
#include "vars_n_defs.h"
#include "adf_defines.h" //for adflib debugging message please edit adf_defines.h in the adf library folder

/* default EEProm settings and structure */

struct tSettings settings
{
   600, 50, 100, 5, 5, 200, 2, 3, 18, 10, 30, mtpModeOn, 0xDEADDA7A, 0xDEADDA7A, 0xDEADDA7A, 0xDEADDA7A, 0xDEADDA7A, 0
};

uint32_t FTChannelValue, FTMx_CnSC, FTPinMuxPort;
#ifdef errLed
IntervalTimer writeTimer;
IntervalTimer errorTimer;
#endif
#ifdef USEBITBAND
byte *stream;
uint16_t *flux;
int *streamBitband;
#else  //USEBITBAND
byte stream[streamSizeHD + 10];
uint16_t *flux = (uint16_t *)&stream;
#endif //USEBITBAND

#include "MTPADF.h"
//defines for MTPMode
#ifdef mtpmode
boolean mtpOn = true;
#else
boolean mtpOn = false;
#endif
boolean preErase = false;
MTPStorage_Amiga storage;
MTPD mtpd(&storage);
uint32_t imageHandle = 0xADFF17E;
uint32_t queuedEvent[2];

boolean badDisk;
boolean busyFormatting = false;
boolean ADFisOpen = false;
// #include "SdFat.h"
// SdFat SD;
// bool noSD = true;

//initialize default for DD Disks
byte low2 = tDDlow2;
byte high2 = tDDhigh2;
byte high3 = tDDhigh3;
byte high4 = tDDhigh4;
byte sectors = 11;

uint16_t timerMode = timerModeDD;
uint16_t filterSetting = filterSettingDD;
uint32_t writeSize = writeSizeDD;
uint32_t streamLen = streamSizeDD;
volatile boolean recordOn = false;

uint16_t hist[256];
byte weakTracks[168];
byte trackLog[168];
volatile uint32_t fluxCount;
volatile uint32_t sectorCnt;
int32_t errorD, errorH;
int32_t errors;
int8_t extError = 0;
uint16_t failureCode = 0;
boolean fail = false;
boolean indexAligned = false;

int currentTrack = -1;
int logTrack = -1;
String inputString(200, 32);
String cmd;
String param;
uint16_t retries = sdRetries_;
uint32_t transitionTime = transTimeDD;
int trackTemp;
int hDet, hDet2;
boolean wProt = true;
boolean noDisk = true;
volatile boolean dchgActive = true;
uint32_t diskTick = 0;
String volumeName(30, 32);
#ifdef errLed
uint16_t blinkCode = 0b0011001100110011;
#endif

struct SectorTable sectorTable[25];

struct rawSector track[23];
byte tempSector[540];

uint32_t zeit;

uint32_t writePtr;
uint32_t writeBitCnt;
volatile byte dataByte;
volatile boolean writeActive;

volatile uint8_t indexCount;
volatile uint32_t indexes[10];
volatile uint32_t bitcells[10];
volatile uint32_t transferred[10];
uint32_t RPMtime = 200000;
