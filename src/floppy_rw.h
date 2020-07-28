/*
   This Module handles all the low level read/write access to the drive
*/
#ifndef FLOPPY_RW_H /* include guards */
#define FLOPPY_RW_H

#include <Arduino.h>
//#include "vars_n_defs.h"
#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

  /****************************************
   read functions
 ****************************************/

  /* Initializes the Registers for the FlexTimer0 Module */
  void setupFTM0();

  /* Interrupt Service Routine for FlexTimer0 Module */
  extern "C" void ftm0_isr(void);

  /* Initializes the Registers for FluxCapture */
  void setupFTM0_flux();

  /* Interrupt Service Routine for FlexTimer0 Flux Mode */
  extern "C" void ftm0_flux_isr(void);

  int64_t readTrack(boolean silent, boolean compOnly);

  /* reads a track from the floppy disk optional parameter silent to supress all debug messages */
  int64_t readTrack2(boolean silent, boolean compOnly, boolean ignoreIndex);

  /* analyses a track from the floppy disk, optional parameter silent to supress all debug messages */
  int64_t analyseTrack(boolean silent, int dens);

  /* captureFlux - test, optional parameter silent to supress all debug messages */
  int64_t captureFlux(boolean silent, int dens);

  /* prints the track index data etc. */
  void getIndexes(int revs);
 
  /* prints the packetcount */
  void getPacketCnt();
 
  /* prints the transcount */
  void getTransCount();

  /* Initializes Variables for reading a track */
  void initRead();

  /* starts recording */
  void startRecord();

  /* stops recording */
  void stopRecord();

  /* read function with verbose output */
  void doRead(int track, boolean ignoreIndex);

  /* tests if inserted media is a HD or DD disk, 1 = dd; 2 = hd; 0 = defect media; -1 = write protected media */
  int16_t probeTrack(boolean report);

  /****************************************
   write functions
 ****************************************/

  /* interrupt routine for writing data to floppydrive */
  void diskWrite();

  /* old diskwrite function without bitband memory i'll let this here in case to port this to another microcontroller */
  void olddiskWrite();

  /* writes a track from buffer to the floppydrive, return -1 if disk is write protected */
  int writeTrack(boolean erase);

  /* write a track with verify */
  int writeWithVerify(int tTrack, int wRetries);

  /* write a track without verify */
  int writeWithoutVerify(int tTrack, int wRetries);

  /****************************************
   format & erase functions
 ****************************************/

  /* formats ONE track */
  int formatTrack(int tTrack, int wRetries, boolean verify);

  /* formats a whole disk in OFS */
  int formatDisk(boolean quick, boolean verify);

  /* interrupt routine for erasing data on floppydrive */
  extern void writeErase();

  /* writes 01 with a given frequency (transitionDelay * F_BUS cycles) to the floppydrive, return -1 if disk is write protected */
  int eraseTrack(uint32_t transitionDelay);

  /* erases the complete disk as fast as possible, return -1 if disk is write protected */
  int eraseDisk(uint32_t transitionDelay);
#ifdef __cplusplus
}
#endif
#endif /* FLOPPY_RW_H */