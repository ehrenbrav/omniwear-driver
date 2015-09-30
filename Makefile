#!/usr/bin/make
#
# Makefile for Omniwear cap host drivers
#

O=o/
TARGET=omni

OS=$(shell uname -s)

ifeq ("$(OS)","Darwin")
  override OS=osx
endif

CFLAGS=-std=c++11 -O3 -g

ifeq ("$(OS)","osx")
CONFIG_OSX=y
endif

hid_SRCS=main.cc

hid_SRCS-$(CONFIG_OSX)=hid-osx.cc
hid_LIBS-$(CONFIG_OSX)=-framework IOKit -framework CoreFoundation 

hid_SRCS+=$(hid_SRCS-y)
hid_LIBS+=$(hid_LIBS-y)

OBJS=$(patsubst %.c,$O%.o, \
     $(patsubst %.cc,$O%.o, \
     $($1_SRCS)))

hid_OBJS:=$(call OBJS,hid)

ifeq ("$(V)","1")
Q=
else
Q=@
endif

.PHONY: all
all: $O$(TARGET)

$O$(TARGET): $(hid_OBJS)
	@echo "LINK   " $@
	$Qg++ $(CFLAGS) -o $@ $(hid_OBJS) $(hid_LIBS)

$(hid_OBJS): $O

$O%.o: %.cc
	@echo "COMPILE" $@
	$Qg++ -c $(CFLAGS) -o $@ $<

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

