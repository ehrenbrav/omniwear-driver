#!/usr/bin/make
#
# Makefile for Omniwear cap host drivers
#

O=o/

OS=$(shell uname -s)
hid_TARGET=omni$(EXE)
dll_TARGET=omniwear_sdk$(SO)

ifeq ("$(OS)","Darwin")
  override OS=osx
endif

ifeq ("$(OS)","Linux")
  override OS=linux
endif

ifeq ("$(OS)","Windows")
  override OS=win
endif

CFLAGS=-std=c++14 -O3 -g

ifeq ("$(OS)","osx")
CONFIG_OSX=y
SO=.so
CFLAGS+=-fPIC
endif

ifeq ("$(OS)","win")
CONFIG_WINDOWS=y
COMPILER_PREFIX=x86_64-w64-mingw32-
SO=.dll
endif

ifeq ("$(OS)","linux")
CONFIG_LINUX=y
endif

OBJS=$(patsubst %.c,$O%.o, \
     $(patsubst %.cc,$O%.o, \
     $($1_SRCS)))

# --- HID test program

hid_SRCS=main.cc omniwear.cc

hid_SRCS-$(CONFIG_OSX)=hid-osx.cc
hid_LIBS-$(CONFIG_OSX)=-framework IOKit -framework CoreFoundation 

hid_SRCS-$(CONFIG_WINDOWS)=hid-windows.cc
hid_LIBS-$(CONFIG_WINDOWS)= \
	-lhid -lsetupapi -static -static-libgcc -static-libstdc++

hid_SRCS+=$(hid_SRCS-y)
hid_LIBS+=$(hid_LIBS-y)

hid_OBJS:=$(call OBJS,hid)


# --- SDK library

dll_SRCS=omniwear_SDK.cc omniwear.cc

dll_SRCS-$(CONFIG_OSX)=hid-osx.cc
dll_LIBS-$(CONFIG_OSX)=-framework IOKit -framework CoreFoundation 
dll_CFLAGS-$(CONFIG_OSX):=-shared -dynamiclib -Wl,-undefined,dynamic_lookup

dll_SRCS-$(CONFIG_WINDOWS)=hid-windows.cc
dll_LIBS-$(CONFIG_WINDOWS)= \
	-lhid -lsetupapi -static -static-libgcc -static-libstdc++
dll_CFLAGS-$(CONFIG_WINDOWS):=-shared -Wl,-soname,$(dll_TARGET)

dll_SRCS+=$(dll_SRCS-y)
dll_LIBS+=$(dll_LIBS-y)

dll_OBJS:=$(call OBJS,dll)
dll_CFLAGS+=$(dll_CFLAGS-y)



CXX=$(COMPILER_PREFIX)g++

ifeq ("$(V)","1")
Q=
else
Q=@
endif

ifeq ("$(OS)","linux")
.PHONY: unsupported
unsupported:
	@echo "=== OS '$(OS)' unsupported"
endif

.PHONY: all
all: $O$(hid_TARGET) $O$(dll_TARGET)

$O$(hid_TARGET): $(hid_OBJS)
	@echo "LINK   " $@
	$Q$(CXX) $(CFLAGS) $(hid_CFLAGS) -o $@ $(hid_OBJS) $(hid_LIBS)

$O$(dll_TARGET): $(dll_OBJS)
	@echo "LINK   " $@
	$Q$(CXX) $(CFLAGS) $(dll_CFLAGS) -o $@ $(dll_OBJS) $(dll_LIBS)

$(hid_OBJS) $(dll_OBJS): $O

$O%.o: %.cc
	@echo "COMPILE" $@
	$Q$(CXX) -c $(CFLAGS) -o $@ $<

$O:
	@echo "MKDIR  " $@
	$Qmkdir -p $O

.PHONY: clean
clean:
	@echo "CLEAN  "
ifneq ("$(wildcard $O*.o $Ohid)","")
	$Q-rm $(wildcard $O*.o $Ohid)
	$Q-rmdir $O
endif

.PHONY: what
what:
	@echo srcs $(hid_SRCS)
	@echo objs $(hid_OBJS)

