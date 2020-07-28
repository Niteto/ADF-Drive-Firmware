/*
   This module contains helper functions for interfacing to the ADFLib by Laurent Clevy
   See the ADFlib folder for details
   The ADFlib included is a modified version for use with the Teensy and ADF-Drive
*/

#include <Arduino.h>
#include "vars_n_defs.h"
#include "adflib_helper.h"
#include "adf_nativ.h"
#include "adf_dir.h"
#include "adf_util.h"
#include "floppy_control.h"

//#include "icons.h"
//#include "MTPADF.h"
extern MTPD mtpd;
struct List *list, *cell;
struct Device *dev;
struct Volume *vol;

/*
   Counts the number of files in the current directory indicated by dirBlock
   and also creates a list of files for later use.
*/
int getNumAmigaFiles(uint32_t dirBlock)
{
  if (dirBlock == 0xffffffff)
    vol->curDirPtr = vol->rootBlock;
  else
    vol->curDirPtr = dirBlock & 0x00000fff;
  cell = list = adfGetDirEnt(vol, vol->curDirPtr);
  int num = 0;
  while (cell)
  {
    num++;
    cell = cell->next;
  }
  return num;
}

/*
   Mounts the inserted Disk into ADFlib
*/
boolean openDrive()
{
  if (ADFisOpen)
    return true;
  int volNum = 0;
  adfEnvInitDefault();
  dev = adfMountDev((char *)"DF0:", writeProtect());
  if (!dev)
  {
#ifdef debugadf
    Serial.printf("Can't mount the dump device '%s'.\n", "DF0:");
#endif
    return false;
  }
  else
  {
    vol = adfMount(dev, volNum, writeProtect());
    if (!vol)
    {
#ifdef debugadf
      Serial.printf("Can't mount the volume\n");
#endif
      return false;
    }
  }
  ADFisOpen = true;
  return true;
}

/*
   Dismounts the inserted Disk into ADFlib
*/
void closeDrive()
{
  if (!ADFisOpen)
    return;
  ADFisOpen = false;
  if (vol)
    adfUnMount(vol);
  if (dev)
    adfUnMountDev(dev);
  adfEnvCleanUp();
}

/*
   deletes an entry from the filelist
*/
void deleteEntry(uint32_t handle)
{
  handle = handle & 0x00000fff;
  while (cell && ((struct Entry *)cell->content)->sector != (int32_t)handle)
  {
    cell = cell->next;
    if (!cell)
      break;
  }
  if (cell)
  {
    ((struct Entry *)cell->content)->sector = 0xfff;
  }
}

/*
   returns needed information for mtp_getobjectinfo
   when the file is not in the list the entry will be read from disk
*/
void AmigaObjectInfo(uint32_t handle,
                     char *name,
                     uint32_t *size,
                     uint32_t *parent, char *date)
{
  //  Serial.printf("list = %lx\n", &list);
  cell = list;
  handle = handle & 0x00000fff;
#ifdef debugadf
  Serial.printf("[ADFlib]Handle: %x\n", handle);
#endif
  while (cell && ((struct Entry *)cell->content)->sector != (int32_t)handle)
  {
    cell = cell->next;
    if (!cell)
      break;
  }
  if (cell)
  {
    if (((struct Entry *)cell->content)->type == ST_DIR)
      *size = 0xFFFFFFFFUL;
    else
      *size = ((struct Entry *)cell->content)->size;

    //  Serial.printf("%4d/%02d/%02d  %2d:%02d:%02d ", entry->year, entry->month, entry->days,
    //                entry->hour, entry->mins, entry->secs);

    strcpy(name, ((struct Entry *)cell->content)->name);
    *parent = ((struct Entry *)cell->content)->parent;
    if (date != NULL)
      sprintf(date, "%4d%02d%02dT%02d%02d%02d.0",
              ((struct Entry *)cell->content)->year,
              ((struct Entry *)cell->content)->month,
              ((struct Entry *)cell->content)->days,
              ((struct Entry *)cell->content)->hour,
              ((struct Entry *)cell->content)->mins,
              ((struct Entry *)cell->content)->secs);
  }
  else
  {
#ifdef debugadf
    Serial.println("no cell");
#endif
    struct bFileHeaderBlock *buf;
    buf = (struct bFileHeaderBlock *)malloc(sizeof(struct bFileHeaderBlock));
    if (!buf)
    {
      Serial.printf("AmigaObjectInfo: malloc bFileHeaderBlock %d bytes\n", sizeof(struct bFileHeaderBlock));
      failureCode = memError;
      fail = true;
      return;
    }
    adfReadEntryBlock(vol, handle, (bEntryBlock *)buf);
    *parent = buf->parent;
    strcpy(name, buf->fileName);
    if (buf->secType == ST_DIR)
      *size = 0xFFFFFFFFUL;
    else
      *size = buf->byteSize;
    if (date != NULL)
    {
      struct DateTime amigaDate;
      adfDays2Date(buf->days, &(amigaDate.year), &(amigaDate.mon), &(amigaDate.day));
      amigaDate.hour = buf->mins / 60;
      amigaDate.min = buf->mins % 60;
      amigaDate.sec = buf->ticks / 50;
      sprintf(date, "%4d%02d%02dT%02d%02d%02d.0",
              amigaDate.year, amigaDate.mon, amigaDate.day, amigaDate.hour, amigaDate.min, amigaDate.sec);
    }
    free(buf);
  }
}

/*
   deletes a File or Directory from the disk according to the mtp filehandle
*/
int deleteAmigaFile(uint32_t handle)
{
  char filename[256];
  uint32_t size, parent;
  if (handle == 0xffffffff)
    return -1;
  AmigaObjectInfo(handle, filename, &size, &parent, NULL);
  int retCode = adfRemoveEntry(vol, parent, filename);
  deleteEntry(handle);
  flushToDisk();
#ifdef debugadf
  Serial.printf("[ADFlib]Handle: %x Name: %s size: %d parent: %x RetCode %d\n", handle, filename, size, parent, retCode);
#endif
  return retCode;
}

/*
   returns a mtp handle for the directory
*/
uint32_t getAmigaDirHandle(struct Volume *vol, char *name)
{
  struct bEntryBlock entry;

  if (adfReadEntryBlock(vol, vol->curDirPtr, &entry) != RC_OK)
    return 0;
  return adfNameToEntryBlk(vol, entry.hashTable, name, &entry, NULL);
}

/*
   true - unmounts the disk and sends mtp events that the storages have been removed
   false - flags the disk as freshly inserted that disk detection routine mounts the disk
   into adflib
*/
void busy(boolean busy)
{
  if (busy)
  {
    if (mtpOn)
    {
      if (ADFisOpen)
      {
        closeDrive();
        mtpd.sendEvent(MTP_EVENT_STORE_REMOVED, FDid);
        mtpd.sendEvent(MTP_EVENT_STORE_REMOVED, FSid);
      }
    }
  }
  else
  {
    noDisk = true;
  }
}
