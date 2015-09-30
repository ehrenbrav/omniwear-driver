/** @file main.cc

   written by Marc Singer
   24 Aug 2015

   Copyright (C) 2015 Marc Singer

   -----------
   DESCRIPTION
   -----------

   g++ -o hid main.cc -lhidapi -std=c++11
   g++ -o hid main.c hid-windows.cc -std=c++11

*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
//#include <hidapi/hidapi.h>
#include "hid.h"
#include <array>
#include <unistd.h>

void service () {
  int c = 1000;
  while (c-- && HID::service ())
    ;
}

enum {
  op_null  = 0,
  op_set   = 1,
  op_reset = 2,
};

void usage () {

  printf (
          "usage: omni [OPTIONS]\n"
          "\n"
          "  MOTOR DUTY      - Set DUTY (0-100) for MOTOR (0-13)\n"
          "  -r              - Reset all motors\n");
  exit (0);
}

int main (int argc, const char** argv)
{
  int motor = -1;
  int duty = 0;
  int op = 0;

  for (--argc, ++argv; argc > 0; --argc, ++argv) {
    if (strcasecmp (argv[0], "-r") == 0) {
      op = op_reset;
      break;
    }
    int value = strtoul (argv[0], nullptr, 0);

    if (motor == -1) {
      motor = value;
      op = op_set;
      continue;
    }

    if (op == op_set && motor != -1)
      duty = value;
  }

  if (op == op_null)
    usage ();

  if (op == op_set && (motor < 0 || motor > 13 || duty < 0 || duty > 100))
    usage ();

//  auto result = hid_init ();
//  printf ("# hid_init %d\n", result);

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

  if (0) {
    auto devices = HID::enumerate ();

    printf ("devices[%d]\n", devices ? int (devices->size ()) : 0);
    if (devices)
      for (auto dev : *devices) {
        printf ("dev %s %s %s\n",
                dev->path_.c_str (),
                dev->manufacturer_.c_str (),
                dev->product_.c_str ());
      }
  }


//  auto d = HID::open ("USB_05ac_0262_1d182000");
//  auto d = HID::open ("USB_03eb_2402_14110000");
  auto d = HID::open (0x3eb, 0x02402);
  if (d == 0) {
    printf ("unable to find omniwear device\n");
    exit (1);
  }
//  printf ("d %p\n", d);

  char frequency = 255;

  if (0){
    std::string s = "Now we have come to the end of the line.";
    auto result = HID::write (d, s.c_str (), s.length ());
//    printf ("write %d\n", result);
  }
  {
    std::string s = "\x02\x01\x02     ";
    auto result = HID::write (d, s.c_str (), s.length ());
//    printf ("write %d\n", result);
  }
  {
    std::string s = "\x02\x02\x01     ";
    auto result = HID::write (d, s.c_str (), s.length ());
//printf ("write %d\n", result);
  }

  switch (op) {
  case op_reset:
    printf ("reset motors\n");
    {
      std::array<char,8> msg = { 0x1, 0x11 };
      auto result = HID::write (d, &msg[0], msg.size ());
//      printf ("write %d\n", result);
    }
    break;
  case op_set:
    printf ("set motor %d  duty %d%%\n", motor, duty);
    {
      std::array<char,8> msg = { 0x4, 0x10,
                                 char (motor), char (duty*255/100), frequency };
      auto result = HID::write (d, &msg[0], msg.size ());
//      printf ("write %d\n", result);
    }
    break;
  }


  exit (0);
}
