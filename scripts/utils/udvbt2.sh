#! /bin/bash

# Compile dvb_t_stack and copy to rpidatv/bin

# set -x

# Stop the transmitter
/home/pi/rpidatv/scripts/b.sh

cd /home/pi/rpidatv/src/dvb_t2_stack
make clean
make -j $(nproc) PI4=1 dvb_t2_stack
if [ $? != "0" ]; then
  echo
  echo "failed install"
  cd /home/pi
  exit
fi

cp dvb_t2_stack /home/pi/rpidatv/bin/dvb_t2_stack
cd /home/pi

exit
