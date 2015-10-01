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

  DeviceP open ();

  bool reset_motors (Device*);
  bool configure_motor (Device*, int motor, int duty);
}

/* ----- Globals */

/* ----- Prototypes */



#endif  /* OMNIWEAR_H_INCLUDED */
