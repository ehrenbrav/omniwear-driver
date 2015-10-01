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
  op_reset,
  op_list,
  op_demo_0,
};

using ArgList = std::vector<std::string>;

void usage () {

  printf (
          "usage: omni [OPTIONS]\n"
          "\n"
          "  MOTOR DUTY      - Set DUTY (0-100) for MOTOR (0-13)\n"
          "  -r              - Reset all motors\n"
          "  -l              - List USB devices\n"
          "  -v              - Show program version\n"
          "  -d M..	     - Demo modes\n"
          "     0 T I	       Mode 0; run each motor in order for T seconds\n"
          "                            at I intensity\n"
          );
  exit (0);
}

HID::DeviceP open_cap () {
  auto d = HID::open (0x3eb, 0x2402);

  if (!d) {
    printf ("unable to find omniwear device\n");
    exit (1);
  }

  return std::move (d);
}

int send_reset_motors (HID::Device* d) {
  std::array<char,8> msg = { 0x1, 0x11 };
  return HID::write (d, &msg[0], msg.size ());
}

int send_config_motor (HID::Device* d, int motor, int duty) {
  std::array<char,8> msg = { 0x4, 0x10,
                             char (motor), char (duty*255/100), char (0xff) };
  return HID::write (d, &msg[0], msg.size ());
}

void send_preamble (HID::Device* d) {
  // Poll for version
  {
    std::string s = "\x02\x01\x02     ";
    auto result = HID::write (d, s.c_str (), s.length ());
    //    printf ("write %d\n", result);
  }
  // Send our version
  {
    std::string s = "\x02\x02\x01     ";
    auto result = HID::write (d, s.c_str (), s.length ());
    //printf ("write %d\n", result);
  }
}

void op_v (ArgList& args) {
  printf ("omni HID test, version 0.3\n");
  exit (0);
}

void op_l (ArgList& args) {
  auto devices = HID::enumerate ();

  printf ("Found %d HID devices\n", devices ? int (devices->size ()) : 0);
  if (devices)
    for (auto& dev : *devices) {
      printf ("dev %s %s %s\n",
              dev->path_.c_str (),
              dev->manufacturer_.c_str (),
              dev->product_.c_str ());
    }
}

void op_r (ArgList& args) {
  auto d = open_cap ();
  printf ("reset motors\n");
  send_preamble (d.get ());
  send_reset_motors (d.get ());
}

void op_config (ArgList& args) {
  if (args.size () < 2)
    usage ();

  int motor = strtoul (args[0].c_str (), nullptr, 0);
  int duty  = strtoul (args[1].c_str (), nullptr, 0);

  if (motor < 0 || motor > 13 || duty < 0 || duty > 100)
    usage ();

  auto d = open_cap ();
  printf ("set motor %d  duty %d%%\n", motor, duty);
  send_preamble (d.get ());
  send_config_motor (d.get (), motor, duty);
}

/** Around the head demo.  Continuous triggering of haptic motors in
    order with a set duration for each activation. */
void op_d_0 (ArgList& args) {
  if (args.size () < 4)
    usage ();

  int time  = strtoul (args[2].c_str (), nullptr, 0);
  int duty  = strtoul (args[3].c_str (), nullptr, 0);

  if (time < 0 || duty < 0 || duty > 100)
    usage ();

  auto d = open_cap ();
  send_preamble (d.get ());

  while (true)
    for (auto motor = 0; motor < 14; ++motor) {
      send_reset_motors (d.get ());
      send_config_motor (d.get (), motor, duty);
      sleep (time);
    }
}

int main (int argc, const char** argv)
{
  int motor = -1;
  int duty = 0;
  int op = 0;

  ArgList args;

  for (--argc, ++argv; argc > 0; --argc, ++argv)
    args.push_back (*argv);

  if (args[0][0] == '-') {
    switch (args[0][1]) {
    case 'l':
      op_l (args);
      break;
    case 'v':
      op_v (args);
      break;
    case 'r':
      op_r (args);
      break;
    case 'd':
      if (args.size () < 2)
        usage ();
      {
        int demo = strtol (args[1].c_str (), nullptr, 0);
        switch (demo) {
        case 0:
          op_d_0 (args);
          break;
        default:
          usage ();
          break;
        }
      }
      break;

    default:
      printf ("unknown switch\n");
      break;
    }
  }
  else
    op_config (args);

  exit (0);
}
