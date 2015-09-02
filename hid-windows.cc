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
      return DefWindowProc (hwnd, msg, wParam, lParam); }

    bool init () {
      printf ("init\n");

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
        return false; }
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


    void update () {
      printf ("update\n");

      if (!init ())                    // Lazy initialization
        return;

      printf ("update: GetClassDevs\n");

      HDEVINFO hDevInfo
        = SetupDiGetClassDevs (&guid_, NULL, NULL,
                               DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);

      printf ("update: enumerating\n");

      for (DWORD index = 0; hDevInfo != INVALID_HANDLE_VALUE; ++index) {
        printf ("update: index %d\n", index);
        SP_DEVICE_INTERFACE_DATA diData;
        bzero (&diData, sizeof (diData));
        diData.cbSize = sizeof (diData);

        printf ("update: size di data\n");
        if (!SetupDiEnumDeviceInterfaces (hDevInfo, 0, &guid_, index, &diData))
          break;

        DWORD cb = 0;
        SetupDiGetDeviceInterfaceDetail (hDevInfo, &diData,
                                         nullptr, 0, &cb, nullptr);
        printf ("update: size di data %d\n", cb);
        if (cb == 0)
          continue;

        char rgb[cb];
        PSP_INTERFACE_DEVICE_DETAIL_DATA pdiDetail
          = reinterpret_cast<PSP_INTERFACE_DEVICE_DETAIL_DATA> (rgb);
        pdiDetail->cbSize = sizeof (SP_DEVICE_INTERFACE_DETAIL_DATA);
        if (!SetupDiGetDeviceInterfaceDetail (hDevInfo, &diData,
                                              pdiDetail, cb, &cb, nullptr)) {
          print_error (GetLastError ());
          continue;

        }
        printf ("path %s\n", pdiDetail->DevicePath);

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

        HANDLE h = open_path (pdiDetail->DevicePath, true);
        if (h == INVALID_HANDLE_VALUE)
          continue;

        HIDD_ATTRIBUTES attr;
        HidD_GetAttributes (h, &attr);

        printf ("vid %04x  pid %04x\n", attr.VendorID, attr.ProductID);

        CloseHandle (h);
      }
      if (hDevInfo != INVALID_HANDLE_VALUE)
        SetupDiDestroyDeviceInfoList (hDevInfo);
    }

  } handler$;

}

namespace HID {
  struct DeviceImpl {
  };

  bool init () { return handler$.init (); }
  void release () {}

  std::vector<DeviceInfo*>* enumerate (uint16_t vid, uint16_t pid) {
    handler$.update ();
    return nullptr; }
  Device* open (uint16_t vid, uint16_t pid, const std::string& serial) {
    return nullptr; }
  Device* open (const std::string& path) {
    return nullptr; }

  int write (Device*, uint8_t report, const char* rgb, size_t cb);
  int write (Device*, const char* rgb, size_t cb);

  int read (Device*, char* rgb, size_t cb);

  void release (std::vector<DeviceInfo*>*);
  void release (Device*);

  void service () {
    handler$.service (); }

}
