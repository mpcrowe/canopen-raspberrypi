# canopen-raspberrypi
This project uses a Raspberry PI 3 as a CAN-bus to wifi interface.  The Pi allows an Android app to connect to the can bus and
directly send and recieve messages.  There are several aspects to this project;
 - An Android app
 - A CANOpen java library
 - Setup of socat to route a CAN-Bus interface to a UDP port
 - A CanFestival Bus master to configure slave nodes
 - This wrapper project that ties all the components together

![Block Diagram](pics/blockDiagram.png)

# Setting up the Raspberry Pi Can Bus Interface
From a fresh Raspberry Pi install, install the following packages

	sudo apt-get update 
	sudo apt-get dist-upgrade
	sudo apt-get install joe can-utils hostapd udhcpd rsync socat git
	sudo raspi-config
		enable spi 
		expand filesystem
		enable i2c
		set hostname


to setup the can bus interface
	sudo joe /boot/config.txt

Add these 3 lines to the end of file:

	# can-bus see interface http://skpang.co.uk/blog/archives/1165 for details 
	dtoverlay=mcp2515-can0,oscillator=16000000,interrupt=25 
	dtoverlay=spi-bcm2835

This will install the basic packages to allow you to bring up a can0 interface on the mcp2515 spi-can interface chip.  The 
can0 interface is used with socat to allow wifi connections to the canbus interace

#Other packages that you may need to edit and compile CanFestival applications
 - python-wxgtk2.8 python-wxglade,  to use the python ObjDict editor in CanFestival
 - diffuse, a nice gui text diffing tool
 - libxml2-dev, required to compile the busMaster tool

# Setting up the socat server
run `./runsocat.sh` from ~/canopen-raspberrypi which will bring up the interface and start a session of 
socat to put can0 on port 2000.  Verify that can0 is up with `ifconfig`

# Wifi hotspot configuration
Setting up the wifi hotspot on the rasberry pi is beyond the scope of this project.  An excellent tutorial is 
given at [RPI-Wireless-Hotspot](http://elinux.org/RPI-Wireless-Hotspot). You can ignore setting up the dns tools if the Pi will not be connecte to the internet

# busMaster
CAN-Open is a distributed internodal communications system.  A master/slave relationship exists on the bus, but 
the master is more of a dynamic information repository and state transition tool than it is a true *master* 
although it can potentially act like one.  This busMaster tool relies heavily on CanFestival for the underlying 
CAN-Open infrastructure.  

The busMaster software does a few things.
 - it configures the dynamic information on the system when a new node comes online.  The dynamic information is contained in config.xml
 - it sends out periodic sync signals
 - it puts nodes into the operational state once configured

To add nodes to the bus unfortunatly, busMaster must be edited and recompiled with the new node ID.  This requires the 
use of wxWidgets to add the correct SDO parameters.

# Installing canfestival socket drivers
	cd .../CanFestival-3-8bfe0ac00cdb/drivers/can_socket
	make clean
	make
	sudo make install
	sudo ldconfig

#to start canopen bus master
	.../busMaster
	./run.sh



[Android CAN-Open Demo] (https://github.com/Awalrod/AndroidCanOpenDemo)






