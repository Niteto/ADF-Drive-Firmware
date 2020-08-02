/*
   Various defines and variables for ADF-Drive
*/
#ifndef VARS_N_DEFS_H /* include guards */
#define VARS_N_DEFS_H

#include <Arduino.h>
#define Version "v1.111"
#define numVersion 1111
/* Debug and Modes */
// #define debug //uncomment for main debug messages
#define mtpmode          // enables USB MTP support
#define mtpModeOn 1
#define hdmode           // enables HD Disk funktions
#include "adf_defines.h" //for adflib debugging message please edit adf_defines.h in the adf library folder
// #define debugmtp // mtp debug messages
// #define debugmtp2 // verbose mtp packet messages
#define errLed //enables error Led blink codes, disable when using SD Card pin 13 -> SCLK
#define USEBITBAND //enables use of ARM BitBanding Memory
#define FIXEDMEMORY
#define PITTIMER

/* flexible timer module */
#define timerModeHD 0x08  // TOF=0 TOIE=0 CPWMS=0 CLKS=01 (Sys clock) PS=000 (divide by 1)
#define timerModeDD 0x09  // TOF=0 TOIE=0 CPWMS=0 CLKS=01 (Sys clock) PS=001 (divide by 2)
#define filterSettingDD 0 
#define filterSettingHD 0 

#define motorMaxTick 5   // Idle Seconds before Motor off
#define sdRetries_ 10    // maximum retries to read a track
#define hdRetries_ 30    // maximum retries to read a track
#define transTimeDD 96 // timing for write transitions in F_BUS cycles (48Mhz) 2µs
#define transTimeHD 48 // timing for write transitions in F_BUS cycles (48Mhz) 1µs

#define FLOPPY_GAP_BYTES 1482
#define streamSizeHD 23 * 1088 + FLOPPY_GAP_BYTES //22 sectors + gap + spare sector
#define streamSizeDD 12 * 1088 + FLOPPY_GAP_BYTES //11 sectors + gap + spare sector
#define writeSizeDD 11 * 1088 + FLOPPY_GAP_BYTES  //11 sectors + xx bytes gap
#define writeSizeHD 22 * 1088 + FLOPPY_GAP_BYTES  //22 sectors + xx bytes gap
#define MFM_MASK 0x55555555

/* timing defines for Teensy HD Disks */
#define tHDlow2 30
#define tHDhigh2 120
#define tHDhigh3 168
#define tHDhigh4 240
/* timing defines for Teensy DD Disks */
#define tDDlow2 30   // 
#define tDDhigh2 120 // 5µs
#define tDDhigh3 168 // 7µs
#define tDDhigh4 240 // 

#define HD 1
#define DD 0

/* extended error defines */
#define NO_ERR 0            // OK
#define WPROT_ERR 1         // Disk is write-protected
#define TIMEOUT_ERR 2       // Timeout in read operation
#define SEEK_ERR 3          // Seek error / Sector Header Error
#define NUMSECTOR_ERR 4     // Incorrect number of sectors on track
#define TRACK0_ERR 5        // Seek Error Track 0
#define SECBOUNDS_ERR 6     // Sector out of bounds
#define HEADERCHKSUM_ERR 10 // Header bad checksum
#define DATACHKSUM_ERR 11   // Data bad checksum
#define UPLOAD_ERR 20       // Track upload error

/* defines for error blink codes */
#define readError 0b0000000000010101
#define writeError 0b0000000101010101
#define memError 0b0000111100001111
#define hardError 0b0101010101010101


/* default EEProm settings and structure */
struct tSettings
{
  uint32_t versionNr;
  uint32_t motorSpinupDelay;   // ms
  uint32_t motorSpindownDelay; // µs
  uint32_t driveSelectDelay;   // µs
  uint32_t driveDeselectDelay; // µs
  uint32_t setDirDelay;        // µs
  uint32_t setSideDelay;       // µs
  uint32_t stepDelay;          // µs
  uint32_t stepSettleDelay;    // ms
  uint32_t gotoTrackSettle;    //  ms
  uint32_t sdRetries;          //10
  uint32_t hdRetries;          //30
  uint32_t config;
  uint32_t reserved3;
  uint32_t reserved4;
  uint32_t reserved5;
  uint32_t reserved6;
  uint32_t reserved7;
  uint32_t checksum;
};
extern struct tSettings settings;

// following macros were posted on stackoverflow by "steveha"
// https://stackoverflow.com/questions/11697820/how-to-use-date-and-time-predefined-macros-in-as-two-integers-then-stri
#define COMPUTE_BUILD_YEAR         \
  (                                \
      (__DATE__[7] - '0') * 1000 + \
      (__DATE__[8] - '0') * 100 +  \
      (__DATE__[9] - '0') * 10 +   \
      (__DATE__[10] - '0'))

#define COMPUTE_BUILD_DAY                                     \
  (                                                           \
      ((__DATE__[4] >= '0') ? (__DATE__[4] - '0') * 10 : 0) + \
      (__DATE__[5] - '0'))

#define BUILD_MONTH_IS_JAN (__DATE__[0] == 'J' && __DATE__[1] == 'a' && __DATE__[2] == 'n')
#define BUILD_MONTH_IS_FEB (__DATE__[0] == 'F')
#define BUILD_MONTH_IS_MAR (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'r')
#define BUILD_MONTH_IS_APR (__DATE__[0] == 'A' && __DATE__[1] == 'p')
#define BUILD_MONTH_IS_MAY (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'y')
#define BUILD_MONTH_IS_JUN (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'n')
#define BUILD_MONTH_IS_JUL (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'l')
#define BUILD_MONTH_IS_AUG (__DATE__[0] == 'A' && __DATE__[1] == 'u')
#define BUILD_MONTH_IS_SEP (__DATE__[0] == 'S')
#define BUILD_MONTH_IS_OCT (__DATE__[0] == 'O')
#define BUILD_MONTH_IS_NOV (__DATE__[0] == 'N')
#define BUILD_MONTH_IS_DEC (__DATE__[0] == 'D')

#define COMPUTE_BUILD_MONTH \
  (                         \
      (BUILD_MONTH_IS_JAN) ? 1 : (BUILD_MONTH_IS_FEB) ? 2 : (BUILD_MONTH_IS_MAR) ? 3 : (BUILD_MONTH_IS_APR) ? 4 : (BUILD_MONTH_IS_MAY) ? 5 : (BUILD_MONTH_IS_JUN) ? 6 : (BUILD_MONTH_IS_JUL) ? 7 : (BUILD_MONTH_IS_AUG) ? 8 : (BUILD_MONTH_IS_SEP) ? 9 : (BUILD_MONTH_IS_OCT) ? 10 : (BUILD_MONTH_IS_NOV) ? 11 : (BUILD_MONTH_IS_DEC) ? 12 : /* error default */ 99)

#define COMPUTE_BUILD_HOUR ((__TIME__[0] - '0') * 10 + __TIME__[1] - '0')
#define COMPUTE_BUILD_MIN ((__TIME__[3] - '0') * 10 + __TIME__[4] - '0')
#define COMPUTE_BUILD_SEC ((__TIME__[6] - '0') * 10 + __TIME__[7] - '0')

#define BUILD_DATE_IS_BAD (__DATE__[0] == '?')

#define BUILD_YEAR ((BUILD_DATE_IS_BAD) ? 99 : COMPUTE_BUILD_YEAR)
#define BUILD_MONTH ((BUILD_DATE_IS_BAD) ? 99 : COMPUTE_BUILD_MONTH)
#define BUILD_DAY ((BUILD_DATE_IS_BAD) ? 99 : COMPUTE_BUILD_DAY)

#define BUILD_TIME_IS_BAD (__TIME__[0] == '?')

#define BUILD_HOUR ((BUILD_TIME_IS_BAD) ? 99 : COMPUTE_BUILD_HOUR)
#define BUILD_MIN ((BUILD_TIME_IS_BAD) ? 99 : COMPUTE_BUILD_MIN)
#define BUILD_SEC ((BUILD_TIME_IS_BAD) ? 99 : COMPUTE_BUILD_SEC)

/* reset macro */
#define CPU_RESTART_ADDR (uint32_t *)0xE000ED0C
#define CPU_RESTART_VAL 0x5FA0004
#define CPU_RESTART (*CPU_RESTART_ADDR = CPU_RESTART_VAL);

//defines for the Teensy 3.2 board
#ifdef __MK20DX256__
#define __teensy
#define BITBAND_ADDR(addr, bit) (((uint32_t) & (addr)-0x20000000) * 32 + (bit)*4 + 0x22000000)
//int _dens;
extern uint32_t FTChannelValue, FTMx_CnSC, FTPinMuxPort;
#ifdef errLed
extern IntervalTimer writeTimer;
extern IntervalTimer errorTimer;
#endif
#ifdef USEBITBAND
extern byte *stream;
extern int *streamBitband;
extern uint16_t *flux;
#else
extern byte stream[streamSizeHD + 10];
extern uint16_t *flux;
#endif
//#include "FreeStack.h"
#include <EEPROM.h>
#endif //Teensy

#include "MTPADF.h"
//defines for MTPMode
extern boolean mtpOn;
extern boolean preErase;
extern MTPStorage_Amiga storage;
extern MTPD mtpd;
extern uint32_t imageHandle;
extern uint32_t queuedEvent[2];

extern boolean badDisk;
extern boolean busyFormatting;
extern boolean ADFisOpen;
// #include "SdFat.h"
// extern SdFat SD;
// extern bool noSD;

//initialize default for DD Disks
extern byte low2;
extern byte high2;
extern byte high3;
extern byte high4;
extern byte sectors;

extern uint16_t timerMode;
extern uint16_t filterSetting;
extern uint32_t writeSize;
extern uint32_t streamLen;
extern volatile boolean recordOn;

extern uint16_t hist[256];
extern byte weakTracks[168];
extern byte trackLog[168];
extern volatile uint32_t fluxCount;
extern volatile uint32_t sectorCnt;
extern int32_t errorD, errorH;
extern int32_t errors;
extern int8_t extError;
extern uint16_t failureCode;
extern boolean fail;
extern boolean indexAligned;

extern int currentTrack;
extern int logTrack;
extern String inputString;
extern String cmd, param;
extern uint16_t retries;
extern uint32_t transitionTime;
extern int trackTemp;
extern int hDet, hDet2;
extern boolean wProt;
extern boolean noDisk;
extern volatile boolean dchgActive;
extern uint32_t diskTick;
extern String volumeName;
#ifdef errLed
extern uint16_t blinkCode;
#endif

struct SectorTable
{
  unsigned long bytePos;
  byte sector;
};
extern struct SectorTable sectorTable[25];

struct Sector
{
  byte format_type;            //0
  byte track;                  //1
  byte sector;                 //2
  byte toGap;                  //3
  byte os_recovery[16];        //4
  unsigned long header_chksum; //20
  unsigned long data_chksum;   //24
  byte data[512];              //28
};

struct rawSector
{
  byte sector[540];
};

extern struct rawSector track[23];
extern byte tempSector[540];

extern uint32_t zeit;

extern uint32_t writePtr;
extern uint32_t writeBitCnt;
extern volatile byte dataByte;
extern volatile boolean writeActive;
extern volatile uint8_t indexCount;
extern volatile uint32_t indexes[10];
extern volatile uint32_t bitcells[10];
extern volatile uint32_t transferred[10];
extern uint32_t RPMtime;


// Rootblock (512 bytes) sector 880 for a DD disk, 1760 for a HD disk
struct rootBlock
{
  long type;         // block primary type = T_HEADER (value 2)
  long header_key;   // unused in rootblock (value 0)
  long high_seq;     // unused (value 0)
  long ht_size;      // Hash table size in long (value 0x48)
  long first_data;   // unused (value 0)
  long chksum;       // sum to check block integrity
  long ht[72];       // hash table (entry block number)
  long bm_flag;      // bitmap flag, -1 means VALID
  long bm_pages[25]; // bitmap blocks pointers (first at 0)
  long bm_ext[4];    // first bitmap extension block (Hard disks only)
  byte name_len;     // disk name length
  char diskname[30]; // disk name
  byte filler[9];
  long days;   // last access date : days since 1 jan 1978
  long mins;   // minutes past midnight
  long ticks;  // ticks (1/50 sec) past last minute
  long c_days; // creation date
  long c_mins;
  long c_ticks;
  long next_hash;  // unused (value = 0)
  long parent_dir; // unused (value = 0)
  long extension;  // FFS: first directory cache block, 0 otherwise
  long sec_type;   // block secondary type = ST_ROOT (value 1)
};

// Bitmap block (512 bytes), often rootblock+1
struct bitmapBlock
{
  long checksum; //  special algorithm
  long map[127];
};

#endif /* VARS_N_DEFS_H */
