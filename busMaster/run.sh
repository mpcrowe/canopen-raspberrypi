

#  OPTIONS:                                                 *
#  *     -l : Can library ["libcanfestival_can_virtual.so"]     *
#  *                                                            *
#  *    Slave:                                                  *
#  *     -i : Slave Node id format [0x01 , 0x7F]                *
#  *                                                            *
#  *    Master:                                                 *
#  *     -m : bus name ["1"]                                    *
#  *     -M : 1M,500K,250K,125K,100K,50K,20K,10K                
#
./DS401_Master -i0x66 -x config.xml -d -d
#./DS401_Master -llibcanfestival_can_socket.so -m can0 -M500K -i0x66
#./DS401_Master -llibcanfestival_can_socket.so -m can0 -M500K -i0x40
#./DS401_Master -llibcanfestival_can_socket.so -m can0 -M500K -i0x20

