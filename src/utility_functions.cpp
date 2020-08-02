#include <Arduino.h>
#include "utility_functions.h"
#include "vars_n_defs.h"
#include "floppy_control.h"
#include "floppy_rw.h"

/*
   Feeds the Watchdog
*/
void watchdogReset()
{
  static elapsedMillis watchdogTimer;

  if (watchdogTimer > 5)
  {
    watchdogTimer = 0;

    noInterrupts();
    WDOG_REFRESH = 0xA602;
    WDOG_REFRESH = 0xB480;
    interrupts();
  }
}

/*
   reads command from the serial port
*/
void getCommand()
{
  inputString = "";
  while (Serial.available())
  {
    char inChar = (char)Serial.read();
    inputString += inChar;
    if (inChar == '\n')
    {
      break;
    }
  }
}

#ifdef debug
void printPtr(byte *dest, String pName)
{
  Serial.printf("%s: %xl -> %xl\n", pName, (long)dest, (long)*dest);
}
#endif

/*
   the ISR for the blinking errorcodes
*/
#ifdef errLed
void errorLed()
{
  if (failureCode == 0)
  {
    digitalWriteFast(13, LOW);
    return;
  }
  else
  {
    blinkCode = ((blinkCode & 0x8000) ? 0x01 : 0x00) | (blinkCode << 1);
    digitalWriteFast(13, blinkCode & 0x0001);
  }
}

void startErrorLed()
{
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
  errorTimer.priority(255);
  errorTimer.begin(errorLed, 250000);
}

void stopErrorLed()
{
  errorTimer.end();
}
#endif
/*
   sets teensy pin to fastmode if OUTPUT
   slightly modified pinMode function from
   \hardware\teensy\avr\cores\teensy3\pins_teensy.c
*/
void pinModeFast(uint8_t pin, uint8_t mode)
{
  volatile uint32_t *config;

  if (pin >= CORE_NUM_DIGITAL)
    return;
  config = portConfigRegister(pin);

  if (mode == OUTPUT || mode == OUTPUT_OPENDRAIN)
  {
#ifdef KINETISK
    *portModeRegister(pin) = 1;
#else
    *portModeRegister(pin) |= digitalPinToBitMask(pin); // TODO: atomic
#endif
    *config = PORT_PCR_DSE | PORT_PCR_MUX(1);
    if (mode == OUTPUT_OPENDRAIN)
    {
      *config |= PORT_PCR_ODE;
    }
    else
    {
      *config &= ~PORT_PCR_ODE;
    }
  }
  else
  {
#ifdef KINETISK
    *portModeRegister(pin) = 0;
#else
    *portModeRegister(pin) &= ~digitalPinToBitMask(pin);
#endif
    if (mode == INPUT || mode == INPUT_PULLUP || mode == INPUT_PULLDOWN)
    {
      *config = PORT_PCR_MUX(1);
      if (mode == INPUT_PULLUP)
      {
        *config |= (PORT_PCR_PE | PORT_PCR_PS); // pullup
      }
      else if (mode == INPUT_PULLDOWN)
      {
        *config |= (PORT_PCR_PE); // pulldown
        *config &= ~(PORT_PCR_PS);
      }
    }
    else
    {
      *config = PORT_PCR_MUX(1) | PORT_PCR_PE | PORT_PCR_PS; // pullup
    }
  }
}

unsigned char reverse(unsigned char b)
{
  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
  return b;
}

void printExtError()
{
  switch (extError)
  {
  case NO_ERR:
    Serial.println("OK\0");
    break;
  case WPROT_ERR:
    Serial.println("Disk is write-protected");
    break;
  case TIMEOUT_ERR:
    Serial.println("Timeout in read operation");
    break;
  case SEEK_ERR:
    Serial.println("Seek error / Wrong track in Sector Header");
    break;
  case NUMSECTOR_ERR:
    Serial.println("Incorrect number of sectors on track");
    break;
  case TRACK0_ERR:
    Serial.println("Seek Error Track 0");
    break;
  case SECBOUNDS_ERR:
    Serial.println("Sector out of bounds");
    break;
  case HEADERCHKSUM_ERR:
    Serial.println("Header bad checksum");
    break;
  case DATACHKSUM_ERR:
    Serial.println("Data bad checksum");
    break;
  case UPLOAD_ERR:
    Serial.println("Track upload error");
    break;
  default:
    Serial.println(extError);
  }
}
/*
   read Diskname from Track 80
*/
String getName()
{
  if (!noDisk)
  {
    String volumeName;
    gotoLogicTrack(80);
    readTrack(true, false);
    struct Sector *aSec = (Sector *)&track[0].sector;
    volumeName = "NDOS";
    int nameLen = aSec->data[432];
    if (nameLen > 30)
      return "NDOS";
    int temp = 0;
    for (int i = 0x04; i < 0x0c; i++)
    {
      temp += aSec->data[i];
    }
    for (int i = 0x10; i < 0x14; i++)
    {
      temp += aSec->data[i];
    }
    for (int i = 463; i < 472; i++)
    {
      temp += aSec->data[i];
    }
    for (int i = 496; i < 504; i++)
    {
      temp += aSec->data[i];
    }
    if (temp != 0)
      return "NDOS";
    for (int i = 0; i < 4; i++)
    {
      temp += aSec->data[i];
    }
    if (temp != 2)
      return "NDOS";
    temp = 0;
    for (int i = 508; i < 512; i++)
    {
      temp += aSec->data[i];
    }
    if (temp != 1)
      return "NDOS";
    volumeName = "";
    for (int i = 0; i < nameLen; i++)
    {
      volumeName += (char)aSec->data[433 + i];
    }
    return volumeName;
  }
  else
  {
    return "NODISK";
  }
}
int findMinima(int start)
{
  int first = 0;
  int last = 0;
  int tMin = 500;
  for (int i = -30; i < 30; i++)
  {
    if (hist[i + start] < tMin)
    {
      tMin = hist[i + start];
      first = i + start;
    }
    if (hist[i + start] == tMin)
    {
      tMin = hist[i + start];
      last = i + start;
    }
  }
  return (first + last) / 2;
}

/*
   adjusts the timings for the transitions lengths, that way there is a better chance
   to read disks written by drives with too slow / fast rpm
*/
void adjustTimings()
{
  setTimings(densMode);
  high2 = findMinima(high2);
  high3 = findMinima(high3);
}

/*
   prints histogram of last read track in ascii
   mainly for debugging purposes
*/
void printHist(int scale)
{
  float zeit;
  uint32_t summe = 0;
  uint16_t maxScale = 0;
  for (int i = 0; i < 256; i++)
    if (hist[i] > maxScale)
      maxScale = hist[i];
  if (scale == -1)
    scale = maxScale / 70;
  for (int i = 0; i < 256; i++)
  {
    if (hist[i] > 0)
    {
      summe = summe + (hist[i] * i);
      if (densMode == HD)
        zeit = (float(i) * 0.02083333) + 0.0625;
      else
        zeit = (float(i) * 0.04166667) + 0.0625;
      // (count * time per count) + FTMx Delay (3 SysClk)
      Serial.print(zeit);
      Serial.printf(":%3d-%5d|", i, hist[i]);
      for (int j = 0; j < (hist[i] / scale); j++)
      {
        Serial.print("+");
      }
      Serial.println();
    }
  }
  Serial.printf("Summe: %ld\n", summe);
  Serial.printf("1. Minima: %d high2: %d\n", findMinima(high2), high2);
  Serial.printf("2. Minima: %d high3: %d\n", findMinima(high3), high3);
}

/*
   outputs the histogram of flux transistions in binary form
*/
void printFlux()
{
  byte a, b, c, d;
  for (int i = 0; i < 256; i++)
  {
    a = hist[i];
    b = hist[i] >> 8;
    c = hist[i] >> 16;
    d = hist[i] >> 24;
    Serial.write(a);
    Serial.write(b);
    Serial.write(c);
    Serial.write(d);
  }
}

/*
   counts the transistions and calculates the real read bits of the last read track
   mainly for debugging
*/
void analyseHist(boolean silent)
{
  uint32_t trackLen = 0;
  uint32_t transitions = 0;
  int samp = 0;
  for (int i = 0; i < 256; i++)
  {
    samp = hist[i];
    if ((i >= low2) && (i <= high2))
    {
      trackLen += 2 * samp;
      transitions += samp;
    }
    if ((i >= high2 + 1) && (i <= high3))
    {
      trackLen += 3 * samp;
      transitions += samp;
    }
    if ((i >= high3 + 1) && (i <= high4))
    {
      trackLen += 4 * samp;
      transitions += samp;
    }
  }
  if (silent == false)
  {
    Serial.print("Transitions: ");
    Serial.print(transitions);
    Serial.print(" Real Bits: ");
    Serial.println(trackLen);
  }
}

/*
   decodes one MFM encoded Sector into Amiga Sector
   partly based on DecodeSectorData and DecodeLongword from AFR.C, written by
   Marco Veneri Copyright (C) 1997 released as public domain
*/
int decodeSector(uint32_t secPtr, int index, boolean silent, boolean ignoreIndex)
{
  secPtr += 8; // skip sync and magic word
  uint32_t tmp[4];
  uint32_t decoded;
  uint32_t chkHeader = 0;
  uint32_t chkData = 0;
  //decode format, track, sector, distance 2 gap
  for (int i = 0; i < 1; i++)
  {
    tmp[0] = ((stream[secPtr + (i * 8) + 0] << 8) + stream[secPtr + (i * 8) + 1]) & 0x5555;
    tmp[1] = ((stream[secPtr + (i * 8) + 2] << 8) + stream[secPtr + (i * 8) + 3]) & 0x5555;
    tmp[2] = ((stream[secPtr + (i * 8) + 4] << 8) + stream[secPtr + (i * 8) + 5]) & 0x5555;
    tmp[3] = ((stream[secPtr + (i * 8) + 6] << 8) + stream[secPtr + (i * 8) + 7]) & 0x5555;

    // even bits
    tmp[0] = (tmp[0] << 1);
    tmp[1] = (tmp[1] << 1);

    // or with odd bits
    tmp[0] |= tmp[2];
    tmp[1] |= tmp[3];

    // final longword
    decoded = ((tmp[0] << 16) | tmp[1]);

    sectorTable[index].sector = (decoded >> 8) & 0xff;
    if (!ignoreIndex)
    {
      index = (decoded >> 8) & 0xff;
      // if sector out of bounds, return with error
      if ((index + 1 > sectors) || (index < 0))
      {
        errors = errors | (1 << 31);
        extError = SECBOUNDS_ERR;
        return -1;
      }
    }
    tempSector[(i * 4) + 0] = decoded >> 24; // format type 0xff = amiga
    tempSector[(i * 4) + 1] = decoded >> 16; // track
    tempSector[(i * 4) + 2] = decoded >> 8;  // sector
    tempSector[(i * 4) + 3] = decoded;       // distance to gap
  }
  //decode checksums
  for (int i = 5; i < 7; i++)
  {
    tmp[0] = ((stream[secPtr + (i * 8) + 0] << 8) + stream[secPtr + (i * 8) + 1]) & 0x5555;
    tmp[1] = ((stream[secPtr + (i * 8) + 2] << 8) + stream[secPtr + (i * 8) + 3]) & 0x5555;
    tmp[2] = ((stream[secPtr + (i * 8) + 4] << 8) + stream[secPtr + (i * 8) + 5]) & 0x5555;
    tmp[3] = ((stream[secPtr + (i * 8) + 6] << 8) + stream[secPtr + (i * 8) + 7]) & 0x5555;
    // even bits
    tmp[0] = (tmp[0] << 1);
    tmp[1] = (tmp[1] << 1);
    // or with odd bits
    tmp[0] |= tmp[2];
    tmp[1] |= tmp[3];
    // final longword
    decoded = ((tmp[0] << 16) | tmp[1]);
    tempSector[(i * 4) + 0] = decoded >> 24;
    tempSector[(i * 4) + 1] = decoded >> 16;
    tempSector[(i * 4) + 2] = decoded >> 8;
    tempSector[(i * 4) + 3] = decoded;
    // store checksums for later use
    if (i == 5)
    {
      chkHeader = decoded;
    }
    else
    {
      chkData = decoded;
    }
  }
  // decode all the even data bits
  uint32_t data;
  for (int i = 0; i < 256; i++)
  {
    data = ((stream[secPtr + (i * 2) + 56] << 8) + stream[secPtr + (i * 2) + 57]) & 0x5555;
    tempSector[(i * 2) + 28] = (unsigned char)(data >> 7);
    tempSector[(i * 2) + 29] = (unsigned char)(data << 1);
  }

  // or with odd data bits
  for (int i = 0; i < 256; i++)
  {
    data = ((stream[secPtr + (i * 2) + 56 + 512] << 8) + stream[secPtr + (i * 2) + 57 + 512]) & 0x5555;
    tempSector[(i * 2) + 28] |= (unsigned char)(data >> 8);
    tempSector[(i * 2) + 29] |= (unsigned char)(data);
  }
  // check für checksum errors and generate error flags
  if (calcChkSum(secPtr, 0, 40) != chkHeader)
  {
    errorH = errorH | (1 << index);
    if (!silent)
      Serial.print("H");
    extError = HEADERCHKSUM_ERR;
  }
  if (calcChkSum(secPtr, 56, 1024) != chkData)
  {
    errorD = errorD | (1 << index);
    if (!silent)
      Serial.print("D");
    extError = DATACHKSUM_ERR;
  }
  return index;
}

/*
   decodes one MFM encoded Sector into Dos Sector
*/
int decodeDos(uint32_t secPtr, int index, boolean silent, boolean ignoreIndex)
{
  secPtr += 8; // skip sync and magic word

  uint32_t data;
  uint8_t outByte;
  for (int i = 0; i < 512; i++)
  {
    outByte = 0;
    data = ((stream[secPtr + (i * 2)] << 8) + stream[secPtr + (i * 2) + 1]);
    for (int j = 0; j < 8; j++)
    {
      if ((data & 0x8000) != 0)
        outByte = outByte | 0x80;
      outByte = outByte >> 1;
      data = data << 2;
    }
    tempSector[i + 28] = outByte;
  }
  return index;
}

/*
   calculates a checksum of <secPtr> at <pos> for <b> bytes length
   returns checksum
*/
uint32_t calcChkSum(uint32_t secPtr, int pos, int b)
{
  uint32_t chkSum = 0;
  uint32_t tSum = 0;
  for (int i = 0; i < b / 4; i++)
  {
    tSum = stream[secPtr + (i * 4) + pos + 0];
    tSum = tSum << 8;
    tSum += stream[secPtr + (i * 4) + pos + 1];
    tSum = tSum << 8;
    tSum += stream[secPtr + (i * 4) + pos + 2];
    tSum = tSum << 8;
    tSum += stream[secPtr + (i * 4) + pos + 3];
    chkSum = chkSum ^ tSum;
  }
  chkSum = chkSum & 0x55555555;
  return chkSum;
}

/*
   decodes a whole track
   optional parameter silent to suppress all serial debug info
*/
int decodeTrack(boolean silent, boolean compOnly, boolean ignoreIndex)
{
  if (!silent)
  {
    Serial.printf("Decoding Bitstream, expecting %d Sectors.\n", sectorCnt);
    Serial.print("Sectors start at: ");
  }
  int index = 0;
  for (uint32_t i = 0; i < sectorCnt; i++)
  {
    if (!silent)
    {
      Serial.print(sectorTable[i].bytePos);
    }
    if (i != sectorCnt - 1)
    {
      if (!silent)
      {
        Serial.print(", ");
      }
    }
    if (ignoreIndex)
      index = decodeDos(sectorTable[i].bytePos, i, silent, ignoreIndex);
    else
      index = decodeSector(sectorTable[i].bytePos, i, silent, ignoreIndex);
    if (index == -1)
      return errorD | errorH;
    if (compOnly)
    {
      if (memcmp(track[index].sector, tempSector, 540) != 0)
        return -1;
    }
    else
    {
      memcpy(track[index].sector, tempSector, 540);
    }
  }
  if (!silent)
  {
    Serial.println();
  }
  errors = errorD | errorH;
  return errors;
}

/*
   returns current track number from decoded sectors in buffer
*/
int getTrackInfo()
{
  int tTrack = 0;
  for (uint32_t i = 0; i < sectorCnt; i++)
  {
    tTrack = tTrack + track[i].sector[1];
  }
  return tTrack / sectors;
}

/*
   dumps the sector <index> from the buffer in human readable acsii to the serial port
   mainly for debugging
*/
void printAmigaSector(int index)
{
  struct Sector *aSec = (Sector *)&track[index].sector;
  Serial.printf("Format Type: %d Track: %d Sector: %d NumSec2Gap: %d Data Chk: ",
                aSec->format_type, aSec->track, aSec->sector, aSec->toGap);
  Serial.print(aSec->data_chksum, HEX);
  Serial.printf(" Header Chk: ");
  Serial.println(aSec->header_chksum, HEX);
  for (int i = 0; i < 16; i++)
  {
    for (int j = 0; j < 32; j++)
    {
      if (aSec->data[(i * 32) + j] < 16)
      {
        Serial.print("0");
      }
      Serial.print(aSec->data[(i * 32) + j], HEX);
      Serial.print(" ");
    }
    for (int j = 0; j < 32; j++)
    {
      Serial.print(byte2char(aSec->data[(i * 32) + j]));
    }
    Serial.println();
  }
}

/*
   dumps the whole track in ascii
   mainly for debugging
*/
void printTrack()
{
  for (uint32_t i = 0; i < sectorCnt; i++)
  {
    printAmigaSector(i);
  }
}

/*
   dumps the data section of a sector in binary format
*/
void dumpSector(int index)
{
  struct Sector *aSec = (Sector *)&track[index].sector;
  /*  for (int i = 0; i < 512; i++) {
      Serial.print((char)aSec->data[i]);
    }*/
  Serial.write(aSec->data, 512);
}

/*
   returns a pointer to the sector
*/
extern "C" byte *ptrSector(int index)
{
  struct Sector *aSec = (Sector *)&track[index].sector;
  return &aSec->data[0];
}

/*
   reads a sector from the serial in binary format
*/
int loadSector(int index)
{
  char tBuffer[512];
  struct Sector *aSec = (Sector *)&track[index].sector;
  int rByte = Serial.readBytes(tBuffer, 512);
  if (rByte != 512)
    return -1;
  for (int i = 0; i < 512; i++)
  {
    aSec->data[i] = tBuffer[i];
  }
  return 0;
}

/*
   dumps the whole track in binary form to the serial port
*/
void downloadTrack()
{
  for (int i = 0; i < sectors; i++)
  {
    dumpSector(i);
  }
}

/*
   reads a whole track from the serial port in binary form
*/
void uploadTrack()
{
  errors = 0;
  extError = NO_ERR;
  for (int i = 0; i < sectors; i++)
  {
    if (loadSector(i) != 0)
    {
      extError = UPLOAD_ERR;
      errors = -1;
      return;
    }
  }
}

/*
   returns c if printable, else returns a whitespace
*/
char byte2char(byte c)
{
  if ((c < 32) | (c > 126))
  {
    return (char)46;
  }
  else
  {
    return (char)c;
  }
}

/*
   prints v in ascii encoded binary
*/
void print_binary(int v, int num_places)
{
  int mask = 0, n;
  for (n = 1; n <= num_places; n++)
  {
    mask = (mask << 1) | 0x0001;
  }
  v = v & mask; // truncate v
  while (num_places)
  {
    if (v & (0x0001 << (num_places - 1)))
    {
      Serial.print("1");
    }
    else
    {
      Serial.print("0");
    }
    --num_places;
  }
}

/*
   prints v in ascii encoded binary reversed
   lowest bit first
*/
void print_binary_rev(uint32_t v, uint32_t num_places)
{
  for (uint32_t i = 0; i < num_places; i++)
  {
    if (v & 0x01)
      Serial.print("1");
    else
      Serial.print("0");
    v = v >> 1;
  }
}

/*
   fills a sector with "ADFCopy"
*/
void fillSector(int sect, boolean fillType)
{
  char adfTag[8] = "ADFCopy";
  for (int i = 0; i < 64; i++)
  {
    if (fillType)
    {
      memcpy(&track[sect].sector[i * 8 + 28], &adfTag, 8);
    }
    else
    {
      memset(&track[sect].sector[i * 8 + 28], 0, 8);
    }
  }
}

/*
   encodes odd bits of a longword to mfm
*/
uint32_t oddLong(uint32_t odd)
{
  odd = ((odd >> 1) & MFM_MASK);
  odd = odd | ((((odd ^ MFM_MASK) >> 1) | 0x80000000) & ((odd ^ MFM_MASK) << 1));
  return odd;
}

/*
   encodes even bits of a longword to mfm
*/
uint32_t evenLonger(uint32_t even)
{
  even = (even & MFM_MASK);
  even = even | ((((even ^ MFM_MASK) >> 1) | 0x80000000) & ((even ^ MFM_MASK) << 1));
  return even;
}

/*
   encodes one byte <curr> into mfm, takes into account if previous byte ended with a set bit
   returns the two mfm bytes as a word
*/
uint16_t mfmByte(byte curr, uint16_t prev)
{
  byte even = ((curr >> 1) & 0x55);
  even = even | ((((even ^ MFM_MASK) >> 1) | 0x8000000) & (((even ^ MFM_MASK) << 1)));
  if ((prev & 0x0001) == 1)
  {
    even = even & 0x7f;
  }
  byte odd = (curr & 0x55);
  odd = odd | ((((odd ^ MFM_MASK) >> 1) | 0x8000000) & (((odd ^ MFM_MASK) << 1)));
  if ((prev & 0x0100) == 0x0100)
  {
    odd = odd & 0x7f;
  }
  return (odd << 8) | even;
}

/*
   stores a longword in the byte array
*/
void putLong(uint32_t tLong, byte *dest)
{
  *(dest + 0) = (byte)((tLong & 0xff000000) >> 24);
  *(dest + 1) = (byte)((tLong & 0xff0000) >> 16);
  *(dest + 2) = (byte)((tLong & 0xff00) >> 8);
  *(dest + 3) = (byte)((tLong & 0xff));
}

/*
   encodes a sector into a mfm bitstream
*/
void encodeSector(uint32_t tra, uint32_t sec, byte *src, byte *dest)
{
  uint32_t tmp, headerChkSum, dataChkSum;
  uint16_t curr;
  uint16_t prev;
  byte prevByte;
  struct Sector *aSec = (Sector *)(src - 28);

  // write sync and magic word
  putLong(0xaaaaaaaa, dest + 0);
  putLong(0x44894489, dest + 4);

  // format, track, sector, distance to gap
  tmp = 0xff000000 | (tra << 16) | (sec << 8) | (sectors - sec);
  putLong(oddLong(tmp), dest + 8);
  putLong(evenLonger(tmp), dest + 12);
  // update the source sector
  aSec->format_type = 0xff;
  aSec->track = tra;
  aSec->sector = sec;
  aSec->toGap = sectors - sec;

  // fill unused space in sector header
  prevByte = *(dest + 15);
  for (int i = 16; i < 48; i++)
  {
    if ((prevByte & 0x01) == 1)
    {
      *(dest + i) = 0xaa & 0x7f;
    }
    else
    {
      *(dest + i) = 0xaa;
    }
    prevByte = *(dest + i);
  }
  //update source sector
  for (int i = 0; i < 16; i++)
    aSec->os_recovery[i] = 0;

  // data block encode
  prev = (uint16_t)prevByte;
  for (int i = 64; i < 576; i++)
  {
    curr = mfmByte(*(src + i - 64), prev);
    prev = curr;
    *(dest + i) = (byte)(curr & 0xff);
    *(dest + i + 512) = (byte)(curr >> 8);
  }

  // calc headerchecksum
  headerChkSum = calcChkSum(sectorTable[sec].bytePos, 8, 40);
  putLong(oddLong(headerChkSum), dest + 48);
  putLong(evenLonger(headerChkSum), dest + 52);
  // update source sector headerChkSum
  aSec->header_chksum = __builtin_bswap32(headerChkSum);

  // calc datachecksum
  dataChkSum = calcChkSum(sectorTable[sec].bytePos, 64, 1024);
  putLong(oddLong(dataChkSum), dest + 56);
  putLong(evenLonger(dataChkSum), dest + 60);
  // update source sector headerChkSum
  aSec->data_chksum = __builtin_bswap32(dataChkSum);
}

/*
   fills bitstream with 0xAA
*/
void fillTrackGap(byte *dst, int len)
{
  for (int i = 0; i < len; i++)
  {
    *dst++ = 0xaa;
  }
}

/*
   encodes a complete track + gap into mfm bitstream
   the gap gets encoded before the sectors to make sure when the track gap is too long for
   the track the first sector doesnt gets overwritten, this way only the track gap gets overwritten
   and the track contains no old sector headers
*/
void floppyTrackMfmEncode(uint32_t track_t, byte *src, byte *dst)
{
  fillTrackGap(dst, FLOPPY_GAP_BYTES);
  for (int i = 0; i < sectors; i++)
  {
    sectorTable[i].bytePos = (i * 1088) + FLOPPY_GAP_BYTES;
    encodeSector(track_t, i, src + 28 + (i * 540), dst + (i * 1088) + FLOPPY_GAP_BYTES);
  }
}

/*
   Sets standard timings for Teensy 3.2
*/
void setTimings(int density)
{
  if (density == 1)
  {
    low2 = tHDlow2;
    high2 = tHDhigh2;
    high3 = tHDhigh3;
    high4 = tHDhigh4;
  }
  else
  {
    low2 = tDDlow2;
    high2 = tDDhigh2;
    high3 = tDDhigh3;
    high4 = tDDhigh4;
  }
}

/*
   initializes variables and timings according to the density mode
*/
void setMode(int density)
{
  setTimings(density);
  if (density == 1)
  {
    sectors = 22;
    timerMode = timerModeHD;
    streamLen = streamSizeHD;
    filterSetting = filterSettingHD;
    writeSize = writeSizeHD;
    transitionTime = transTimeHD;
    if (densMode != HD)
    {
      if (retries < settings.hdRetries)
        retries = settings.hdRetries;
    }
    densMode = HD;
  }
  else
  {
    sectors = 11;
    timerMode = timerModeDD;
    streamLen = streamSizeDD;
    filterSetting = filterSettingDD;
    writeSize = writeSizeDD;
    transitionTime = transTimeDD;
    if (densMode != DD)
    {
      if (retries < settings.sdRetries)
        retries = settings.sdRetries;
    }
    densMode = DD;
  }
}

/*
   tries to detect the density of the disk by counting the transitions on a track
*/
void densityDetect()
{
  countFluxes();
#ifdef hdmode
  if (fluxCount > 60000)
  {
    setMode(HD);
  }
  else
  {
    setMode(DD);
  }
#endif
  getDensity = false;
}

/*
   returns the creation and modified date of the disk from rootblock
*/
void diskDates(char *cdate, char *ldate)
{
  tmElements_t timeTemp;
  if (logTrack != 80)
  {
    gotoLogicTrack(80);
    readTrack(true, false);
  }
  struct Sector *rootTemp = (Sector *)&track[0].sector;
  struct rootBlock *root = (rootBlock *)&rootTemp->data;

  time_t tZeit = (__builtin_bswap32(root->days) + 2922) * 24 * 60 * 60 + (__builtin_bswap32(root->mins) * 60) + (__builtin_bswap32(root->ticks) / 50);
  breakTime(tZeit, timeTemp);
  sprintf(ldate, "%4d%02d%02dT%02d%02d%02d.0\n",
          timeTemp.Year + 1970,
          timeTemp.Month,
          timeTemp.Day,
          timeTemp.Hour,
          timeTemp.Minute,
          timeTemp.Second);

  tZeit = (__builtin_bswap32(root->c_days) + 2922) * 24 * 60 * 60 + (__builtin_bswap32(root->c_mins) * 60) + (__builtin_bswap32(root->c_ticks) / 50);
  breakTime(tZeit, timeTemp);
  sprintf(cdate, "%4d%02d%02dT%02d%02d%02d.0\n",
          timeTemp.Year + 1970,
          timeTemp.Month,
          timeTemp.Day,
          timeTemp.Hour,
          timeTemp.Minute,
          timeTemp.Second);
}

/*
   prints various stuff from the root and bootblock for debugging
*/
void diskInfoOld()
{
  tmElements_t timeTemp;
  gotoLogicTrack(80);
  readTrack(true, false);
  struct Sector *rootTemp = (Sector *)&track[0].sector;
  struct rootBlock *root = (rootBlock *)&rootTemp->data;

  Serial.printf("type: %d\n", (__builtin_bswap32(root->type)));
  Serial.printf("header_key: %d\n", (__builtin_bswap32(root->header_key)));
  Serial.printf("high_seq: %d\n", (__builtin_bswap32(root->high_seq)));
  Serial.printf("ht_size: %d\n", (__builtin_bswap32(root->ht_size)));
  Serial.printf("first_data: %d\n", (__builtin_bswap32(root->first_data)));
  Serial.printf("chksum: %x\n", (__builtin_bswap32(root->chksum)));

  unsigned long newsum;
  root->chksum = 0;
  newsum = 0L;
  for (int i = 0; i < 128; i++)
    newsum += rootTemp->data[i * 4] << 24 | rootTemp->data[i * 4 + 1] << 16 | rootTemp->data[i * 4 + 2] << 8 | rootTemp->data[i * 4 + 3];
  newsum = -newsum;
  Serial.printf("newsum: %x\n", newsum);

  Serial.print("ht[72]\n");
  for (int i = 0; i < 72; i++)
  {
    Serial.print(__builtin_bswap32(root->ht[i]));
    Serial.print(" ");
    if ((i + 1) % 16 == 0)
      Serial.println();
  }
  Serial.println();
  Serial.printf("bm_flag: %d\n", (long)__builtin_bswap32(root->bm_flag));
  Serial.printf("bm_pages[25]\n");
  for (int i = 0; i < 25; i++)
  {
    Serial.print(__builtin_bswap32(root->bm_pages[i]));
    Serial.print(" ");
    if ((i + 1) % 16 == 0)
      Serial.println();
  }
  Serial.println();
  Serial.printf("bm_ext: %x\n", (__builtin_bswap32(root->bm_ext[0])));
  Serial.printf("name_len[%d]: ", root->name_len);
  for (int i = 0; i < root->name_len; i++)
  {
    Serial.print(root->diskname[i]);
  }
  Serial.println();
  time_t tZeit = (__builtin_bswap32(root->days) + 2922) * 24 * 60 * 60 + (__builtin_bswap32(root->mins) * 60) + (__builtin_bswap32(root->ticks) / 50);
  breakTime(tZeit, timeTemp);
  Serial.printf("Last access: %4d%02d%02dT%02d%02d%02d.0\n",
                timeTemp.Year + 1970,
                timeTemp.Month,
                timeTemp.Day,
                timeTemp.Hour,
                timeTemp.Minute,
                timeTemp.Second);
  tZeit = (__builtin_bswap32(root->c_days) + 2922) * 24 * 60 * 60 + (__builtin_bswap32(root->c_mins) * 60) + (__builtin_bswap32(root->c_ticks) / 50);
  breakTime(tZeit, timeTemp);
  Serial.printf("Created: %4d%02d%02dT%02d%02d%02d.0\n",
                timeTemp.Year + 1970,
                timeTemp.Month,
                timeTemp.Day,
                timeTemp.Hour,
                timeTemp.Minute,
                timeTemp.Second);
  Serial.printf("next_hash: %x\n", (__builtin_bswap32(root->next_hash)));
  Serial.printf("parent_dir: %x\n", (__builtin_bswap32(root->parent_dir)));
  Serial.printf("extension: %d\n", (__builtin_bswap32(root->extension)));
  Serial.printf("sec_type: %x\n", (__builtin_bswap32(root->sec_type)));

  struct Sector *bitmapTemp = (Sector *)&track[1].sector;
  struct bitmapBlock *bitmap = (bitmapBlock *)&bitmapTemp->data;
  Serial.printf("bitmap checksum: %x\n", (__builtin_bswap32(bitmap->checksum)));
  newsum = 0L;
  for (int i = 1; i < 128; i++)
    newsum -= bitmapTemp->data[i * 4] << 24 | bitmapTemp->data[i * 4 + 1] << 16 | bitmapTemp->data[i * 4 + 2] << 8 | bitmapTemp->data[i * 4 + 3];
  Serial.printf("bitmap newsum: %x\n", newsum);
  uint32_t bCount = 0;
  if (densMode == HD)
  {
    for (int i = 0; i < 110; i++)
    {
      bCount += __builtin_popcount(bitmap->map[i]);
    }
  }
  else
  {
    for (int i = 0; i < 55; i++)
    {
      bCount += __builtin_popcount(bitmap->map[i]);
    }
  }
  Serial.printf("Blocks Free: %d\n", bCount);
  gotoLogicTrack(0);
  readTrack(true, false);
  struct Sector *bootBlock = (Sector *)&track[0].sector;
  uint32_t temp = 0;
  temp = bootBlock->data[0] << 24 | bootBlock->data[1] << 16 | bootBlock->data[2] << 8 | bootBlock->data[3];
  switch (temp)
  {
  case 0x444F5300:
    Serial.println("OFS");
    break;
  case 0x444F5301:
    Serial.println("FFS");
    break;
  case 0x444F5302:
    Serial.println("OFS Intl.");
    break;
  case 0x444F5303:
    Serial.println("FFS Intl.");
    break;
  case 0x444F5304:
    Serial.println("OFS DirCache");
    break;
  case 0x444F5305:
    Serial.println("FFS DirCache");
    break;
  case 0x444F5306:
    Serial.println("OFS Intl DirCache");
    break;
  case 0x444F5307:
    Serial.println("FFS Intl DirCache");
    break;
  default:
    Serial.println("NDOS");
    break;
  }
  Serial.println(temp, HEX);
}

void diskInfo()
{
  uint32_t j, newsum;
  tmElements_t timeTemp;
  gotoLogicTrack(0);
  readTrack(true, false);
  if (extError != NO_ERR)
  {
    Serial.println("BootChecksum: Invalid");
    Serial.println("Rootblock->: 0");
    Serial.println("Filesystem: Blank?");
    Serial.println("RootBlocktype: Invalid");
    Serial.println("RootblockChecksum: Invalid");
    Serial.println("Name: Unreadable Disk");
    Serial.println("Modified: Unknown");
    Serial.println("Created: Unknown");
    Serial.println("BitmapChecksum: Invalid");
    Serial.println("Blocks Free: Unknown");
    return;
  }
  struct Sector *bootBlock = (Sector *)&track[0].sector;
  uint32_t *tLong = (uint32_t *)&track[0].sector;
  uint32_t oldBootChkSum = __builtin_bswap32(tLong[8]);
  tLong[8] = 0;
  uint32_t bootChkSum = 0;
  for (int i = 0; i < 128; i++)
  {
    j = uint32_t(__builtin_bswap32(tLong[i + 7]));
    if ((0xffffffffU - bootChkSum) < j)
      bootChkSum++;
    bootChkSum += j;
  }
  for (int i = 0; i < 128; i++)
  {
    j = uint32_t(__builtin_bswap32(tLong[i + 142]));
    if ((0xffffffffU - bootChkSum) < j)
      bootChkSum++;
    bootChkSum += j;
  }
  bootChkSum = ~bootChkSum;
  tLong[8] = bootChkSum;
  if (oldBootChkSum == bootChkSum)
    Serial.println("BootChecksum: Valid");
  else
    Serial.println("BootChecksum: Invalid");
  j = uint32_t(__builtin_bswap32(tLong[9]));
  Serial.printf("Rootblock->: %d\n", j);
  j = bootBlock->data[0] << 24 | bootBlock->data[1] << 16 | bootBlock->data[2] << 8 | bootBlock->data[3];
  Serial.print("Filesystem: ");
  switch (j)
  {
  case 0x444F5300:
    Serial.println("OFS");
    break;
  case 0x444F5301:
    Serial.println("FFS");
    break;
  case 0x444F5302:
    Serial.println("OFS Intl.");
    break;
  case 0x444F5303:
    Serial.println("FFS Intl.");
    break;
  case 0x444F5304:
    Serial.println("OFS DirCache");
    break;
  case 0x444F5305:
    Serial.println("FFS DirCache");
    break;
  case 0x444F5306:
    Serial.println("OFS Intl DirCache");
    break;
  case 0x444F5307:
    Serial.println("FFS Intl DirCache");
    break;
  default:
    Serial.println("NDOS");
    break;
  }
  gotoLogicTrack(80);
  readTrack(true, false);
  struct Sector *rootTemp = (Sector *)&track[0].sector;
  struct rootBlock *root = (rootBlock *)&rootTemp->data;

  if ((__builtin_bswap32(root->type)) != 2)
    Serial.println("RootBlocktype: Invalid");
  else
    Serial.println("RootBlocktype: Valid");

  j = __builtin_bswap32(root->chksum);
  root->chksum = 0;
  newsum = 0;
  for (int i = 0; i < 128; i++)
    newsum += rootTemp->data[i * 4] << 24 | rootTemp->data[i * 4 + 1] << 16 | rootTemp->data[i * 4 + 2] << 8 | rootTemp->data[i * 4 + 3];
  newsum = -newsum;
  //  Serial.printf("%08x %08x\n", j, newsum);
  if (newsum == j)
    Serial.println("RootblockChecksum: Valid");
  else
    Serial.println("RootblockChecksum: Invalid");

  Serial.print("Name: ");
  for (int i = 0; i < root->name_len; i++)
  {
    Serial.print(root->diskname[i]);
  }
  Serial.println();
  time_t tZeit = (__builtin_bswap32(root->days) + 2922) * 24 * 60 * 60 + (__builtin_bswap32(root->mins) * 60) + (__builtin_bswap32(root->ticks) / 50);
  breakTime(tZeit, timeTemp);
  Serial.printf("Modified: %02d:%02d:%02d %02d.%02d.%04d\n",
                timeTemp.Hour,
                timeTemp.Minute,
                timeTemp.Second,
                timeTemp.Day,
                timeTemp.Month,
                timeTemp.Year + 1970);
  tZeit = (__builtin_bswap32(root->c_days) + 2922) * 24 * 60 * 60 + (__builtin_bswap32(root->c_mins) * 60) + (__builtin_bswap32(root->c_ticks) / 50);
  breakTime(tZeit, timeTemp);
  Serial.printf("Created: %02d:%02d:%02d %02d.%02d.%04d\n",
                timeTemp.Hour,
                timeTemp.Minute,
                timeTemp.Second,
                timeTemp.Day,
                timeTemp.Month,
                timeTemp.Year + 1970);
  struct Sector *bitmapTemp = (Sector *)&track[1].sector;
  struct bitmapBlock *bitmap = (bitmapBlock *)&bitmapTemp->data;
  newsum = 0L;
  for (int i = 1; i < 128; i++)
    newsum -= bitmapTemp->data[i * 4] << 24 | bitmapTemp->data[i * 4 + 1] << 16 | bitmapTemp->data[i * 4 + 2] << 8 | bitmapTemp->data[i * 4 + 3];
  if (newsum == (__builtin_bswap32(bitmap->checksum)))
    Serial.println("BitmapChecksum: Valid");
  else
    Serial.println("BitmapChecksum: Invalid");
  uint32_t bCount = 0;
  if (densMode == HD)
  {
    for (int i = 0; i < 110; i++)
    {
      bCount += __builtin_popcount(bitmap->map[i]);
    }
  }
  else
  {
    for (int i = 0; i < 55; i++)
    {
      bCount += __builtin_popcount(bitmap->map[i]);
    }
  }
  Serial.printf("Blocks Free: %d\n", bCount);
}

void dumpBitmap()
{
  gotoLogicTrack(80);
  readTrack(true, false);
  struct Sector *bitmapTemp = (Sector *)&track[1].sector;
  struct bitmapBlock *bitmap = (bitmapBlock *)&bitmapTemp->data;
  Serial.print("00"); //bootblocks are always occupied and not included in bitmap
  if (densMode == HD)
  {
    for (int i = 0; i < 109; i++)
    {
      print_binary_rev(__builtin_bswap32(bitmap->map[i]), 32);
    }
    print_binary_rev(__builtin_bswap32(bitmap->map[109]), 30);
  }
  else
  {
    for (int i = 0; i < 54; i++)
    {
      print_binary_rev(__builtin_bswap32(bitmap->map[i]), 32);
    }
    print_binary_rev(__builtin_bswap32(bitmap->map[54]), 30);
  }
  Serial.println();
}

void printTimings(struct tSettings settings)
{
  Serial.print("Motor Spinup Delay: ");
  Serial.println(settings.motorSpinupDelay);
  Serial.print("Motor Spindown Delay: ");
  Serial.println(settings.motorSpindownDelay);
  Serial.print("Drive Select Delay: ");
  Serial.println(settings.driveSelectDelay);
  Serial.print("Drive Deselect Delay: ");
  Serial.println(settings.driveDeselectDelay);
  Serial.print("Set Direction Delay: ");
  Serial.println(settings.setDirDelay);
  Serial.print("Set Side Delay: ");
  Serial.println(settings.setSideDelay);
  Serial.print("Single Step Delay: ");
  Serial.println(settings.stepDelay);
  Serial.print("Step Settle Delay: ");
  Serial.println(settings.stepSettleDelay);
  Serial.print("gotoTrack Settle Delay: ");
  Serial.println(settings.gotoTrackSettle);
  Serial.print("sdRetries: ");
  Serial.println(settings.sdRetries);
  Serial.print("hdRetries: ");
  Serial.println(settings.hdRetries);
  Serial.print("config: 0b");
  Serial.println(settings.config, BIN);
  Serial.print("reserved3: 0x");
  Serial.println(settings.reserved3, HEX);
  Serial.print("reserved4: 0x");
  Serial.println(settings.reserved4, HEX);
  Serial.print("reserved5: 0x");
  Serial.println(settings.reserved5, HEX);
  Serial.print("reserved6: 0x");
  Serial.println(settings.reserved6, HEX);
  Serial.print("reserved7: 0x");
  Serial.println(settings.reserved7, HEX);
}

void setDefaultTimings()
{
  settings.motorSpinupDelay = 600;  // ms
  settings.motorSpindownDelay = 50; // µs
  settings.driveSelectDelay = 100;  // µs
  settings.driveDeselectDelay = 5;  // µs
  settings.setDirDelay = 5;         // µs
  settings.setSideDelay = 200;      // µs
  settings.stepDelay = 2;           // µs
  settings.stepSettleDelay = 3;     // ms
  settings.gotoTrackSettle = 18;    // ms
  settings.sdRetries = 10;
  settings.hdRetries = 30;
  settings.config = mtpModeOn;
  settings.reserved3 = 0xDEADDA7A;
  settings.reserved4 = 0xDEADDA7A;
  settings.reserved5 = 0xDEADDA7A;
  settings.reserved6 = 0xDEADDA7A;
  settings.reserved7 = 0xDEADDA7A;
}

void storeTimings()
{
  settings.versionNr = numVersion;
  settings.reserved3 = 0xDEADDA7A;
  settings.reserved4 = 0xDEADDA7A;
  settings.reserved5 = 0xDEADDA7A;
  settings.reserved6 = 0xDEADDA7A;
  settings.reserved7 = 0xDEADDA7A;
  settings.checksum = 0x00000000;
  uint32_t *settingsArray = &(settings.versionNr);
  uint32_t checksum = 0xC0FFEE;
  for (int i = 0; i < 18; i++)
  {
    checksum += settingsArray[i];
  }
#ifdef debug
  Serial.printf("Checksum: %lx\n", checksum);
#endif
  settings.checksum = checksum;
  EEPROM.put(0, settings);
}

void storeDefaultTimings()
{
  setDefaultTimings();
  storeTimings();
}

int restoreTimings()
{
  tSettings tempSettings;
  EEPROM.get(0, tempSettings);
  tempSettings.versionNr = numVersion;
  uint32_t *settingsArray = &(tempSettings.versionNr);
  uint32_t checksum = 0xC0FFEE;
  for (int i = 0; i < 18; i++)
  {
    checksum += settingsArray[i];
  }
#ifdef debug
  Serial.printf("Checksum calculated: %lx\n", checksum);
  Serial.printf("Checksum in EEPROM: %lx\n", tempSettings.checksum);
  printTimings(tempSettings);
#endif
  if (tempSettings.checksum == checksum)
  {
    settings = tempSettings;
    if ((settings.config & 0x01) == 1)
      mtpOn = true;
    else
      mtpOn = false;
    return 0;
  }
  else
  {
    return -1;
  }
}
