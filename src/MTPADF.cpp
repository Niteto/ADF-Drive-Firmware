/*  Based upon work from Fredrik Hubinette, see original header below
    Changes - a lot ;)
    - added event sending
    - handling of multiple storages
    - specific code for adf images and amiga filesystem

    this requires changes in boards.txt, usc_desc.c and usb_desc.h to enable
    mtp on teensy 3.2
*/

// MTP.h - Teensy MTP Responder library
// Copyright (C) 2017 Fredrik Hubinette <hubbe@hubbe.net>
//
// With updates from MichaelMC, YoongHM
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <Arduino.h>
#include "MTPADF.h"
#include "vars_n_defs.h"
#include "adflib_helper.h"
#include "adf_util.h"
#include "icons.h"
#include "adf_dir.h"
#include "adflib.h"
#include "adf_env.h"
#include "adf_nativ.h"
#include "utility_functions.h"
#include "floppy_control.h"
#include "floppy_rw.h"

//SdFatSdioEX SD;
// extern SdFat SD;
// extern String getName();

// extern "C" void gotoLogicTrack(int track);
// extern int64_t readTrack(boolean silent, boolean compOnly);
// //extern byte* ptrSector(int index);
// extern uint32_t imageHandle;
// extern bool noSD;
// extern bool noDisk;
// extern boolean badDisk;
// extern bool busyFormatting;
// extern int logTrack;
// extern byte sectors;

// extern int formatDisk(boolean quick, boolean verify);
// extern int writeWithVerify(int tTrack, int wRetries);
// extern bool wProt;
/*
extern int getNumAmigaFiles(uint32_t parent);
extern boolean openDrive();
extern void closeDrive();
extern void AmigaObjectInfo(uint32_t handle, char* name, uint32_t* size, uint32_t* parent, char* date);
extern int deleteAmigaFile(uint32_t handle);
extern uint32_t getAmigaDirHandle(struct Volume* vol, char *name);
extern struct List *list, *cell;
extern struct Volume *vol;
extern struct Device *dev;
extern void printEnt(struct Volume *vol, struct Entry* entry, String path, BOOL sect, BOOL comment);*/
extern "C" boolean writeProtect();
extern "C" int densMode;
extern uint16_t retries;
extern int8_t extError;
extern uint16_t failureCode;
extern boolean fail;
extern uint32_t queuedEvent[2];

#define offsetStart 0x00100000
uint32_t amigaHandleOffset = offsetStart;

// These should probably be weak.
void mtp_yield() {}
void mtp_lock_storage(bool lock) {}

// This interface lets the MTP responder interface any storage.
// We'll need to give the MTP responder a pointer to one of these.
//class MTPStorageInterface
// Return true if this storage is read-only
bool MTPStorageInterface::readonly() { return 0; }

// Does it have directories?
bool MTPStorageInterface::has_directories() { return 0; }

// Return size of storage in bytes.
uint64_t MTPStorageInterface::size() { return 0; }

// Return free space in bytes.
uint64_t MTPStorageInterface::free() { return 0; }

// parent = 0 means get all handles.
// parent = 0xFFFFFFFF means get root folder.
// void MTPStorageInterface::StartGetObjectHandles(uint32_t parent){}
// uint32_t MTPStorageInterface::GetNextObjectHandle(){return 0;}

// Size should be 0xFFFFFFFF if it's a directory.
// void MTPStorageInterface::GetObjectInfo(uint32_t handle,
//                            char* name,
//                            uint32_t* size,
//                            uint32_t* parent){}
uint32_t MTPStorageInterface::GetSize(uint32_t handle) { return 0; }
// void MTPStorageInterface::read(uint32_t handle,
//                   uint32_t pos,
//                   char* buffer,
//                   uint32_t bytes){}
// uint32_t MTPStorageInterface::Create(uint32_t parent,
//                         bool folder,
//                         const char* filename){return 0;}
// void MTPStorageInterface::write(const char* data, uint32_t size){};
// void MTPStorageInterface::close(){};
// bool MTPStorageInterface::DeleteObject(uint32_t object){return 0;}
//end class MTPStorageInterface

// Storage implementation for AmigaFS. AmigaFS needs to be already initialized.
// class MTPStorage_AmigaFS : public MTPStorageInterface {
// File index_;

// uint8_t mode_ = 0;
// uint32_t open_file_ = 0xFFFFFFFEUL;
// File f_;
// uint32_t index_entries_ = 0;

bool MTPStorage_Amiga::readonly()
{
  return false;
}
bool MTPStorage_Amiga::has_directories()
{
  return true;
}
uint64_t MTPStorage_Amiga::size()
{
  return 901120;
}

uint64_t MTPStorage_Amiga::free()
{
  return 901120;
}

// void MTPStorage_Amiga::OpenIndex() {
//   if (index_) return;
//   mtp_lock_storage(true);
//   index_ = SD.open("mtpindex.dat", FILE_WRITE);
//   mtp_lock_storage(false);
// }

/* writes Indexrecord r at Indexposition i */
// void MTPStorage_Amiga::WriteIndexRecord(uint32_t i, const Record& r) {
//   OpenIndex();
//   mtp_lock_storage(true);
//   index_.seek(sizeof(r) * i);
//   index_.write((char*)&r, sizeof(r));
//   mtp_lock_storage(false);
// }

/* puts a new indexrecord at the end */
// uint32_t MTPStorage_Amiga::AppendIndexRecord(const Record& r) {
//   uint32_t new_record = index_entries_++;
//   WriteIndexRecord(new_record, r);
//   return new_record;
// }
/* Reads IndexRecord i */
// Record MTPStorage_Amiga::ReadIndexRecord(uint32_t i) {
//   Record ret;
//   if (i > index_entries_) {
//     memset(&ret, 0, sizeof(ret));
//     return ret;
//   }
//   OpenIndex();
//   mtp_lock_storage(true);
//   index_.seek(sizeof(ret) * i);
//   index_.read(&ret, sizeof(ret));
//   mtp_lock_storage(false);
//   return ret;
// }

// void MTPStorage_Amiga::ConstructFilename(int i, char* out) {
//   if (i == 0) {
//     strcpy(out, "/");
//   } else {
//     Record tmp = ReadIndexRecord(i);
//     ConstructFilename(tmp.parent, out);
//     if (out[strlen(out) - 1] != '/')
//       strcat(out, "/");
//     strcat(out, tmp.name);
//   }
// }

// void MTPStorage_Amiga::OpenFileByIndex(uint32_t i, oflag_t mode) {
//   if (open_file_ == i && mode_ == mode)
//     return;
//   char filename[256];
//   ConstructFilename(i, filename);
//   mtp_lock_storage(true);
//   f_.close();
//   f_ = SD.open(filename, mode);
//   open_file_ = i;
//   mode_ = mode;
//   mtp_lock_storage(false);
// }

// MTP object handles should not change or be re-used during a session.
// This would be easy if we could just have a list of all files in memory.
// Since our RAM is limited, we'll keep the index in a file instead.
// void MTPStorage_Amiga::GenerateIndex() {
//   if (index_generated) return;
//   index_generated = true;

//   mtp_lock_storage(true);
//   SD.remove("mtpindex.dat");
//   mtp_lock_storage(false);
//   index_entries_ = 0;

//   Record r;
//   r.parent = 0;
//   r.sibling = 0;
//   r.child = 0;
//   r.isdir = true;
//   r.scanned = false;
//   strcpy(r.name, "/");
//   AppendIndexRecord(r);
// }

// void MTPStorage_Amiga::ScanDir(uint32_t i) {
//   Record record = ReadIndexRecord(i);
// }

// void MTPStorage_Amiga::ScanAll() {
//   if (all_scanned_) return;
//   all_scanned_ = true;

//   GenerateIndex();
//   for (uint32_t i = 0; i < index_entries_; i++) {
//     ScanDir(i);
//   }
// }

// void MTPStorage_Amiga::StartGetObjectHandles(uint32_t parent) {
//   // GenerateIndex();
//   if (parent) {
//     if (parent == 0xFFFFFFFF) parent = 0;

//     ScanDir(parent);
//     follow_sibling_ = true;
//     // Root folder?
//     next_ = ReadIndexRecord(parent).child;
//   } else {
//     // ScanAll();
//     follow_sibling_ = false;
//     next_ = 1;
//   }
// }

// uint32_t MTPStorage_Amiga::GetNextObjectHandle() {
//   while (true) {
//     if (next_ == 0) return 0;

//     int ret = next_;
//     Record r = ReadIndexRecord(ret);
//     if (follow_sibling_) {
//       next_ = r.sibling;
//     } else {
//       next_++;
//       if (next_ >= index_entries_)
//         next_ = 0;
//     }
//     if (r.name[0]) return ret;
//   }
// }

// void MTPStorage_Amiga::GetObjectInfo(uint32_t handle,
//                    char* name,
//                    uint32_t* size,
//                    uint32_t* parent) {
//   Record r = ReadIndexRecord(handle);
//   strcpy(name, r.name);
//   *parent = r.parent;
//   *size = r.isdir ? 0xFFFFFFFFUL : r.child;
// }

uint32_t MTPStorage_Amiga::GetSize(uint32_t handle)
{
  return 0;
}

// void MTPStorage_Amiga::read(uint32_t handle,
//           uint32_t pos,
//           char* out,
//           uint32_t bytes) {
//   OpenFileByIndex(handle);
//   mtp_lock_storage(true);
//   f_.seek(pos);
//   f_.read(out, bytes);
//   mtp_lock_storage(false);
// }

// bool MTPStorage_Amiga::DeleteObject(uint32_t object) {
//   char filename[256];
//   Record r;
//   while (true) {
//     r = ReadIndexRecord(object == 0xFFFFFFFFUL ? 0 : object);
//     if (!r.isdir) break;
//     if (!r.child) break;
//     if (!DeleteObject(r.child))
//       return false;
//   }

// We can't actually delete the root folder,
// but if we deleted everything else, return true.
//   if (object == 0xFFFFFFFFUL) return true;

//   ConstructFilename(object, filename);
//   bool success;
//   mtp_lock_storage(true);
//   if (r.isdir) {
//     success = SD.rmdir(filename);
//   } else {
//     success = SD.remove(filename);
//   }
//   mtp_lock_storage(false);
//   if (!success) return false;
//   r.name[0] = 0;
//   int p = r.parent;
//   WriteIndexRecord(object, r);
//   Record tmp = ReadIndexRecord(p);
//   if (tmp.child == object) {
//     tmp.child = r.sibling;
//     WriteIndexRecord(p, tmp);
//   } else {
//     int c = tmp.child;
//     while (c) {
//       tmp = ReadIndexRecord(c);
//       if (tmp.sibling == object) {
//         tmp.sibling = r.sibling;
//         WriteIndexRecord(c, tmp);
//         break;
//       } else {
//         c = tmp.sibling;
//       }
//     }
//   }
//   return true;
// }

// uint32_t MTPStorage_Amiga::Create(uint32_t parent,
//                 bool folder,
//                 const char* filename) {
//   uint32_t ret;
//   if (parent == 0xFFFFFFFFUL) parent = 0;
//   Record p = ReadIndexRecord(parent);
//   Record r;
//   if (strlen(filename) > 62) return 0;
//   strcpy(r.name, filename);
//   r.parent = parent;
//   r.child = 0;
//   r.sibling = p.child;
//   r.isdir = folder;
//   // New folder is empty, scanned = true.
//   r.scanned = 1;
//   ret = p.child = AppendIndexRecord(r);
//   WriteIndexRecord(parent, p);
//   if (folder) {
//     char filename[256];
//     ConstructFilename(ret, filename);
//     mtp_lock_storage(true);
//     SD.mkdir(filename);
//     mtp_lock_storage(false);
//   } else {
//     OpenFileByIndex(ret, (uint8_t)FILE_WRITE);
//   }
//   return ret;
// }

// void MTPStorage_Amiga::write(const char* data, uint32_t bytes) {
//   mtp_lock_storage(true);
//   f_.write(data, bytes);
//   mtp_lock_storage(false);
// }

// void MTPStorage_Amiga::close() {
//   mtp_lock_storage(true);
//   uint64_t size = f_.size();
//   f_.close();
//   mtp_lock_storage(false);
//   Record r = ReadIndexRecord(open_file_);
//   r.child = size;
//   WriteIndexRecord(open_file_, r);
//   open_file_ = 0xFFFFFFFEUL;
// }

// MTP Responder.
// class MTPD {
//

struct MTPHeader
{
  uint32_t len;            // 0
  uint16_t type;           // 4
  uint16_t op;             // 6
  uint32_t transaction_id; // 8
};

char MTPD::byte2char(byte c)
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
void MTPD::PrintPacket(const usb_packet_t *x)
{
#ifdef verbose
  Serial.print("[MTP]Length: ");
  Serial.println(x->len);
  for (int i = 0; i < x->len; i++)
  {
    Serial.print("0123456789ABCDEF"[x->buf[i] >> 4]);
    Serial.print("0123456789ABCDEF"[x->buf[i] & 0xf]);
    if ((i & 3) == 3)
      Serial.print(" ");
    Serial.send_now();
  }
  Serial.println("");
  for (int i = 0; i < x->len; i++)
  {
    Serial.print(byte2char(x->buf[i]));
    Serial.send_now();
  }
  Serial.println("");

#endif
#ifdef verbose
  MTPContainer *tmp = (struct MTPContainer *)(x->buf);
  Serial.print(" len = ");
  Serial.print(tmp->len, HEX);
  Serial.print(" type = ");
  Serial.print(tmp->type, HEX);
  Serial.print(" op = ");
  Serial.print(tmp->op, HEX);
  Serial.print(" transaction_id = ");
  Serial.print(tmp->transaction_id, HEX);
  for (int i = 0; i * 4 < x->len - 12; i++)
  {
    Serial.print(" p");
    Serial.print(i);
    Serial.print(" = ");
    Serial.print(tmp->params[i], HEX);
  }
  Serial.println("");
#endif
}

void MTPD::get_buffer()
{
  while (!data_buffer_)
  {
    data_buffer_ = usb_malloc();
    if (!data_buffer_)
      mtp_yield();
  }
}

void MTPD::receive_buffer()
{
  while (!data_buffer_)
  {
    data_buffer_ = usb_rx(MTP_RX_ENDPOINT);
    if (!data_buffer_)
      mtp_yield();
  }
}

void MTPD::write(const char *data, int len)
{
  if (write_get_length_)
  {
    write_length_ += len;
  }
  else
  {
    int pos = 0;
    while (pos < len)
    {
      get_buffer();
      int avail = sizeof(data_buffer_->buf) - data_buffer_->len;
      int to_copy = min(len - pos, avail);
      memcpy(data_buffer_->buf + data_buffer_->len,
             data + pos,
             to_copy);
      data_buffer_->len += to_copy;
      pos += to_copy;
      if (data_buffer_->len == sizeof(data_buffer_->buf))
      {
#ifdef debugmtp2
        Serial.println("[MTP]SENT...");
        PrintPacket(data_buffer_);
#endif
        usb_tx(MTP_TX_ENDPOINT, data_buffer_);
        data_buffer_ = NULL;
      }
    }
  }
}

void MTPD::write8(uint8_t x)
{
  write((char *)&x, sizeof(x));
}

void MTPD::write16(uint16_t x)
{
  write((char *)&x, sizeof(x));
}

void MTPD::write32(uint32_t x)
{
  write((char *)&x, sizeof(x));
}

void MTPD::write64(uint64_t x)
{
  write((char *)&x, sizeof(x));
}

void MTPD::writestring(const char *str)
{
  if (*str)
  {
    write8(strlen(str) + 1);
    while (*str)
    {
      write16(*str);
      ++str;
    }
    write16(0);
  }
  else
  {
    write8(0);
  }
}

void MTPD::WriteDescriptor()
{
  write16(100); // MTP version
  write32(6);   // MTP extension
  //    write32(0xFFFFFFFFUL);    // MTP extension
  write16(100); // MTP version
  writestring("microsoft.com: 1.0;");
  write16(0); // functional mode

  // Supported operations (array of uint16)
  write32(18);
  write16(MTP_OPERATION_GET_DEVICE_INFO);
  write16(MTP_OPERATION_OPEN_SESSION);
  write16(MTP_OPERATION_CLOSE_SESSION);
  write16(MTP_OPERATION_GET_STORAGE_IDS);

  write16(MTP_OPERATION_GET_STORAGE_INFO);
  write16(MTP_OPERATION_GET_NUM_OBJECTS);
  write16(MTP_OPERATION_GET_OBJECT_HANDLES);
  write16(MTP_OPERATION_GET_OBJECT_INFO);

  write16(MTP_OPERATION_GET_OBJECT);
  write16(MTP_OPERATION_GET_THUMB);
  write16(MTP_OPERATION_DELETE_OBJECT);
  write16(MTP_OPERATION_SEND_OBJECT_INFO);

  write16(MTP_OPERATION_SEND_OBJECT);
  write16(MTP_OPERATION_FORMAT_STORE);
  write16(MTP_OPERATION_GET_DEVICE_PROP_DESC);
  write16(MTP_OPERATION_GET_DEVICE_PROP_VALUE);

  write16(MTP_OPERATION_SET_DEVICE_PROP_VALUE);
  //      write16(MTP_OPERATION_GET_OBJECT_PROPS_SUPPORTED); // bugs linux devices
  write16(MTP_OPERATION_GET_OBJECT_PROP_DESC);
  //      write16(MTP_OPERATION_GET_OBJECT_PROP_VALUE);

  //    write16(MTP_OPERATION_RESET_DEVICE);
  //    write16(MTP_OPERATION_MOVE_OBJECT);
  //    write16(MTP_OPERATION_COPY_OBJECT);

  write32(9); // Events (array of uint16)
  write16(MTP_EVENT_CANCEL_TRANSACTION);
  write16(MTP_EVENT_OBJECT_ADDED);   // ObjectAdded
  write16(MTP_EVENT_OBJECT_REMOVED); // ObjectRemoved
  write16(MTP_EVENT_STORE_ADDED);    // StoreAdded
  write16(MTP_EVENT_STORE_REMOVED);  // StoreRemoved

  write16(MTP_EVENT_OBJECT_INFO_CHANGED);  // ObjectInfoChanged
  write16(MTP_EVENT_DEVICE_RESET);         // DeviceReset
  write16(MTP_EVENT_STORAGE_INFO_CHANGED); // StoreInfoChanged
  write16(MTP_EVENT_UNREPORTED_STATUS);    // UnreportedStatus

  write32(3);                                        // Device properties (array of uint16)
  write16(MTP_DEVICE_PROPERTY_BATTERY_LEVEL);        // Battery Level
  write16(MTP_DEVICE_PROPERTY_DATETIME);             // DateTime
  write16(MTP_DEVICE_PROPERTY_DEVICE_FRIENDLY_NAME); // Device friendly name
  //      write16(MTP_DEVICE_PROPERTY_DEVICE_ICON);  // Device icon
  //      write16(MTP_DEVICE_PROPERTY_PERCEIVED_DEVICE_TYPE);  // Perceived Device Type

  write32(0); // Capture formats (array of uint16)

  write32(2);                      // Playback formats (array of uint16)
  write16(MTP_FORMAT_UNDEFINED);   // Undefined format
  write16(MTP_FORMAT_ASSOCIATION); // Folders (associations)

  writestring("Niteto");              // Manufacturer
  writestring("ADF-Drive");           // Model
  writestring(Version);               // version
  writestring(__TIME__ " " __DATE__); // serial
  adfSetCurrentTime(BUILD_YEAR, BUILD_MONTH, BUILD_DAY - 1, BUILD_HOUR, BUILD_MIN, BUILD_SEC);
}

void MTPD::WriteStorageIDs()
{
  uint32_t numStorage = 0;
  if (!noDisk)
  {
    numStorage++;
    if (!badDisk)
      numStorage++;
  }
  write32(numStorage); // Number of entries /storages
  if (!noDisk)
  {
    write32(FDid); // 2 storage -> DF0:
    if (!badDisk)
      write32(FSid); // 3 Amiga FileSys
  }
}

void MTPD::GetStorageInfo(uint32_t storage)
{
#ifdef debugmtp
  //if (!write_get_length_)
  Serial.printf("[MTP]Storage Info No.: %lx", storage);
#endif
  switch (storage)
  {
  case SDid:
    write16(storage_->readonly() ? MTP_STORAGE_FIXED_ROM : MTP_STORAGE_REMOVABLE_RAM);                        // storage type (removable RAM)
    write16(storage_->has_directories() ? MTP_STORAGE_FILESYSTEM_HIERARCHICAL : MTP_STORAGE_FILESYSTEM_FLAT); // filesystem type (generic hierarchical)
    write16(MTP_STORAGE_READ_WRITE);
    write64(storage_->size()); // max capacity
    write64(storage_->free()); // free space
    write32(0xFFFFFFFFUL);     // free space (objects)
    writestring("SD Card");    // storage descriptor
    writestring("");           // volume identifier
    break;
  case FDid:
    if (busyFormatting)
    {
      write16(MTP_STORAGE_REMOVABLE_ROM);
      write16(MTP_STORAGE_FILESYSTEM_FLAT);
      write16(MTP_STORAGE_READ_ONLY_WITHOUT_DELETE);
      write64(512 * sectors * 160);      // max capacity
      write64(512 * sectors * logTrack); // free space
      write32(0);                        // free space (objects)
      writestring("BUSY:Formatting...");
    }
    else
    {
      if (writeProtect())
      {
        write16(MTP_STORAGE_REMOVABLE_ROM);
        write16(MTP_STORAGE_FILESYSTEM_FLAT);
        write16(MTP_STORAGE_READ_ONLY_WITHOUT_DELETE);
      }
      else
      {
        write16(MTP_STORAGE_REMOVABLE_RAM);
        write16(MTP_STORAGE_FILESYSTEM_FLAT);
        write16(MTP_STORAGE_READ_WRITE);
      }
      write64(512 * sectors * 160); // max capacity
      write64(512 * sectors * 160); // free space
      write32(0x1UL);               // free space (objects)

      if (badDisk)
        writestring("BAD DISK"); // storage descriptor
      else
        switch (failureCode)
        {
        case readError:
          writestring("DF0: Read Error"); // storage descriptor
          break;
        case writeError:
          writestring("DF0: Write Error"); // storage descriptor
          break;
        case memError:
          writestring("DF0: Memory Error"); // storage descriptor
          break;
        default:
          writestring("DF0:"); // storage descriptor
          break;
        }
    }
    writestring(""); // volume identifier
    break;
  case FSid:
    if (writeProtect() /* || (densMode == 1)*/)
    {
      write16(MTP_STORAGE_REMOVABLE_ROM);            // storage type (removable ROM)
      write16(MTP_STORAGE_FILESYSTEM_HIERARCHICAL);  // filesystem type (hierarchical)
      write16(MTP_STORAGE_READ_ONLY_WITHOUT_DELETE); // access capability (read-only)
    }
    else
    {
      write16(MTP_STORAGE_REMOVABLE_RAM);           // storage type (removable RAM)
      write16(MTP_STORAGE_FILESYSTEM_HIERARCHICAL); // filesystem type (hierarchical)
      write16(MTP_STORAGE_READ_WRITE);              // access capability (read-write)
    }
    if (badDisk)
    {
      write64(0);
      write64(0);              // free space (none)
      write32(0);              // free space (objects)
      writestring("BAD DISK"); // storage descriptor
      writestring("");         // volume identifier
    }
    else
    {
      if (getName() != "NDOS")
      {
        if (openDrive())
        {
          write64(vol->datablockSize * dev->sectors * 160);      // max capacity
          write64(adfCountFreeBlocks(vol) * vol->datablockSize); // free space
          write32(0xFFFFFFFFUL);                                 // free space (objects)
          writestring(vol->volName);                             // storage descriptor
          writestring("");                                       // volume identifier
        }
        else
        {
          write64(0);          // max capacity
          write64(0);          // free space (none)
          write32(0);          // free space (objects)
          writestring("NDOS"); // storage descriptor
          writestring("");     // volume identifier
        }
      }
    }
    break;
  default:
    write16(MTP_STORAGE_REMOVABLE_ROM);            // storage type (removable ROM)
    write16(MTP_STORAGE_FILESYSTEM_FLAT);          // filesystem type (generic flat)
    write16(MTP_STORAGE_READ_ONLY_WITHOUT_DELETE); // access capability (read-only)
    write64(512 * sectors * 160);                  // max capacity
    write64(0);                                    // free space
    write32(0xFFFFFFFFUL);                         // free space (objects)
    writestring("Error");                          // storage descriptor
    writestring("");                               // volume identifier
    break;
  }
}

uint32_t MTPD::GetNumObjects(uint32_t storage, uint32_t parent)
{
  int num = 0;
  switch (storage)
  {
  case FDid: // adf image drive
#ifdef debugmtp
    if (!write_get_length_)
      Serial.printf("[MTP]GetNumObject on Storage No.: %ld ObjectCount: %d", storage, 1);
#endif
    if (badDisk)
      num = 0;
    else
      num = 1;
  case FSid: // filesystem drive
    if (getName() != "NDOS")
    {
      num = getNumAmigaFiles(parent);
    }
    else
      num = 0;
#ifdef debugmtp
    if (!write_get_length_)
      Serial.printf("[MTP]GetNumObject on Storage No.: %ld ObjectCount: %d", storage, num);
#endif
    break;
  default:
    num = 0;
    break;
  }
  return num;
}

void MTPD::GetObjectHandles(uint32_t storage,
                            uint32_t parent)
{
  uint32_t num = 0;
  uint32_t handle;
  switch (storage)
  {
  case FDid: //adf image drive
    if (badDisk)
    {
      num = 0;
      handle = 0;
      write32(num);
    }
    else
    {
      num = 1;
      handle = imageHandle; // ADF-File Special Token - 0xADFF17E;)
      write32(num);
      write32(handle);
    }
#ifdef debugmtp
    if (!write_get_length_)
      Serial.printf("[MTP]GetObjectHandles: Storage: 0x%.8lX Parent: 0x%.8lX Num: %d Handle: 0x%.8lX", storage, parent, num, handle);
#endif
    break;
  case FSid: // filesystem drive
    if (write_get_length_)
    {
      if (list != NULL)
        adfFreeDirList(list);
      tempNum = getNumAmigaFiles(parent);
    }
#ifdef debugmtp
//          Serial.print("FreeStack: ");
//          Serial.println(FreeStack());
#endif
    num = tempNum;
    write32(num);
    cell = list;
    handle = 0;
    while (cell)
    {
      handle = (((struct Entry *)cell->content)->sector) + amigaHandleOffset;
      //            printEnt(vol, (struct Entry*)cell->content, "", true, true);
      //            Serial.println(handle, HEX);
      write32(handle);
      cell = cell->next;
    }

#ifdef debugmtp
    if (!write_get_length_)
      Serial.printf("[MTP]GetObjectHandles: Storage: 0x%.8lX Parent: 0x%.8lX Num: 0x%.8lX Handle: 0x%.8lX", storage, parent & 0x00000fff, num, handle & 0x00000fff);
#endif
    break;

  default:
    break;
  }
}
#define objform MTP_FORMAT_TEXT
void MTPD::GetObjectInfo(uint32_t handle)
{
  char filename[256];
  char dateCreated[20];
  char dateModified[20];
  uint32_t size, parent;
  uint32_t storageID = 0;
  if (handle == imageHandle)
  {
    storageID = FDid;
  }
  else
  {
    if (handle >= offsetStart)
    {
      storageID = FSid;
    }
  }
  switch (storageID)
  {
  case FDid:
  {
    String nameHelper = getName() + ".adf";
    diskDates(dateCreated, dateModified);
    strcpy(filename, nameHelper.c_str());
    size = 512 * sectors * 160;
    write32(FDid);           // storage
    write16(objform);        // format
    write16(1);              // protection
    write32(size);           // size
    write16(MTP_FORMAT_PNG); // thumb format PNG
    write32(g_adfIcon_Size); // thumb size
    write32(64);             // thumb width
    write32(64);             // thumb height
    write32(0);              // pix width
    write32(0);              // pix height
    write32(0);              // bit depth
    write32(0);              // parent
    write16(0);              // association type 0=file
    write32(0);              // association description
    write32(0);              // sequence number
    writestring(filename);
    writestring(dateCreated);  // date created
    writestring(dateModified); // date modified
    writestring("");           // keywords
#ifdef debugmtp
    if (!write_get_length_)
      Serial.printf("[MTP]GetObjectInfo: %s Size: %ld Handle: 0x%.8lX", filename, size, handle);
#endif
    break;
  }
  case FSid:
    // amiga filesys
    AmigaObjectInfo(handle, filename, &size, &parent, dateCreated);
    write32(FSid);                                                    // storage
    write16(size == 0xFFFFFFFFUL ? MTP_FORMAT_ASSOCIATION : objform); // format
    write16(0);                                                       // protection
    write32(size);                                                    // size

    write16(0); // thumb format PNG
    write32(0); // thumb size
    write32(0); // thumb width
    write32(0); // thumb height
    /*
                  write16(0x380B); // thumb format PNG
                  write32(g_myData_Size); // thumb size
                  write32(64); // thumb width
                  write32(64); // thumb height
          */
    write32(0);                            // pix width
    write32(0);                            // pix height
    write32(0);                            // bit depth
    write32(parent + amigaHandleOffset);   // parent
    write16(size == 0xFFFFFFFFUL ? 1 : 0); // association type
    write32(0);                            // association description
    write32(0);                            // sequence number
    writestring(filename);
    writestring(dateCreated); // date created
    writestring("");          // date modified
    writestring("");          // keywords
#ifdef debugmtp
    if (!write_get_length_)
      Serial.printf("[MTP]GetObjectInfo: %s Size: %ld Handle: 0x%.8lX", filename, size, handle);
#endif
    break;

  default:
    break;
  }
}

//     void MTPD::GetObject(uint32_t object_id) {
//       uint32_t size;
//       size = storage_->GetSize(object_id);
// #ifdef debugmtp
//       if (!write_get_length_) Serial.printf("GetObject: 0x%.8lX Size: %ld", object_id, size);
// #endif
//       if (write_get_length_) {
//         write_length_ += size;
//       } else {
//         uint32_t pos = 0;

//         //other files
//         while (pos < size) {
//           get_buffer();
//           uint32_t avail = sizeof(data_buffer_->buf) - data_buffer_->len;
//           uint32_t to_copy = min(size - pos, avail);
//           // Read directly from storage into usb buffer.
//           storage_->read(object_id, pos,
//                          (char*)(data_buffer_->buf + data_buffer_->len),
//                          to_copy);
// #ifdef debugmtp2
//           Serial.printf("ID: 0x%.8X Pos: %ld to_copy: %ld avail: %ld \n", object_id, pos, to_copy, avail);
// #endif
//           pos += to_copy;
//           data_buffer_->len += to_copy;
//           if (data_buffer_->len == sizeof(data_buffer_->buf)) {
// #ifdef debugmtp2
//             Serial.printf("ID: 0x%.8X Pos: %ld to_copy: %ld avail: %ld \n", object_id, pos, to_copy, avail);
//             Serial.printf("SENT...\n");
//             PrintPacket(data_buffer_);
// #endif
//             usb_tx(MTP_TX_ENDPOINT, data_buffer_);
//             data_buffer_ = NULL;
//           }
//         } //end while
//       } //end else
//     }

void MTPD::GetThumb(uint32_t object_id)
{
  uint32_t size = g_adfIcon_Size;
  if (write_get_length_)
  {
    write_length_ += size;
  }
  else
  {
#ifdef debugmtp
    Serial.printf("[MTP]GetThumb: 0x%.8lX Size: %ld", object_id, size);
#endif

    uint32_t pos = 0;
    while (pos < size)
    {
      get_buffer();
      uint32_t avail = sizeof(data_buffer_->buf) - data_buffer_->len;
      uint32_t to_copy = min(pos - size, avail);
      // Copy directly into usb buffer.
      memcpy((char *)(data_buffer_->buf + data_buffer_->len), &g_adfIcon[pos], to_copy);
#ifdef debugmtp2
      Serial.printf("[MTP]ThumbCopy ID: 0x%.8X Pos: %ld to_copy: %ld avail: %ld \n", object_id, pos, to_copy, avail);
#endif
      pos += to_copy;
      data_buffer_->len += to_copy;
      if (data_buffer_->len == sizeof(data_buffer_->buf))
      {
#ifdef debugmtp2
        Serial.printf("[MTP]ID: 0x%.8X Pos: %ld to_copy: %ld avail: %ld \n", object_id, pos, to_copy, avail);
        Serial.printf("[MTP]SENT...\n");
        PrintPacket(data_buffer_);
#endif
        usb_tx(MTP_TX_ENDPOINT, data_buffer_);
        data_buffer_ = NULL;
      }
    }
  }
}
void MTPD::printAFile(AFile *file)
{
  Serial.printf("[MTP]fileHdr headerKey: %lx\n", file->fileHdr->headerKey);
  Serial.printf("[MTP]curDataPtr: %lx\n", file->curDataPtr);
}

int MTPD::getAmigaFile(uint32_t handle, usb_packet_t *receive_buffer)
{
  struct AFile *file;
  unsigned char *extbuf;
  uint32_t n = 0;
  failureCode = 0;
  adfClearError();
  if (vol != NULL)
  {
    extbuf = (unsigned char *)malloc(512 * sizeof(char));
    if (!extbuf)
    {
      Serial.printf("[MTP]malloc error: extbuf %d bytes\n", 512 * sizeof(char));
      failureCode = memError;
      fail = true;
      return failureCode;
    }
    file = (struct AFile *)malloc(sizeof(struct AFile));
    if (!file)
    {
      Serial.printf("[MTP]adfFileOpen : malloc AFile %d bytes\n", sizeof(struct AFile));
      free(extbuf);
      failureCode = memError;
      fail = true;
      return failureCode;
    }
    file->fileHdr = (struct bFileHeaderBlock *)malloc(sizeof(struct bFileHeaderBlock));
    if (!file->fileHdr)
    {
      Serial.printf("[MTP]adfFileOpen : malloc bFileHeaderBlock %d bytes\n", sizeof(struct bFileHeaderBlock));
      free(extbuf);
      free(file);
      failureCode = memError;
      fail = true;
      return failureCode;
    }
    file->currentData = malloc(512 * sizeof(uint8_t));
    if (!file->currentData)
    {
      Serial.printf("[MTP]adfFileOpen : malloc fileblock %d bytes\n", 512 * sizeof(uint8_t));
      free(extbuf);
      free(file->fileHdr);
      free(file);
      failureCode = memError;
      fail = true;
      return failureCode;
    }
    file->volume = vol;
    file->pos = 0;
    file->posInExtBlk = 0;
    file->posInDataBlk = 0;
    file->writeMode = false;
    file->currentExt = NULL;
    file->nDataBlock = 0;
    adfReadEntryBlock(vol, handle & 0x00000fff, (bEntryBlock *)file->fileHdr);
    file->eof = false;

    write_length_ = ((bEntryBlock *)file->fileHdr)->byteSize;
    //        Serial.printf("[MTP]Size: %d Handle: %d\n", write_length_, handle & 0x00000fff);
    MTPHeader header;
    header.len = write_length_ + 12;
    header.type = 2;
    header.op = contains(receive_buffer)->op;
    header.transaction_id = contains(receive_buffer)->transaction_id;
    //      Serial.println(contains(receive_buffer)->transaction_id);
    write((char *)&header, sizeof(header));
    data_buffer_->len = 12;
    //        printAFile(file);
    if (file)
    {
      while (!adfEndOfFile(file))
      {
        n = adfReadFile(file, 512, extbuf);
        write((char *)extbuf, n);
        if ((extError != 0) | adfError())
        {
          fail = true;
          failureCode = readError;
          adfCloseFile(file);
          free(extbuf);
          get_buffer();
          usb_tx(MTP_TX_ENDPOINT, data_buffer_);
          data_buffer_ = NULL;
          return failureCode;
        }
      }
      adfCloseFile(file);
      free(extbuf);
    }
    else
    {
      fail = true;
      failureCode = readError;
    }
    get_buffer();
    usb_tx(MTP_TX_ENDPOINT, data_buffer_);
    data_buffer_ = NULL;
    return failureCode;
  }
  return 0;
}

int MTPD::getAdf(usb_packet_t *receive_buffer)
{
  uint32_t size = 512 * sectors * 160;
  write_length_ = size;
  MTPHeader header;
  header.len = write_length_ + 12;
  header.type = 2;
  header.op = contains(receive_buffer)->op;
  header.transaction_id = contains(receive_buffer)->transaction_id;
  //      Serial.println(contains(receive_buffer)->transaction_id);
  write((char *)&header, sizeof(header));
  data_buffer_->len = 12;
  failureCode = 0;
  retries = 30;
  for (int k = 0; k < 160; k++)
  {
    gotoLogicTrack(k);
    if (readTrack(true, false) != 0)
    {
      fail = true;
      failureCode = readError;
      get_buffer();
      usb_tx(MTP_TX_ENDPOINT, data_buffer_);
      data_buffer_ = NULL;
      return failureCode;
    }
    for (int j = 0; j < sectors; j++)
    {
      write((char *)ptrSector(j), 512);
#ifdef debugmtp
      //          Serial.printf("[MTP]ADFCopy ID: 0x%.8X Sector: %d Track: %d\n", imageHandle, j, k);
#endif
    }
  }
  get_buffer();
  usb_tx(MTP_TX_ENDPOINT, data_buffer_);
  data_buffer_ = NULL;
  return failureCode;
}

void MTPD::read(char *data, uint32_t size)
{
  while (size)
  {
    receive_buffer();
    uint32_t to_copy = data_buffer_->len - data_buffer_->index;
    to_copy = min(to_copy, size);
    if (data)
    {
      memcpy(data, data_buffer_->buf + data_buffer_->index, to_copy);
      data += to_copy;
    }
    size -= to_copy;
    data_buffer_->index += to_copy;
    if (data_buffer_->index == data_buffer_->len)
    {
      usb_free(data_buffer_);
      data_buffer_ = NULL;
    }
  }
}

uint32_t MTPD::ReadMTPHeader()
{
  MTPHeader header;
  read((char *)&header, sizeof(MTPHeader));
  // check that the type is data
  return header.len - 12;
}

uint8_t MTPD::read8()
{
  uint8_t ret;
  read((char *)&ret, sizeof(ret));
  return ret;
}

uint16_t MTPD::read16()
{
  uint16_t ret;
  read((char *)&ret, sizeof(ret));
  return ret;
}

uint32_t MTPD::read32()
{
  uint32_t ret;
  read((char *)&ret, sizeof(ret));
  return ret;
}

void MTPD::readstring(char *buffer)
{
  int len = read8();
  if (!buffer)
  {
    read(NULL, len * 2);
  }
  else
  {
    for (int i = 0; i < len; i++)
    {
      *(buffer++) = read16();
    }
  }
}
void MTPD::read_until_short_packet()
{
  bool done = false;
  while (!done)
  {
    //Serial.println(usb_queue_byte_count(data_buffer_));
    if (usb_queue_byte_count(data_buffer_) != 0)
      receive_buffer();
    done = data_buffer_->len != sizeof(data_buffer_->buf);
    usb_free(data_buffer_);
    data_buffer_ = NULL;
  }
}
uint32_t MTPD::usb_queue_byte_count(const usb_packet_t *p)
{
  uint32_t count = 0;

  __disable_irq();
  for (; p; p = p->next)
  {
    count += p->len;
  }
  __enable_irq();
  return count;
}

uint32_t MTPD::SendObjectInfo(uint32_t storage, uint32_t parent)
{
  uint32_t len __attribute__((unused));
  len = ReadMTPHeader();
  char filename[256];
  char dateCreated[20];

  read32();                                      // storage
  bool dir = read16() == MTP_FORMAT_ASSOCIATION; // folder?
  read16();                                      // protection
  int32_t size = read32();                       // size
  read16();                                      // thumb format
  read32();                                      // thumb size
  read32();                                      // thumb width
  read32();                                      // thumb height
  read32();                                      // pix width
  read32();                                      // pix height
  read32();                                      // bit depth
  read32();                                      // parent
  read16();                                      // association type
  read32();                                      // association description
  read32();                                      // sequence number

  readstring(filename);
  filename[30] = 0;
  readstring(dateCreated);
  int year, mon, day, hour, min, sec;
  sscanf(dateCreated, "%4d%02d%02dT%02d%02d%02d.0", &year, &mon, &day, &hour, &min, &sec);
#ifdef debugmtp
  Serial.println(dateCreated);
  Serial.printf("%2d.%2d.%4d\n", day, mon, year);
#endif
  adfSetCurrentTime(year, mon, day - 1, hour, min, sec);
  Serial.println(usb_queue_byte_count(data_buffer_));
  readstring(NULL);
  Serial.println(usb_queue_byte_count(data_buffer_));
  readstring(NULL);
  Serial.println(usb_queue_byte_count(data_buffer_));
  //read_until_short_packet();  // ignores dates & keywords
  // if (storage == SDid)
  //   return storage_->Create(parent, dir, filename);
  // else {
  if (parent == 0xffffffff)
    vol->curDirPtr = vol->rootBlock;
  else
    vol->curDirPtr = parent & 0x00000fff;
#ifdef debugmtp
  Serial.printf("[MTP]Created: %s CurDir: %d\n", dateCreated, vol->curDirPtr);
  Serial.printf("[MTP]Name: %s Size: %ld Free: %ld\n", filename, size, adfCountFreeBlocks(vol) * vol->datablockSize);
#endif
  if (size < adfCountFreeBlocks(vol) * vol->datablockSize)
  {
    if (dir)
    {
#ifdef debugmtp
      Serial.printf("[MTP]CreateDir: %s curDir: %ld\n", filename, vol->curDirPtr);
#endif
      if (adfCreateDir(vol, vol->curDirPtr, filename) == RC_OK)
      {
        flushToDisk();
        return getAmigaDirHandle(vol, filename) + amigaHandleOffset;
      }
    }
    else
    {
      if (sendObjectFile != 0)
      {
        Serial.println("[MTP]sendObjectfile != NULL");
        if (sendObjectFile->currentExt)
          free(sendObjectFile->currentExt);
        if (sendObjectFile->currentData)
          free(sendObjectFile->currentData);
        free(sendObjectFile->fileHdr);
        free(sendObjectFile);
      }
      sendObjectFile = adfOpenFile(vol, filename, (char *)"w");
    }
    if (!sendObjectFile)
    {
#ifdef debugmtp
      Serial.println("[MTP]open file error");
#endif
      return 0;
      /* error handling */
    }
#ifdef debugmtp
    Serial.printf("[MTP]Handle: %lx\n", sendObjectFile->fileHdr->headerKey + amigaHandleOffset);
#endif
    return sendObjectFile->fileHdr->headerKey + amigaHandleOffset;
  }
  else
    return -1;
  // }
}

// void MTPD::SendObject() {
//   uint32_t len = ReadMTPHeader();
//   while (len) {
//     receive_buffer();
//     uint32_t to_copy = data_buffer_->len - data_buffer_->index;
//     to_copy = min(to_copy, len);
//     storage_->write((char*)(data_buffer_->buf + data_buffer_->index),
//                     to_copy);
//     data_buffer_->index += to_copy;
//     len -= to_copy;
//     if (data_buffer_->index == data_buffer_->len) {
//       usb_free(data_buffer_);
//       data_buffer_ = NULL;
//     }
//   }
//   storage_->close();
// }

int MTPD::SendAFile()
{
  uint8_t buf[512];
  uint32_t len = ReadMTPHeader();
  uint32_t blocks = len / 512;
  uint32_t rest = len % 512;
  failureCode = 0;
  adfClearError();
  for (uint32_t i = 0; i < blocks; i++)
  {
    read((char *)buf, 512);
    adfWriteFile(sendObjectFile, 512, buf);
    if ((extError != 0) | adfError())
    {
      adfCloseFile(sendObjectFile);
      flushToDisk();
      read_until_short_packet(); // ignores dates & keywords
      failureCode = writeError;
      fail = true;
      return failureCode;
    }
  }
  if (rest > 0)
  {
    read((char *)buf, rest);
    adfWriteFile(sendObjectFile, rest, buf);
    if ((extError != 0) | adfError())
    {
      adfCloseFile(sendObjectFile);
      flushToDisk();
      read_until_short_packet(); // ignores dates & keywords
      failureCode = writeError;
      fail = true;
      return failureCode;
    }
  }
  adfCloseFile(sendObjectFile);
  flushToDisk();
  return failureCode;
}

uint32_t MTPD::SendADFInfo(uint32_t storage, uint32_t parent)
{
  ReadMTPHeader();
  char filename[256];

  read32();                 // storage
  read16();                 // format
  read16();                 // protection
  uint32_t size = read32(); // size
  read16();                 // thumb format
  read32();                 // thumb size
  read32();                 // thumb width
  read32();                 // thumb height
  read32();                 // pix width
  read32();                 // pix height
  read32();                 // bit depth
  read32();                 // parent
  read16();                 // association type
  read32();                 // association description
  read32();                 // sequence number

  readstring(filename);
  read_until_short_packet(); // ignores dates & keywords
#ifdef debugmtp
  Serial.printf("[MTP]ADF Filename: %s Size: %ld\n", filename, size);
#endif
  return size;
}

int MTPD::SendADF()
{
  closeDrive();
  ReadMTPHeader();
  failureCode = 0;
  for (int k = 0; k < 160; k++)
  {
    for (int j = 0; j < sectors; j++)
    {
      checkEvent();
      read((char *)ptrSector(j), 512);
    }
    if (writeWithVerify(k, 15) != 0)
    {
      read_until_short_packet();
      failureCode = writeError;
      fail = true;
      return failureCode;
    }
#ifdef debugmtp
    //                  Serial.printf("ADFCopy ID: 0x%.8X Sector: %d Track: %d\n", imageHandle, j, k);
#endif
  }
  return failureCode;
}

void MTPD::GetDevicePropValue(uint32_t prop)
{
  switch (prop)
  {
  case 0x5001: // Battery Level
    break;
  case 0x5011: // DateTime
    writestring("20170910T115700");
    break;
  case 0xd402: // Device friendly name
    // This is the name we'll actually see in the windows explorer.
    // Should probably be configurable.
    writestring("ADF-Drive");
    break;
  case 0xd405: // Device icon
    write((char *)&g_devIcon, g_devIcon_Size);
    break;
  }
}

void MTPD::GetDevicePropDesc(uint32_t prop)
{
  switch (prop)
  {
  case MTP_DEVICE_PROPERTY_BATTERY_LEVEL:
    write16(prop);
    write16(0x0002); // uint8
    write8(0);       // read-only
    write8(100);     // factory default (fully charged)
    write8(100);     // current value (fully charged)
    write8(0x01);    // range form
    write8(0);       // minimum value (alternate power source)
    write8(100);     // maximum value (fully charged)
    write8(10);      // step size
    break;
  case MTP_DEVICE_PROPERTY_DATETIME:
    write16(prop);
    write16(0xFFFF); // string type
    write8(1);       // read write
    GetDevicePropValue(prop);
    GetDevicePropValue(prop);
    write8(0); // date time form
    break;
  case MTP_DEVICE_PROPERTY_DEVICE_FRIENDLY_NAME:
    write16(prop);
    write16(0xFFFF); // string type
    write8(0);       // read-only
    GetDevicePropValue(prop);
    GetDevicePropValue(prop);
    write8(0); // no form
    break;
  case MTP_DEVICE_PROPERTY_DEVICE_ICON:
    write16(prop);
    write16(0x4002); // icon type
    write8(0);       // read-only
    GetDevicePropValue(prop);
    //          GetDevicePropValue(prop);
    write8(0); // no form
    break;
  }
}

void MTPD::GetObjectPropSupp(uint32_t prop)
{
  write32(1);
  write16(0xdc07); // Object File Name
}

void MTPD::GetObjectPropDesc(uint32_t prop)
{
  switch (prop)
  {
  case 0xdc07: // Object File Name
    write16(prop);
    write16(0xFFFF);        // String
    write8(0);              // read-only
    writestring(".{1,30}"); // current value (fully charged)
    write32(0);
    write8(0x05);
    break;
  }
}

void MTPD::GetObjectPropValue(uint32_t prop)
{
  switch (prop)
  {
  case 0xdc07: // Object File Name
    write16(prop);
    write16(0xFFFF);        // String
    write8(0);              // read-only
    writestring(".{1,30}"); // current value (fully charged)
    write32(0);
    write8(0x05);
    break;
  }
}

void MTPD::checkEvent()
{
  usb_packet_t *receive_buffer;
  if ((receive_buffer = usb_rx(MTP_EVENT_ENDPOINT)))
  {
#ifdef debugmtp
    Serial.println("event received");
#endif
    usb_free(receive_buffer);
  }
}

#ifdef debugmtp
void dumpCommand(uint32_t type, uint32_t op, uint32_t p1, uint32_t p2, uint32_t p3)
{
  switch (op)
  {
  case MTP_OPERATION_GET_DEVICE_INFO:
    Serial.printf("[MTP]%d GetDescription - Command: %#.4x Params: none\n",
                  millis() / 1000, op);
    break;
  case MTP_OPERATION_OPEN_SESSION:
    Serial.printf("[MTP]%d OpenSession - Command: %#.4x SessionID: %x\n",
                  millis() / 1000, op, p1);
    break;
  case MTP_OPERATION_CLOSE_SESSION:
    Serial.printf("[MTP]%d CloseSession - Command: %#.4x Params: none\n",
                  millis() / 1000, op);
    break;
  case MTP_OPERATION_GET_STORAGE_IDS:
    Serial.printf("[MTP]%d GetStorageID - Command: %#.4x Params: none\n",
                  millis() / 1000, op);
    break;
  case MTP_OPERATION_GET_STORAGE_INFO:
    Serial.printf("[MTP]%d GetStorageInfo - Command: %#.4x StorageID: %x\n",
                  millis() / 1000, op, p1);
    break;
  case MTP_OPERATION_GET_NUM_OBJECTS:
    Serial.printf("[MTP]%d GetNumObjects - Command: %#.4x StorageID: %x [ObjFormatCode: %x] [ObjHndlAsc: %x] \n",
                  millis() / 1000, op, p1, p2, p3);
    break;
  case MTP_OPERATION_GET_OBJECT_HANDLES:
    Serial.printf("[MTP]%d GetObjectHandles - Command: %#.4x StorageID: %x [ObjFormatCode: %x] [ObjHndlAsc: %x] \n",
                  millis() / 1000, op, p1, p2, p3);
    break;
  case MTP_OPERATION_GET_OBJECT_INFO:
    Serial.printf("[MTP]%d GetObjectInfo - Command: %#.4x Handle: %x\n",
                  millis() / 1000, op, p1);
    break;
  case MTP_OPERATION_GET_OBJECT:
    Serial.printf("[MTP]%d GetObject - Command: %#.4x Handle: %x\n",
                  millis() / 1000, op, p1);
    break;
  case MTP_OPERATION_GET_THUMB:
    Serial.printf("[MTP]%d GetThumb - Command: %#.4x Handle: %x\n",
                  millis() / 1000, op, p1);
    break;
  case MTP_OPERATION_DELETE_OBJECT:
    Serial.printf("[MTP]%d DeleteObject - Command: %#.4x Handle: %x [ObjFormatCode: %x]\n",
                  millis() / 1000, op, p1, p2);
    break;
  case MTP_OPERATION_SEND_OBJECT_INFO:
    Serial.printf("[MTP]%d SendObjectInfo - Command: %#.4x StorageID: %x ParentHandle: %x\n",
                  millis() / 1000, op, p1, p2);
    break;
  case MTP_OPERATION_SEND_OBJECT:
    Serial.printf("[MTP]%d SendObject - Command: %#.4x\n",
                  millis() / 1000, op);
    break;
  case MTP_OPERATION_FORMAT_STORE:
    Serial.printf("[MTP]%d FormatStorage - Command: %#.4x StorageID: %x [FileSystemFormat: %x]\n",
                  millis() / 1000, op, p1, p2);
    break;
  case MTP_OPERATION_GET_DEVICE_PROP_DESC:
    Serial.printf("[MTP]%d GetDevicePropDesc - Command: %#.4x Code: %x \n",
                  millis() / 1000, op, p1);
    break;
  case MTP_OPERATION_GET_DEVICE_PROP_VALUE:
    Serial.printf("[MTP]%d GetDevicePropvalue - Command: %#.4x DevicePropCode: %x\n",
                  millis() / 1000, op, p1);
    break;
  case MTP_OPERATION_GET_OBJECT_PROPS_SUPPORTED:
    Serial.printf("[MTP]%d MTP_OPERATION_GET_OBJECT_PROPS_SUPPORTED - Command: %#.4x DevicePropCode: %x\n",
                  millis() / 1000, op, p1);
    break;
  case MTP_OPERATION_GET_OBJECT_PROP_DESC:
    Serial.printf("[MTP]%d MTP_OPERATION_GET_OBJECT_PROP_DESC - Command: %#.4x DevicePropCode: %x\n",
                  millis() / 1000, op, p1);
    break;
  case MTP_OPERATION_GET_OBJECT_PROP_VALUE:
    Serial.printf("[MTP]%d MTP_OPERATION_GET_OBJECT_PROP_VALUE - Command: %#.4x DevicePropCode: %x\n",
                  millis() / 1000, op, p1);
    break;
  default:
    Serial.printf("\n[MTP]%d Type: %d Unhandled Command: %#.4x P1: %x P2: %x P3: %x \n",
                  millis() / 1000, type, op, p1, p2, p3);
    break;
  }
}
#endif

uint32_t handle2block(uint32_t handle)
{
  handle = handle & 0x00000fff;
  return handle;
}

uint32_t block2handle(uint32_t block)
{
  block = block + amigaHandleOffset;
  return block;
}
uint32_t eventID = 1;
bool sendADF = false;

//public:
void MTPD::sendEvent(int event, uint32_t param)
{
#ifdef debugmtp
  String eventName = "";
  switch (event)
  {
  case MTP_EVENT_UNDEFINED:
    eventName = "MTP_EVENT_UNDEFINED";
    break;
  case MTP_EVENT_CANCEL_TRANSACTION:
    eventName = "MTP_EVENT_CANCEL_TRANSACTION";
    break;
  case MTP_EVENT_OBJECT_ADDED:
    eventName = "MTP_EVENT_OBJECT_ADDED";
    break;
  case MTP_EVENT_OBJECT_REMOVED:
    eventName = "MTP_EVENT_OBJECT_REMOVED";
    break;
  case MTP_EVENT_STORE_ADDED:
    eventName = "MTP_EVENT_STORE_ADDED";
    break;
  case MTP_EVENT_STORE_REMOVED:
    eventName = "MTP_EVENT_STORE_REMOVED";
    break;
  case MTP_EVENT_DEVICE_PROP_CHANGED:
    eventName = "MTP_EVENT_DEVICE_PROP_CHANGED";
    break;
  case MTP_EVENT_OBJECT_INFO_CHANGED:
    eventName = "MTP_EVENT_OBJECT_INFO_CHANGED";
    break;
  case MTP_EVENT_DEVICE_INFO_CHANGED:
    eventName = "MTP_EVENT_DEVICE_INFO_CHANGED";
    break;
  case MTP_EVENT_REQUEST_OBJECT_TRANSFER:
    eventName = "MTP_EVENT_REQUEST_OBJECT_TRANSFER";
    break;
  case MTP_EVENT_STORE_FULL:
    eventName = "MTP_EVENT_STORE_FULL";
    break;
  case MTP_EVENT_DEVICE_RESET:
    eventName = "MTP_EVENT_DEVICE_RESET";
    break;
  case MTP_EVENT_STORAGE_INFO_CHANGED:
    eventName = "MTP_EVENT_STORAGE_INFO_CHANGED";
    break;
  case MTP_EVENT_CAPTURE_COMPLETE:
    eventName = "MTP_EVENT_CAPTURE_COMPLETE";
    break;
  case MTP_EVENT_UNREPORTED_STATUS:
    eventName = "MTP_EVENT_UNREPORTED_STATUS";
    break;
  case MTP_EVENT_OBJECT_PROP_CHANGED:
    eventName = "MTP_EVENT_OBJECT_PROP_CHANGED";
    break;
  case MTP_EVENT_OBJECT_PROP_DESC_CHANGED:
    eventName = "MTP_EVENT_OBJECT_PROP_DESC_CHANGED";
    break;
  case MTP_EVENT_OBJECT_REFERENCES_CHANGED:
    eventName = "MTP_EVENT_OBJECT_REFERENCES_CHANGED";
    break;
  default:
    eventName = "MTP_EVENT_UNKNOWN";
    break;
  }
  Serial.print("[MTP]sendEvent: ");
  Serial.print(eventName);
  Serial.printf(" (%x) %x\n", event, param);
#endif
  usb_packet_t *eventBuffer = NULL;
  while (!eventBuffer)
  {
    eventBuffer = usb_malloc();
    if (!eventBuffer)
      mtp_yield();
  }
  eventBuffer->len = 16;
  eventBuffer->index = 0;
  eventBuffer->next = NULL;
  MTPContainer eventContainer;
  eventContainer.len = 16;   // maximum length for an event container
  eventContainer.type = 4;   // Type: Event
  eventContainer.op = event; // event code
  /*  the event codes must be included in WriteDescriptor()
          otherwise the responder just ignores the event code
      */
  eventContainer.transaction_id = eventID++;
  eventContainer.params[0] = param;
  memcpy(eventBuffer->buf, (char *)&eventContainer, 16);
  usb_tx(MTP_EVENT_ENDPOINT, eventBuffer);
  /*  the MTP_EVENT_ENDPOINT must be defined as
          "ENDPOINT_TRANSMIT_AND_RECEIVE" in
          ...\hardware\teensy\avr\cores\teensy3\usb_desc.h
      */
  get_buffer();
  usb_tx(MTP_EVENT_ENDPOINT, data_buffer_); // send empty packet to finish transaction
  data_buffer_ = NULL;
}

/* Main loop of MTP Responder */

void MTPD::loop()
{
  usb_packet_t *receive_buffer;
  if ((receive_buffer = usb_rx(MTP_RX_ENDPOINT)))
  {
    uint32_t return_code = 0;
    uint32_t p1 = 0;
    if (receive_buffer->len >= 12)
    {
#ifdef debugmtp
      dumpCommand(CONTAINER->type, CONTAINER->op, CONTAINER->params[0], CONTAINER->params[1], CONTAINER->params[2]);
#endif
      return_code = MTP_RESPONSE_OK;
      receive_buffer->len = 16;
      if (CONTAINER->type == 1)
      {
        switch (CONTAINER->op)
        {
        case MTP_OPERATION_GET_DEVICE_INFO:
          TRANSMIT(WriteDescriptor());
          break;
        case MTP_OPERATION_OPEN_SESSION:
          break;
        case MTP_OPERATION_CLOSE_SESSION:
          break;
        case MTP_OPERATION_GET_STORAGE_IDS:
          TRANSMIT(WriteStorageIDs());
          break;
        case MTP_OPERATION_GET_STORAGE_INFO:
          if (
              ((CONTAINER->params[0] == FDid) && (!noDisk)) || ((CONTAINER->params[0] == FSid) && (!noDisk && !busyFormatting)))
          {
            TRANSMIT(GetStorageInfo(CONTAINER->params[0]));
          }
          else
            return_code = MTP_RESPONSE_STORE_NOT_AVAILABLE;
          break;
        case MTP_OPERATION_GET_NUM_OBJECTS:
          if (CONTAINER->params[1])
          {
            return_code = MTP_RESPONSE_SPECIFICATION_BY_FORMAT_UNSUPPORTED;
          }
          else
          {
            p1 = GetNumObjects(CONTAINER->params[0],
                               CONTAINER->params[2]);
          }
          break;
        case MTP_OPERATION_GET_OBJECT_HANDLES:
          if (CONTAINER->params[1])
          {
            return_code = MTP_RESPONSE_SPECIFICATION_BY_FORMAT_UNSUPPORTED;
          }
          else
          {
            TRANSMIT(GetObjectHandles(CONTAINER->params[0],
                                      CONTAINER->params[2]));
          }
          break;
        case MTP_OPERATION_GET_OBJECT_INFO:
          TRANSMIT(GetObjectInfo(CONTAINER->params[0]));
          break;
        case MTP_OPERATION_GET_OBJECT:
          if (CONTAINER->params[0] == imageHandle)
          {
            failureCode = getAdf(receive_buffer);
          }
          else
          {
            if (CONTAINER->params[0] >= offsetStart)
              failureCode = getAmigaFile(CONTAINER->params[0], receive_buffer);
          }
          if (failureCode != 0)
          {
            return_code = MTP_RESPONSE_INCOMPLETE_TRANSFER;
            queuedEvent[0] = MTP_EVENT_STORAGE_INFO_CHANGED;
            queuedEvent[1] = FDid;
          }
          break;
        case MTP_OPERATION_GET_THUMB:
          TRANSMIT(GetThumb(CONTAINER->params[0]));
          //    return_code = MTP_RESPONSE_OPERATION_NOT_SUPPORTED;
          break;
        case MTP_OPERATION_DELETE_OBJECT:
          if (CONTAINER->params[1])
          {
            return_code = MTP_RESPONSE_SPECIFICATION_BY_FORMAT_UNSUPPORTED;
          }
          else
          {
            if (CONTAINER->params[0] == imageHandle)
            { // DF0:
              return_code = MTP_RESPONSE_OK;
              /*
                       we don't really delete it, we just play along because
                       windows issues a delete when the drag & drop image has
                       the same filename as the inserted disks volume label
                    */
              break;
            }
            if (CONTAINER->params[0] >= offsetStart)
            { // Amiga FS
              if (wProt)
                return_code = MTP_RESPONSE_STORE_READ_ONLY;
              else if (deleteAmigaFile(CONTAINER->params[0]) != RC_OK)
                return_code = MTP_RESPONSE_PARTIAL_DELETION;
              break;
            }
            // if (!storage_->DeleteObject(CONTAINER->params[0])) { // SD Card
            //   return_code = MTP_RESPONSE_PARTIAL_DELETION;
            //   break;
            // }
            return_code = MTP_RESPONSE_STORE_READ_ONLY;
          }
          break;
        case MTP_OPERATION_SEND_OBJECT_INFO:
          switch (CONTAINER->params[0])
          {
          case FDid:
          {
            CONTAINER->params[2] = imageHandle;
            uint32_t imageSize = SendADFInfo(CONTAINER->params[0],
                                             CONTAINER->params[1]);
            if (imageSize == 901120 || imageSize == 1802240)
              return_code = MTP_RESPONSE_OK;
            else
              return_code = MTP_RESPONSE_INVALID_DATASET;
            if (wProt)
              return_code = MTP_RESPONSE_STORE_READ_ONLY;
            if (noDisk)
              return_code = MTP_RESPONSE_ACCESS_DENIED;
            p1 = CONTAINER->params[0];
            if (!p1)
              p1 = 1;
            CONTAINER->len = receive_buffer->len = 12 + 3 * 4;
            sendADF = true;
            sendState = FDid;
            break;
          }
          case FSid:
            if (!wProt && !noDisk)
            {
              CONTAINER->params[2] =
                  SendObjectInfo(CONTAINER->params[0],  // storage
                                 CONTAINER->params[1]); // parent
            }
            else
            {
              read_until_short_packet(); //flush rest of packet
              CONTAINER->params[2] = 0;
            }
            if (CONTAINER->params[2] == 0xffffffff)
            {
              read_until_short_packet(); // flush rest of packet
              CONTAINER->params[2] = 0;
              return_code = MTP_RESPONSE_INVALID_DATASET;
            }
            if (wProt)
              return_code = MTP_RESPONSE_STORE_READ_ONLY;
            if (noDisk)
              return_code = MTP_RESPONSE_ACCESS_DENIED;
            p1 = CONTAINER->params[0];
            if (!p1)
              p1 = 1;
            CONTAINER->len = receive_buffer->len = 12 + 3 * 4;
            sendADF = false;
            sendState = FSid;
            break;
          default:
            return_code = MTP_RESPONSE_INVALID_STORAGE_ID;
            break;
          }
          break;
        case MTP_OPERATION_SEND_OBJECT:
          switch (sendState)
          {
          case FDid:
            if (SendADF() == 0)
            {
              return_code = MTP_RESPONSE_OK;
              sendEvent(MTP_EVENT_STORE_REMOVED, FDid);
              sendEvent(MTP_EVENT_STORE_ADDED, FDid);
              sendEvent(MTP_EVENT_STORE_REMOVED, FSid);
              sendEvent(MTP_EVENT_STORE_ADDED, FSid);
            }
            else
              return_code = MTP_RESPONSE_INCOMPLETE_TRANSFER; // Incomplete_Transfer
            break;
          case FSid:
            if (SendAFile() == 0)
            {
              return_code = MTP_RESPONSE_OK;
            }
            else
              return_code = MTP_RESPONSE_INCOMPLETE_TRANSFER; // Incomplete_Transfer
            break;
          default:
            return_code = MTP_RESPONSE_GENERAL_ERROR; // The General is in da house!
            break;
          }
          queuedEvent[0] = MTP_EVENT_STORAGE_INFO_CHANGED;
          queuedEvent[1] = FDid;
          break;
        case MTP_OPERATION_FORMAT_STORE:
          if (CONTAINER->params[0] == FDid)
          {
            if (!writeProtect())
            {
              return_code = MTP_RESPONSE_OK; //OK
            }
            else
            {
              return_code = MTP_RESPONSE_STORE_READ_ONLY;
            }

            closeDrive();
            CONTAINER->type = 3;
            CONTAINER->op = return_code;
            CONTAINER->params[0] = p1;
            tID = CONTAINER->transaction_id;
#ifdef debugmtp
            Serial.printf(" Result: 0x%.4X tID: 0x%.8lX\n", return_code, tID);
#endif
            usb_tx(MTP_TX_ENDPOINT, receive_buffer);
            receive_buffer = 0;
            sendEvent(MTP_EVENT_STORAGE_INFO_CHANGED, FDid);
            sendEvent(MTP_EVENT_STORE_REMOVED, FSid);
            if (return_code == MTP_RESPONSE_OK)
              formatDisk(false, true);
            sendEvent(MTP_EVENT_STORAGE_INFO_CHANGED, FDid);
            sendEvent(MTP_EVENT_STORE_ADDED, FSid);
            return_code = 0;
          }
          else
          {
            return_code = MTP_EVENT_STORE_REMOVED;
          }
          break;
        case MTP_OPERATION_GET_DEVICE_PROP_DESC:
          TRANSMIT(GetDevicePropDesc(CONTAINER->params[0]));
          break;
        case MTP_OPERATION_GET_DEVICE_PROP_VALUE:
          TRANSMIT(GetDevicePropValue(CONTAINER->params[0]));
          break;
        case MTP_OPERATION_GET_OBJECT_PROPS_SUPPORTED:
          TRANSMIT(GetObjectPropSupp(CONTAINER->params[0]));
          break;
        case MTP_OPERATION_GET_OBJECT_PROP_DESC:
          TRANSMIT(GetObjectPropDesc(CONTAINER->params[0]));
          break;
        case MTP_OPERATION_GET_OBJECT_PROP_VALUE:
          TRANSMIT(GetObjectPropValue(CONTAINER->params[0]));
          break;
        default:
          return_code = MTP_RESPONSE_OPERATION_NOT_SUPPORTED;
          break;
        }
      }
      else
      {
        return_code = MTP_RESPONSE_UNDEFINED;
      }
    }
    if (return_code)
    {
      CONTAINER->type = 3;
      CONTAINER->op = return_code;
      CONTAINER->params[0] = p1;
      tID = CONTAINER->transaction_id;
#ifdef debugmtp
      Serial.printf("[MTP]->Result: 0x%.4X tID: 0x%.8lX\n", return_code, tID);
#endif
      usb_tx(MTP_TX_ENDPOINT, receive_buffer);
      receive_buffer = 0;
    }
    else
    {
      usb_free(receive_buffer);
    }
  }
  checkEvent();
}
