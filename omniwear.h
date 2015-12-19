/** @file omniwear.h

   written by Marc Singer
   1 Oct 2015

   Copyright (C) 2015 Marc Singer

   -----------
   DESCRIPTION
   -----------

*/

#if !defined (OMNIWEAR_H_INCLUDED)
#    define   OMNIWEAR_H_INCLUDED

/* ----- Includes */

#include "hid.h"

/* ----- Macros */

/* ----- Types */

namespace Omniwear {

  using DeviceP = HID::DeviceP;
  using Device = HID::Device;

  DeviceP open (bool option_talk = false);

  bool reset_motors (Device*);
  bool configure_motor (Device*, int motor, int duty);
  bool define_packed (Device*, const uint8_t* intensities, int count);
  bool define_packed_linear (Device*,
                             int numerator, int denominator, int intercept);
  bool configure_motors_packed (Device*, const int* duties, int count);
}

/* ----- Globals */

/* ----- Prototypes */



#endif  /* OMNIWEAR_H_INCLUDED */
