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
#include <libusb-1.0/libusb.h>

#include <string.h>
#include <functional>
#include <iostream>
#include <iomanip>
#include <sstream>


namespace {
  bool failed_;
  bool init_;

  libusb_context* ctx$;

  static constexpr auto MS_TIMEOUT = 10000;

  namespace USB {
    using Device = struct libusb_device;
    using Handle = struct libusb_device_handle;
  }

  std::string path (USB::Device* d) {
    libusb_device_descriptor descriptor;
    auto result = ::libusb_get_device_descriptor (d, &descriptor);
    uint8_t bus = ::libusb_get_bus_number (d);
    uint8_t ports[128];
    result = ::libusb_get_port_numbers (d, ports, sizeof (ports));
    std::ostringstream o;
    o << std::setbase (16)
      << std::setfill ('0') << std::setw (4) << descriptor.idVendor
      << ":"
      << std::setfill ('0') << std::setw (4) << descriptor.idProduct
      << "/" << int (bus);
      ;

    if (result > 0) {
      for (int i = 0; i < result; ++i)
        o << "/" << int (ports[i]);
    }
    return o.str ();
 }

  std::string lookup_string (USB::Handle* h, int index) {
    if (!h)
      return "";

    auto constexpr C_MAX = 128;
    char sz[C_MAX];
    auto result = libusb_get_string_descriptor_ascii (h, index,
                                                      (uint8_t*) sz,
                                                      sizeof (sz));
    return result > 0 ? std::string (sz) : std::string ();

  }
}


namespace HID {
  struct Device::Impl {
    libusb_device_handle* device_handle_ = 0;
    ~Impl () {
      if (device_handle_) {
        ::libusb_release_interface (device_handle_, 0);
        ::libusb_close (device_handle_);
      }
    }
  };

  Device::Device () {
    impl_ = std::make_unique<Device::Impl> (); }
  Device::~Device () {}         // Required for unique_ptr Impl

  bool init () {
    if (failed_)
      return false;
    if (!init_) {
      auto result = ::libusb_init (&ctx$);
      failed_ = result < 0;
//      if (!failed_)
//        ::libusb_set_debug (ctx$, 3);
      init_ = true;
    }
    return init_;
  }

  void enumerate (std::function<bool (USB::Device*,
                                      USB::Handle*,
                                      HID::DeviceInfo&)> f) {
    if (!init ())
      return;

    libusb_device** devices;
    auto count = ::libusb_get_device_list (ctx$, &devices);

    for (size_t i = 0; i < count; ++i) {
      auto device = devices[i];
      libusb_device_descriptor descriptor;
      auto result = ::libusb_get_device_descriptor (device, &descriptor);
      libusb_device_handle* h = nullptr;
      result = ::libusb_open (device, &h);
      // Build the information structure
      HID::DeviceInfo device_info {
        descriptor.idVendor,
          descriptor.idProduct,
          path (device),
          lookup_string (h, descriptor.iSerialNumber),
          0, // attr.VersionNumber,
          lookup_string (h, descriptor.iManufacturer),
          lookup_string (h, descriptor.iProduct),
          //            caps.UsagePage,
          //            caps.Usage
          };
      if (h)
        ::libusb_close (h);
      f (device, nullptr, device_info);
    }
    ::libusb_free_device_list (devices, 1);
  }

  HID::DevicesP enumerate (uint16_t vid, uint16_t pid) {
    if (!init)
      return nullptr;

    auto devices
      = std::make_unique <std::vector<std::unique_ptr<HID::DeviceInfo>>>();

    enumerate ([&] (USB::Device* usb_device, USB::Handle* usb_handle,
                    HID::DeviceInfo& device_info) {
//        printf (" vid %04x pid %04x\n",
//                device_info.vid_, device_info.pid_);
        libusb_device_descriptor descriptor;
        auto result = ::libusb_get_device_descriptor (usb_device,
                                                      &descriptor);
        if (false
            // Discard devices that don't match selection criteria
            || (vid && vid != descriptor.idVendor)
            || (pid && pid != descriptor.idProduct)
            // Discard devices without VID/PID
            || (descriptor.idVendor == 0
                && descriptor.idProduct == 0)
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

    enumerate ([&] (USB::Device* usb_device, USB::Handle* usb_handle,
                    const HID::DeviceInfo& device_info){
                 if (false
                     // Discard devices that don't match selection criteria
                     || (vid && vid != device_info.vid_)
                     || (pid && pid != device_info.pid_)
                     // Discard devices without VID/PID
                     || (device_info.vid_ == 0 && device_info.pid_ == 0)
                     )
                   return true;
                 // *** FIXME: verify that this works.
                 auto result = ::libusb_open (usb_device, &usb_handle);
                 if (result < 0 || usb_handle == nullptr)
                   return false;
//                 printf ("pid open %d\n", result);
//                 ::libusb_ref_device (usb_device);
                 result = ::libusb_detach_kernel_driver (usb_handle, 0);
//                 printf ("detach %d\n", result);
                 result = ::libusb_claim_interface (usb_handle, 0);
                 if (result < 0)
                   return false;
//                 printf ("claim %d\n", result);
                 device = std::make_unique<HID::Device> ();
                 device->impl_->device_handle_ = usb_handle;
                 return false;
               });
    return device; }

  DeviceP open (const std::string& path) {
    if (!init ())
      return nullptr;

    DeviceP device = nullptr;

    enumerate ([&] (USB::Device* usb_device, USB::Handle* usb_handle,
                    const DeviceInfo& device_info) {
        if (path.compare (device_info.path_) == 0) {
          // *** FIXME: verify that this works.
          auto result = ::libusb_open (usb_device, &usb_handle);
          if (result < 0 || usb_handle == nullptr)
            return false;
//          printf ("path open %d\n", result);
          //                 ::libusb_ref_device (usb_device);
          result = ::libusb_detach_kernel_driver (usb_handle, 0);
          result = ::libusb_claim_interface (usb_handle, 0);
          if (result < 0)
            return false;
//          printf ("claim %d\n", result);
//          ::libusb_ref_device (usb_device);
//          ::libusb_claim_interface (usb_handle, 0);
          device = std::make_unique<HID::Device> ();
          device->impl_->device_handle_ = usb_handle;
          return false;
        }
        return true; });

    return device;
  }

  int write (const Device* d, uint8_t report, const char* rgb, size_t cb) {
    return write (d, rgb, cb); }

  int write (const Device* d, const char* rgbPayload, size_t cbPayload) {
#if 1
    int cbWritten = 0;
    auto result = ::libusb_interrupt_transfer (d->impl_->device_handle_,
                                               2, (uint8_t*) rgbPayload,
                                               cbPayload,
                                               &cbWritten, MS_TIMEOUT);
//    printf ("write %d %d\n", result, cbWritten);
#else
    static const int CONTROL_REQUEST_TYPE_OUT
      = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS
      | LIBUSB_RECIPIENT_INTERFACE;
    static const int HID_SET_REPORT = 9;

    auto result = ::libusb_control_transfer (d->impl_->device_handle_,
                                             CONTROL_REQUEST_TYPE_OUT,
                                             HID_SET_REPORT,
                                             0x00,
                                             0,
                                             (uint8_t*) rgb, cb, MS_TIMEOUT);
    printf ("write %d\n", result);
#endif
    return result;
  }

}
