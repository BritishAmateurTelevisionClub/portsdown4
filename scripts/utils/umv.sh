#! /bin/bash

# Compile Meteorview and copy to rpidatv/bin

# set -x

########### Function to display message ####################

DisplayMsg() {
  # Delete any old update message image
  rm /home/pi/tmp/update.jpg >/dev/null 2>/dev/null

  # Create the update image in the tempfs folder
  convert -size 800x480 xc:black -font FreeSans -fill white \
    -gravity Center -pointsize 30 -annotate 0 "$1" \
    /home/pi/tmp/update.jpg

  # Display the update message on the desktop
  sudo fbi -T 1 -noverbose -a /home/pi/tmp/update.jpg >/dev/null 2>/dev/null
  (sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work
}


sudo killall rpidatvgui >/dev/null 2>/dev/null

# Kill the current process if it is running
sudo killall meteorview >/dev/null 2>/dev/null

echo Compiling MeteorViewer

cd /home/pi/rpidatv/src/meteorview
touch meteorview.c
make -j 4 -O
if [ $? != "0" ]; then
  echo
  echo "failed install"
  cd /home/pi
  exit
fi

sudo killall rpidatvgui >/dev/null 2>/dev/null

cp meteorview /home/pi/rpidatv/bin/meteorview
cd /home/pi

      DisplayMsg "Restarting SDRPlay Service\n\nThis may take up to 90 seconds"
      sudo systemctl restart sdrplay
      DisplayMsg " "                      # Display Blank screen

      lsusb | grep -q '1df7:'             # check for SDRPlay
      if [ $? != 0 ]; then                # Not detected
        DisplayMsg "Unable to detect SDRPlay\n\nResetting the USB Bus"
        sudo uhubctl -R -a 2              # So reset USB bus
        sleep 1
        lsusb | grep -q '1df7:'
        if [ $? != 0 ]; then              # Check again
          sudo uhubctl -R -a 2            # Try reset USB bus again
          sleep 1
          lsusb | grep -q '1df7:'         
          if [ $? != 0 ]; then            # If still no joy
            DisplayMsg "Still Unable to detect SDRPlay\n\n\nCheck connections"
            sleep 2
            DisplayMsg " "                # Display Blank screen
            GUI_RETURN_CODE=129           # Return to Portsdown     
          fi
        fi
      fi


/home/pi/rpidatv/bin/meteorview

exit
