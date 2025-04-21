#!/bin/bash

# Called by rpidatvtouch to delay the PTT 5 seconds to allow Muntjac to start up

############### PIN AND TIMING DEFINITIONS ###########

# DELAY_TIME in seconds
DELAY_TIME=5

# PTT_BIT: 0 for Receive, 1 for (delayed) transmit = BCM 21 / Header pin 40
PTT_BIT=21

# Set PTT BIT as output
gpio -g mode $PTT_BIT out


############### MAIN PROGRAM ###########

sleep "$DELAY_TIME"

# Only proceed if muntjac Running

if (pgrep -x "muntjacsdr_dvb" > /dev/null)
then
  # set PTT high
  gpio -g write $PTT_BIT 1

  # Check again after 1 second, to make sure that PTT hadn't just been cancelled
  # If not running cancel PTT
  sleep 1
  if !(pgrep -x "muntjacsdr_dvb" > /dev/null)
  then
    gpio -g write $PTT_BIT 0
  fi
fi

exit