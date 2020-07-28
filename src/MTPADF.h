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


#ifndef MTPADF_H
#define MTPADF_H

#include <Arduino.h>


#include "usb_dev.h"
#include "SdFat.h"
#include "adflib.h"


#define SDid 0x11110001
#define FDid 0x22220002
#define FSid 0x33330003

// MTP Operation Codes
#define MTP_OPERATION_GET_DEVICE_INFO                       0x1001
#define MTP_OPERATION_OPEN_SESSION                          0x1002
#define MTP_OPERATION_CLOSE_SESSION                         0x1003
#define MTP_OPERATION_GET_STORAGE_IDS                       0x1004
#define MTP_OPERATION_GET_STORAGE_INFO                      0x1005
#define MTP_OPERATION_GET_NUM_OBJECTS                       0x1006
#define MTP_OPERATION_GET_OBJECT_HANDLES                    0x1007
#define MTP_OPERATION_GET_OBJECT_INFO                       0x1008
#define MTP_OPERATION_GET_OBJECT                            0x1009
#define MTP_OPERATION_GET_THUMB                             0x100A
#define MTP_OPERATION_DELETE_OBJECT                         0x100B
#define MTP_OPERATION_SEND_OBJECT_INFO                      0x100C
#define MTP_OPERATION_SEND_OBJECT                           0x100D
#define MTP_OPERATION_INITIATE_CAPTURE                      0x100E
#define MTP_OPERATION_FORMAT_STORE                          0x100F
#define MTP_OPERATION_RESET_DEVICE                          0x1010
#define MTP_OPERATION_SELF_TEST                             0x1011
#define MTP_OPERATION_SET_OBJECT_PROTECTION                 0x1012
#define MTP_OPERATION_POWER_DOWN                            0x1013
#define MTP_OPERATION_GET_DEVICE_PROP_DESC                  0x1014
#define MTP_OPERATION_GET_DEVICE_PROP_VALUE                 0x1015
#define MTP_OPERATION_SET_DEVICE_PROP_VALUE                 0x1016
#define MTP_OPERATION_RESET_DEVICE_PROP_VALUE               0x1017
#define MTP_OPERATION_TERMINATE_OPEN_CAPTURE                0x1018
#define MTP_OPERATION_MOVE_OBJECT                           0x1019
#define MTP_OPERATION_COPY_OBJECT                           0x101A
#define MTP_OPERATION_GET_PARTIAL_OBJECT                    0x101B
#define MTP_OPERATION_INITIATE_OPEN_CAPTURE                 0x101C
#define MTP_OPERATION_GET_OBJECT_PROPS_SUPPORTED            0x9801
#define MTP_OPERATION_GET_OBJECT_PROP_DESC                  0x9802
#define MTP_OPERATION_GET_OBJECT_PROP_VALUE                 0x9803
#define MTP_OPERATION_SET_OBJECT_PROP_VALUE                 0x9804
#define MTP_OPERATION_GET_OBJECT_PROP_LIST                  0x9805
#define MTP_OPERATION_SET_OBJECT_PROP_LIST                  0x9806
#define MTP_OPERATION_GET_INTERDEPENDENT_PROP_DESC          0x9807
#define MTP_OPERATION_SEND_OBJECT_PROP_LIST                 0x9808
#define MTP_OPERATION_GET_OBJECT_REFERENCES                 0x9810
#define MTP_OPERATION_SET_OBJECT_REFERENCES                 0x9811
#define MTP_OPERATION_SKIP                                  0x9820

// MTP Response Codes
#define MTP_RESPONSE_UNDEFINED                                  0x2000
#define MTP_RESPONSE_OK                                         0x2001
#define MTP_RESPONSE_GENERAL_ERROR                              0x2002
#define MTP_RESPONSE_SESSION_NOT_OPEN                           0x2003
#define MTP_RESPONSE_INVALID_TRANSACTION_ID                     0x2004
#define MTP_RESPONSE_OPERATION_NOT_SUPPORTED                    0x2005
#define MTP_RESPONSE_PARAMETER_NOT_SUPPORTED                    0x2006
#define MTP_RESPONSE_INCOMPLETE_TRANSFER                        0x2007
#define MTP_RESPONSE_INVALID_STORAGE_ID                         0x2008
#define MTP_RESPONSE_INVALID_OBJECT_HANDLE                      0x2009
#define MTP_RESPONSE_DEVICE_PROP_NOT_SUPPORTED                  0x200A
#define MTP_RESPONSE_INVALID_OBJECT_FORMAT_CODE                 0x200B
#define MTP_RESPONSE_STORAGE_FULL                               0x200C
#define MTP_RESPONSE_OBJECT_WRITE_PROTECTED                     0x200D
#define MTP_RESPONSE_STORE_READ_ONLY                            0x200E
#define MTP_RESPONSE_ACCESS_DENIED                              0x200F
#define MTP_RESPONSE_NO_THUMBNAIL_PRESENT                       0x2010
#define MTP_RESPONSE_SELF_TEST_FAILED                           0x2011
#define MTP_RESPONSE_PARTIAL_DELETION                           0x2012
#define MTP_RESPONSE_STORE_NOT_AVAILABLE                        0x2013
#define MTP_RESPONSE_SPECIFICATION_BY_FORMAT_UNSUPPORTED        0x2014
#define MTP_RESPONSE_NO_VALID_OBJECT_INFO                       0x2015
#define MTP_RESPONSE_INVALID_CODE_FORMAT                        0x2016
#define MTP_RESPONSE_UNKNOWN_VENDOR_CODE                        0x2017
#define MTP_RESPONSE_CAPTURE_ALREADY_TERMINATED                 0x2018
#define MTP_RESPONSE_DEVICE_BUSY                                0x2019
#define MTP_RESPONSE_INVALID_PARENT_OBJECT                      0x201A
#define MTP_RESPONSE_INVALID_DEVICE_PROP_FORMAT                 0x201B
#define MTP_RESPONSE_INVALID_DEVICE_PROP_VALUE                  0x201C
#define MTP_RESPONSE_INVALID_PARAMETER                          0x201D
#define MTP_RESPONSE_SESSION_ALREADY_OPEN                       0x201E
#define MTP_RESPONSE_TRANSACTION_CANCELLED                      0x201F
#define MTP_RESPONSE_SPECIFICATION_OF_DESTINATION_UNSUPPORTED   0x2020
#define MTP_RESPONSE_INVALID_OBJECT_PROP_CODE                   0xA801
#define MTP_RESPONSE_INVALID_OBJECT_PROP_FORMAT                 0xA802
#define MTP_RESPONSE_INVALID_OBJECT_PROP_VALUE                  0xA803
#define MTP_RESPONSE_INVALID_OBJECT_REFERENCE                   0xA804
#define MTP_RESPONSE_GROUP_NOT_SUPPORTED                        0xA805
#define MTP_RESPONSE_INVALID_DATASET                            0xA806
#define MTP_RESPONSE_SPECIFICATION_BY_GROUP_UNSUPPORTED         0xA807
#define MTP_RESPONSE_SPECIFICATION_BY_DEPTH_UNSUPPORTED         0xA808
#define MTP_RESPONSE_OBJECT_TOO_LARGE                           0xA809
#define MTP_RESPONSE_OBJECT_PROP_NOT_SUPPORTED                  0xA80A

// MTP Event Codes
#define MTP_EVENT_UNDEFINED                         0x4000
#define MTP_EVENT_CANCEL_TRANSACTION                0x4001
#define MTP_EVENT_OBJECT_ADDED                      0x4002
#define MTP_EVENT_OBJECT_REMOVED                    0x4003
#define MTP_EVENT_STORE_ADDED                       0x4004
#define MTP_EVENT_STORE_REMOVED                     0x4005
#define MTP_EVENT_DEVICE_PROP_CHANGED               0x4006
#define MTP_EVENT_OBJECT_INFO_CHANGED               0x4007
#define MTP_EVENT_DEVICE_INFO_CHANGED               0x4008
#define MTP_EVENT_REQUEST_OBJECT_TRANSFER           0x4009
#define MTP_EVENT_STORE_FULL                        0x400A
#define MTP_EVENT_DEVICE_RESET                      0x400B
#define MTP_EVENT_STORAGE_INFO_CHANGED              0x400C
#define MTP_EVENT_CAPTURE_COMPLETE                  0x400D
#define MTP_EVENT_UNREPORTED_STATUS                 0x400E
#define MTP_EVENT_OBJECT_PROP_CHANGED               0xC801
#define MTP_EVENT_OBJECT_PROP_DESC_CHANGED          0xC802
#define MTP_EVENT_OBJECT_REFERENCES_CHANGED         0xC803

// MTP Device Property Codes
#define MTP_DEVICE_PROPERTY_BATTERY_LEVEL                   0x5001
#define MTP_DEVICE_PROPERTY_DATETIME                        0x5011
#define MTP_DEVICE_PROPERTY_DEVICE_FRIENDLY_NAME            0xD402
#define MTP_DEVICE_PROPERTY_DEVICE_ICON                     0xD405
#define MTP_DEVICE_PROPERTY_PERCEIVED_DEVICE_TYPE           0xD407

// MTP Format Codes
#define MTP_FORMAT_UNDEFINED                            0x3000   // Undefined object
#define MTP_FORMAT_ASSOCIATION                          0x3001   // Association (for example, a folder)
#define MTP_FORMAT_TEXT                                 0x3004   // Text file
#define MTP_FORMAT_PNG                                  0x380B   // Portable Network Graphics

// Storage Type
#define MTP_STORAGE_FIXED_ROM                       0x0001
#define MTP_STORAGE_REMOVABLE_ROM                   0x0002
#define MTP_STORAGE_FIXED_RAM                       0x0003
#define MTP_STORAGE_REMOVABLE_RAM                   0x0004
// Storage File System
#define MTP_STORAGE_FILESYSTEM_FLAT                 0x0001
#define MTP_STORAGE_FILESYSTEM_HIERARCHICAL         0x0002
#define MTP_STORAGE_FILESYSTEM_DCF                  0x0003
// Storage Access Capability
#define MTP_STORAGE_READ_WRITE                      0x0000
#define MTP_STORAGE_READ_ONLY_WITHOUT_DELETE        0x0001
#define MTP_STORAGE_READ_ONLY_WITH_DELETE           0x0002

// These should probably be weak.
void mtp_yield();
void mtp_lock_storage(bool lock);

struct Record
{
  uint32_t parent;
  uint32_t child; // size stored here for files
  uint32_t sibling;
  uint8_t isdir;
  uint8_t scanned;
  char name[64];
};

// This interface lets the MTP responder interface any storage.
// We'll need to give the MTP responder a pointer to one of these.
class MTPStorageInterface
{
public:
  // Return true if this storage is read-only
  virtual bool readonly() = 0;

  // Does it have directories?
  virtual bool has_directories() = 0;

  // Return size of storage in bytes.
  virtual uint64_t size() = 0;

  // Return free space in bytes.
  virtual uint64_t free() = 0;

  // parent = 0 means get all handles.
  // parent = 0xFFFFFFFF means get root folder.
  // virtual void StartGetObjectHandles(uint32_t parent) = 0;
  // virtual uint32_t GetNextObjectHandle() = 0;

  // Size should be 0xFFFFFFFF if it's a directory.
  // virtual void GetObjectInfo(uint32_t handle,
  //                            char* name,
  //                            uint32_t* size,
  //                            uint32_t* parent) = 0;
  virtual uint32_t GetSize(uint32_t handle) = 0;
  // virtual void read(uint32_t handle,
  //                   uint32_t pos,
  //                   char* buffer,
  //                   uint32_t bytes) = 0;
  // virtual uint32_t Create(uint32_t parent,
  //                         bool folder,
  //                         const char* filename) = 0;
  // virtual void write(const char* data, uint32_t size);
  // virtual void close();
  // virtual bool DeleteObject(uint32_t object) = 0;
};

// Storage implementation for Amiga FS.
class MTPStorage_Amiga : public MTPStorageInterface
{
private:
  /* Reads IndexRecord i */
  Record ReadIndexRecord(uint32_t i);

  bool readonly();
  bool has_directories();
  uint64_t size();
  uint64_t free();
  void OpenIndex();
  /* writes Indexrecord r at Indexposition i */
  void WriteIndexRecord(uint32_t i, const Record &r);
  /* puts a new indexrecord at the end */
  uint32_t AppendIndexRecord(const Record &r);

  // void ConstructFilename(int i, char* out);

  // void OpenFileByIndex(uint32_t i, oflag_t mode = O_RDONLY);

  // MTP object handles should not change or be re-used during a session.
  // This would be easy if we could just have a list of all files in memory.
  // Since our RAM is limited, we'll keep the index in a file instead.
  // bool index_generated = false;
  // void GenerateIndex();
  // void ScanDir(uint32_t i);
  // bool all_scanned_ = false;
  // void ScanAll();

  uint32_t next_;
  bool follow_sibling_;
  // void StartGetObjectHandles(uint32_t parent) override;

  // uint32_t GetNextObjectHandle() override;

  // void GetObjectInfo(uint32_t handle,
  //                    char* name,
  //                    uint32_t* size,
  //                    uint32_t* parent) override;

  uint32_t GetSize(uint32_t handle);

  // void read(uint32_t handle,
  //           uint32_t pos,
  //           char* out,
  //           uint32_t bytes) override;

  // bool DeleteObject(uint32_t object) override;

  // uint32_t Create(uint32_t parent,
  //                 bool folder,
  //                 const char* filename) override;

  // void write(const char* data, uint32_t bytes) override;

  // void close() override;
};

// MTP Responder.
class MTPD
{
public:
  explicit MTPD(MTPStorageInterface *storage) : storage_(storage) {}

private:
  MTPStorageInterface *storage_;
  uint32_t tID;
  struct MTPContainer
  {
    uint32_t len;            // 0
    uint16_t type;           // 4
    uint16_t op;             // 6
    uint32_t transaction_id; // 8
    uint32_t params[5];      // 12
  };

  /*
       returns c if printable, else returns a whitespace
    */
  char byte2char(byte c);
  void PrintPacket(const usb_packet_t *x);

  usb_packet_t *data_buffer_ = NULL;
  void get_buffer();

  void receive_buffer();

  bool write_get_length_ = false;
  uint32_t write_length_ = 0;
  void write(const char *data, int len);

  void write8(uint8_t x);

  void write16(uint16_t x);

  void write32(uint32_t x);

  void write64(uint64_t x);

  void writestring(const char *str);

  void WriteDescriptor();

  void WriteStorageIDs();

  void GetStorageInfo(uint32_t storage);

  uint32_t GetNumObjects(uint32_t storage, uint32_t parent);

  uint32_t tempNum = 0;
  void GetObjectHandles(uint32_t storage,
                        uint32_t parent);
#define objform MTP_FORMAT_TEXT
  void GetObjectInfo(uint32_t handle);

  // void GetObject(uint32_t object_id);

  void GetThumb(uint32_t object_id);
  void printAFile(AFile *file);

  int getAmigaFile(uint32_t handle, usb_packet_t *receive_buffer);

  int getAdf(usb_packet_t *receive_buffer);

  inline MTPContainer *contains(usb_packet_t *receive_buffer)
  {
    return (MTPContainer *)(receive_buffer->buf);
  }
#define CONTAINER contains(receive_buffer)

#define TRANSMIT(FUN)                                  \
  do                                                   \
  {                                                    \
    write_length_ = 0;                                 \
    write_get_length_ = true;                          \
    FUN;                                               \
    write_get_length_ = false;                         \
    MTPHeader header;                                  \
    header.len = write_length_ + 12;                   \
    header.type = 2;                                   \
    header.op = CONTAINER->op;                         \
    header.transaction_id = CONTAINER->transaction_id; \
    write((char *)&header, sizeof(header));            \
    FUN;                                               \
    get_buffer();                                      \
    usb_tx(MTP_TX_ENDPOINT, data_buffer_);             \
    data_buffer_ = NULL;                               \
  } while (0)

  void read(char *data, uint32_t size);

  uint32_t ReadMTPHeader();

  uint8_t read8();
  uint16_t read16();
  uint32_t read32();

  void readstring(char *buffer);

  void read_until_short_packet();
  uint32_t usb_queue_byte_count(const usb_packet_t *p);
  AFile *sendObjectFile;

  uint32_t SendObjectInfo(uint32_t storage, uint32_t parent);

  // void SendObject();

  int SendAFile();

  uint32_t SendADFInfo(uint32_t storage, uint32_t parent);

  int SendADF();

  void GetDevicePropValue(uint32_t prop);

  void GetDevicePropDesc(uint32_t prop);

  void GetObjectPropSupp(uint32_t prop);

  void GetObjectPropDesc(uint32_t prop);

  void GetObjectPropValue(uint32_t prop);

  void checkEvent();

  uint32_t handle2block(uint32_t handle);

  uint32_t block2handle(uint32_t block);

  uint32_t eventID = 1;
  bool sendADF = false;

public:
  void sendEvent(int event, uint32_t param);

  /* Main loop of MTP Responder */
  int sendState = 0;
  void loop();
};
#endif /* MTPADF_H */
