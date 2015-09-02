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

*/

//#include <unistd.h>
//#include <stdio.h>
//#include <stdlib.h>

#include "hid.h"

#include <windows.h>
#include <ntdef.h>
#include <setupapi.h>
#include <winioctl.h>
#include <hidsdi.h>
#include <hidclass.h>
#include <dbt.h>

#if 0
extern "C" {
 BOOL WINAPI HidD_SetOutputReport(HANDLE, PVOID, ULONG);
 BOOL WINAPI HidD_SetFeature (HANDLE, PVOID, ULONG);
 BOOL WINAPI HidD_GetInputReport(HANDLE, PVOID, ULONG);
 BOOL WINAPI HidD_GetFeature (HANDLE, PVOID, ULONG);
}
#endif

namespace {
  struct Handler {
    bool failed_;
    HWND hwndNotify_;
    HANDLE hNotify_;

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
      switch (msg) {
      case WM_DEVICECHANGE:
        switch (wParam) {
        case DBT_DEVNODES_CHANGED:     update_devices (); break;
        case DBT_DEVICEREMOVECOMPLETE: update_devices (); break;
        case DBT_DEVICEARRIVAL:				  break;
        default:
          break;
        }
        break;

      case WM_POWERBROADCAST:
        break;
      }
      return DefWindowProc (hwnd, msg, wParam, lParam); }

    bool init () {
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

      if (!RegisterClassEx (&wc)) {
        failed_ = true;
        return false;
      }

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

      hNotify_ = RegisterDeviceNotificationA (hwndNotify_, &filter,
                                              DEVICE_NOTIFY_WINDOW_HANDLE);
      if (!hNotify_) {
        failed_ = true;
        return false; }
    }

    void update_devices () {}

  } handler$;

}

namespace HID {
  struct DeviceImpl {
  };

  bool init () { return handler$.init (); }
  void release ();

  std::vector<DeviceInfo*>* enumerate (uint16_t vid, uint16_t pid) {
    return nullptr; }
  Device* open (uint16_t vid, uint16_t pid, const std::string& serial);
  Device* open (const std::string& path);

  int write (Device*, uint8_t report, const char* rgb, size_t cb);
  int write (Device*, const char* rgb, size_t cb);

  int read (Device*, char* rgb, size_t cb);

  void release (std::vector<DeviceInfo*>*);
  void release (Device*);

  void service ();
}
