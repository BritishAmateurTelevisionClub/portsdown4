#! /bin/bash

# Compile rpidatv gui

# set -x

sudo killall rpidatvgui >/dev/null 2>/dev/null
cd /home/pi/rpidatv/src/gui
# make clean

make -j 4 -O
if [ $? != "0" ]; then
  echo
  echo "failed install"
  cd /home/pi
  exit
fi
sudo make install
cd ../

cd /home/pi
reset

/home/pi/rpidatv/scripts/scheduler_debug.sh


