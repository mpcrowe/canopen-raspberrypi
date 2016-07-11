
CAN_STATE=`ifconfig can0 2>&1 | grep UP`
echo ${CAN_STATE}

if [[ ${CAN_STATE} == *"UP"* ]]; then
	echo can bus is up
else
	echo can0 not found, brining up ...
	sudo ip link set can0 up type can bitrate 500000
fi

#sudo socat -d -d -d -d INTERFACE:can0,pf=29,type=3,prototype=1 TCP-LISTEN:2000,fork,reuseaddr
#sudo socat -d -d -d -d INTERFACE:can0,pf=29,type=3,prototype=1 UDP-LISTEN:2000,fork,reuseaddr
sudo socat INTERFACE:can0,pf=29,type=3,prototype=1 UDP-LISTEN:2000,fork,reuseaddr
#sudo socat INTERFACE:can0,pf=29,type=3,prototype=1 TCP-LISTEN:2000,fork,reuseaddr
