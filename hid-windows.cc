/** @file hid-windows.cc

   written by Marc Singer
   2 Sep 2015

   Copyright (C) 2015 Marc Singer

   -----------
   DESCRIPTION
   -----------

   Windows implementation of our HID interface.

   NOTES
   =====

   o An embarrassment.  Microsoft should be ashamed to have released a
     product that offers such a ridiculously complex interface for
     enumerating attached USB HID devices.

x86_64-w64-mingw32-g++ -o hid hid-windows.cc main.cc -std=c++11 -lhid -lsetupapi -static -static-libgcc -static-libstdc++
*/

//#include <unistd.h>
//#include <stdio.h>
//#include <stdlib.h>

#include "hid.h"
#include <string.h>
#include <functional>

extern "C" {
#include <windows.h>
#include <initguid.h>
#include <ntdef.h>
#include <setupapi.h>
#include <winioctl.h>
#include <hidsdi.h>
#include <hidclass.h>
#include <dbt.h>
}

#if 0
extern "C" {
 BOOL WINAPI HidD_SetOutputReport(HANDLE, PVOID, ULONG);
 BOOL WINAPI HidD_SetFeature (HANDLE, PVOID, ULONG);
 BOOL WINAPI HidD_GetInputReport(HANDLE, PVOID, ULONG);
 BOOL WINAPI HidD_GetFeature (HANDLE, PVOID, ULONG);
}
#endif

namespace HID {
  struct DeviceImpl {
    HANDLE h_;
    ~DeviceImpl () {
      if (h_) {
        CloseHandle (h_);
        h_ = 0; } }
  };

  Device::Device () {
    impl_ = new DeviceImpl; }

  Device::~Device () {
    if (impl_) {
      delete impl_;
      impl_ = nullptr;
    }
  }
}

namespace {

  void bzero (void* pv, size_t cb) {
    memset (pv, 0, cb); }

  void print_error (DWORD dw) {
    LPVOID lpMsgBuf;

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );
    printf ("error: %s\n", lpMsgBuf); }

  HANDLE open_path (const char* path, bool enum_only) {
    return CreateFile (path,
                       enum_only ? 0 : GENERIC_READ | GENERIC_WRITE,
                       FILE_SHARE_READ
                       | (enum_only ? FILE_SHARE_WRITE : 0),
                       nullptr,
                       OPEN_EXISTING,
                       0, //FILE_FLAG_OVERLAPPED,
                       0); }

  /** Convert UCS-2LE, as given by USB devices, to UTF-8.  The input
      string does not need to be null terminated.  The output will
      contain a NULL. */
  std::string ucs2toutf8 (WCHAR* str, int c)
  {
    char sz[256];
    WideCharToMultiByte(CP_UTF8, 0, str, c, sz, sizeof (sz), NULL, NULL);
    return std::string (sz);
  }

  std::string serial (HANDLE h)
  {
    auto constexpr C_MAX = 128;
    WCHAR string[C_MAX];
    if (HidD_GetSerialNumberString (h, string, sizeof (string)))
      return ucs2toutf8 (string, sizeof (string));

    return std::string ();
  }

  std::string manufacturer (HANDLE h)
  {
    auto constexpr C_MAX = 128;
    WCHAR string[C_MAX];
    if (HidD_GetManufacturerString (h, string, sizeof (string)))
      return ucs2toutf8 (string, sizeof (string));

    return std::string ();
  }

  std::string product (HANDLE h)
  {
    auto constexpr C_MAX = 128;
    WCHAR string[C_MAX];
    if (HidD_GetProductString (h, string, sizeof (string)))
      return ucs2toutf8 (string, sizeof (string));

    return std::string ();
  }


  struct Handler {
    bool failed_;
    GUID guid_;
    HWND hwndNotify_;
    HANDLE hNotify_;

    Handler () {
      HidD_GetHidGuid (&guid_); }

    /** Window handler that accepts USB notifications from Windows.
        We only use these events to invoke functions to update the
        internal state of the handler.  Windows doesn't provide any
        other way to be notified of changes in USB device states. */
    static LRESULT CALLBACK wndproc (HWND hwnd, UINT msg,
                              WPARAM wParam, LPARAM lParam) {
      auto p = reinterpret_cast<Handler*>
        (GetWindowLongPtr (hwnd, GWLP_USERDATA));
      return p ? p->wndprocx (hwnd, msg, wParam, lParam)
        : DefWindowProc (hwnd, msg, wParam, lParam); }
    LRESULT wndprocx (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
#if 0
      switch (msg) {
      case WM_DEVICECHANGE:
        switch (wParam) {
        case DBT_DEVNODES_CHANGED:     update (); break;
        case DBT_DEVICEREMOVECOMPLETE: update (); break;
        case DBT_DEVICEARRIVAL:			  break;
        default:
          break;
        }
        break;

      case WM_POWERBROADCAST:
        break;
      }
#endif
      return DefWindowProc (hwnd, msg, wParam, lParam); }

    bool init () {
      return true;

      printf ("init (%d)\n", failed_);

      if (failed_)
        return false;

      static constexpr char szTitle[] = "hid-window";
      static constexpr char szClass[] = "hid-window-class";

      WNDCLASSEX wc {
        sizeof (WNDCLASSEX),
          0,                    // style
          wndproc,              // WindowProc
          0, sizeof (this),     // class extra, window extra
          GetModuleHandle (nullptr),
          nullptr, nullptr,   // hIcon, hCursor
          nullptr, nullptr,   // hbrBackground, szMenu
          szClass,            // szClass
          };

      printf ("init: register class\n");

      if (!RegisterClassEx (&wc)) {
        failed_ = true;
        return false;
      }

      printf ("init: create window\n");

      hwndNotify_ = CreateWindowEx (0, szClass, szTitle,
                                    WS_OVERLAPPEDWINDOW,
                                    CW_USEDEFAULT, CW_USEDEFAULT, 0, 0,
                                    nullptr, nullptr, 0, nullptr);

      if (!hwndNotify_) {
        failed_ = true;
        return false; }

      SetWindowLongPtr (hwndNotify_, GWLP_USERDATA, (LONG_PTR) this);

      DEV_BROADCAST_DEVICEINTERFACE_A filter = {
        sizeof (filter),
        DBT_DEVTYP_DEVICEINTERFACE,
        0,
        GUID_DEVINTERFACE_HID,
        { 0 }
      };

      printf ("init: register notify\n");

      hNotify_ = RegisterDeviceNotificationA (hwndNotify_, &filter,
                                              DEVICE_NOTIFY_WINDOW_HANDLE);
      printf ("hNotify %x\n", hNotify_);

      if (!hNotify_) {
        failed_ = true;
        return false;
      }
      printf ("init: success\n");
      return true;
    }

    void service () {
      if (!init ())
        return;

      int tries = 10;
      while (tries--) {
        MSG msg;
        if (PeekMessage (&msg, hwndNotify_, 0, 0, PM_REMOVE)) {
          TranslateMessage (&msg);
          DispatchMessage (&msg);
        }
        else
          break;
      } }


    void enumerate (std::function<bool (HDEVINFO hDevInfo,
                                        const HID::DeviceInfo& device_info)> f) {
      if (!init ())
        return;

      HDEVINFO hDevInfo
        = SetupDiGetClassDevs (&guid_, NULL, NULL,
                               DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);

      for (int i = 0; !!hDevInfo; ++i) {
        // Get the interface @i
        SP_DEVICE_INTERFACE_DATA diData = { sizeof (diData) };
        if (!SetupDiEnumDeviceInterfaces (hDevInfo, 0, &guid_, i, &diData))
          break;

        // Get the interface path
        DWORD cb;
        SetupDiGetDeviceInterfaceDetail (hDevInfo, &diData,
                                         nullptr, 0, &cb, nullptr);
        if (cb == 0)
          continue;

        char rgb[cb];
        auto pdiDetail
          = reinterpret_cast<PSP_INTERFACE_DEVICE_DETAIL_DATA> (rgb);
        pdiDetail->cbSize = sizeof (SP_DEVICE_INTERFACE_DETAIL_DATA);
        if (!SetupDiGetDeviceInterfaceDetail (hDevInfo, &diData,
                                              pdiDetail, cb,
                                              nullptr, nullptr)) {
          print_error (GetLastError ());
          continue;
        }

#if 0
        // Probably redundant check for this device being HIDClass.
        // For now, we ignore the data, but we might want to add this
        // filter later.
        for (int i = 0; ;++i) {
          SP_DEVINFO_DATA dinfData;
          dinfData.cbSize = sizeof (dinfData);
          if (!SetupDiEnumDeviceInfo (hDevInfo, i, &dinfData))
            break;
          char sz[64];
          if (!SetupDiGetDeviceRegistryProperty (hDevInfo, &dinfData,
                                                 SPDRP_CLASS, nullptr,
                                                 (uint8_t*) sz, sizeof (sz),
                                                 nullptr))
            continue;
//          printf ("registry %s\n", sz);
        }
#endif

        HANDLE h = open_path (pdiDetail->DevicePath, true);
        if (h == INVALID_HANDLE_VALUE)
          continue;

        HIDD_ATTRIBUTES attr;
        HidD_GetAttributes (h, &attr);

        PHIDP_PREPARSED_DATA prepdata;
        HIDP_CAPS caps;
        if (HidD_GetPreparsedData (h, &prepdata)) {
          if (HidP_GetCaps(prepdata, &caps) != HIDP_STATUS_SUCCESS)
            bzero (&caps, sizeof (caps));
          HidD_FreePreparsedData (prepdata);
        }

        HID::DeviceInfo device_info {
          attr.VendorID,
            attr.ProductID,
            std::string (pdiDetail->DevicePath),
            serial (h),
            attr.VersionNumber,
            manufacturer (h),
            product (h),
            caps.UsagePage,
            caps.Usage
            };

        CloseHandle (h);

        if (!f (hDevInfo, device_info))
          break;
      }
      if (hDevInfo != INVALID_HANDLE_VALUE)
        SetupDiDestroyDeviceInfoList (hDevInfo);
    }

#if 0
    void update () {
      enumerate ([] (HDEVINFO hDevInfo,
                     const HID::DeviceInfo& device_info) {
                   printf ("path: %s\n", device_info.path_.c_str ());
                   printf ("  vid %04x  pid %04x\n",
                           device_info.vid_,
                           device_info.pid_);
                   return true;
                 });
    }
#endif

    std::vector<HID::DeviceInfo*>* enumerate (uint16_t vid, uint16_t pid) {
      if (!init ())
        return nullptr;

      auto devices = new std::vector<HID::DeviceInfo*>;

      enumerate ([&] (HDEVINFO hDevInfo,
                      const HID::DeviceInfo& device_info){
                   if (false
                       // Discard devices that don't match selection criteria
                       || (vid && vid != device_info.vid_)
                       || (pid && pid != device_info.pid_)
                       // Discard devices without VID/PID
                       || (device_info.vid_ == 0 && device_info.pid_ == 0)
                       )
                     return true;
                   devices->push_back (new HID::DeviceInfo (device_info));
                   return true;
                 });

      return devices;
    }

    HID::Device* open (uint16_t vid, uint16_t pid, const std::string& serial) {
      return nullptr; }

    HID::Device* open (const std::string& path) {
      HANDLE h = open_path (path.c_str (), false);
      if (h == INVALID_HANDLE_VALUE)
        return nullptr;

      auto device = new HID::Device;
      device->impl_->h_ = h;
      return device; }

  } handler$;



}

namespace HID {
  bool init () { return handler$.init (); }
  void release () {}

  std::vector<DeviceInfo*>* enumerate (uint16_t vid, uint16_t pid) {
    return handler$.enumerate (vid, pid); }

  Device* open (uint16_t vid, uint16_t pid, const std::string& serial) {
    return handler$.open (vid, pid, serial); }
  Device* open (const std::string& path) {
    return handler$.open (path); }

  int write (Device*, uint8_t report, const char* rgb, size_t cb);
  int write (Device*, const char* rgb, size_t cb);

  int read (Device*, char* rgb, size_t cb);

  void release (std::vector<DeviceInfo*>*);
  void release (Device*);

  void service () {
    handler$.service (); }

}
