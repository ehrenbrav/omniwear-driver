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
#include <iostream>
#include <iomanip>
#include <sstream>



namespace {
  bool failed_;
  bool init_;

  static constexpr auto MS_TIMEOUT = 10000;

  namespace USB {
    using Device = struct usb_device;
    using Handle = struct usb_dev_handle;
  }

  std::string path (USB::Device* d) {
    std::ostringstream o;
    o << std::setbase (16)
      << std::setfill ('0') << std::setw (4) << d->descriptor.idVendor
      << ":"
      << std::setfill ('0') << std::setw (4) << d->descriptor.idProduct
      << "@" << d->bus->dirname
      << "/" << d->filename;
    return o.str (); }

  std::string lookup_string (USB::Handle* h, int index) {
    auto constexpr C_MAX = 128;
    char sz[C_MAX];
    auto result = usb_get_string_simple (h, index, sz, sizeof (sz));
    return result > 0 ? std::string (sz) : std::string ();
  }
}


namespace HID {
  struct Device::Impl {
    usb_dev_handle* device_handle_ = 0;
    ~Impl () {
      if (device_handle_)
        ::usb_close (device_handle_);
    }
  };

  Device::Device () {
    impl_ = std::make_unique<Device::Impl> (); }
  Device::~Device () {}         // Required for unique_ptr Impl

  bool init () {
    if (failed_)
      return false;
    if (!init_) {
      ::usb_init ();
      init_ = true;
    }
    return init_;
  }

  void enumerate (std::function<bool (USB::Device*, HID::DeviceInfo&)> f) {
    if (!init ())
      return;

    ::usb_find_busses ();
    ::usb_find_devices ();

    for (auto bus = usb_get_busses (); bus; bus = bus->next)
      for (auto device = bus->devices; device; device = device->next) {
        auto h = usb_open (device);

        // Build the information structure
        HID::DeviceInfo device_info {
          device->descriptor.idVendor,
            device->descriptor.idProduct,
            path (device),
            lookup_string (h, device->descriptor.iSerialNumber),
            0, // attr.VersionNumber,
            lookup_string (h, device->descriptor.iManufacturer),
            lookup_string (h, device->descriptor.iProduct),
//            caps.UsagePage,
//            caps.Usage
            };
        usb_close (h);
        f (device, device_info);
      }
  }

  HID::DevicesP enumerate (uint16_t vid, uint16_t pid) {
    if (!init)
      return nullptr;

    auto devices
      = std::make_unique <std::vector<std::unique_ptr<HID::DeviceInfo>>>();

    enumerate ([&] (USB::Device* usb_device, HID::DeviceInfo& device_info) {
                   printf (" vid %04x pid %04x\n",
                           device_info.vid_, device_info.pid_);
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
                   (std::make_unique<HID::DeviceInfo> (device_info));
                 return true;
               });

    return devices;

  }

  DeviceP open (uint16_t vid, uint16_t pid, const std::string& serial) {
    if (!init ())
      return nullptr;

    HID::DeviceP device = nullptr;

    enumerate ([&] (USB::Device* usb_device,
                    const HID::DeviceInfo& device_info){
                 if (false
                     // Discard devices that don't match selection criteria
                     || (vid && vid != device_info.vid_)
                     || (pid && pid != device_info.pid_)
                     // Discard devices without VID/PID
                     || (device_info.vid_ == 0 && device_info.pid_ == 0)
                     )
                   return true;

                 auto h = usb_open (usb_device);
                 device = std::make_unique<HID::Device> ();
                 device->impl_->device_handle_ = h;
                 return false;
               });
    return device; }

  DeviceP open (const std::string& path) {
    if (!init ())
      return nullptr;

    DeviceP device = nullptr;

    enumerate ([&] (USB::Device* usb_device, const DeviceInfo& device_info) {
        if (path.compare (device_info.path_) == 0) {
          auto h = usb_open (usb_device);
          device = std::make_unique<HID::Device>();
          device->impl_->device_handle_ = h;
          return false;
        }
        return true; });

    return device;
  }

  int write (const Device* d, uint8_t report, const char* rgb, size_t cb) {
    return write (d, rgb, cb); }

  int write (const Device* d, const char* rgbPayload, size_t cbPayload) {
    char rgb[32];
    size_t cb = 1 + cbPayload;
    rgb[0] = 1;
    if (cbPayload > sizeof (rgb) - 1)
      cbPayload = sizeof (rgb) - 1;
    memcpy (rgb + 1, rgbPayload, cbPayload);
    auto result = usb_control_msg (d->impl_->device_handle_,
                                   USB_ENDPOINT_OUT | USB_TYPE_CLASS
                                   | USB_RECIP_INTERFACE,
                                   0x09, 0, 1, rgb, cb, MS_TIMEOUT);
    return result; }

}
