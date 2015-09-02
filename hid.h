/** @file hid.h

   written by Marc Singer
   24 Aug 2015

   Copyright (C) 2015 Marc Singer

   -----------
   DESCRIPTION
   -----------

   HID interface class.  The interface is OS agnostic and should be
   appropriate for any platform.  Platform specific details are hidden
   from the caller.

*/

#if !defined (HID_H_INCLUDED)
#    define   HID_H_INCLUDED

/* ----- Includes */

#include <string>
#include <vector>

/* ----- Macros */

/* ----- Types */

namespace HID {
  bool init ();
  void release ();

  struct DeviceInfo {
    uint16_t vid_;
    uint16_t pid_;
    std::string path_;
    std::string serial_;
    uint16_t version_;
    std::string manufacturer_;
    std::string product_;
    uint16_t usage_page_ = 0;
    uint16_t usage_ = 0;
    int interface_ = -1;

    DeviceInfo (uint16_t vid, uint16_t pid, std::string path,
                std::string serial, uint16_t version,
                std::string manufacturer, std::string product,
                uint16_t usage_page, uint16_t usage)
    : vid_ (vid), pid_ (pid), path_ (path), serial_ (serial),
      version_ (version), manufacturer_ (manufacturer),
      product_ (product), usage_page_ (usage_page), usage_ (usage) {}
  };

  struct DeviceImpl;
  struct Device {
    Device ();
    ~Device ();
    DeviceImpl* impl_;
  };

  std::vector<DeviceInfo*>* enumerate (uint16_t vid = 0, uint16_t pid = 0);
  Device* open (uint16_t vid, uint16_t pid, const std::string& serial);
  Device* open (const std::string& path);

  int write (Device*, uint8_t report, const char* rgb, size_t cb);
  int write (Device*, const char* rgb, size_t cb);

  int read (Device*, char* rgb, size_t cb);

  void release (std::vector<DeviceInfo*>*);
  void release (Device*);

  void service ();
}

/* ----- Globals */

/* ----- Prototypes */



#endif  /* HID_H_INCLUDED */
