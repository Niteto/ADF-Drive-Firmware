/*
   This Module handles all the low level read/write access to the drive
*/
#include <Arduino.h>
#include "vars_n_defs.h"
#include "floppy_rw.h"
#include "floppy_control.h"
//#include "MTPADF.h"
#include "utility_functions.h"
extern MTPD mtpd;
#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */
  /*************************************************************************************************************************************************************
   read functions
 *************************************************************************************************************************************************************/

  // only modified by the ISR therefore not volatile;
  uint16_t sample = 0;
  uint16_t lastSample = 0;
  int16_t sectLen = 0;
  uint8_t bCnt = 0;
  uint32_t readBuff;
  uint32_t readPtr;
  uint16_t cellTime = 0;

  /*
   Initializes the Registers for the FlexTimer0 Module
*/
  void setupFTM0()
  {
    // Input Filter waits for n cycles of stable input
    FTM0_FILTER = filterSetting; // usually 0

    // Enable the FlexTimerModule and write registers
    // FAULTIE=0, FAULTM=00, CAPTEST=0, PWMSYNC=0, WPDIS=1, INIT=0, FTMEN=1
    FTM0_MODE = 0x05;

    // Initialize Timer registers
    FTM0_SC = 0x00;    // Diable Interrupts and Clocksource before initialization
    FTM0_CNT = 0x0000; // set counter value to 0
    FTM0_MOD = 0xFFFF; // set modulo to max value
    //  (*(volatile uint32_t *)FTMx_CnSC) = 0x48;

    (*(volatile uint32_t *)FTMx_CnSC) = FTM_CSC_CHIE | FTM_CSC_ELSB;

    // CHF=0  CHIE=1 (enable interrupt)
    // MSB=0  MSA=0 (Channel Mode Input Capture)
    // ELSB=1 ELSA=0 (Input Capture on falling edge)
    // DMA=1/0  DMA on/off

    // Enable FTM0 interrupt inside NVIC
    NVIC_SET_PRIORITY(IRQ_FTM0, 0);
    NVIC_ENABLE_IRQ(IRQ_FTM0);
    (*(volatile uint32_t *)FTPinMuxPort) = 0x403; // setup pin for Input Capture FTM0 in Pin Mux
    lastSample = 0;
  }

  /*
   Interrupt Service Routine for FlexTimer0 Module
*/

  extern "C" void ftm0_isr(void)
  {
    sample = (*(uint32_t *)FTChannelValue) - lastSample;
    // FTM0_C7SC &= ~FTM_CSC_CHF; // clear channel event flag
    (*(volatile uint32_t *)FTMx_CnSC) &= ~FTM_CSC_CHF; // clear channel event flag
    lastSample = (*(uint32_t *)FTChannelValue);
    // skip too short / long samples, occurs usually in the track gap
    fluxCount++;
    if (sample > high4)
    {
      hist[255]++; // add 255 sample to histogram
      return;
    }
    if (sample < low2)
    {
      hist[sample]++; // add sample to histogram
      return;
    }
    // fills buffer according to transition length with 10, 100 or 1000 (4,6,8µs transition)
    readBuff = (readBuff << 2) | B10; //short transition
    bCnt += 2;
    if (sample > high2)
    { // medium transition
      readBuff = readBuff << 1;
      bCnt++;
    }
    if (sample > high3)
    { // long transition
      readBuff = readBuff << 1;
      bCnt++;
    }
    if (bCnt >= 8) // do we have a complete byte?
    {
      stream[readPtr++] = readBuff >> (bCnt - 8); // store byte in streambuffer
      bCnt = bCnt - 8;                            // decrease bit count by 8
    }
    if (readBuff == 0xA4489448)
    { // look for magic word. usually 44894489, but detecting this way its
      // easier to byte align the received bitstream from floppy
      if (sectorCnt < sectors)
      { // as long we dont have x sectors store the sector start in a table
        sectorTable[sectorCnt].bytePos = readPtr - 7;
        sectorCnt++;
        bCnt = 4; // set bit count to 4 to align to byte
      }
    }
    hist[sample]++;          // add sample to histogram
    if (readPtr > streamLen) // stop when buffer is full
    {
      recordOn = false;
      FTM0_SC = 0x00; // Timer off
    }
  }

  int64_t readTrack(boolean silent, boolean compOnly)
  {
    return readTrack2(silent, compOnly, false);
  }
  /*
   reads a track from the floppy disk
   optional parameter silent to supress all debug messages
*/
  int64_t readTrack2(boolean silent, boolean compOnly, boolean ignoreIndex)
  {
    setMode(densMode);
    int j = 0;
    for (j = 0; j < retries; j++)
    {
      for (int i = 0; i < 256; i++)
      {
        hist[i] = 0;
      }
      motorOn();
      initRead();
      uint32_t tZeit = millis();
      watchdogReset();
      startRecord();
      while (recordOn)
      {
        if ((millis() - tZeit) > 300)
        {
          if (!silent)
          {
            Serial.print("Timeout reached\n");
          }
          stopRecord();
          errors = -1;
          extError = TIMEOUT_ERR;
        }
      }
      NVIC_DISABLE_IRQ(IRQ_FTM0);
      tZeit = millis() - tZeit;
      if (!silent)
      {
        Serial.printf("ReadTime: %d\n", tZeit);
      }
      tZeit = micros();
      errors = decodeTrack(silent, compOnly, ignoreIndex);
      tZeit = micros() - tZeit;
      if (!silent)
      {
        Serial.printf("Decode took %d Error: %d\n", tZeit, errors);
      }
      if (getTrackInfo() != logTrack)
      {
        errors = -1;
        extError = SEEK_ERR;
      }
      if ((errors == 0) && (sectorCnt == sectors))
      {
        extError = NO_ERR;
        break;
      }
      if (!silent)
      {
        Serial.printf("Read Error, retries left: %d CurrentTrack: %d error:", retries - j, logTrack);
        Serial.print(errorD | errorH, BIN);
        Serial.printf(" %d errors: %d sectorCount: %d\n", extError, errors, sectorCnt);
      }
      adjustTimings();
      // the following code tries to move the stepper / seek 0 before retrying to read the track
      // but i found out that about 6 retries are sufficient to determine if a track is bad
      // but you may enable it and set retries to 25 if you want to give it a shot
      if (!silent)
      {
        Serial.printf(" high2: %d high3: %d\n", high2, high3);
      }
      int tempTrack = logTrack;
      switch (j)
      {
      case 10:
        gotoLogicTrack(tempTrack - 2);
        gotoLogicTrack(tempTrack);
        break;
      case 15:
        gotoLogicTrack(tempTrack + 2);
        gotoLogicTrack(tempTrack);
        delay(10);
        break;
      case 20:
        currentTrack = -1;
        gotoLogicTrack(tempTrack);
        delay(10);
        break;
      case 25:
        currentTrack = -1;
        gotoLogicTrack(tempTrack);
        delay(10);
        break;
      default:
        // do nothing
        break;
      }
    }
    weakTracks[logTrack] = j;
    trackLog[logTrack] = getTrackInfo();
    if (sectorCnt != sectors)
    {
      errors = -1;
      extError = NUMSECTOR_ERR;
    }
    return errors;
  }

  /*
   analyses a track from the floppy disk
   optional parameter silent to supress all debug messages
*/
  int64_t analyseTrack(boolean silent, int dens)
  {
    errors = 0;
    setMode(dens);
    for (int i = 0; i < 256; i++)
    {
      hist[i] = 0;
    }
    int j = 0;
    for (j = 0; j < 10; j++)
    {
      motorOn();
      initRead();
      uint32_t tZeit = millis();
      watchdogReset();
      startRecord();
      while (recordOn)
      {
        if ((millis() - tZeit) > 300)
        {
          if (!silent)
          {
            //  Serial << "Timeout reached\n";
          }
          stopRecord();
          errors = -1;
          extError = TIMEOUT_ERR;
        }
      }
    }
    setMode(densMode);
    return errors;
  }

  volatile boolean upperFull, lowerFull;
  volatile uint32_t cellCount;
  volatile uint32_t transCount;
  volatile uint32_t lastCNTVal;
  volatile uint32_t overflow;
  volatile uint32_t packetCnt;
  /*
  Interrupt Service Routine for FlexTimer0 Flux Mode
  the Kinetis K20 has a shared ISR for timer overflow and input capture, it is not determeable which triggered first
  it seems that sometimes the timer overflow interrupt is delayed after an input capture interrupt
  fix - this is sorted out at application level
*/

  extern "C" void ftm0_flux_isr(void)
  {
    if ((FTM0_C7SC & FTM_CSC_CHF) && (FTM0_SC & FTM_SC_TOF)) //channel event + timer overflow simultanously (or within isr latency)
    {
      FTM0_C7SC &= ~FTM_CSC_CHF; // clear channel event flag
      FTM0_SC &= ~FTM_SC_TOF;    // clear Overflow Flag
      uint16_t value = FTM0_C7V;
      cellCount += 2;
      if (value == 0)
        value = 1;
      if (value > 0x8000)
      {
        flux[readPtr++] = value;
        readPtr &= 0x1fff;
        if (readPtr == 0x0000)
        {
          upperFull = true;
          packetCnt++;
        }
        if (readPtr == 0x1000)
        {
          lowerFull = true;
          packetCnt++;
        }
        flux[readPtr++] = 0x0000;
        readPtr &= 0x1fff;
        if (readPtr == 0x0000)
        {
          upperFull = true;
          packetCnt++;
        }
        if (readPtr == 0x1000)
        {
          lowerFull = true;
          packetCnt++;
        }
      }
      else
      {
        flux[readPtr++] = 0x0000;
        readPtr &= 0x1fff;
        if (readPtr == 0x0000)
        {
          upperFull = true;
          packetCnt++;
        }
        if (readPtr == 0x1000)
        {
          lowerFull = true;
          packetCnt++;
        }
        flux[readPtr++] = value;
        readPtr &= 0x1fff;
        if (readPtr == 0x0000)
        {
          upperFull = true;
          packetCnt++;
        }
        if (readPtr == 0x1000)
        {
          lowerFull = true;
          packetCnt++;
        }
      }
      return;
    }
    if (FTM0_C7SC & FTM_CSC_CHF) // check Channel Event Flag
    {
      FTM0_C7SC &= ~FTM_CSC_CHF; // clear channel event flag
      uint16_t value = FTM0_C7V;
      if (value == 0)
        flux[readPtr++] = 1;
      else
        flux[readPtr++] = value;
      cellCount++;
      readPtr &= 0x1fff;
        if (readPtr == 0x0000)
        {
          upperFull = true;
          packetCnt++;
        }
        if (readPtr == 0x1000)
        {
          lowerFull = true;
          packetCnt++;
        }
    }
    if (FTM0_SC & FTM_SC_TOF) // Timer Overflow Flag
    {
      FTM0_SC &= ~FTM_SC_TOF;   // clear Overflow Flag
      flux[readPtr++] = 0x0000; // store overflow in streambuffer
      readPtr &= 0x1fff;        // pointer wrap around
      cellCount++;
        if (readPtr == 0x0000)
        {
          upperFull = true;
          packetCnt++;
        }
        if (readPtr == 0x1000)
        {
          lowerFull = true;
          packetCnt++;
        }
    }
  }

  /*
   Initializes the Registers for FluxCapture
*/
  void setupFTM0_flux()
  {
    // Input Filter waits for n cycles of stable input
    FTM0_FILTER = 0x00; // Filter off

    // Enable the FlexTimerModule and write registers
    // FAULTIE=0, FAULTM=00, CAPTEST=0, PWMSYNC=0, WPDIS=1, INIT=0, FTMEN=1
    FTM0_MODE = FTM_MODE_WPDIS | FTM_MODE_FTMEN;

    // Initialize Timer registers
    FTM0_SC = 0x00;                               // Diable Interrupts and Clocksource before initialization
    FTM0_CNT = 0x0000;                            // set counter value to 0
    FTM0_MOD = 0xFFFF;                            // set modulo to max value
    (*(volatile uint32_t *)FTPinMuxPort) = 0x413; // setup pin for Input Capture FTM0 in Pin Mux

    (*(volatile uint32_t *)FTMx_CnSC) = FTM_CSC_CHIE | FTM_CSC_ELSB;
    // CHF=0  CHIE=1 (enable interrupt)
    // MSB=0  MSA=0 (Channel Mode Input Capture)
    // ELSB=1 ELSA=0 (Input Capture on falling edge)
    // DMA=1/0  DMA on/off
    FTM0_C7SC &= ~FTM_CSC_CHF; // clear channel event flag
    FTM0_SC &= ~FTM_SC_TOF;
    FTM0_C7V = 0;

    // Enable FTM0 interrupt inside NVIC
    NVIC_SET_PRIORITY(IRQ_FTM0, 0);
  }

  void indexIrq()
  {
    indexCount++;
    indexes[indexCount] = micros();
    bitcells[indexCount] = cellCount;
    transferred[indexCount] = transCount;
  }

  /*
   captureFlux - test
   optional parameter silent to supress all debug messages
*/
  int64_t captureFlux(boolean silent, int revs)
  {
    if (revs == 0)
      revs = 1;
    errors = 0;
    for (uint8_t i = 0; i < 10; i++)
      indexes[i] = 0;
    indexCount = -1;
    streamLen = 0x2000; // 8192 Words
    for (int i = 0; i < 256; i++)
    {
      hist[i] = 0;
    }
    getDensity = false;
    motorOn();
    readPtr = 0;
    cellCount = 0;
    lastCNTVal = 0;
    overflow = 0;
    lowerFull = false;
    upperFull = false;
    packetCnt = 0;
    extError = NO_ERR;
    transCount = 0;
    for (uint32_t i = 0; i < streamLen; i++)
    {
      flux[i] = 0x0000;
    }
    attachInterruptVector(IRQ_FTM0, ftm0_flux_isr);
    setupFTM0_flux();
    watchdogReset();
    attachInterrupt(_index, indexIrq, FALLING);
    while (indexCount != 0)
      ;
    uint32_t timeout = micros();
#define waitforcell 20
    while (digitalReadFast(_readdata))
    {
      if (micros() > timeout + waitforcell)
        break;
    }
    timeout = micros();
    while (digitalReadFast(_readdata))
    {
      if (micros() > timeout + waitforcell)
        break;
    }
    FTM0_C7SC &= ~FTM_CSC_CHF;                                    // clear channel event flag
    FTM0_SC &= ~FTM_SC_TOF;                                       // clear timer overflow flag
    FTM0_SC = FTM_SC_TOIE | FTM_SC_CLKS(0b01) | FTM_SC_PS(0b000); // start capture
    NVIC_ENABLE_IRQ(IRQ_FTM0);
    flux[0] = 0;
    uint32_t tZeit = millis();
    //    boolean first = true;
    while (indexCount != revs)
    {
      if (upperFull)
      {
        upperFull = false;
        if (silent)
        {
          Serial.write(0x00);
          Serial.write(0x10);
#define paketsize 8
          for (int i = 0; i < 8192 / paketsize; i++)
          {
            Serial.write(&stream[8192 + i * paketsize], paketsize);
          }
          // Serial.write(&stream[8192], 8192);
        }
        else
        {
          for (int i = 1; i < 4096; i++)
            if ((flux[i + 4096] - flux[i + 4095]) > 500)
              Serial.printf("Pos: %d flux[%d]: %d flux[%d]: %d\n", transCount + i, i, flux[i + 4096], i - 1, flux[i + 4095]);
          Serial.printf("Time: %d Pos: %d Len: %d Buf: upper\n", millis() - tZeit, cellCount - 4096, 4096);
          //Serial.write(&stream[8192], 8192);
        }
        transCount += 4096;
      }
      if (lowerFull)
      {
        lowerFull = false;
        if (silent)
        {
          // Serial.write(millis() - zeit);
          // Serial.write(cellCount - 4096);
          // Serial.write((uint16_t)0x1000);
          Serial.write(0x00);
          Serial.write(0x10);
          for (int i = 0; i < 8192 / paketsize; i++)
          {
            Serial.write(&stream[i * paketsize], paketsize);
          }
          // Serial.write(&stream[0], 8192);
        }
        else
        {
          // if (first)
          // {
          //   for (int l = 0; l < 16; l++)
          //   {
          //     Serial.printf("%d ", flux[l]);
          //   }
          //   Serial.println();
          //   first = false;
          // }
          for (int i = 1; i < 4096; i++)
            if ((flux[i] - flux[i - 1]) > 480)
              Serial.printf("Pos: %d flux[%d]: %d flux[%d]: %d\n", transCount + i, i, flux[i], i - 1, flux[i - 1]);
          Serial.printf("Time: %d Pos: %d Len: %d Buf: lower\n", millis() - tZeit, cellCount - 4096, 4096);
          //Serial.write(&stream[0], 8192);
        }
        transCount += 4096;
      }
    }
    FTM0_SC = 0x00; // Timer off
    NVIC_DISABLE_IRQ(IRQ_FTM0);
    detachInterrupt(_index);
    if (readPtr < 4096)
    {
      if (silent)
      {
        // Serial.write(millis() - zeit);
        // Serial.write(fluxCount - readPtr);
        Serial.write((uint8_t)readPtr);
        Serial.write((uint8_t)(readPtr >> 8));
        for (int i = 0; i < 8192 / paketsize; i++)
        {
          Serial.write(&stream[i * paketsize], paketsize);
        }
        // Serial.write(&stream[0], 8192);
        packetCnt++;
      }
      else
      {
        Serial.printf("Time: %d Pos: %d Len: %d Buf: lower\n", millis() - tZeit, cellCount - readPtr, readPtr);
        //Serial.write(&stream[0], 8192);
      }
      transCount += readPtr;
    }
    else
    {
      if (silent)
      {
        // Serial.write(millis() - zeit);
        // Serial.write(fluxCount - (readPtr - 4096));
        Serial.write((uint8_t)readPtr - 4096);
        Serial.write((uint8_t)((readPtr - 4096) >> 8));
        for (int i = 0; i < 8192 / paketsize; i++)
        {
          Serial.write(&stream[8192 + i * paketsize], paketsize);
        }
        // Serial.write(&stream[8192], 8192);
        packetCnt++;
      }
      else
      {
        Serial.printf("Time: %d Pos: %d Len: %d Buf: upper\n", millis() - tZeit, cellCount - (readPtr - 4096), readPtr - 4096);
        //Serial.write(&stream[8192], 8192);
      }
      transCount += readPtr - 4096;
    }
    if (!silent)
    {
      for (int i = 1; i <= revs; i++)
        Serial.printf("Index[%d] at %d us, cellcount: %d\n", i, indexes[i] - indexes[i - 1], bitcells[i]);
      Serial.printf("Transfered: %d ReadPtr@ 0x%x cellCount %d cells in %dms\n", transCount, readPtr, cellCount, millis() - tZeit);
      // for (int l = 0; l < 16; l++)
      // {
      //   Serial.printf("%d ", flux[l]);
      // }
      // Serial.println();
    }
    setMode(densMode);
    attachInterruptVector(IRQ_FTM0, ftm0_isr);
    return errors;
  }

  void SerialWriteLong(uint32_t n)
  {
    Serial.write(n);
    Serial.write(n >> 8);
    Serial.write(n >> 16);
    Serial.write(n >> 24);
  }

  void getIndexes(int revs)
  {
    for (int i = 1; i <= revs; i++)
    {
      SerialWriteLong(indexes[i] - indexes[i - 1]);
      SerialWriteLong(bitcells[i]);
      SerialWriteLong(transferred[i]);
    }
  }

  void getTransCount()
  {
    SerialWriteLong(transCount);
  }

  void getPacketCnt()
  {
    SerialWriteLong(packetCnt);
  }
  /*
   Initializes Variables for reading a track
*/
  void initRead()
  {
    bCnt = 0;
    readPtr = 0;
    fluxCount = 0;
    sectorCnt = 0;
    errors = 0;
    errorH = 0;
    errorD = 0;
    extError = NO_ERR;
    for (uint32_t i = 0; i < streamLen; i++)
    {
      stream[i] = 0x00;
    }
    setupFTM0();
  }

  /*
   starts recording
*/
  void startRecord()
  {
    FTM0_CNT = 0x0000; // Reset the count to zero
    uint32_t zeit = millis();
    while (digitalReadFast(_readdata))
    {
      if (millis() > zeit + 1)
        break;
    }
    recordOn = true;
    FTM0_SC = timerMode;
  }

  /*
   stops recording
*/
  void stopRecord()
  {
    recordOn = false;
    FTM0_SC = 0x00; // Timer off
  }

  /*
   read function with verbose output
*/
  void doRead(int track, boolean ignoreIndex)
  {
    gotoLogicTrack(track);
    errors = readTrack2(false, false, ignoreIndex);
    if (errors != -1)
    {
      Serial.printf("Sectors found: %d Errors found: ", sectorCnt);
      Serial.print(errorD | errorH, BIN);
      Serial.println();
      Serial.printf("Track expected: %d Track found: %d\n", param.toInt(), getTrackInfo());
      Serial.printf("high2 %d high3 %d fluxCount: %d\n", high2, high3, fluxCount);
      Serial.println("OK");
    }
    else
    {
      Serial.printf("Sectors found: %d Errors found: ", sectorCnt);
      Serial.print(errorD | errorH, BIN);
      Serial.println();
      Serial.printf("Track expected: %d Track found: %d\n", param.toInt(), getTrackInfo());
      Serial.printf("high2 %d high3 %d fluxCount: %d\n", high2, high3, fluxCount);
      Serial.println("Read failed!");
    }
  }

  /*
  tests if inserted media is a HD or DD disk
  1 = dd; 2 = hd; 0 = defect media; -1 = write protected media
*/
  int16_t probeTrack(boolean report)
  {
    if (eraseTrack(transTimeDD) == -1)
      return -1;
    countFluxes();
    int32_t diff = writeBitCnt - fluxCount;
    if (report)
      Serial.printf("2µs write: %6d read: %6d diff: %5d\n", writeBitCnt, fluxCount, diff);
    eraseTrack(transTimeDD * 2);
    countFluxes();
    if (report)
      Serial.printf("4µs write: %6d read: %6d diff: %5d\n", writeBitCnt, fluxCount, diff);
    if (fluxCount < 40000)
      return 0;
    if (diff > 10000)
      return 1;
    eraseTrack(transTimeDD);
    return 2;
  }

  /*************************************************************************************************************************************************************
   write functions
 *************************************************************************************************************************************************************/
#ifdef PITTIMER
  /*
   Interrupt Service Routine for PIT Module
*/

#ifdef USEBITBAND
  extern "C" void write_isr(void)
  {
    // if (writeActive == false)
    //   return;
    digitalWriteFast(_writedata, !streamBitband[writePtr++]);
    PIT_TFLG2 = 1; // clear interrupt flag
    if (writePtr >= ((writeSize * 8) + 8))
    {
      writeActive = false;
      digitalWriteFast(_writedata, HIGH);
      PIT_TCTRL2 = 0; // Timer Interrupt disable, Timer disable
    }
  }
#else
  extern "C" void write_isr(void)
  {
    static byte dataByte = 0xaa;
    static byte writeBitCnt = 0;
    static uint16_t writePtr = 0;
    if (writeActive == false)
      return;
    digitalWriteFast(_writedata, !(dataByte >> 7));
    writeBitCnt++;
    dataByte = dataByte << 1;
    if (writeBitCnt == 8)
    {
      writePtr++;
      dataByte = stream[writePtr];
      writeBitCnt = 0;
    }
    if (writePtr >= writeSize)
    {
      writeActive = false;
      digitalWriteFast(_writedata, HIGH);
      dataByte = 0xaa;
      writeBitCnt = 0;
      writePtr = 0;
      PIT_TCTRL2 = 0; // Timer Interrupt disable, Timer disable
    }
    PIT_TFLG2 = 1; // clear interrupt flag
  }
#endif
  /*
   starts the Precision Interrupt Timer (PIT) Module
*/
  void startPIT2(uint32_t clock, void (*function)(void))
  {
    stopErrorLed();
    NVIC_DISABLE_IRQ(IRQ_PIT_CH2);
    SIM_SCGC6 |= SIM_SCGC6_PIT;
    PIT_MCR = 1;
    attachInterruptVector(IRQ_PIT_CH2, function);
    //  uint32_t clock = (float)(F_BUS / 1000000) * transTime - 0.5;
    // Serial.println(clock);
    PIT_LDVAL2 = clock;
    PIT_TFLG2 = 1;                              // clear interrupt flag
    PIT_TCTRL2 = PIT_TCTRL_TIE | PIT_TCTRL_TEN; // Timer Interrupt Enable, Timer Enable

    NVIC_SET_PRIORITY(IRQ_PIT_CH2, 0);
    NVIC_ENABLE_IRQ(IRQ_PIT_CH2);
  }

  void stopPIT2()
  {
    PIT_MCR = 1;
    PIT_TFLG2 = 1;  // clear interrupt flag
    PIT_TCTRL2 = 0; // Timer Interrupt disable, Timer disable

    NVIC_SET_PRIORITY(IRQ_PIT_CH2, 128);
    NVIC_DISABLE_IRQ(IRQ_PIT_CH2);
    startErrorLed();
  }
  /*
   writes a track from buffer to the floppydrive
   return -1 if disk is write protected
*/
  int writeTrack(boolean erase)
  {
    // NVIC_DISABLE_IRQ(IRQ_PIT_CH0);
    // NVIC_DISABLE_IRQ(IRQ_USBOTG);
    if (erase)
      eraseTrack(transitionTime * 2);
    watchdogReset();
    motorOn();
    if (writeProtect())
    {
      extError = WPROT_ERR;
      return -1;
    }
#ifdef USEBITBAND
    for (uint32_t i = 0; i < writeSize; i++)
    {
      stream[i] = reverse(stream[i]);
    }
#endif //USEBITBAND
    writePtr = 0;
    writeBitCnt = 0;
    dataByte = stream[writePtr];
    writeActive = true;
    if (indexAligned)
    {
      waitForIndex();
      delay(200 - 23);
    }
    int zeit = millis();
    startPIT2(transitionTime, write_isr);
    digitalWriteFast(_writeen, LOW); // enable writegate after starting timer because first interrupt
    // occurs in about 2µs
    while (writeActive == true)
    {
    }
    stopPIT2();
    zeit = millis() - zeit;
    // Serial.printf("writeSize: %d writePtr: %d writeBitCount: %d\n", writeSize, writePtr, writeBitCnt);
    // Serial.printf("Time taken: %d ms\n",zeit);
    delayMicroseconds(2);
    digitalWriteFast(_writeen, HIGH);
    // NVIC_ENABLE_IRQ(IRQ_PIT_CH0);
    // NVIC_ENABLE_IRQ(IRQ_USBOTG);
    delay(5);
    return 0;
  }
#else // not PITTIMER
/*
   interrupt routine for writing data to floppydrive
*/
#ifdef USEBITBAND
void diskWrite()
{
  if (writeActive == false)
    return;
  //writeTimer.update(transitionTime);
  digitalWriteFast(_writedata, !streamBitband[writePtr]);
  writePtr++;
  if (writePtr >= ((writeSize * 8) + 8))
  {
    writeActive = false;
    digitalWriteFast(_writedata, HIGH);
  }
}
#else
/*
   diskwrite function without bitband memory i'll let this here
   in case to port this to another microcontroller
*/
void diskWrite()
{
  static byte dataByte = 0xaa;
  static byte writeBitCnt = 0;
  static uint16_t writePtr = 0;
  if (writeActive == false)
    return;
  //writeTimer.update(transitionTime);
  //digitalWriteFast(_writedata, !(dataByte & 0x80));
  digitalWriteFast(_writedata, !(dataByte >> 7));
  writeBitCnt++;
  dataByte = dataByte << 1;
  if (writeBitCnt == 8)
  {
    writePtr++;
    dataByte = stream[writePtr];
    writeBitCnt = 0;
  }
  if (writePtr >= writeSize)
  {
    writeActive = false;
    digitalWriteFast(_writedata, HIGH);
    dataByte = 0xaa;
    writeBitCnt = 0;
    writePtr = 0;
  }
}
#endif //USEBITBAND

/*
   writes a track from buffer to the floppydrive
   return -1 if disk is write protected
*/
int writeTrack(boolean erase)
{
  NVIC_DISABLE_IRQ(IRQ_PIT_CH0);
  NVIC_DISABLE_IRQ(IRQ_USBOTG);
  if (erase)
    eraseTrack(transitionTime * 2);
  watchdogReset();
  motorOn();
  if (writeProtect())
  {
    extError = WPROT_ERR;
    return -1;
  }
#ifdef USEBITBAND
  for (uint32_t i = 0; i < writeSize; i++)
  {
    stream[i] = reverse(stream[i]);
  }
#endif //USEBITBAND
  writePtr = 0;
  writeBitCnt = 0;
  dataByte = stream[writePtr];
  writeActive = true;
  writeTimer.priority(0);
  delayMicroseconds(100);
  //waitForIndex();
  int zeit = millis();
  writeTimer.begin(diskWrite, transitionTime);
  digitalWriteFast(_writeen, LOW); // enable writegate after starting timer because first interrupt
  // occurs in about 2µs
  while (writeActive == true)
  {
  }
  writeTimer.end();
  zeit = millis() - zeit;
  // Serial << "Time taken: " << zeit << "ms\n";
  delayMicroseconds(2);
  digitalWriteFast(_writeen, HIGH);
  NVIC_ENABLE_IRQ(IRQ_PIT_CH0);
  NVIC_ENABLE_IRQ(IRQ_USBOTG);
  delay(5);
  return 0;
}
#endif
  /*
   write a track with verify
*/
  extern "C" int writeWithVerify(int tTrack, int wRetries)
  {
    int error = 0;
    motorOn();
    floppyTrackMfmEncode(tTrack, (byte *)track, stream);
    gotoLogicTrack(tTrack);
    for (int i = 0; i < wRetries; i++)
    {
      writeTrack(preErase);
      error = readTrack(true, true);
#ifdef debug
      Serial.printf("%d ", error);
      printExtError();
#endif
      if (error == 0)
        return 0;
      floppyTrackMfmEncode(tTrack, (byte *)track, stream);
    }
    if (error != 0)
      return 1;
    else
      return 0;
  }

  /*
   write a track without verify
*/
  int writeWithoutVerify(int tTrack, int wRetries)
  {
    int error = 0;
    uint32_t zeit = millis();
    motorOn();
    Serial.printf("Motor on: %d ", millis() - zeit);
    zeit = millis();
    floppyTrackMfmEncode(tTrack, (byte *)track, stream);
    Serial.printf(" Encode: %d ", millis() - zeit);
    zeit = millis();
    gotoLogicTrack(tTrack);
    Serial.printf(" goto Track: %d ", millis() - zeit);
    zeit = millis();
    writeTrack(preErase);
    Serial.printf(" Write: %d\n", millis() - zeit);
    if (error != 0)
      return 1;
    else
      return 0;
  }

  /****************************************
   format & erase functions
 ****************************************/

  /*
   formats ONE track
*/
  int formatTrack(int tTrack, int wRetries, boolean verify)
  {
#ifdef debug
    Serial.printf("Formatting Track %d\n", tTrack);
#endif
    for (int i = 0; i < sectors; i++)
    {
      fillSector(i, true); // add ADFCopy TAG
    }
    // special handling if track = 0 -> bootblock
    if (tTrack == 0)
    {
      track[0].sector[28] = 0x44; //"D";
      track[0].sector[29] = 0x4f; //"O";
      track[0].sector[30] = 0x53; //"S";
      track[0].sector[31] = 0x00;
      for (int i = 32; i < 40; i++)
      {
        track[0].sector[i] = 0;
      }
    }
    // special handling for rootblock and bitmap
    if (tTrack == 80)
    {
      fillSector(0, false);
      fillSector(1, false);
      // construction of rootblock
      struct Sector *rootTemp = (Sector *)&track[0].sector;
      struct rootBlock *root = (rootBlock *)&rootTemp->data;
      root->type = __builtin_bswap32(2);
      root->ht_size = __builtin_bswap32(72);
      root->bm_flag = __builtin_bswap32(-1);
      if (densMode == HD)
        root->bm_pages[0] = __builtin_bswap32(1761);
      else
        root->bm_pages[0] = __builtin_bswap32(881);
      char dName[30] = "Empty";
      root->name_len = strlen(dName);
      memcpy(&root->diskname[0], &dName[0], strlen(dName));
      root->extension = 0;
      root->sec_type = __builtin_bswap32(1);
      uint32_t newsum = 0;
      root->chksum = 0;
      for (int i = 0; i < 128; i++)
        newsum += rootTemp->data[i * 4] << 24 | rootTemp->data[i * 4 + 1] << 16 | rootTemp->data[i * 4 + 2] << 8 | rootTemp->data[i * 4 + 3];
      newsum = -newsum;
      root->chksum = __builtin_bswap32(newsum);
      // construction of bitmap block
      struct Sector *bitmapTemp = (Sector *)&track[1].sector;
      struct bitmapBlock *bitmap = (bitmapBlock *)&bitmapTemp->data;
      if (densMode == HD)
      {
        for (int i = 0; i < 110; i++)
        {
          bitmap->map[i] = 0xffffffff;
        }
        bitmap->map[54] = __builtin_bswap32(0x3fffffff);
      }
      else
      {
        for (int i = 0; i < 55; i++)
        {
          bitmap->map[i] = 0xffffffff;
        }
        bitmap->map[27] = __builtin_bswap32(0xffff3fff);
      }
      newsum = 0;
      for (int i = 1; i < 128; i++)
        newsum -= bitmapTemp->data[i * 4] << 24 | bitmapTemp->data[i * 4 + 1] << 16 | bitmapTemp->data[i * 4 + 2] << 8 | bitmapTemp->data[i * 4 + 3];
      bitmap->checksum = __builtin_bswap32(newsum);
    }
    int ret = 0;
    if (verify)
      ret = writeWithVerify(tTrack, wRetries);
    else
      ret = writeWithoutVerify(tTrack, wRetries);
    return ret;
  }

  /*
   formats a whole disk in OFS
*/
  int formatDisk(boolean quick, boolean verify)
  {
    int errors = 0;
    motorOn();
    if (!writeProtect())
    {
      busyFormatting = true;
      if (quick)
      {
        formatTrack(0, 6, verify);
        formatTrack(80, 6, verify);
      }
      else
        for (int i = 0; i < 160; i++)
        {
          errors += formatTrack(i, 6, verify);
          if (mtpOn)
          {
            mtpd.loop();
            if ((i + 1) % 10 == 0)
              mtpd.sendEvent(MTP_EVENT_STORAGE_INFO_CHANGED, FDid);
            //          mtpd.sendEvent(MTP_EVENT_UNREPORTED_STATUS, 0);
          }
          if (errors != 0)
            return -1;
        }
      busyFormatting = false;
    }
    else
    {
#ifdef debug
      Serial.println("Disk ist write protected.");
#endif
      return 1;
    }
    return errors;
  }

#ifdef PITTIMER
  /*
   interrupt routine for erasing data on floppydrive
*/
  extern "C" void writeErase(void)
  {
    PIT_TFLG2 = 1; // clear interrupt flag
    if (writeActive == false)
      return;
    digitalWriteFast(_writedata, LOW);
    writeBitCnt++;
    digitalWriteFast(_writedata, HIGH);
  }

  /*
   writes 01 with a given frequency to the floppydrive
   return -1 if disk is write protected
*/
  int eraseTrack(uint32_t transitionDelay)
  {
    watchdogReset();
    motorOn();
    if (writeProtect())
    {
      extError = WPROT_ERR;
      return -1;
    }
    writeBitCnt = 0;
    writeActive = true;
    startPIT2(transitionDelay, writeErase);
    digitalWriteFast(_writeen, LOW); //
    delay(5);
    writeBitCnt = 0;
    // delayMicroseconds(RPMtime);
    delay(RPMtime / 1000);
    writeActive = false;
    stopPIT2();
    delayMicroseconds(2);
    digitalWriteFast(_writeen, HIGH);
    delay(5);
    return 0;
  }

  /*
   erases the complete disk as fast as possible
   return -1 if disk is write protected
*/
  int eraseDisk(uint32_t transitionDelay)
  {
    watchdogReset();
    motorOn();
    if (writeProtect())
    {
      extError = WPROT_ERR;
      return -1;
    }
    gotoLogicTrack(0);
    setDir(1);
    writeBitCnt = 0;
    writeActive = true;
    int zeit = millis();
    startPIT2(transitionDelay * 2, writeErase);
    for (int i = 0; i < 80; i++)
    {
      Serial.printf("%d\n", i * 2);
      digitalWriteFast(_writeen, LOW);
      delay(205);
      digitalWriteFast(_writeen, HIGH);
      watchdogReset();
      setSide(1);
      Serial.printf("%d\n", i * 2 + 1);
      digitalWriteFast(_writeen, LOW);
      delay(205);
      digitalWriteFast(_writeen, HIGH);
      watchdogReset();
      setSide(0);
      step1();
    }
    stopPIT2();
    zeit = millis() - zeit;
    gotoLogicTrack(0);
    return 0;
  }
#else  //not PITTIMER
/*
   interrupt routine for erasing data on floppydrive
*/
void writeErase()
{
  /*  static boolean writeBit = false;
    if (writeActive == false) return;
    digitalWriteFast(_writedata, writeBit);
    writeBit=!writeBit;
    writeBitCnt++;
  */
  if (writeActive == false)
    return;
  digitalWriteFast(_writedata, LOW);
  writeBitCnt++;
  digitalWriteFast(_writedata, HIGH);
}

/*
   writes 01 with a given frequency to the floppydrive
   return -1 if disk is write protected
*/
int eraseTrack(float transitionDelay)
{
  watchdogReset();
  motorOn();
  if (writeProtect())
  {
    extError = WPROT_ERR;
    return -1;
  }
  writeBitCnt = 0;
  writeActive = true;
  writeTimer.priority(0);
  writeTimer.begin(writeErase, transitionDelay);
  digitalWriteFast(_writeen, LOW); //
  delay(205);
  writeTimer.end();
  delayMicroseconds(2);
  digitalWriteFast(_writeen, HIGH);
  delay(5);
  return 0;
}

/*
   erases the complete disk as fast as possible
   return -1 if disk is write protected
*/
int eraseDisk(float transitionDelay)
{
  watchdogReset();
  motorOn();
  if (writeProtect())
  {
    extError = WPROT_ERR;
    return -1;
  }
  gotoLogicTrack(0);
  setDir(1);
  writeBitCnt = 0;
  writeActive = true;
  writeTimer.priority(0);
  int zeit = millis();
  writeTimer.begin(writeErase, transitionDelay);
  for (int i = 0; i < 80; i++)
  {
    Serial.printf("%d\n", i * 2);
    digitalWriteFast(_writeen, LOW);
    delay(205);
    digitalWriteFast(_writeen, HIGH);
    watchdogReset();
    setSide(1);
    Serial.printf("%d\n", i * 2 + 1);
    digitalWriteFast(_writeen, LOW);
    delay(205);
    digitalWriteFast(_writeen, HIGH);
    watchdogReset();
    setSide(0);
    step1();
  }
  writeTimer.end();
  zeit = millis() - zeit;
  gotoLogicTrack(0);
  return 0;
}
#endif // PITTIMER
#ifdef __cplusplus
}
#endif