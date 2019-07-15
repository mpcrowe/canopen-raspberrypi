#! gmake

#
# Copyright (C) 2006 Laurent Bessard
# 
# This file is part of canfestival, a library implementing the canopen
# stack
# 
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
# 
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
# 

CC = gcc-9.1
CXX = g++
LD = gcc-9.1
OPT_CFLAGS = -O2
CFLAGS = $(OPT_CFLAGS)
PROG_CFLAGS = 
EXE_CFLAGS =  -lpthread -lrt -ldl
OS_NAME = Linux
ARCH_NAME = arm
PREFIX = /usr/local
TARGET = unix
CAN_DRIVER = can_virtual
TIMERS_DRIVER = timers_unix
DIST_INSTALL_PATH=/usr/local/include/canfestival
LIB_PATH=/usr/local/lib
XML_INCLUDE_PATH=/usr/include/libxml2
SYSD_LIB=$(shell pkg-config --libs libsystemd)
SYSD_FLAGS=$(shell pkg-config --cflags libsystemd)

INCLUDES = -I$(XML_INCLUDE_PATH) -I$(DIST_INSTALL_PATH) -I$(DIST_INSTALL_PATH)/$(TARGET) -I$(DIST_INSTALL_PATH)/$(CAN_DRIVER) -I$(DIST_INSTALL_PATH)/$(TIMERS_DRIVER)
INCLUDES += $(SYSD_FLAGS)

GITHEAD := -D'GIT_REV="$(shell git rev-list --count --first-parent HEAD)"'

MASTER_OBJS = TestMaster.o DS401_Master.o configParser.o

OBJS = $(MASTER_OBJS) $(LIB_PATH)/libcanfestival.a $(LIB_PATH)/libcanfestival_$(TARGET).a

all: DS401_Master

#../../drivers/$(TARGET)/libcanfestival_$(TARGET).a:
#	$(MAKE) -C ../../drivers/$(TARGET) libcanfestival_$(TARGET).a


DS401_Master: TestMaster.c $(OBJS)
	$(LD) $(CFLAGS) $(GITHEAD) $(PROG_CFLAGS) -lxml2 ${PROGDEFINES} $(INCLUDES) -o $@ $(OBJS) $(SYSD_LIB) $(EXE_CFLAGS)

	
TestMaster.c: TestMaster.od
#	$(MAKE) -C ../../objdictgen gnosis
	../../canfestival/objdictgen/objdictgen.py TestMaster.od TestMaster.c
edit:
	../../canfestival/objdictgen/objdictedit.py TestMaster.od

%.o: %.c
	$(CC) $(GITHEAD) $(CFLAGS) $(PROG_CFLAGS) ${PROGDEFINES} $(INCLUDES) -o $@ -c $<

clean:
	rm -f *~
	rm -f $(MASTER_OBJS)
	rm -f DS401_Master

mrproper: clean
	rm -f TestMaster.c
	
install: DS401_Master
#	mkdir -p $(DESTDIR)$(PREFIX)/bin/
	sudo cp $< $(DESTDIR)$(PREFIX)/bin/
	
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/DS401_Master

