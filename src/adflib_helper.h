/*
   This module contains helper functions for interfacing to the ADFLib by Laurent Clevy
   See the ADFlib folder for details
   The ADFlib included is a modified version for use with the Teensy and ADF-Drive
*/

#ifndef ADFLIB_HELPER_H /* include guards */
#define ADFLIB_HELPER_H

#include <Arduino.h>
//#include "adf_nativ.h"
//#include "vars_n_defs.h"

extern struct List *list, *cell;
extern struct Device *dev;
extern struct Volume *vol;

/* Counts the number of files in the current directory indicated by dirBlock
   and also creates a list of files for later use. */
int getNumAmigaFiles(uint32_t dirBlock);

/* Mounts the inserted Disk into ADFlib */
boolean openDrive();

/* Dismounts the inserted Disk into ADFlib */
void closeDrive();

/* deletes an entry from the filelist */
void deleteEntry(uint32_t handle);

/* returns needed information for mtp_getobjectinfo when the file is not in the list the entry will be read from disk */
void AmigaObjectInfo(uint32_t handle,
                     char *name,
                     uint32_t *size,
                     uint32_t *parent, char *date);

/* deletes a File or Directory from the disk according to the mtp filehandle */
int deleteAmigaFile(uint32_t handle);

/* returns a mtp handle for the directory */
uint32_t getAmigaDirHandle(struct Volume *vol, char *name);

/* true - unmounts the disk and sends mtp events that the storages have been removed
   false - flags the disk as freshly inserted that disk detection routine mounts the disk
   into adflib */
void busy(boolean busy);

#endif /* ADFLIB_HELPER_H */