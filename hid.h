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

   NOTES
   =====

   o libhid and others.  While there are cross-platform libraries for
     managing the HID interface; they are in C; we've found errors in
     their constructions; we want type safety as much as we can; we
     don't want the 'library' to spawn threads; we will offer a C
     interface for compatibity on top of the C++ interface.  Also, we
     don't want another DLL.  This code should be linked into the
     application.

   o UTF-8.  To the best of our ability, we will offer UTF-8 strings
     instead of wide-character strings.  Doing so should smooth out
     the differenced between the platforms.  We *might* need to
     back-pedal on this if Windows turns out to be problematic.

   o Threadless.  To the best of our ability, this library (of sorts)
     will be threadless.  Each platform specific interface sprouts a
     service() call that the user can either invoke by hand or place
     in a thread to perform operations that the library requires.

*/

#if !defined (HID_H_INCLUDED)
#    define   HID_H_INCLUDED

/* ----- Includes */

#include <stdint.h>

#if defined (__cplusplus)
# include <string>
# include <vector>
#endif

/* ----- Macros */

/* ----- Types */

#if defined (__cplusplus)

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

#endif

/* ----- Globals */

/* ----- Prototypes */



#endif  /* HID_H_INCLUDED */
