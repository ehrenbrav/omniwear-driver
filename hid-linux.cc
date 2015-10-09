/** @file hid-linux.cc

   written by Marc Singer
   3 Oct 2015

   Copyright (C) 2015 Marc Singer

   -----------
   DESCRIPTION
   -----------

   Linux implementation of our HID interface.

*/

#include "hid.h"
#include <usb.h>

#include <string.h>
#include <functional>


namespace {
  bool failed_;
  bool init_;
}


namespace HID {

  bool init () {
    if (failed_)
      return false;
    if (init_)
      return true;

    ::usb_init ();
    init_ = true;
  }

  void enumerate (std::functional<bool (::usb_device*)> f) {
    if (!init ())
      return;

    ::usb_find_busses ();
    ::usb_find_devices ();

    for (auto bus = usb_get_busses (); bus; bus = bus->next)
      for (auto device = bus->devices; device; device = device->next)
        f (device);
  }

  HID::DevicesP enumerate (uint16_t vid, uint16_t pid) {
    if (!init)
      return nullptr;

    auto devices
      = std::make_unique <std::vector<std::unique_ptr<HID::DeviceInfo>>>();

    enumerate ([&] (::usb_device* usb_device) {
//                   printf (" vid %x pid %x\n",
//                           device_info.vid_, device_info.pid_);
                 if (false
                     // Discard devices that don't match selection criteria
                     || (vid && vid != usb_device->descriptor.idVendor)
                     || (pid && pid != usb_device->descriptor.idProduct)
                     // Discard devices without VID/PID
                     || (usb_device->descriptor.idVendor == 0
                         && usb_device->descriptor.idProduct == 0)
                     )
                   return true;
                 devices->push_back
                   (std::make_unique<::usb_device> (usb_device));
                 return true;
               });

    return devices;

  }
