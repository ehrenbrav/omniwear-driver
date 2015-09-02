/** @file main.cc

   written by Marc Singer
   24 Aug 2015

   Copyright (C) 2015 Marc Singer

   -----------
   DESCRIPTION
   -----------

   g++ -o hid main.cc -lhidapi -std=c++11

*/

#include <stdio.h>
#include <stdlib.h>
#include <hidapi/hidapi.h>
#include "hid.h"

int main (int argc, const char** argv)
{
  auto result = hid_init ();
  printf ("# hid_init %d\n", result);

#if 0
  {
    auto devices = hid_enumerate (0, 0);
    printf ("# hid_enumerate %p\n", devices);
    for (auto p = devices; p; p = p->next)
      printf ("# device %p %04x:%04x %ls %ls %s\n",
              p, p->vendor_id, p->product_id,
              p->manufacturer_string, p->product_string, p->path);
  }
#endif

  {
    auto devices = HID::enumerate ();

    for (auto dev : *devices) {
      printf ("dev %s %s %s\n",
              dev->path_.c_str (),
              dev->manufacturer_.c_str (),
              dev->product_.c_str ());
    }
  }


  auto d = HID::open ("USB_05ac_0262_1d182000");
  printf ("d %p\n", d);

  exit (0);
}
