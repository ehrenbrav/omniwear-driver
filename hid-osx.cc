/** @file hid-osx.cc

   written by Marc Singer
   1 Sep 2015

   Copyright (C) 2015 Marc Singer

   -----------
   DESCRIPTION
   -----------

   OSX implementation of our HID interface.

   NOTES
   =====

   o Opening a device.  The way we open devices is a little clumsy.
     The identifier for a device is a tuple VID/PID/SERIAL.  The
     application can either fetch this string from an enumeration, or
     it can make the open request with these fields.

   o UTF8.  All strings are converted to UTF8, even the USB ones that
     are UTF16 or something like it.

*/

//#include "standard.h"
#include "hid.h"

// From featureful implementation
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/hid/IOHIDLib.h>

// From hidapi
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <CoreFoundation/CoreFoundation.h>

#include <locale.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <functional>

namespace OSXHID {

  template<typename T_, CFNumberType type>
  T_ number_prop (IOHIDDeviceRef dev, CFStringRef key) {
    T_ result = {};
    auto ref = IOHIDDeviceGetProperty (dev, key);
    if (ref && CFGetTypeID (ref) == CFNumberGetTypeID ())
      CFNumberGetValue ((CFNumberRef) ref, type, &result);
    return result; }

  std::string string_prop (IOHIDDeviceRef dev, CFStringRef prop) {
    std::string result;
    auto ref = IOHIDDeviceGetProperty (dev, prop);
    if (ref && CFGetTypeID (ref) == CFStringGetTypeID ()) {
      CFStringRef str = CFStringRef (ref);

      if (str) {
        auto len = CFStringGetLength (str);
        result.resize (len*4 + 1, 0);

        CFRange range = {0, len};
        CFIndex copied;
        CFStringGetBytes (str, range,
                          kCFStringEncodingUTF8,
                          (char) '?',
                          false,
                          (uint8_t*) result.c_str (),
                          result.capacity (),
                          &copied);
        result.resize (copied);
      }
    }
    return result; }

  uint16_t vendor_id (IOHIDDeviceRef dev) {
    return number_prop<uint16_t, kCFNumberSInt16Type>
      (dev, CFSTR (kIOHIDVendorIDKey)); }

  uint16_t product_id (IOHIDDeviceRef dev) {
    return number_prop<uint16_t, kCFNumberSInt16Type>
      (dev, CFSTR (kIOHIDProductIDKey)); }

  int32_t location_id (IOHIDDeviceRef dev) {
    return number_prop<uint32_t, kCFNumberSInt32Type>
      (dev, CFSTR (kIOHIDLocationIDKey)); }

  uint16_t max_report_length (IOHIDDeviceRef dev) {
    return number_prop<uint16_t, kCFNumberSInt16Type>
      (dev, CFSTR (kIOHIDMaxInputReportSizeKey)); }

  uint16_t usage_page (IOHIDDeviceRef dev) {
    return number_prop<uint16_t, kCFNumberSInt16Type>
      (dev, CFSTR (kIOHIDPrimaryUsagePageKey)); }

  uint16_t usage (IOHIDDeviceRef dev) {
    return number_prop<uint16_t, kCFNumberSInt16Type>
      (dev, CFSTR (kIOHIDPrimaryUsageKey)); }

  uint16_t version (IOHIDDeviceRef dev) {
    return number_prop<uint16_t, kCFNumberSInt16Type>
      (dev, CFSTR (kIOHIDVersionNumberKey)); }

  std::string serial (IOHIDDeviceRef dev) {
    return string_prop (dev, CFSTR (kIOHIDSerialNumberKey)); }

  std::string manufacturer (IOHIDDeviceRef dev) {
    return string_prop (dev, CFSTR (kIOHIDManufacturerKey)); }

  std::string product (IOHIDDeviceRef dev) {
    return string_prop (dev, CFSTR (kIOHIDProductKey)); }

  std::string path (IOHIDDeviceRef dev) {
    std::string transport = string_prop (dev, CFSTR (kIOHIDTransportKey));

    if (!transport.size ())
      return std::string ();

    auto location = location_id (dev);
    auto vid      = vendor_id (dev);
    auto pid      = product_id (dev);

    std::ostringstream o;
    o << transport << std::setbase (16)
      << "_" << std::setfill ('0') << std::setw (4) << vid
      << "_" << std::setfill ('0') << std::setw (4) << pid
      << "_" << std::setw(8) << location;
    auto result = o.str ();
    return o.str ();
  }

}

namespace {
  IOHIDManagerRef hid_manager;
}

namespace HID {

  bool init () {
    if (!hid_manager) {
      hid_manager = IOHIDManagerCreate (kCFAllocatorDefault,
                                        kIOHIDOptionsTypeNone);
      if (hid_manager) {
        IOHIDManagerSetDeviceMatching (hid_manager, NULL);
        IOHIDManagerScheduleWithRunLoop (hid_manager, CFRunLoopGetCurrent (),
                                         kCFRunLoopDefaultMode);
      }
    }
    return !!hid_manager; }

  void release () {
    if (hid_manager) {
      IOHIDManagerClose (hid_manager, kIOHIDOptionsTypeNone);
      CFRelease (hid_manager);
      hid_manager = nullptr; } }

  void enumerate (std::function<bool (IOHIDDeviceRef, const DeviceInfo&)> f) {
    if (!init ())
      return;

    service ();  // *** FIXME: should we do this or should we let the
                 // *** application handle it?

    IOHIDManagerSetDeviceMatching (hid_manager, NULL);
    CFSetRef device_set = IOHIDManagerCopyDevices (hid_manager);

    auto num_devices = CFSetGetCount (device_set);
    IOHIDDeviceRef device_refs[num_devices];
    CFSetGetValues (device_set, (const void **) device_refs);

    for (auto dev : device_refs) {
      if (!dev)
        continue;

      DeviceInfo device_info {
        OSXHID::vendor_id (dev),
          OSXHID::product_id (dev),
          OSXHID::path (dev), OSXHID::serial (dev),
          OSXHID::version (dev),
          OSXHID::manufacturer (dev),
          OSXHID::product (dev),
          OSXHID::usage_page (dev),
          OSXHID::usage (dev) };

      if (!f (dev, device_info))
        break;
    }

    CFRelease (device_set);
  }

  std::vector<DeviceInfo*>* enumerate (uint16_t vid, uint16_t pid) {
    if (!init ())
      return nullptr;

    auto devices = new std::vector<DeviceInfo*>;

    enumerate ([&] (IOHIDDeviceRef os_dev, const DeviceInfo& device_info) {
        if (false
          	// Discard devices that don't match selection criteria
            || (vid && vid != device_info.vid_)
            || (pid && pid != device_info.pid_)
          	// Discard devices without VID/PID
            || (device_info.vid_ == 0 && device_info.pid_ == 0)
            )
          return true;
        devices->push_back (new DeviceInfo (device_info));
        return true;
      });

    return devices;
  }

  Device* open (uint16_t vid, uint16_t pid, const std::string& serial) {
    if (!init ())
      return nullptr;

    Device* result = nullptr;

    enumerate ([&] (IOHIDDeviceRef os_dev, const DeviceInfo& device_info) {
        if (true
            && (!vid || vid == device_info.vid_)
            && (!pid || pid == device_info.pid_)
            && (!serial.length () || serial.compare (device_info.serial_))
            && (IOHIDDeviceOpen(os_dev, kIOHIDOptionsTypeSeizeDevice)
                == kIOReturnSuccess)) {
          CFRetain (os_dev);
          result = new Device (os_dev);
        }
        return !result;
      });

    return result; }

  Device* open (const std::string& path) {
    if (!init ())
      return nullptr;

    Device* result = nullptr;
    enumerate ([&] (IOHIDDeviceRef os_dev, const DeviceInfo& device_info) {
        printf ("enum %s\n", device_info.path_.c_str ());
        if (path.compare (device_info.path_) == 0
            && (IOHIDDeviceOpen(os_dev, kIOHIDOptionsTypeSeizeDevice)
                == kIOReturnSuccess)) {
          CFRetain (os_dev);
          result = new Device (os_dev);
        }
        return !result;
      });

    return result;
  }

  int write (uint8_t report, const char* rgb, size_t cb) { return 0; }
  int write (const char* rgb, size_t cb) { return 0; }

  int read (char* rgb, size_t cb) { return 0; }

  void release (std::vector<Device*>* devices) {
    if (devices) {
      for (auto dev : *devices)
        delete dev;
      delete devices;
    }
  }

  void release (Device* device) {
    delete device; }

  void service () {
    if (!init ())
      return;

    SInt32 res;
    // *** FIXME: this should terminate even if there are events, no?
    do {
      res = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.001, FALSE);
    } while(res != kCFRunLoopRunFinished && res != kCFRunLoopRunTimedOut);
  }

}
