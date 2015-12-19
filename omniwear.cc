/** @file omniwear.cc

   written by Marc Singer
   1 Oct 2015

   Copyright (C) 2015 Marc Singer

   -----------
   DESCRIPTION
   -----------

   Protocol interface for Omniwear devices over HID.

   NOTES
   =====

   o Linear packed.  For the best linearization, we round to the
     nearest which means y = int (x*m + 0.5) + b)

   o A linear ramp from 128 to 255 is defined by
     num = 127, denom = 15, intercept = 128.

*/

#include "hid.h"
#include "omniwear.h"
#include <array>
#include <stdlib.h>

#define DBG(a ...) \
//  printf(a)

namespace {
  // Four bit mapping between duty codes and duties (0-255).
  std::array<uint8_t,16> packed_mapping;

  void send_preamble (Omniwear::Device* d, bool option_talk) {
    // Send our version
    {
      char option = option_talk ? 1 : 0;
      std::array<char,8> data = { 0x03, 0x02, 0x01, option };
      auto result = HID::write (d, &data[0], data.size ());
      //printf ("write %d\n", result);
    }
    // Poll for version
    {
      std::string s = "\x02\x01\x02     ";
      auto result = HID::write (d, s.c_str (), s.length ());
      //    printf ("write %d\n", result);
    }
  }

  int nearest_packed_code (int intensity) {
    int best = 0;
    uint8_t duty = (intensity*255)/100; // Convert 0-100% to 0-255
    int delta = abs (packed_mapping[best] - duty);
    for (size_t i = 1; i < packed_mapping.size (); ++i) {
      int d = abs (packed_mapping[i] - duty);
//      if (intensity)
//        printf ("%d %d %zd %d %d %d\n", intensity, duty, i, d, delta, best);
      if (d < delta) {
        best = i;
        delta = d;
      }
    }
    return best;
  }

}


namespace Omniwear {

  DeviceP open (bool option_talk) {
    auto d = HID::open (0x3eb, 0x2402);
    if (d)
      send_preamble (d.get (), option_talk);
    DBG ("Omniwear::open %p\n", d ? d.get () : nullptr);
    return std::move (d); }

  bool reset_motors (Device* d) {
    std::array<char,8> msg = { 0x1, 0x11 };
    return HID::write (d, &msg[0], msg.size ()) == msg.size (); }

  bool configure_motor (Device* d, int motor, int duty) {
    DBG ("config %d %d\n", motor, duty);
    std::array<char,8> msg = { 0x4, 0x10,
                               char (motor), char (duty*255/100),
                               char (0xff) };
    return HID::write (d, &msg[0], msg.size ()) == msg.size (); }

  bool define_packed (Device* d, const uint8_t* intensities, int count) {
    if (intensities == nullptr || count != 16)
      return false;
    for (size_t i = 0; i < count; ++i) {
      packed_mapping[i] = *intensities++;
      std::array<char,8> msg = { 0x4, 0x21, 4, char (i),
                                 char (packed_mapping[i]) };
      auto result =  HID::write (d, &msg[0], msg.size ()) == msg.size ();
      if (!result)
        return false;
    }
    return true;
  }

  /** Create a linear mapping from packed codes to intensities.  The
      0th entry is always zero. */
  bool define_packed_linear (Device* d, int numerator, int denominator,
                             int intercept) {
    packed_mapping[0] = 0;
    for (size_t i = 1; i < 16; ++i) {
      int v = (i*numerator + denominator/2)/denominator + intercept;
      if (v < 0)
        v = 0;
      if (v > 255)
        v = 255;
      packed_mapping[i] = v;
    }
//    printf ("mapping");
//    for (auto v : packed_mapping)
//      printf (" %3d", v);
//    printf ("\n");
    return define_packed (d, &packed_mapping[0], packed_mapping.size ());
  }

  bool configure_motors_packed (Device* d, int* intensities, int count)
  {
    if (!intensities || count < 0 || count > 14)
      return false;

    std::array<char,8> msg = { char (0xf1), 0, 0, 0, 0, 0, 0, 0 };

    for (int i = 0; i < count; ++i) {
      msg[1 + i/2] |= (nearest_packed_code (intensities[i]) & 0xf)
        << ((i & 1) ? 0 : 4);
    }

    return HID::write (d, &msg[0], msg.size ()) == msg.size (); }

}

