# canopen-raspberrypi
A CanOpen Raspberry Pi Project that integrates several other projects.
This project attempts to use a Raspberry PI 3 as a can bus wifi interface.  The Pi allows an Android app to connect to the can bus and
directly send and recieve messages.  There are several parts of this project
-An Android app
-A CANOpen java library
-A CanFestival Bus master to configure slave nodes
-This wrapper project that ties all the components together

![Block Diagram](https://cdn.rawgit.com/mpcrowe/canopen-raspberrrypi/pics/blockDiagram.svg)

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

# Setting up the socat server
run `./runsocat.sh` from ~/canopen-raspberrypi which will bring up the interface and start a session of 
socat to put can0 on port 2000.  Verify that can0 is up with `ifconfig`

# Wifi hotspot configuration
Setting up the wifi hotspot on the rasberry pi is beyond the scope of this project.  An excellent tutorial is 
given at [RPI-Wireless-Hotspot](http://elinux.org/RPI-Wireless-Hotspot). You can ignore setting up the dns tools if the Pi will not be connecte to the internet


to install canfestival library 

cd /home/pi/canbus/canfest/CanFestival-3-8bfe0ac00cdb/drivers/can_socket
make clean
make
sudo make install
sudo ldconfig

to start canopen bus master
cd /home/pi/canbus/canfest/CanFestival-3-8bfe0ac00cdb/examples/DS401_Master
./run.sh


for bus master / canfestival development install wx widgets
sudo apt-get install python-wxgtk2.8 python-wxglade

This takes  ~250M to install

for debugging install
sudo apt-get install diffuse
~25M





