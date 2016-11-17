#!/usr/bin/make
#
# Makefile for Omniwear cap host drivers
#

O=o/

OS=$(shell uname -s)
hid_TARGET=omni$(EXE)
sdk_TARGET=omni-sdk$(EXE)
dll_TARGET=libomniwear_sdk$(SO)

ifeq ("$(OS)","Darwin")
  override OS=osx
endif

ifeq ("$(OS)","Linux")
  override OS=linux
endif

ifeq ("$(OS)","Windows")
  override OS=win
endif

ifeq ("CYGWIN_NT","$(findstring CYGWIN_NT,$(OS))")
  override OS=win
endif

CFLAGS+=-std=c++14 -O2 -g

ifeq ("$(OS)","osx")
CONFIG_OSX=y
SO=.dylib
CFLAGS+=-fPIC
endif

ifeq ("$(OS)","win")
CONFIG_WINDOWS=y
COMPILER_PREFIX=x86_64-w64-mingw32-
#COMPILER_PREFIX=i686-w64-mingw32-
SO=.dll
CFLAGS+=-DOMNIWEAR_BUILD
ALL+=$O$(basename $(dll_TARGET)).lib
EXE=.exe
endif

ifeq ("$(OS)","linux")
CONFIG_LINUX=y
CFLAGS+=-fPIC
SO=.so
endif

OBJS=$(patsubst %.c,$O%.o, \
     $(patsubst %.cc,$O%.o, \
     $($1_SRCS)))

# --- HID test program, statically linked to HID code

hid_SRCS=main.cc omniwear.cc

hid_SRCS-$(CONFIG_OSX)=hid-osx.cc
hid_LIBS-$(CONFIG_OSX)=-framework IOKit -framework CoreFoundation

hid_SRCS-$(CONFIG_WINDOWS)=hid-windows.cc
hid_LIBS-$(CONFIG_WINDOWS)= \
	-lhid -lntoskrnl -lsetupapi -static -static-libgcc -static-libstdc++

hid_SRCS-$(CONFIG_LINUX)=hid-linux.cc
hid_LIBS-$(CONFIG_LINUX)=-lusb-1.0

hid_SRCS+=$(hid_SRCS-y)
hid_LIBS+=$(hid_LIBS-y)

hid_OBJS:=$(call OBJS,hid)

# --- HID test program, dynamic linked to HID code

sdk_SRCS=main-sdk.cc

sdk_LIBS+=-lomniwear_sdk -L$O

sdk_LIBS-$(CONFIG_OSX)=-framework IOKit -framework CoreFoundation

sdk_LIBS-$(CONFIG_WINDOWS)= \
	-lhid -lsetupapi -static -static-libgcc -static-libstdc++

sdk_LIBS-$(CONFIG_LINUX)= \
	-lusb-1.0

sdk_DEPS-$(CONFIG_WINDOWS)+=$O$(basename $(dll_TARGET)).lib

sdk_SRCS+=$(sdk_SRCS-y)
sdk_LIBS+=$(sdk_LIBS-y)
sdk_DEPS+=$(sdk_DEPS-y)

sdk_OBJS:=$(call OBJS,sdk)

# --- SDK library

dll_SRCS=omniwear_SDK.cc omniwear.cc

dll_SRCS-$(CONFIG_OSX)=hid-osx.cc
dll_LIBS-$(CONFIG_OSX)=-framework IOKit -framework CoreFoundation
dll_CFLAGS-$(CONFIG_OSX):=-shared -dynamiclib -Wl,-undefined,dynamic_lookup

dll_SRCS-$(CONFIG_WINDOWS)=hid-windows.cc
dll_LIBS-$(CONFIG_WINDOWS)= \
	-lhid -lsetupapi -static -static-libgcc -static-libstdc++
dll_CFLAGS-$(CONFIG_WINDOWS):=-shared -Wl,-soname,$(dll_TARGET) -Wl,--output-def,$O$(basename $(dll_TARGET)).def

dll_SRCS-$(CONFIG_LINUX)=hid-linux.cc
dll_CFLAGS-$(CONFIG_LINUX)=-shared
dll_LIBS-$(CONFIG_LINUX)=-lusb-1.0

dll_SRCS+=$(dll_SRCS-y)
dll_LIBS+=$(dll_LIBS-y)

dll_OBJS:=$(call OBJS,dll)
dll_CFLAGS+=$(dll_CFLAGS-y)

# --- SDK Archive

zip_SRCS+=omniwear_SDK.h $O$(dll_TARGET) $O$(sdk_TARGET) $O$(hid_TARGET)

zip_SRCS-$(CONFIG_OSX)+=

zip_SRCS-$(CONFIG_WINDOWS)+=$O$(basename $(dll_TARGET)).lib \
	$O$(basename $(dll_TARGET)).a

zip_SRCS+=$(zip_SRCS-y)
zip_DIR=$Oomniwear_sdk-$(OS)
zip_OUT=$Oomniwear_sdk-$(OS).zip


# ---

CXX=$(COMPILER_PREFIX)g++
DLLTOOL=$(COMPILER_PREFIX)dlltool

ifeq ("$(V)","1")
Q=
else
Q=
endif

.PHONY: all
all: $O$(hid_TARGET) $O$(dll_TARGET) $O$(sdk_TARGET) $(ALL)

$(zip_OUT): $(zip_SRCS)
	mkdir -p $(zip_DIR)
	cp $(zip_SRCS) $(zip_DIR)
	cd $O ; zip -r $(notdir $@) $(notdir $(zip_DIR))

.PHONY: archive
archive: $(zip_OUT)

$O$(sdk_TARGET): $O$(dll_TARGET) $(sdk_DEPS)

$O$(hid_TARGET): $(hid_OBJS)
	@echo "LINK   " $@
	$Q$(CXX) $(CFLAGS) $(hid_CFLAGS) -o $@ $(hid_OBJS) $(hid_LIBS)

$O$(sdk_TARGET): $(sdk_OBJS)
	@echo "LINK   " $@
	$Q$(CXX) $(CFLAGS) $(sdk_CFLAGS) -o $@ $(sdk_OBJS) $(sdk_LIBS)

$O$(dll_TARGET): $(dll_OBJS)
	@echo "LINK   " $@
	$Q$(CXX) $(CFLAGS) $(dll_CFLAGS) -o $@ $(dll_OBJS) $(dll_LIBS)

$O$(basename $(dll_TARGET)).lib: $O$(dll_TARGET)
	@echo "LIB    " $@
	$Q$(DLLTOOL) -D $(dll_TARGET) -d $O$(basename $(dll_TARGET)).def -l $@
	$Qcp $@ $O$(basename $(dll_TARGET)).a

$(hid_OBJS) $(dll_OBJS) $(sdk_OBJS): $O

$O%.o: %.cc
	@echo "COMPILE" $@
	$Q$(CXX) -c $(CFLAGS) -o $@ $<

$O:
	@echo "MKDIR  " $@
	$Qmkdir -p $O

.PHONY: lib
lib: $O$(dll_TARGET)
	[ ! -d ~/lib ] || cp $O$(dll_TARGET) ~/lib

.PHONY: clean
clean:
	@echo "CLEAN  "
ifneq ("$(wildcard $O*.o $Ohid)","")
	$Q-rm $(wildcard $O*.o $Ohid)
	$Q-rmdir $O
endif

.PHONY: what
what:
	@echo os   $(OS)
	@echo srcs $(hid_SRCS)
	@echo objs $(hid_OBJS)
