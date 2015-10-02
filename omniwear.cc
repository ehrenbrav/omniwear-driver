/** @file omniwear.cc

   written by Marc Singer
   1 Oct 2015

   Copyright (C) 2015 Marc Singer

   -----------
   DESCRIPTION
   -----------

   Protocol interface for Omniwear devices over HID.

*/

#include "hid.h"
#include "omniwear.h"
#include <array>

#define DBG(a ...) \
//  printf(a)

namespace {
  void send_preamble (Omniwear::Device* d) {
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

}


namespace Omniwear {

  DeviceP open () {
    auto d = HID::open (0x3eb, 0x2402);
    if (d)
      send_preamble (d.get ());
    DBG ("Omniwear::open %p\n", d ? d.get () : nullptr);
    return std::move (d); }

  bool reset_motors (Device* d) {
    std::array<char,8> msg = { 0x1, 0x11 };
    return HID::write (d, &msg[0], msg.size ()) == msg.size (); }

  bool configure_motor (HID::Device* d, int motor, int duty) {
    DBG ("config %d %d\n", motor, duty);
    std::array<char,8> msg = { 0x4, 0x10,
                               char (motor), char (duty*255/100), char (0xff) };
    return HID::write (d, &msg[0], msg.size ()) == msg.size (); }

}

