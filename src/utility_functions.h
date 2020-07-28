#ifndef UTILITY_FUNCTIONS_H /* include guards */
#define UTILITY_FUNCTIONS_H

#include <Arduino.h>
/* Feeds the Watchdog */
void watchdogReset();

/* reads command from the serial port */
void getCommand();

#ifdef debug
void printPtr(byte *dest, String pName);
#endif

/* the ISR for the blinking errorcodes */
#ifdef errLed
void errorLed();

void startErrorLed();

void stopErrorLed();
#endif
/* sets teensy pin to fastmode if OUTPUT
   slightly modified pinMode function from \hardware\teensy\avr\cores\teensy3\pins_teensy.c */
void pinModeFast(uint8_t pin, uint8_t mode);

unsigned char reverse(unsigned char b);

void printExtError();

/* read Diskname from Track 80 */
String getName();

int findMinima(int start);

/*
   adjusts the timings for the transitions lengths, that way there is a better chance
   to read disks written by drives with too slow / fast rpm
*/
void adjustTimings();

/* prints histogram of last read track in ascii mainly for debugging purposes */
void printHist(int scale);

/* outputs the histogram of flux transistions in binary form */
void printFlux();

/* counts the transistions and calculates the real read bits of the last read track mainly for debugging */
void analyseHist(boolean silent);

/*
   decodes one MFM encoded Sector into Amiga Sector
   partly based on DecodeSectorData and DecodeLongword from AFR.C, written by
   Marco Veneri Copyright (C) 1997 released as public domain
*/
int decodeSector(uint32_t secPtr, int index, boolean silent, boolean ignoreIndex);

/* decodes one MFM encoded Sector into Dos Sector */
int decodeDos(uint32_t secPtr, int index, boolean silent, boolean ignoreIndex);

/* calculates a checksum of <secPtr> at <pos> for <b> bytes length returns checksum */
uint32_t calcChkSum(uint32_t secPtr, int pos, int b);

/* decodes a whole track, optional parameter silent to suppress all serial debug info */
int decodeTrack(boolean silent, boolean compOnly, boolean ignoreIndex);

/* returns current track number from decoded sectors in buffer */
int getTrackInfo();

/* dumps the sector <index> from the buffer in human readable acsii to the serial port, mainly for debugging */
void printAmigaSector(int index);

/* dumps the whole track in ascii - mainly for debugging */
void printTrack();

/* dumps the data section of a sector in binary format */
void dumpSector(int index);

/* returns a pointer to the sector */
extern "C" byte *ptrSector(int index);

/* reads a sector from the serial in binary format */
int loadSector(int index);

/* dumps the whole track in binary form to the serial port */
void downloadTrack();

/* reads a whole track from the serial port in binary form */
void uploadTrack();

/* returns c if printable, else returns a whitespace */
char byte2char(byte c);

/* prints v in ascii encoded binary */
void print_binary(int v, int num_places);

/* prints v in ascii encoded binary reversed, lowest bit first */
void print_binary_rev(uint32_t v, uint32_t num_places);

/* fills a sector with "ADFCopy" */
void fillSector(int sect, boolean fillType);

/* encodes odd bits of a longword to mfm */
uint32_t oddLong(uint32_t odd);

/* encodes even bits of a longword to mfm */
uint32_t evenLonger(uint32_t even);

/* encodes one byte <curr> into mfm, takes into account if previous byte ended with a set bit, returns the two mfm bytes as a word */
uint16_t mfmByte(byte curr, uint16_t prev);

/* stores a longword in the byte array */
void putLong(uint32_t tLong, byte *dest);

/* encodes a sector into a mfm bitstream */
void encodeSector(uint32_t tra, uint32_t sec, byte *src, byte *dest);

/* fills bitstream with 0xAA */
void fillTrackGap(byte *dst, int len);

/* encodes a complete track + gap into mfm bitstream
   the gap gets encoded before the sectors to make sure when the track gap is too long for
   the track the first sector doesnt gets overwritten, this way only the track gap gets overwritten
   and the track contains no old sector headers */
void floppyTrackMfmEncode(uint32_t track_t, byte *src, byte *dst);

/* Sets standard timings for Teensy 3.2 @ 96Mhz*/
void setTimings(int density);

/* initializes variables and timings according to the density mode */
void setMode(int density);

/* tries to detect the density of the disk by counting the transitions on a track */
void densityDetect();

/* returns the creation and modified date of the disk from rootblock */
void diskDates(char *cdate, char *ldate);

/* prints various stuff from the root and bootblock for debugging */
void diskInfoOld();

void diskInfo();

void dumpBitmap();

void printTimings(struct tSettings settings);

void setDefaultTimings();

void storeTimings();

void storeDefaultTimings();

extern int restoreTimings();

#endif /* UTILITY_FUNCTIONS_H */