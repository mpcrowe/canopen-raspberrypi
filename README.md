# canopen-raspberrypi
a CanOpen Raspberry Pi Project that integrates several other projects
This project attempts to use a Raspberry PI 3 as a can bus wifi interface.  The Pi allows an Android app to connect to the can bus and
directly send and recieve messages.  


From a fresh install, install the following packages

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

This will install the basic packages to allow you to bring up a can0 interface on the mcp2515 spi-can interface chip.


run from /home/pi/canbus
./runsocat.sh
which will bring up the interface and start a session of socat to put can0 on port 2000

type ifconfig and verify that can0 is up!


to install canfestival library 

cd /home/pi/canbus/canfest/CanFestival-3-8bfe0ac00cdb/drivers/can_socket
make clean
make
sudo make install
sudo ldconfig

to start canopen bus master
cd /home/pi/canbus/canfest/CanFestival-3-8bfe0ac00cdb/examples/DS401_Master
./run.sh

Use directiona to configure wifi

http://elinux.org/RPI-Wireless-Hotspot


for bus master / canfestival development install wx widgets
sudo apt-get install python-wxgtk2.8 python-wxglade

This takes  ~250M to install

for debugging install
sudo apt-get install diffuse
~25M





