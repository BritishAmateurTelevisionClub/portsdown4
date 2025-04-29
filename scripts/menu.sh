#!/bin/bash

# Version 202212070

# set -x

############ Set Environment Variables ###############

PATHSCRIPT=/home/pi/rpidatv/scripts
PATHRPI=/home/pi/rpidatv/bin
PATHCONFIGS="/home/pi/rpidatv/scripts/configs"  ## Path to config files
PCONFIGFILE="/home/pi/rpidatv/scripts/portsdown_config.txt"
PATH_PPRESETS="/home/pi/rpidatv/scripts/portsdown_presets.txt"
PATH_STREAMPRESETS="/home/pi/rpidatv/scripts/stream_presets.txt"
PATH_LIME_CAL="/home/pi/rpidatv/scripts/limecalfreq.txt"


GPIO_PTT=29  ## WiringPi value, not BCM

############ Function to Write to Config File ###############

set_config_var() {
lua - "$1" "$2" "$3" <<EOF > "$3.bak"
local key=assert(arg[1])
local value=assert(arg[2])
local fn=assert(arg[3])
local file=assert(io.open(fn))
local made_change=false
for line in file:lines() do
if line:match("^#?%s*"..key.."=.*$") then
line=key.."="..value
made_change=true
end
print(line)
end
if not made_change then
print(key.."="..value)
end
EOF
mv "$3.bak" "$3"
}

############ Function to Read from Config File ###############

get_config_var() {
lua - "$1" "$2" <<EOF
local key=assert(arg[1])
local fn=assert(arg[2])
local file=assert(io.open(fn))
for line in file:lines() do
local val = line:match("^#?%s*"..key.."=(.*)$")
if (val ~= nil) then
print(val)
break
end
end
EOF
}

############ Function to Read value with - from Config File ###############

get-config_var() {
lua - "$1" "$2" <<EOF
local key=assert(arg[1])
local fn=assert(arg[2])
local file=assert(io.open(fn))
for line in file:lines() do
local val = line:match("^#?%s*"..key.."=[+-]?(.*)$")
if (val ~= nil) then
print(val)
break
end
end
EOF
}


############ Function to Select Files ###############

Filebrowser() {
if [ -z $1 ]; then
imgpath=$(ls -lhp / | awk -F ' ' ' { print $9 " " $5 } ')
else
imgpath=$(ls -lhp "/$1" | awk -F ' ' ' { print $9 " " $5 } ')
fi
if [ -z $1 ]; then
pathselect=$(whiptail --menu "$FileBrowserTitle""$filename" 20 50 10 --cancel-button Cancel --ok-button Select $imgpath 3>&1 1>&2 2>&3)
else
pathselect=$(whiptail --menu "$FileBrowserTitle""$filename" 20 50 10 --cancel-button Cancel --ok-button Select ../ BACK $imgpath 3>&1 1>&2 2>&3)
fi
RET=$?
if [ $RET -eq 1 ]; then
## This is the section where you control what happens when the user hits Cancel
Cancel
elif [ $RET -eq 0 ]; then
	if [[ -d "/$1$pathselect" ]]; then
		Filebrowser "/$1$pathselect"
	elif [[ -f "/$1$pathselect" ]]; then
		## Do your thing here, this is just a stub of the code I had to do what I wanted the script to do.
		fileout=`file "$1$pathselect"`
		filename=`readlink -m $1$pathselect`
	else
		echo pathselect $1$pathselect
		whiptail --title "$FileMenuTitle" --msgbox "$FileMenuContext" 8 44
		unset base
		unset imgpath
		Filebrowser
	fi
fi
}

############ Function to Select Paths ###############

Pathbrowser() {
if [ -z $1 ]; then
imgpath=$(ls -lhp / | awk -F ' ' ' { print $9 " " $5 } ')
else
imgpath=$(ls -lhp "/$1" | awk -F ' ' ' { print $9 " " $5 } ')
fi
if [ -z $1 ]; then
pathselect=$(whiptail --menu "$FileBrowserTitle""$filename" 20 50 10 --cancel-button Cancel --ok-button Select $imgpath 3>&1 1>&2 2>&3)
else
pathselect=$(whiptail --menu "$FileBrowserTitle""$filename" 20 50 10 --cancel-button Cancel --ok-button Select ../ BACK $imgpath 3>&1 1>&2 2>&3)
fi
RET=$?
if [ $RET -eq 1 ]; then
## This is the section where you control what happens when the user hits Cancel
Cancel	
elif [ $RET -eq 0 ]; then
	if [[ -d "/$1$pathselect" ]]; then
		Pathbrowser "/$1$pathselect"
	elif [[ -f "/$1$pathselect" ]]; then
		## Do your thing here, this is just a stub of the code I had to do what I wanted the script to do.
		fileout=`file "$1$pathselect"`
		filenametemp=`readlink -m $1$pathselect`
		filename=`dirname $filenametemp` 

	else
		echo pathselect $1$pathselect
		whiptail --title "$FileMenuTitle" --msgbox "$FileMenuContext" 8 44
		unset base
		unset imgpath
		Pathbrowser
	fi

fi
}

################################### Menus ####################################

do_input_setup()
{
  menuchoice=$(whiptail --title "Select Category of Input" --menu "Encoding Mode:" 16 78 7 \
    "1 H264" "SD H264 and Externally Encoded Inputs"  \
    "2 SD MPEG-2" "SD MPEG-2 Encoded Inputs" \
    "3 Widescreen" "Widescreen and HD Encoded Inputs" \
      3>&2 2>&1 1>&3)
      case "$menuchoice" in
        1\ *) do_input_setup_h264 ;;
        2\ *) do_input_setup_mpeg2 ;;
        3\ *) do_input_setup_wide ;;
      esac
}

# CAMH264
# ANALOGCAM
# WEBCAMH264
# CARDH264
# PATERNAUDIO
# CONTEST
# DESKTOP
# CARRIER
# TESTMODE
# FILETS
# IPTSIN
# VNC

# CAMMPEG-2
# ANALOGMPEG-2
# WEBCAMMPEG-2
# CARDMPEG-2
# CONTESTMPEG-2

# CAM16MPEG-2
# CAMHDMPEG-2
# ANALOG16MPEG-2
# WEBCAM16MPEG-2
# WEBCAMHDMPEG-2
# CARD16MPEG-2
# CARDHDMPEG-2
# C920H264
# C920HDH264
# C920FHDH264

do_input_setup_h264()
{
  MODE_INPUT=$(get_config_var modeinput $PCONFIGFILE)
  Radio1=OFF
  Radio2=OFF
  Radio3=OFF
  Radio4=OFF
  Radio5=OFF
  Radio6=OFF
  Radio7=OFF
  Radio8=OFF
  Radio9=OFF
  Radio10=OFF
  Radio11=OFF
  Radio12=OFF
  Radio13=OFF
  Radio14=OFF
  Radio15=OFF

  case "$MODE_INPUT" in
  CAMH264)
    Radio1=ON
  ;;
  ANALOGCAM)
    Radio2=ON
  ;;
  WEBCAMH264)
    Radio3=ON
  ;;
  CARDH264)
    Radio4=ON
  ;;
  PATERNAUDIO)
    Radio5=ON
  ;;
  CONTEST)
    Radio6=ON
  ;;
  DESKTOP)
    Radio7=ON
  ;;
  CARRIER)
    Radio8=ON
  ;;
  TESTMODE)
    Radio9=ON
  ;;
  FILETS)
    Radio10=ON
  ;;
  IPTSIN)
    Radio11=ON
  ;;
  IPTSIN264)
    Radio12=ON
  ;;
  IPTSIN265)
    Radio13=ON
  ;;
  VNC)
    Radio14=ON
  ;;
  *)
    Radio15=ON
  ;;
  esac

  chinput=$(whiptail --title "$StrInputSetupTitle" --radiolist \
    "$StrInputSetupDescription" 22 78 16 \
    "CAMH264" "$StrInputSetupCAMH264" $Radio1 \
    "ANALOGCAM" "$StrInputSetupANALOGCAM" $Radio2 \
    "WEBCAMH264" "H264 SD USB Webcam, no audio" $Radio3 \
    "CARDH264" "H264 Static Test Card F, no audio" $Radio4 \
    "PATERNAUDIO" "$StrInputSetupPATERNAUDIO" $Radio5 \
    "CONTEST" "$StrInputSetupCONTEST" $Radio6  \
    "DESKTOP" "$StrInputSetupDESKTOP" $Radio7 \
    "CARRIER" "$StrInputSetupCARRIER" $Radio8 \
    "TESTMODE" "$StrInputSetupTESTMODE" $Radio9 \
    "FILETS" "$StrInputSetupFILETS" $Radio10\
    "IPTSIN" "$StrInputSetupIPTSIN" $Radio11 \
    "IPTSIN264" "TS, add H264 metadata" $Radio12 \
    "IPTSIN265" "TS, add H265 metadata" $Radio13 \
    "VNC" "$StrInputSetupVNC" $Radio14 \
    "OTHER" "MPEG-2 or Widescreen Mode" $Radio15 \
  3>&2 2>&1 1>&3)

  if [ $? -eq 0 ]; then
    case "$chinput" in

    ANALOGCAM)
      ANALOGCAMNAME=$(get_config_var analogcamname $PCONFIGFILE)
      Radio1=OFF
      Radio2=OFF
      Radio3=OFF

      case "$ANALOGCAMNAME" in
        "/dev/video0")
          Radio1=ON
        ;;
        "/dev/video1")
          Radio2=ON
        ;;
        auto)
          Radio3=ON
        ;;
        *)
          Radio1=ON
        ;;
      esac
      newcamname=$(whiptail --title "$StrInputSetupANALOGCAMName" --radiolist \
        "$StrInputSetupANALOGCAMTitle" 20 78 5 \
        "/dev/video0" "Normal with no PiCam" $Radio1 \
        "/dev/video1" "Sometimes required with PiCam" $Radio2 \
        "auto" "Automatically select device name" $Radio3 \
        3>&2 2>&1 1>&3)
      if [ $? -eq 0 ]; then
         set_config_var analogcamname "$newcamname" $PCONFIGFILE
      fi

      FORMAT=$(get_config_var format $PCONFIGFILE)
      Radio1=OFF
      Radio2=OFF

      case "$FORMAT" in
        "4:3")
          Radio1=ON
        ;;
        "16:9")
          Radio2=ON
        ;;
        *)
          Radio1=ON
        ;;
      esac
      newformat=$(whiptail --title "Select 4:3 or 16:9 Transmission Format" --radiolist \
        "Select one option with spacebar and press enter" 20 78 5 \
        "4:3" "Old-style 4:3 Transmission Format" $Radio1 \
        "16:9" "Widescreen Transmission Format" $Radio2 \
        3>&2 2>&1 1>&3)
      if [ $? -eq 0 ]; then
         set_config_var format "$newformat" $PCONFIGFILE
      fi
    ;;
    PATERNAUDIO)
      PATERNFILE=$(get_config_var paternfile $PCONFIGFILE)
      filename=$PATERNFILE
      FileBrowserTitle=JPEG:
      Pathbrowser "$PATHTS/"
      whiptail --title "$StrInputSetupPATERNAUDIOName" --msgbox "$filename" 8 44
      set_config_var paternfile "$filename" $PCONFIGFILE
      set_config_var pathmedia "$filename" $PCONFIGFILE
    ;;
    FILETS)
      TSVIDEOFILE=$(get_config_var tsvideofile $PCONFIGFILE)
      filename=$TSVIDEOFILE
      FileBrowserTitle=TS:
      Filebrowser "$PATHTS/"
      whiptail --title "$StrInputSetupFILETSName" --msgbox "$filename" 8 44
      set_config_var tsvideofile "$filename" $PCONFIGFILE
      PATHTS=`dirname $filename`
      set_config_var pathmedia "$PATHTS" $PCONFIGFILE
    ;;
    IPTSIN)
      CURRENTIP=$(ifconfig | grep -Eo 'inet (addr:)?([0-9]*\.){3}[0-9]*' | grep -Eo '([0-9]*\.){3}[0-9]*' | grep -v '127.0.0.1')
      whiptail --title "$StrInputSetupIPTSINTitle" --msgbox "$StrInputSetupIPTSINName""$CURRENTIP" 8 78
    ;;
    IPTSIN264)
      CURRENTIP=$(ifconfig | grep -Eo 'inet (addr:)?([0-9]*\.){3}[0-9]*' | grep -Eo '([0-9]*\.){3}[0-9]*' | grep -v '127.0.0.1')
      whiptail --title "$StrInputSetupIPTSINTitle" --msgbox "$StrInputSetupIPTSINName""$CURRENTIP" 8 78
    ;;
    IPTSIN265)
      CURRENTIP=$(ifconfig | grep -Eo 'inet (addr:)?([0-9]*\.){3}[0-9]*' | grep -Eo '([0-9]*\.){3}[0-9]*' | grep -v '127.0.0.1')
      whiptail --title "$StrInputSetupIPTSINTitle" --msgbox "$StrInputSetupIPTSINName""$CURRENTIP" 8 78
    ;;
    VNC)
      VNCADDR=$(get_config_var vncaddr $PCONFIGFILE)
      VNCADDR=$(whiptail --inputbox "$StrInputSetupVNCName" 8 78 $VNCADDR --title "$StrInputSetupVNCTitle" 3>&1 1>&2 2>&3)
      if [ $? -eq 0 ]; then
        set_config_var vncaddr "$VNCADDR" $PCONFIGFILE
      fi
    ;;
    esac
    if [ "$chinput" != "OTHER" ]; then
      set_config_var modeinput "$chinput" $PCONFIGFILE
    fi
  fi
}

do_input_setup_mpeg2()
{
  MODE_INPUT=$(get_config_var modeinput $PCONFIGFILE)
  Radio1=OFF
  Radio2=OFF
  Radio3=OFF
  Radio4=OFF
  Radio5=OFF
  Radio6=OFF
  Radio7=OFF

  case "$MODE_INPUT" in
  CAMMPEG-2)
    Radio1=ON
  ;;
  ANALOGMPEG-2)
    Radio2=ON
  ;;
  WEBCAMMPEG-2)
    Radio3=ON
  ;;
  CARDMPEG-2)
    Radio4=ON
  ;;
  CONTESTMPEG-2)
    Radio5=ON
  ;;
  *)
    Radio6=ON
  ;;
  esac

  chinput=$(whiptail --title "$StrInputSetupTitle" --radiolist \
    "$StrInputSetupDescription" 20 78 10 \
    "CAMMPEG-2" "$StrInputSetupCAMMPEG_2" $Radio1 \
    "ANALOGMPEG-2" "MPEG-2 and sound from Comp Video Input" $Radio2 \
    "WEBCAMMPEG-2" "MPEG-2 SD USB Webcam with Audio" $Radio3 \
    "CARDMPEG-2" "MPEG-2 Static Test Card F with Audio" $Radio4 \
    "CONTESTMPEG-2" "$StrInputSetupCONTEST" $Radio5  \
    "OTHER" "H264 or Widescreen Mode" $Radio6 \
  3>&2 2>&1 1>&3)

  if [ $? -eq 0 ]; then
    case "$chinput" in

    CAMMPEG-2)
      # Make sure that the camera driver is loaded
      lsmod | grep -q 'bcm2835_v4l2'
      if [ $? != 0 ]; then   ## not loaded
        sudo modprobe bcm2835_v4l2
      fi
    ;;
    ANALOGMPEG-2)
      ANALOGCAMNAME=$(get_config_var analogcamname $PCONFIGFILE)
      Radio1=OFF
      Radio2=OFF
      Radio3=OFF

      case "$ANALOGCAMNAME" in
        "/dev/video0")
          Radio1=ON
        ;;
        "/dev/video1")
          Radio2=ON
        ;;
        auto)
          Radio3=ON
        ;;
        *)
          Radio1=ON
        ;;
      esac
      newcamname=$(whiptail --title "$StrInputSetupANALOGCAMName" --radiolist \
        "$StrInputSetupANALOGCAMTitle" 20 78 5 \
        "/dev/video0" "Normal with no PiCam" $Radio1 \
        "/dev/video1" "Sometimes required with PiCam" $Radio2 \
        "auto" "Automatically select device name" $Radio3 \
        3>&2 2>&1 1>&3)
      if [ $? -eq 0 ]; then
         set_config_var analogcamname "$newcamname" $PCONFIGFILE
      fi
    ;;
    esac
    if [ "$chinput" != "OTHER" ]; then
      set_config_var modeinput "$chinput" $PCONFIGFILE
    fi
  fi
}

do_input_setup_wide()
{
  MODE_INPUT=$(get_config_var modeinput $PCONFIGFILE)
  Radio1=OFF
  Radio2=OFF
  Radio3=OFF
  Radio4=OFF
  Radio5=OFF
  Radio6=OFF
  Radio7=OFF
  Radio8=OFF
  Radio9=OFF
  Radio10=OFF
  Radio11=OFF
  Radio12=OFF
  Radio13=OFF
  Radio14=OFF
  Radio15=OFF

  case "$MODE_INPUT" in
  CAM16MPEG-2)
    Radio1=ON
  ;;
  CAMHDMPEG-2)
    Radio2=ON
  ;;
  ANALOG16MPEG-2)
    Radio3=ON
  ;;
  WEBCAM16MPEG-2)
    Radio4=ON
  ;;
  WEBCAMHDMPEG-2)
    Radio5=ON
  ;;
  CARD16MPEG-2)
    Radio6=ON
  ;;
  CARDHDMPEG-2)
    Radio7=ON
  ;;
  C920H264)
    Radio8=ON
  ;;
  C920HDH264)
    Radio9=ON
  ;;
  C920FHDH264)
    Radio10=ON
  ;;
  HDMI)
    Radio11=ON
  ;;
  IPTSIN)
    Radio12=ON
  ;;
  IPTSIN264)
    Radio13=ON
  ;;
  IPTSIN265)
    Radio14=ON
  ;;
  *)
    Radio15=ON
  ;;
  esac

  chinput=$(whiptail --title "$StrInputSetupTitle" --radiolist \
    "$StrInputSetupDescription" 20 78 15 \
    "CAM16MPEG-2" "MPEG-2 1024x576 16:9 Pi Cam with Audio" $Radio1 \
    "CAMHDMPEG-2" "MPEG-2 1280x720 HD Pi Cam with Audio" $Radio2 \
    "ANALOG16MPEG-2" "MPEG-2 1024x576 16:9 Comp Vid with Audio" $Radio3 \
    "WEBCAM16MPEG-2" "MPEG-2 1024x576 16:9 USB Webcam with Audio" $Radio4 \
    "WEBCAMHDMPEG-2" "MPEG-2 1280x720 HD USB Webcam with Audio" $Radio5 \
    "CARD16MPEG-2" "MPEG-2 1024x576 HD 16:9 Static Test Card with Audio" $Radio6 \
    "CARDHDMPEG-2" "MPEG-2 1280x720 HD 16:9 Static Test Card with Audio" $Radio7 \
    "C920H264" "H264 640x480 with Audio from C920 Webcam" $Radio8 \
    "C920HDH264" "H264 1024x720 with Audio from C920 Webcam" $Radio9 \
    "C920FHDH264" "H264 1920x1080 with Audio from C920 Webcam" $Radio10 \
    "HDMI" "H264 HDMI from Elgato CamLink 4K" $Radio11 \
    "IPTSIN" "IPTS With Service Information" $Radio12 \
    "IPTSIN264" "H264 IPTS Without Service Information" $Radio13 \
    "IPTSIN265" "H265 IPTS Without Service Information" $Radio14 \
    "OTHER" "Non-Widescreen Mode" $Radio15 \
  3>&2 2>&1 1>&3)

  if [ $? -eq 0 ]; then
    case "$chinput" in

    "CAM16MPEG-2" | "CAMHDMPEG-2")
      # Make sure that the camera driver is loaded
      lsmod | grep -q 'bcm2835_v4l2'
      if [ $? != 0 ]; then   ## not loaded
        sudo modprobe bcm2835_v4l2
      fi
    ;;
    ANALOG16MPEG-2)
      ANALOGCAMNAME=$(get_config_var analogcamname $PCONFIGFILE)
      Radio1=OFF
      Radio2=OFF
      Radio3=OFF

      case "$ANALOGCAMNAME" in
        "/dev/video0")
          Radio1=ON
        ;;
        "/dev/video1")
          Radio2=ON
        ;;
        auto)
          Radio3=ON
        ;;
        *)
          Radio1=ON
        ;;
      esac
      newcamname=$(whiptail --title "$StrInputSetupANALOGCAMName" --radiolist \
        "$StrInputSetupANALOGCAMTitle" 20 78 5 \
        "/dev/video0" "Normal with no PiCam" $Radio1 \
        "/dev/video1" "Sometimes required with PiCam" $Radio2 \
        "auto" "Automatically select device name" $Radio3 \
        3>&2 2>&1 1>&3)
      if [ $? -eq 0 ]; then
         set_config_var analogcamname "$newcamname" $PCONFIGFILE
      fi
    ;;
    HDMI)
      do_output_format
    ;;
    esac
    if [ "$chinput" != "OTHER" ]; then
      set_config_var modeinput "$chinput" $PCONFIGFILE
    fi
  fi
}

do_station_setup()
{
  CALL=$(get_config_var call $PCONFIGFILE)
  CALL=$(whiptail --inputbox "$StrCallContext" 8 78 $CALL --title "$StrCallTitle" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var call "$CALL" $PCONFIGFILE
  fi

  LOCATOR=$(get_config_var locator $PCONFIGFILE)
  LOCATOR=$(whiptail --inputbox "$StrLocatorContext" 8 78 $LOCATOR --title "$StrLocatorTitle" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var locator "$LOCATOR" $PCONFIGFILE
  fi
}

do_output_setup_mode()
{
  MODE_OUTPUT=$(get_config_var modeoutput $PCONFIGFILE)
  Radio1=OFF
  Radio2=OFF
  Radio3=OFF
  Radio4=OFF
  Radio5=OFF
  Radio6=OFF
  Radio7=OFF
  Radio8=OFF
  Radio9=OFF

  case "$MODE_OUTPUT" in
  STREAMER)
    Radio1=ON
  ;;
  DATVEXPRESS)
    Radio2=ON
  ;;
  IP)
    Radio3=ON
  ;;
  COMPVID)
    Radio4=ON
  ;;
  LIMEMINI)
    Radio5=ON
  ;;
  LIMEUSB)
    Radio6=ON
  ;;
  LIMEDVB)
    Radio7=ON
  ;;
  PLUTO)
    Radio8=ON
  ;;
  *)
    Radio9=ON
  ;;
  esac

  choutput=$(whiptail --title "$StrOutputSetupTitle" --radiolist \
    "$StrOutputSetupContext" 20 78 11 \
    "STREAMER" "Stream to BATC or other Streaming Facility" $Radio1 \
    "DATVEXPRESS" "$StrOutputSetupDATVExpress" $Radio2 \
    "IP" "$StrOutputSetupIP" $Radio3 \
    "COMPVID" "Output PAL Comp Video from Raspberry Pi AV Socket" $Radio4 \
    "LIMEMINI" "Transmit using a LimeSDR Mini" $Radio5 \
    "LIMEUSB" "Transmit using a LimeSDR USB" $Radio6 \
    "LIMEDVB" "Use a LimeSDR Mini with Custom DVB Firmware" $Radio7 \
    "PLUTO" "Use a Pluto" $Radio8 \
    3>&2 2>&1 1>&3)

  if [ $? -eq 0 ]; then
    case "$choutput" in
    STREAMER)
      STREAM_URL=$(get_config_var streamurl $PCONFIGFILE)
      STREAM=$(whiptail --inputbox "Enter the stream URL: rtmp://rtmp.batc.org.uk/live" 8 78\
        $STREAM_URL --title "Enter Stream Details" 3>&1 1>&2 2>&3)
      if [ $? -eq 0 ]; then
        set_config_var streamurl "$STREAM" $PCONFIGFILE
      fi
      STREAM_KEY=$(get_config_var streamkey $PCONFIGFILE)
      STREAMK=$(whiptail --inputbox "Enter the streamname and key" 8 78 $STREAM_KEY --title "Enter Stream Details" 3>&1 1>&2 2>&3)
      if [ $? -eq 0 ]; then
        set_config_var streamkey "$STREAMK" $PCONFIGFILE
      fi
    ;;
    DATVEXPRESS)
      echo "Starting the DATV Express Server.  Please wait."
      if pgrep -x "express_server" > /dev/null; then
        # Express already running
        sudo killall express_server  >/dev/null 2>/dev/null
      fi
      # Make sure that the Control file is not locked
      sudo rm /tmp/expctrl >/dev/null 2>/dev/null
      # Start Express from its own folder otherwise it doesn't read the config file
      cd /home/pi/express_server
      sudo nice -n -40 /home/pi/express_server/express_server  >/dev/null 2>/dev/null &
      cd /home/pi
      sleep 5
      # Set the ports
      $PATHSCRIPT"/ctlfilter.sh"
    ;;
    IP)
      UDPOUTADDR=$(get_config_var udpoutaddr $PCONFIGFILE)
      UDPOUTADDR=$(whiptail --inputbox "$StrOutputSetupIPTSOUTName" 8 78 $UDPOUTADDR --title "$StrOutputSetupIPTSOUTTitle" 3>&1 1>&2 2>&3)
      if [ $? -eq 0 ]; then
        set_config_var udpoutaddr "$UDPOUTADDR" $PCONFIGFILE
      fi
    ;;
    COMPVID)
      :
    ;;
    LIMEMINI)
      :
    ;;
    LIMEUSB)
      :
    ;;
    LIMEDVB)
      :
    ;;
    PLUTO)
      :
    ;;
    esac
    set_config_var modeoutput "$choutput" $PCONFIGFILE
  fi
}

do_symbolrate_setup()
{
  SYMBOLRATE=$(get_config_var symbolrate $PCONFIGFILE)
  SYMBOLRATE=$(whiptail --inputbox "$StrOutputSymbolrateContext" 8 78 $SYMBOLRATE --title "$StrOutputSymbolrateTitle" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var symbolrate "$SYMBOLRATE" $PCONFIGFILE
  fi
}


do_fec_lookup()
{
  case "$FEC" in
  1) 
    FECNUM=1
    FECDEN=2
  ;;
  2)
    FECNUM=2
    FECDEN=3
  ;;
  3)
    FECNUM=3
    FECDEN=4
  ;;
  5)
    FECNUM=5
    FECDEN=6
  ;;
  7)
    FECNUM=7
    FECDEN=8
  ;;
  14)
    FECNUM=1
    FECDEN=4
  ;;
  13)
    FECNUM=1
    FECDEN=3
  ;;
  12)
    FECNUM=1
    FECDEN=2
  ;;
  35)
    FECNUM=3
    FECDEN=5
  ;;
  23)
    FECNUM=2
    FECDEN=3
  ;;
  34)
    FECNUM=3
    FECDEN=4
  ;;
  56)
    FECNUM=5
    FECDEN=6
  ;;
  89)
    FECNUM=8
    FECDEN=9
  ;;
  91)
    FECNUM=9
    FECDEN=10
  ;;
  *)
    FECNUM=0
    FECDEN=0
  ;;
  esac
}


do_fec_setup()
{
  Radio1=OFF
  Radio2=OFF
  Radio3=OFF
  Radio4=OFF
  Radio5=OFF
  Radio6=OFF
  Radio7=OFF
  Radio8=OFF
  Radio9=OFF
  Radio10=OFF
  Radio11=OFF
  Radio12=OFF
  Radio13=OFF
  Radio14=OFF

  FEC=$(get_config_var fec $PCONFIGFILE)
  case "$FEC" in
  1) 
    Radio1=ON
  ;;
  2)
    Radio2=ON
  ;;
  3)
    Radio3=ON
  ;;
  5)
    Radio4=ON
  ;;
  7)
    Radio5=ON
  ;;
  14)
    Radio6=ON
  ;;
  13)
    Radio7=ON
  ;;
  12)
    Radio8=ON
  ;;
  35)
    Radio9=ON
  ;;
  23)
    Radio10=ON
  ;;
  34)
    Radio11=ON
  ;;
  56)
    Radio12=ON
  ;;
  89)
    Radio13=ON
  ;;
  91)
    Radio14=ON
  ;;
  *)
    Radio1=ON
  ;;
  esac

  FEC=$(whiptail --title "$StrOutputFECTitle" --radiolist \
    "$StrOutputFECContext" 20 78 14 \
    "1" "DVB-S FEC 1/2" $Radio1 \
    "2" "DVB-S FEC 2/3" $Radio2 \
    "3" "DVB-S FEC 3/4" $Radio3 \
    "5" "DVB-S FEC 5/6" $Radio4 \
    "7" "DVB-S FEC 7/8" $Radio5 \
    "14" "DVB-S2 QPSK FEC 1/4" $Radio6 \
    "13" "DVB-S2 QPSK FEC 1/3" $Radio7 \
    "12" "DVB-S2 QPSK FEC 1/2" $Radio8 \
    "35" "DVB-S2 FEC 3/5" $Radio9 \
    "23" "DVB-S2 FEC 2/3" $Radio10 \
    "34" "DVB-S2 FEC 3/4" $Radio11 \
    "56" "DVB-S2 FEC 5/6" $Radio12 \
    "89" "DVB-S2 FEC 8/9" $Radio13 \
    "91" "DVB-S2 FEC 9/10" $Radio14 \
  3>&2 2>&1 1>&3)

  if [ $? -eq 0 ]; then
    set_config_var fec "$FEC" $PCONFIGFILE
    do_fec_lookup
  fi
}


do_PID_setup()
{
  PIDPMT=$(get_config_var pidpmt $PCONFIGFILE)
  PIDPMT=$(whiptail --inputbox "$StrPIDSetupContext" 18 78 $PIDPMT --title "$StrPIDSetupTitle" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var pidpmt "$PIDPMT" $PCONFIGFILE
  fi
  PIDPCR=$(get_config_var pidstart $PCONFIGFILE)
  PIDPCR=$(whiptail --inputbox "PCR PID - Not Implemented Yet. Will be set same as Video PID" 8 78 $PIDPCR --title "$StrPIDSetupTitle" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var pidstart "$PIDPCR" $PCONFIGFILE
  fi
  PIDVIDEO=$(get_config_var pidvideo $PCONFIGFILE)
  PIDVIDEO=$(whiptail --inputbox "Video PID" 8 78 $PIDVIDEO --title "$StrPIDSetupTitle" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var pidvideo "$PIDVIDEO" $PCONFIGFILE
  fi
  PIDAUDIO=$(get_config_var pidaudio $PCONFIGFILE)
  PIDAUDIO=$(whiptail --inputbox "Audio PID" 8 78 $PIDAUDIO --title "$StrPIDSetupTitle" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var pidaudio "$PIDAUDIO" $PCONFIGFILE
  fi

  whiptail --title "PID Selection Check - but not all will be set by software yet!" \
    --msgbox "PMT: "$PIDPMT" PCR: "$PIDPCR" Video: "$PIDVIDEO" Audio: "$PIDAUDIO".  Press any key to continue" 8 78
}

do_freq_setup()
{
  FREQ_OUTPUT=$(get_config_var freqoutput $PCONFIGFILE)
  FREQ_OUTPUT=$(whiptail --inputbox "$StrOutputRFFreqContext" 8 78 $FREQ_OUTPUT --title "$StrOutputRFFreqTitle" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var freqoutput "$FREQ_OUTPUT" $PCONFIGFILE
  fi

  # Now select transverters
  BAND=$(get_config_var band $PCONFIGFILE)
  Radio1=OFF
  Radio2=OFF
  Radio3=OFF
  Radio4=OFF
  Radio5=OFF
  case "$BAND" in
  d1)
    Radio1=ON
  ;;
  d2)
    Radio1=ON
  ;;
  d3)
    Radio1=ON
  ;;
  d4)
    Radio1=ON
  ;;
  d5)
    Radio1=ON
  ;;
  t1)
    Radio2=ON
  ;;
  t2)
    Radio3=ON
  ;;
  t3)
    Radio4=ON
  ;;
  t4)
    Radio5=ON
  ;;
  *)
    Radio1=ON
  ;;
  esac

  BAND=$(whiptail --title "SELECT TRANSVERTER OR DIRECT" --radiolist \
    "Select one" 20 78 8 \
    "Direct" "No Transverter" $Radio1 \
    "t1" "Transverter 1" $Radio2 \
    "t2" "Transverter 2" $Radio3 \
    "t3" "Transverter 3" $Radio4 \
    "t4" "Transverter 4" $Radio5 \
    3>&2 2>&1 1>&3)

  if [[ "$BAND" == "Direct" || "$BAND" == "d1" || "$BAND" == "d2" || "$BAND" == "d3" || "$BAND" == "d4" || "$BAND" == "d5" ]]; then 
    ## If direct, look up which band

    INT_FREQ_OUTPUT=${FREQ_OUTPUT%.*}       # Change frequency to integer

    if [ "$INT_FREQ_OUTPUT" -lt 100 ]; then
      BAND=d1;
    elif [ "$INT_FREQ_OUTPUT" -lt 250 ]; then
      BAND=d2;
    elif [ "$INT_FREQ_OUTPUT" -lt 950 ]; then
      BAND=d3;
    elif [ "$INT_FREQ_OUTPUT" -lt 2000 ]; then
      BAND=d4;
    elif [ "$INT_FREQ_OUTPUT" -lt 4400 ]; then
      BAND=d5;
    else
      BAND=d1;
    fi
  fi

  set_config_var band "$BAND" $PCONFIGFILE

  ## Now set the band levels and labels

  case "$BAND" in
  d1)
    ATTENLEVEL=$(get_config_var d1attenlevel $PATH_PPRESETS)
    set_config_var attenlevel "$ATTENLEVEL" $PCONFIGFILE
    LIMEGAIN=$(get_config_var d1limegain $PATH_PPRESETS)
    set_config_var limegain "$LIMEGAIN" $PCONFIGFILE
    EXPLEVEL=$(get_config_var d1explevel $PATH_PPRESETS)
    set_config_var explevel "$EXPLEVEL" $PCONFIGFILE
    EXPPORTS=$(get_config_var d1expports $PATH_PPRESETS)
    set_config_var expports "$EXPPORTS" $PCONFIGFILE
    LABEL=$(get_config_var d1label $PATH_PPRESETS)
    set_config_var labelofband "$LABEL" $PCONFIGFILE
    NUMBERS=$(get_config_var d1numbers $PATH_PPRESETS)
    set_config_var numbers "$NUMBERS" $PCONFIGFILE
  ;;
  d2)
    ATTENLEVEL=$(get_config_var d2attenlevel $PATH_PPRESETS)
    set_config_var attenlevel "$ATTENLEVEL" $PCONFIGFILE
    LIMEGAIN=$(get_config_var d2limegain $PATH_PPRESETS)
    set_config_var limegain "$LIMEGAIN" $PCONFIGFILE
    EXPLEVEL=$(get_config_var d2explevel $PATH_PPRESETS)
    set_config_var explevel "$EXPLEVEL" $PCONFIGFILE
    EXPPORTS=$(get_config_var d2expports $PATH_PPRESETS)
    set_config_var expports "$EXPPORTS" $PCONFIGFILE
    LABEL=$(get_config_var d2label $PATH_PPRESETS)
    set_config_var labelofband "$LABEL" $PCONFIGFILE
    NUMBERS=$(get_config_var d2numbers $PATH_PPRESETS)
    set_config_var numbers "$NUMBERS" $PCONFIGFILE
  ;;
  d3)
    ATTENLEVEL=$(get_config_var d3attenlevel $PATH_PPRESETS)
    set_config_var attenlevel "$ATTENLEVEL" $PCONFIGFILE
    LIMEGAIN=$(get_config_var d3limegain $PATH_PPRESETS)
    set_config_var limegain "$LIMEGAIN" $PCONFIGFILE
    EXPLEVEL=$(get_config_var d3explevel $PATH_PPRESETS)
    set_config_var explevel "$EXPLEVEL" $PCONFIGFILE
    EXPPORTS=$(get_config_var d3expports $PATH_PPRESETS)
    set_config_var expports "$EXPPORTS" $PCONFIGFILE
    LABEL=$(get_config_var d3label $PATH_PPRESETS)
    set_config_var labelofband "$LABEL" $PCONFIGFILE
    NUMBERS=$(get_config_var d3numbers $PATH_PPRESETS)
    set_config_var numbers "$NUMBERS" $PCONFIGFILE
  ;;
  d4)
    ATTENLEVEL=$(get_config_var d4attenlevel $PATH_PPRESETS)
    set_config_var attenlevel "$ATTENLEVEL" $PCONFIGFILE
    LIMEGAIN=$(get_config_var d4limegain $PATH_PPRESETS)
    set_config_var limegain "$LIMEGAIN" $PCONFIGFILE
    EXPLEVEL=$(get_config_var d4explevel $PATH_PPRESETS)
    set_config_var explevel "$EXPLEVEL" $PCONFIGFILE
    EXPPORTS=$(get_config_var d4expports $PATH_PPRESETS)
    set_config_var expports "$EXPPORTS" $PCONFIGFILE
    LABEL=$(get_config_var d4label $PATH_PPRESETS)
    set_config_var labelofband "$LABEL" $PCONFIGFILE
    NUMBERS=$(get_config_var d4numbers $PATH_PPRESETS)
    set_config_var numbers "$NUMBERS" $PCONFIGFILE
  ;;
  d5)
    ATTENLEVEL=$(get_config_var d5attenlevel $PATH_PPRESETS)
    set_config_var attenlevel "$ATTENLEVEL" $PCONFIGFILE
    LIMEGAIN=$(get_config_var d5limegain $PATH_PPRESETS)
    set_config_var limegain "$LIMEGAIN" $PCONFIGFILE
    EXPLEVEL=$(get_config_var d5explevel $PATH_PPRESETS)
    set_config_var explevel "$EXPLEVEL" $PCONFIGFILE
    EXPPORTS=$(get_config_var d5expports $PATH_PPRESETS)
    set_config_var expports "$EXPPORTS" $PCONFIGFILE
    LABEL=$(get_config_var d5label $PATH_PPRESETS)
    set_config_var labelofband "$LABEL" $PCONFIGFILE
    NUMBERS=$(get_config_var d5numbers $PATH_PPRESETS)
    set_config_var numbers "$NUMBERS" $PCONFIGFILE
  ;;
  t1)
    ATTENLEVEL=$(get_config_var t1attenlevel $PATH_PPRESETS)
    set_config_var attenlevel "$ATTENLEVEL" $PCONFIGFILE
    LIMEGAIN=$(get_config_var t1limegain $PATH_PPRESETS)
    set_config_var limegain "$LIMEGAIN" $PCONFIGFILE
    EXPLEVEL=$(get_config_var t1explevel $PATH_PPRESETS)
    set_config_var explevel "$EXPLEVEL" $PCONFIGFILE
    EXPPORTS=$(get_config_var t1expports $PATH_PPRESETS)
    set_config_var expports "$EXPPORTS" $PCONFIGFILE
    LABEL=$(get_config_var t1label $PATH_PPRESETS)
    set_config_var labelofband "$LABEL" $PCONFIGFILE
    NUMBERS=$(get_config_var t1numbers $PATH_PPRESETS)
    set_config_var numbers "$NUMBERS" $PCONFIGFILE
  ;;
  t2)
    ATTENLEVEL=$(get_config_var t2attenlevel $PATH_PPRESETS)
    set_config_var attenlevel "$ATTENLEVEL" $PCONFIGFILE
    LIMEGAIN=$(get_config_var t2limegain $PATH_PPRESETS)
    set_config_var limegain "$LIMEGAIN" $PCONFIGFILE
    EXPLEVEL=$(get_config_var t2explevel $PATH_PPRESETS)
    set_config_var explevel "$EXPLEVEL" $PCONFIGFILE
    EXPPORTS=$(get_config_var t2expports $PATH_PPRESETS)
    set_config_var expports "$EXPPORTS" $PCONFIGFILE
    LABEL=$(get_config_var t2label $PATH_PPRESETS)
    set_config_var labelofband "$LABEL" $PCONFIGFILE
    NUMBERS=$(get_config_var t2numbers $PATH_PPRESETS)
    set_config_var numbers "$NUMBERS" $PCONFIGFILE
  ;;
  t3)
    ATTENLEVEL=$(get_config_var t3attenlevel $PATH_PPRESETS)
    set_config_var attenlevel "$ATTENLEVEL" $PCONFIGFILE
    LIMEGAIN=$(get_config_var t3limegain $PATH_PPRESETS)
    set_config_var limegain "$LIMEGAIN" $PCONFIGFILE
    EXPLEVEL=$(get_config_var t3explevel $PATH_PPRESETS)
    set_config_var explevel "$EXPLEVEL" $PCONFIGFILE
    EXPPORTS=$(get_config_var t3expports $PATH_PPRESETS)
    set_config_var expports "$EXPPORTS" $PCONFIGFILE
    LABEL=$(get_config_var t3label $PATH_PPRESETS)
    set_config_var labelofband "$LABEL" $PCONFIGFILE
    NUMBERS=$(get_config_var t3numbers $PATH_PPRESETS)
    set_config_var numbers "$NUMBERS" $PCONFIGFILE
  ;;
  t4)
    ATTENLEVEL=$(get_config_var t4attenlevel $PATH_PPRESETS)
    set_config_var attenlevel "$ATTENLEVEL" $PCONFIGFILE
    LIMEGAIN=$(get_config_var t4limegain $PATH_PPRESETS)
    set_config_var limegain "$LIMEGAIN" $PCONFIGFILE
    EXPLEVEL=$(get_config_var t4explevel $PATH_PPRESETS)
    set_config_var explevel "$EXPLEVEL" $PCONFIGFILE
    EXPPORTS=$(get_config_var t4expports $PATH_PPRESETS)
    set_config_var expports "$EXPPORTS" $PCONFIGFILE
    LABEL=$(get_config_var t4label $PATH_PPRESETS)
    set_config_var labelofband "$LABEL" $PCONFIGFILE
    NUMBERS=$(get_config_var t4numbers $PATH_PPRESETS)
    set_config_var numbers "$NUMBERS" $PCONFIGFILE
  ;;
  *)
    ATTENLEVEL=$(get_config_var d1attenlevel $PATH_PPRESETS)
    set_config_var attenlevel "$ATTENLEVEL" $PCONFIGFILE
    LIMEGAIN=$(get_config_var d1limegain $PATH_PPRESETS)
    set_config_var limegain "$LIMEGAIN" $PCONFIGFILE
    EXPLEVEL=$(get_config_var d1explevel $PATH_PPRESETS)
    set_config_var explevel "$EXPLEVEL" $PCONFIGFILE
    EXPPORTS=$(get_config_var d1expports $PATH_PPRESETS)
    set_config_var expports "$EXPPORTS" $PCONFIGFILE
    LABEL=$(get_config_var d1label $PATH_PPRESETS)
    set_config_var labelofband "$LABEL" $PCONFIGFILE
    NUMBERS=$(get_config_var d1numbers $PATH_PPRESETS)
    set_config_var numbers "$NUMBERS" $PCONFIGFILE
  ;;
  esac

  $PATHSCRIPT"/ctlfilter.sh" ## Refresh the band and port switching
}

do_caption_setup()
{
  CAPTION=$(get_config_var caption $PCONFIGFILE)
  Radio1=OFF
  Radio2=OFF
  case "$CAPTION" in
  on)
    Radio1=ON
  ;;
  off)
    Radio2=ON
  ;;
  *)
    Radio1=ON
  ;;
  esac

  CAPTION=$(whiptail --title "SET CAPTION ON OR OFF (MPEG-2 ONLY)" --radiolist \
    "Select one" 20 78 8 \
    "on" "Callsign Caption inserted on transmitted signal" $Radio1 \
    "off" "No Callsign Caption" $Radio2 \
    3>&2 2>&1 1>&3)

  if [ $? -eq 0 ]; then                     ## If the selection has changed
    set_config_var caption "$CAPTION" $PCONFIGFILE
  fi
}

do_output_standard()
{
  OPSTD=$(get_config_var outputstandard $PCONFIGFILE)
  Radio1=OFF
  Radio2=OFF
  case "$OPSTD" in
  576)
    Radio1=ON
  ;;
  480)
    Radio2=ON
  ;;
  *)
    Radio1=ON
  ;;
  esac

  OPSTD=$(whiptail --title "SET OUTPUT STANDARD" --radiolist \
    "Select one" 20 78 8 \
    "576" "DATV 576x720 25 fps, Comp Video PAL" $Radio1 \
    "480" "DATV 480x720 30 fps, Comp Video NTSC" $Radio2 \
    3>&2 2>&1 1>&3)

  if [ $? -eq 0 ]; then                     ## If the selection has changed
    set_config_var outputstandard "$OPSTD" $PCONFIGFILE
  fi
}

do_output_format()
{
  FORMAT=$(get_config_var format $PCONFIGFILE)
  Radio1=OFF
  Radio2=OFF
  Radio3=OFF
  Radio4=OFF

  case "$FORMAT" in
  4:3)
    Radio1=ON
  ;;
  16:9)
    Radio2=ON
  ;;
  720p)
    Radio3=ON
  ;;
  1080p)
    Radio4=ON
  ;;
  *)
    Radio1=ON
  ;;
  esac

  FORMAT=$(whiptail --title "SET VIDEO FORMAT" --radiolist \
    "Select one" 20 78 5 \
    "4:3" "Aspect ratio 4:3" $Radio1 \
    "16:9" "Aspect Ratio 16:9" $Radio2 \
    "720p" "720p Definition" $Radio3 \
    "1080p" "1080p Definition" $Radio4 \
    3>&2 2>&1 1>&3)

  if [ $? -eq 0 ]; then                     ## If the selection has changed
    set_config_var format "$FORMAT" $PCONFIGFILE
  fi
}


do_modulation()
{
  MODULATION=$(get_config_var modulation $PCONFIGFILE)
  Radio1=OFF
  Radio2=OFF
  Radio3=OFF
  Radio4=OFF
  Radio5=OFF

  case "$MODULATION" in
  DVB-S)
    Radio1=ON
  ;;
  S2QPSK)
    Radio2=ON
  ;;
  8PSK)
    Radio3=ON
  ;;
  16APSK)
    Radio4=ON
  ;;
  32APSK)
    Radio5=ON
  ;;
  *)
    Radio1=ON
  ;;
  esac

  MODULATION=$(whiptail --title "SET MODULATION" --radiolist \
    "Select one" 20 78 8 \
    "DVB-S" "DVB-S" $Radio1 \
    "S2QPSK" "DVB-S2 QPSK (Lime/Pluto only)" $Radio2 \
    "8PSK" "DVB-S2 8PSK (Lime/Pluto only)" $Radio3 \
    "16APSK" "DVB-S2 16APSK (Lime/Pluto only)" $Radio4 \
    "32APSK" "DVB-S2 32APSK (Lime/pluto only)" $Radio5 \
    3>&2 2>&1 1>&3)

  if [ $? -eq 0 ]; then                     ## If the selection has changed
    set_config_var modulation "$MODULATION" $PCONFIGFILE
  fi
}


do_mic_gain()
{
  MICGAIN=$(get_config_var micgain $PCONFIGFILE)
  MICGAIN=$(whiptail --inputbox "Enter between 1 (min) and 30 (max)" 8 78 $MICGAIN --title "Set USB Dongle Mic Gain" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var micgain "$MICGAIN" $PCONFIGFILE
  fi
}


do_output_setup() {
menuchoice=$(whiptail --title "$StrOutputTitle" --menu "$StrOutputContext" 16 78 10 \
  "1 SymbolRate" "$StrOutputSR"  \
  "2 FEC" "$StrOutputFEC" \
  "3 Output mode" "$StrOutputMode" \
  "4 PID" "$StrPIDSetup" \
  "5 Frequency" "$StrOutputRFFreqContext" \
  "6 Caption" "Callsign Caption in MPEG-2 on/off" \
  "7 Standard" "Output 576PAL or 480NTSC" \
  "8 Format" "4:3, 16:9, 720p or 1080p" \
  "9 Modulation" "DVB-S or DVB-S2 modes" \
  "10 Mic Gain" "Adjust USB Dongle Mic Gain" \
	3>&2 2>&1 1>&3)
	case "$menuchoice" in
      1\ *) do_symbolrate_setup ;;
      2\ *) do_fec_setup   ;;
	  3\ *) do_output_setup_mode ;;
	  4\ *) do_PID_setup ;;
	  5\ *) do_freq_setup ;;
	  6\ *) do_caption_setup ;;
	  7\ *) do_output_standard ;;
	  8\ *) do_output_format ;;
	  9\ *) do_modulation ;;
	  10\ *) do_mic_gain ;;
    esac
}


do_set_DVBS_FEC()
{
  FEC="7"
  do_fec_lookup
  INFO=$CALL": "$MODE_INPUT" -> "$MODE_OUTPUT" "$FREQ_OUTPUT" MHz "$SYMBOLRATEK" KS, "$MODULATION", FEC "$FECNUM"/"$FECDEN""
  set_config_var fec "$FEC" $PCONFIGFILE
}

 
do_set_DVBS2_FEC()
{
  FEC="91"
  do_fec_lookup
  INFO=$CALL": "$MODE_INPUT" -> "$MODE_OUTPUT" "$FREQ_OUTPUT" MHz "$SYMBOLRATEK" KS, "$MODULATION", FEC "$FECNUM"/"$FECDEN""
  set_config_var fec "$FEC" $PCONFIGFILE
}


do_check_FEC()
{
  if [ "$MODULATION" == "DVB-S" ]; then
    case "$FEC" in
      14) do_set_DVBS_FEC ;;
      13) do_set_DVBS_FEC ;;
      12) do_set_DVBS_FEC ;;
      35) do_set_DVBS_FEC ;;
      23) do_set_DVBS_FEC ;;
      34) do_set_DVBS_FEC ;;
      56) do_set_DVBS_FEC ;;
      89) do_set_DVBS_FEC ;;
      91) do_set_DVBS_FEC ;;
    esac
  elif [ "$MODULATION" == "S2QPSK" ]; then
    case "$FEC" in
      1) do_set_DVBS2_FEC ;;
      2) do_set_DVBS2_FEC ;;
      3) do_set_DVBS2_FEC ;;
      5) do_set_DVBS2_FEC ;;
      7) do_set_DVBS2_FEC ;;
    esac
  elif [ "$MODULATION" == "8PSK" ]; then
    case "$FEC" in
      1) do_set_DVBS2_FEC ;;
      2) do_set_DVBS2_FEC ;;
      3) do_set_DVBS2_FEC ;;
      5) do_set_DVBS2_FEC ;;
      7) do_set_DVBS2_FEC ;;
      14) do_set_DVBS2_FEC ;;
      13) do_set_DVBS2_FEC ;;
      12) do_set_DVBS2_FEC ;;
    esac
  elif [ "$MODULATION" == "16APSK" ]; then
    case "$FEC" in
      1) do_set_DVBS2_FEC ;;
      2) do_set_DVBS2_FEC ;;
      3) do_set_DVBS2_FEC ;;
      5) do_set_DVBS2_FEC ;;
      7) do_set_DVBS2_FEC ;;
      14) do_set_DVBS2_FEC ;;
      13) do_set_DVBS2_FEC ;;
      12) do_set_DVBS2_FEC ;;
      35) do_set_DVBS2_FEC ;;
    esac
  elif [ "$MODULATION" == "32APSK" ]; then
    case "$FEC" in
      1) do_set_DVBS2_FEC ;;
      2) do_set_DVBS2_FEC ;;
      3) do_set_DVBS2_FEC ;;
      5) do_set_DVBS2_FEC ;;
      7) do_set_DVBS2_FEC ;;
      14) do_set_DVBS2_FEC ;;
      13) do_set_DVBS2_FEC ;;
      12) do_set_DVBS2_FEC ;;
      35) do_set_DVBS2_FEC ;;
      23) do_set_DVBS2_FEC ;;
    esac
  fi
}


do_transmit() 
{
  TX_OK=YES
  AVC2TS_FAILED_ONCE=NO


  # Check the FEC is valid for DVB-S or DVB-S2
  do_check_FEC

  if [ "$MODE_STARTUP" == "TX_boot" ] && { [ "$MODE_INPUT" == "ANALOGCAM" ] || [ "$MODE_INPUT" == "HDMI" ]; }; then  

                                                      # Super resilient script for repeater

    $PATHSCRIPT"/a.sh" >/dev/null 2>/dev/null &       # Call a.sh in an additional process to start the transmitter

    if [ "$MODE_OUTPUT" == "LIMEMINI" ]; then         # Turn the PTT on after a delay for the Lime
      /home/pi/rpidatv/scripts/lime_ptt.sh &
    fi
    if [ "$MODE_OUTPUT" == "LIMEDVB" ]; then
      /home/pi/rpidatv/scripts/lime_ptt.sh &
    fi
    if [ "$MODE_OUTPUT" == "LIMEUSB" ]; then
      /home/pi/rpidatv/scripts/lime_ptt.sh &
    fi

    sleep 20                                          # Wait for things to settle

    while [ TRUE ]                                    # Monitor the transmit processes
    do
      v4l2-ctl --list-devices | grep -E -q "usbtv|Cam Link 4K" 
 
                                                      # Check EasyCap or Cam Link HDMI
      if [ $? == 1 ]; then                            # EasyCap has crashed, so
        do_stop_transmit                              # Stop transmit.  Should have reboot but it doesn't work
      fi

      ps -el | grep -E -q "limesdr_dvb|limesdr_send"  # Check Lime process is running
      if [ $? == 1 ]; then
        TX_OK=NO                                      # Not running, so restart TX
      fi

      ps -el | grep -E -q "avc2ts"                    # Check encoding process is running
      if [ $? == 1 ]; then                            # avc2ts not running, so restart TX
        TX_OK=NO
      else                                            # It is running, is it doing anything?
        AVC2TS_LOAD=$(top -b -n 2 -d 0.2 -p $(pgrep -d',' avc2ts) | tail -1 | awk '{print $9}')
        THRESHOLD=2.0
        if (( $(echo "$AVC2TS_LOAD < $THRESHOLD" |bc -l) )); then
          if [ "$AVC2TS_FAILED_ONCE" == "YES" ]; then # Needs to happen twice before restart
            TX_OK=NO
            AVC2TS_FAILED_ONCE=NO                     # Reset counter
          else
            AVC2TS_FAILED_ONCE=YES                    # For next time      
          fi
        else
          AVC2TS_FAILED_ONCE=NO                       # Reset counter
        fi
      fi

      if [ "$TX_OK" == "NO" ]; then                   # If not running, stop and restart TX
        ps -el | grep -E -q "limesdr_stopcha"         # If limesdrstop channel is running
        if [ $? == 0 ]; then
          sleep 4
          sudo killall -9 avc2ts
          sudo killall -9 limesdr_stopchannel
          do_stop_transmit
          sleep 5
          sudo shutdown -r now                        # only a reboot will sort it
        fi

        do_stop_transmit

        # Call a.sh in an additional process to restart the transmitter
        $PATHSCRIPT"/a.sh" >/dev/null 2>/dev/null &

        # Turn the PTT on after a delay for the Lime
        # rpidatv turns it on for other modes
        if [ "$MODE_OUTPUT" == "LIMEMINI" ]; then
          /home/pi/rpidatv/scripts/lime_ptt.sh &
        fi
        if [ "$MODE_OUTPUT" == "LIMEDVB" ]; then
          /home/pi/rpidatv/scripts/lime_ptt.sh &
        fi
        if [ "$MODE_OUTPUT" == "LIMEUSB" ]; then
          /home/pi/rpidatv/scripts/lime_ptt.sh &
        fi
        TX_OK=YES                                     # Assume that TX is OK until next check
      fi
      sleep 20
    done
  else

    # Call a.sh in an additional process to start the transmitter
    $PATHSCRIPT"/a.sh" >/dev/null 2>/dev/null &

    # Call TXstartextras.sh in an additional process
    $PATHSCRIPT"/TXstartextras.sh" >/dev/null 2>/dev/null &

    # Start the Viewfinder display if user sets it on
    if [ "$V_FINDER" == "on" ]; then
      do_display_on
    else
      do_display_off
    fi

    # Turn the PTT on after a delay for the Lime
    # rpidatv turns it on for other modes
    if [ "$MODE_OUTPUT" == "LIMEMINI" ]; then
      /home/pi/rpidatv/scripts/lime_ptt.sh &
    fi
    if [ "$MODE_OUTPUT" == "LIMEDVB" ]; then
      /home/pi/rpidatv/scripts/lime_ptt.sh &
    fi
    if [ "$MODE_OUTPUT" == "LIMEUSB" ]; then
      /home/pi/rpidatv/scripts/lime_ptt.sh &
    fi

    # Wait here transmitting until user presses a key
    whiptail --title "$StrStatusTitle" --msgbox "$INFO" 8 78
  fi

  # Stop the transmit processes and clean up
  do_stop_transmit
  do_display_off
}

do_stop_transmit()
{
  # Make sure that the PTT is released (required for carrier, Lime and test modes)
  gpio mode $GPIO_PTT out
  gpio write $GPIO_PTT 0

  # Stop DATV Express transmitting if required
  if [ "$MODE_OUTPUT" == "DATVEXPRESS" ]; then
    echo "set car off" >> /tmp/expctrl
    echo "set ptt rx" >> /tmp/expctrl
    sudo killall netcat >/dev/null 2>/dev/null
  fi

  # Call TXstopextras.sh in an additional process
  $PATHSCRIPT"/TXstop.sh" >/dev/null 2>/dev/null &
  $PATHSCRIPT"/TXstopextras.sh" >/dev/null 2>/dev/null &

  # Turn the Local Oscillator off
  sudo $PATHRPI"/adf4351" off

  # Kill the key processes as nicely as possible
  sudo killall rpidatv >/dev/null 2>/dev/null
  sudo killall ffmpeg >/dev/null 2>/dev/null
  sudo killall tcanim1v16 >/dev/null 2>/dev/null
  sudo killall avc2ts >/dev/null 2>/dev/null
  sudo killall netcat >/dev/null 2>/dev/null
  sudo killall dvb2iq >/dev/null 2>/dev/null
  sudo killall limesdr_send >/dev/null 2>/dev/null
  sudo killall limesdr_dvb >/dev/null 2>/dev/null
  sudo killall sox >/dev/null 2>/dev/null

  # Then pause and make sure that avc2ts has really been stopped (needed at high SRs)
  sleep 0.1
  sudo killall -9 avc2ts >/dev/null 2>/dev/null

  # And make sure rpidatv has been stopped (required for brief transmit selections)
  sudo killall -9 rpidatv >/dev/null 2>/dev/null

  # Stop the audio for CompVid mode
  sudo killall arecord >/dev/null 2>/dev/null

  # And make sure limesdr_send has been stopped
  sudo killall -9 limesdr_send >/dev/null 2>/dev/null
  sudo killall -9 limesdr_dvb >/dev/null 2>/dev/null
  sudo killall -9 sox >/dev/null 2>/dev/null

  # Set the SR Filter correctly, because it might have been set all high by Lime
  /home/pi/rpidatv/scripts/ctlSR.sh

  # Reset the LimeSDR
  #/home/pi/rpidatv/bin/limesdr_stopchannel & # Fork as this sometimes hangs

  # Display the BATC Logo on the Touchscreen
  sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png >/dev/null 2>/dev/null
  (sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work

  # Kill a.sh which sometimes hangs during testing
  sudo killall a.sh >/dev/null 2>/dev/null

  # Check if driver for Logitech C270, C525 or C910 needs to be reloaded
  dmesg | grep -E -q "046d:0825|Webcam C525|046d:0821"
  if [ $? == 0 ]; then
    echo
    echo "Please wait for Webcam driver to be reset"
    sleep 3
    READY=0
    while [ $READY == 0 ]
    do
      v4l2-ctl --list-devices >/dev/null 2>/dev/null
      if [ $? == 1 ] ; then
        echo
        echo "Still waiting...."
        sleep 3
      else
        READY=1
      fi
    done
  fi
}

do_display_on()
{
  v4l2-ctl --overlay=1 >/dev/null 2>/dev/null
}

do_display_off()
{
  v4l2-ctl --overlay=0 >/dev/null 2>/dev/null
}

do_receive_status()
{
  whiptail --title "RECEIVE" --msgbox "$INFO" 8 78
  sudo killall rpidatvgui >/dev/null 2>/dev/null
  sudo killall leandvb >/dev/null 2>/dev/null
  sudo killall hello_video.bin >/dev/null 2>/dev/null
  sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png
}

do_receive()
{
  if pgrep -x "rtl_tcp" > /dev/null; then
    # rtl_tcp is running, so kill it, pause and really kill it
    killall rtl_tcp >/dev/null 2>/dev/null
    sleep 0.5
    sudo killall -9 rtl_tcp >/dev/null 2>/dev/null
  fi

  MODE_OUTPUT=$(get_config_var modeoutput $PCONFIGFILE)
  case "$MODE_OUTPUT" in
  BATC)
    ORGINAL_MODE_INPUT=$(get_config_var modeinput $PCONFIGFILE)
    sleep 0.1
    set_config_var modeinput "DESKTOP" $PCONFIGFILE
    sleep 0.1
    /home/pi/rpidatv/bin/rpidatvgui 0 1  >/dev/null 2>/dev/null & 
    $PATHSCRIPT"/a.sh" >/dev/null 2>/dev/null &
    do_receive_status
    set_config_var modeinput "$ORGINAL_MODE_INPUT" $PCONFIGFILE
  ;;
  *)
    /home/pi/rpidatv/bin/rpidatvgui 0 1  >/dev/null 2>/dev/null & 
    do_receive_status
  ;;
  esac
}

do_start_rtl_tcp()
{
  # Look up current wired IP.  If no wired, use wireless
  CURRENTIP=$(ifconfig | grep -Eo 'inet (addr:)?([0-9]*\.){3}[0-9]*' | grep -Eo '([0-9]*\.){3}[0-9]*' | grep -v '127.0.0.1' | head -1)
  rtl_tcp -a $CURRENTIP &>/dev/null &
}

do_stop_rtl_tcp()
{
  PROCESS=$(ps | grep rtl_tcp | grep -Eo '([0-9]+){4} ')
  kill -9 "$PROCESS"  >/dev/null
}

do_streamrx()
{
  # Stop the stream in case it is already running
  do_stop_streamrx

  # Give the user a chance to change the stream
  STREAM0=$(get_config_var stream0 $PATH_STREAMPRESETS)
  STREAM0=$(whiptail --inputbox "Enter the full stream URL" 8 78 $STREAM0 --title "SET URL FOR STREAM TO BE DISPLAYED" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var stream0 "$STREAM0" $PATH_STREAMPRESETS
  fi

  # Start the stream receiver
  /home/pi/rpidatv/bin/streamrx >/dev/null 2>/dev/null &
}

do_stop_streamrx()
{
  sudo killall streamrx >/dev/null 2>/dev/null
  sudo killall omxplayer.bin >/dev/null 2>/dev/null
}


do_receive_menu()
{
  menuchoice=$(whiptail --title "Select Receive Option" --menu "RTL Menu" 20 78 13 \
    "1 Receive DATV" "Use the RTL to Receive with default settings"  \
    "2 Start RTL-TCP" "Start the RTL-TCP Server for use with SDR Sharp"  \
    "3 Stop RTL-TCP" "Stop the RTL-TCP Server" \
    "4 Start Stream RX" "Display the Selected Stream" \
    "5 Stop Stream RX" "Stop Displaying the Selected Stream" \
    3>&2 2>&1 1>&3)
  case "$menuchoice" in
    1\ *) do_receive ;;
    2\ *) do_start_rtl_tcp ;;
    3\ *) do_stop_rtl_tcp  ;;
    4\ *) do_streamrx  ;;
    5\ *) do_stop_streamrx  ;;
  esac
}

do_autostart_setup()
{
  MODE_STARTUP=$(get_config_var startup $PCONFIGFILE)
  Radio1=OFF
  Radio2=OFF
  Radio3=OFF
  Radio4=OFF
  Radio5=OFF
  Radio6=OFF
  Radio7=OFF

  case "$MODE_STARTUP" in
    Display_boot)
      Radio1=ON
    ;;
    Langstone_boot)
      Radio2=ON
    ;;
    Bandview_boot)
      Radio3=ON
    ;;
    Meteorview_boot)
      Radio4=ON
    ;;
    Meteorbeacon_boot)
      Radio5=ON
    ;;
    TX_boot)
      Radio6=ON
    ;;
    Keyed_TX_boot)
      Radio7=ON
    ;;
    *)
      Radio1=ON
    ;;
  esac

  chstartup=$(whiptail --title "$StrAutostartSetupTitle" --radiolist \
   "$StrAutostartSetupContext" 20 78 12 \
   "Display_boot" "$AutostartSetupDisplay_boot" $Radio1 \
   "Langstone_boot" "Boot-up to the Langstone TRX" $Radio2 \
   "Bandview_boot" "Boot-up to the Band Viewer" $Radio3 \
   "Meteorview_boot" "Boot-up to the Meteor Viewer " $Radio4 \
   "Meteorbeacon_boot" "Boot-up to the Meteor Beacon RX Server " $Radio5 \
   "TX_boot" "$AutostartSetupTX_boot" $Radio6 \
   "Keyed_TX_boot" "Boot up to GPIO Keyed Repeater TX" $Radio7 \
   3>&2 2>&1 1>&3)

  if [ $? -eq 0 ]; then
     set_config_var startup "$chstartup" $PCONFIGFILE
     MODE_STARTUP=$chstartup
  fi

  # If keyed or continuous TX selected, set up cron for 12-hourly reboot
  if [[ "$chstartup" == "Keyed_TX_boot" || "$chstartup" == "TX_boot" ]]; then
    sudo crontab /home/pi/rpidatv/scripts/configs/rptrcron
  else
    sudo crontab /home/pi/rpidatv/scripts/configs/blankcron
  fi
}

do_display_setup()
{
  MODE_DISPLAY=$(get_config_var display $PCONFIGFILE)
  Radio1=OFF
  Radio2=OFF
  Radio3=OFF
  Radio4=OFF
  Radio5=OFF
  Radio6=OFF
  Radio7=OFF
  Radio8=OFF
  case "$MODE_DISPLAY" in
  HDMITouch)
    Radio1=ON
  ;;
  Console)
    Radio2=ON
  ;;
  Element14_7)
    Radio3=ON
  ;;
  dfrobot5)
    Radio4=ON
  ;;
  Browser)
    Radio5=ON
  ;;
  *)
    Radio1=ON
  ;;		
  esac

  chdisplay=$(whiptail --title "$StrDisplaySetupTitle" --radiolist \
    "$StrDisplaySetupContext" 20 78 10 \
    "HDMITouch" "$DisplaySetupHDMI" $Radio1 \
    "Console" "$DisplaySetupConsole" $Radio2 \
    "Element14_7" "Element 14 RPi 7 inch Display (Default)" $Radio3 \
    "dfrobot5" "DF Robot DFR0550 5 inch Display" $Radio4 \
    "Browser" "Web Browser Control.  NO Display" $Radio5 \
 	 3>&2 2>&1 1>&3)

  if [ $? -eq 0 ]; then                     ## If the selection has changed

    ## This section modifies and replaces the end of /boot/config.txt
    ## to allow (only) the correct LCD drivers to be loaded at next boot

    ## Set constants for the amendment of /boot/config.txt
    PATHCONFIGS="/home/pi/rpidatv/scripts/configs"  ## Path to config files
    lead='^## Begin LCD Driver'               ## Marker for start of inserted text
    tail='^## End LCD Driver'                 ## Marker for end of inserted text
    CHANGEFILE="/boot/config.txt"             ## File requiring added text
    APPENDFILE=$PATHCONFIGS"/lcd_markers.txt" ## File containing both markers
    TRANSFILE=$PATHCONFIGS"/transfer.txt"     ## File used for transfer

    grep -q "$lead" "$CHANGEFILE"     ## Is the first marker already present?
    if [ $? -ne 0 ]; then
      sudo bash -c 'cat '$APPENDFILE' >> '$CHANGEFILE' '  ## If not append the markers
    fi

    case "$chdisplay" in              ## Select the correct driver text
      HDMITouch) INSERTFILE=$PATHCONFIGS"/hdmitouch.txt" ;;
      Console)   INSERTFILE=$PATHCONFIGS"/console.txt" ;;
      Element14_7)  INSERTFILE=$PATHCONFIGS"/element14_7.txt" ;;
      dfrobot5)  INSERTFILE=$PATHCONFIGS"/dfrobot5.txt" ;;
      Browser)  INSERTFILE=$PATHCONFIGS"/browser.txt" ;;
    esac

    ## Replace whatever is between the markers with the driver text
    sed -e "/$lead/,/$tail/{ /$lead/{p; r $INSERTFILE
	        }; /$tail/p; d }" $CHANGEFILE >> $TRANSFILE

    sudo cp "$TRANSFILE" "$CHANGEFILE"          ## Copy from the transfer file
    rm $TRANSFILE                               ## Delete the transfer file

    set_config_var display "$chdisplay" $PCONFIGFILE

    if [[ "$chdisplay" == "Browser" ]]; then
      set_config_var webcontrol "enabled" $PCONFIGFILE
    fi
  fi
}

do_IP_setup()
{
  CURRENTIP=$(ifconfig | grep -Eo 'inet (addr:)?([0-9]*\.){3}[0-9]*' | grep -Eo '([0-9]*\.){3}[0-9]*' | grep -v '127.0.0.1')
  whiptail --title "IP" --msgbox "$CURRENTIP" 8 78
}

do_WiFi_setup()
{
  $PATHSCRIPT"/wifisetup.sh"
}

do_WiFi_Off()
{
  sudo ifconfig wlan0 down                           ## Disable it now
  cp $PATHCONFIGS"/text.wifi_off" /home/pi/.wifi_off ## Disable at start-up
}


do_Web_Control()
{
  WEBCONTROL=$(get_config_var webcontrol $PCONFIGFILE)
  MODE_DISPLAY=$(get_config_var display $PCONFIGFILE)
  Radio1=OFF
  Radio2=OFF

  if [[ "$WEBCONTROL" == "enabled" ]]; then
    Radio1=ON
  else
    Radio2=ON
  fi

  ch_Web_Control=$(whiptail --title "Enable or Disable Web Control" --radiolist \
    "Select Option with spacebar then press enter" 20 78 5 \
    "Enabled" "Enable Web Control" $Radio1 \
    "Disabled" "Disable Web Control" $Radio2 \
 	 3>&2 2>&1 1>&3)

  if [ $? -eq 0 ]; then                                    ## If the selection has changed
    if [[ "$ch_Web_Control" == "Enabled" ]]; then          ## Select web control.  Check display below
      set_config_var webcontrol "enabled" $PCONFIGFILE
      WEBCONTROL="enabled"
    else                                                   ## Not Web Control so check display
      set_config_var webcontrol "disabled" $PCONFIGFILE
      WEBCONTROL="disabled"
      if [[ "$MODE_DISPLAY" == "Browser" ]]; then          ## Correct the display mode as it is set to browser
        do_display_setup
      fi
    fi
  fi

  # Make sure that the correct display option is selected if using Web Control
  if [[ "$WEBCONTROL" == "enabled" ]]; then
    Radio1=OFF
    Radio2=OFF
    if [[ "$MODE_DISPLAY" == "Browser" ]]; then
      Radio2=ON
    else
      Radio1=ON
    fi

    TOUCHSCREEN=$(whiptail --title "Is Touchscreen Fitted?" --radiolist \
    "Select Option with spacebar then press enter" 20 78 5 \
    "Yes" "Touchscreen AND web control" $Radio1 \
    "No" "No Touchscreen Fitted. Web control only" $Radio2 \
 	 3>&2 2>&1 1>&3)

    chdisplay=$MODE_DISPLAY
    if [[ "$TOUCHSCREEN" == "No" ]] && [[ "$MODE_DISPLAY" != "Browser" ]]; then  ## Change display to browser
      chdisplay="Browser"
    fi

    if [[ "$TOUCHSCREEN" == "Yes" ]] && [[ "$MODE_DISPLAY" == "Browser" ]]; then  ## Change display to Element14
      chdisplay="Element14_7"
    fi

    if [[ "$MODE_DISPLAY" != "$chdisplay" ]]; then   ## different, so execute the display change

      ## This section modifies and replaces the end of /boot/config.txt
      ## to allow (only) the correct LCD drivers to be loaded at next boot

      ## Set constants for the amendment of /boot/config.txt
      PATHCONFIGS="/home/pi/rpidatv/scripts/configs"  ## Path to config files
      lead='^## Begin LCD Driver'               ## Marker for start of inserted text
      tail='^## End LCD Driver'                 ## Marker for end of inserted text
      CHANGEFILE="/boot/config.txt"             ## File requiring added text
      APPENDFILE=$PATHCONFIGS"/lcd_markers.txt" ## File containing both markers
      TRANSFILE=$PATHCONFIGS"/transfer.txt"     ## File used for transfer

      grep -q "$lead" "$CHANGEFILE"     ## Is the first marker already present?
      if [ $? -ne 0 ]; then
        sudo bash -c 'cat '$APPENDFILE' >> '$CHANGEFILE' '  ## If not append the markers
      fi

      case "$chdisplay" in              ## Select the correct driver text
        Element14_7)  INSERTFILE=$PATHCONFIGS"/element14_7.txt" ;;
        Browser)  INSERTFILE=$PATHCONFIGS"/browser.txt" ;;
      esac

      ## Replace whatever is between the markers with the driver text
      sed -e "/$lead/,/$tail/{ /$lead/{p; r $INSERTFILE
	          }; /$tail/p; d }" $CHANGEFILE >> $TRANSFILE

      sudo cp "$TRANSFILE" "$CHANGEFILE"          ## Copy from the transfer file
      rm $TRANSFILE                               ## Delete the transfer file

      set_config_var display "$chdisplay" $PCONFIGFILE
    fi
  fi
}


do_EasyCap()
{
    ## Check and set the input
    ACINPUT=$(get_config_var analogcaminput $PCONFIGFILE)
    ACINPUT=$(whiptail --inputbox "Enter 0 for Composite, 1 for S-Video, - for not set" 8 78 $ACINPUT --title "SET EASYCAP INPUT NUMBER" 3>&1 1>&2 2>&3)
    if [ $? -eq 0 ]; then
        if [ "$ACINPUT" == "-" ]; then
            set_config_var analogcaminput "$ACINPUT" $PCONFIGFILE
        else
            if [[ $ACINPUT =~ ^[0-9]+$ ]]; then
                set_config_var analogcaminput "$ACINPUT" $PCONFIGFILE
            else
                whiptail --title "Error" --msgbox "Please enter only numbers or a -.  Please press enter to continue and reselect" 8 78
            fi
        fi
    fi

    ## Check and set the standard
    ACSTANDARD=$(get_config_var analogcamstandard $PCONFIGFILE)
    ACSTANDARD=$(whiptail --inputbox "Enter 0 for NTSC, 6 for PAL, - for not set" 8 78 $ACSTANDARD --title "SET EASYCAP VIDEO STANDARD" 3>&1 1>&2 2>&3)
    if [ $? -eq 0 ]; then
        if [ "$ACSTANDARD" == "-" ]; then
            set_config_var analogcamstandard "$ACSTANDARD" $PCONFIGFILE
        else
            if [[ $ACSTANDARD =~ ^[0-9]+$ ]]; then
                set_config_var analogcamstandard "$ACSTANDARD" $PCONFIGFILE
            else
                whiptail --title "Error" --msgbox "Please enter only numbers or a -.  Please press enter to continue and reselect" 8 78
            fi
        fi

    fi
}

do_audio_switch()
{
  AUDIO=$(get_config_var audio $PCONFIGFILE)
  Radio1=OFF
  Radio2=OFF
  Radio3=OFF
  Radio4=OFF
  Radio5=OFF
  Radio6=OFF
  case "$AUDIO" in
  auto)
    Radio1=ON
  ;;
  mic)
    Radio2=ON
  ;;
  video)
    Radio3=ON
  ;;
  webcam)
    Radio4=ON
  ;;
  bleeps)
    Radio5=ON
  ;;
  no_audio)
    Radio65=ON
  ;;
  *)
    Radio1=ON
  ;;
  esac

  AUDIO=$(whiptail --title "SELECT AUDIO SOURCE" --radiolist \
    "Select one" 20 78 8 \
    "auto" "Auto-select from Mic or EasyCap Dongle" $Radio1 \
    "mic" "Use the USB Audio Dongle Mic Input" $Radio2 \
    "video" "Use the EasyCap Video Dongle Audio Input" $Radio3 \
    "webcam" "Use the Webcam Microphone" $Radio4 \
    "bleeps" "Generate test bleeps" $Radio5 \
    "no_audio" "Do not include audio" $Radio6 \
    3>&2 2>&1 1>&3)

  if [ $? -eq 0 ]; then                     ## If the selection has changed
    set_config_var audio "$AUDIO" $PCONFIGFILE
  fi
}

do_attenuator()
{
  ATTEN=$(get_config_var attenuator $PCONFIGFILE)
  Radio1=OFF
  Radio2=OFF
  Radio3=OFF
  Radio4=OFF
  case "$ATTEN" in
  NONE)
    Radio1=ON
  ;;
  PE4312)
    Radio2=ON
  ;;
  PE43713)
    Radio3=ON
  ;;
  HMC1119)
    Radio4=ON
  ;;
  *)
    Radio1=ON
  ;;
  esac

  ATTEN=$(whiptail --title "SELECT OUTPUT ATTENUATOR TYPE" --radiolist \
    "Select one" 20 78 4 \
    "NONE" "No Output Attenuator in Circuit" $Radio1 \
    "PE4312" "PE4302 or PE4312 Attenuator in Use" $Radio2 \
    "PE43713" "PE43703 or PE43713 Attenuator in Use" $Radio3 \
    "HMC1119" "HMC1119 Attenuator in Use" $Radio4 \
    3>&2 2>&1 1>&3)

  if [ $? -eq 0 ]; then                     ## If the selection has changed
    set_config_var attenuator "$ATTEN" $PCONFIGFILE
  fi
}

do_LimeStatus()
{
reset

printf "LimeSDR Firmware Status\n"
printf "=========================\n\n"

LimeUtil --make

printf "\n\nPress any key to return to the main menu\n"
read -n 1

}


do_LimeUpdate()
{
reset

printf "LimeSDR Firmware Update\n"
printf "=======================\n\n"

LimeUtil --update

printf "\n\nPress any key to return to the main menu\n"
read -n 1

}

do_Update()
{
reset
$PATHSCRIPT"/check_for_update.sh"
}

do_presets()
{
  PFREQ1=$(get_config_var pfreq1 $PATH_PPRESETS)
  PFREQ1=$(whiptail --inputbox "Enter Preset Frequency 1 in MHz" 8 78 $PFREQ1 --title "SET TOUCHSCREEN PRESETS" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var pfreq1 "$PFREQ1" $PATH_PPRESETS
  fi

  PFREQ2=$(get_config_var pfreq2 $PATH_PPRESETS)
  PFREQ2=$(whiptail --inputbox "Enter Preset Frequency 2 in MHz" 8 78 $PFREQ2 --title "SET TOUCHSCREEN PRESETS" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var pfreq2 "$PFREQ2" $PATH_PPRESETS
  fi

  PFREQ3=$(get_config_var pfreq3 $PATH_PPRESETS)
  PFREQ3=$(whiptail --inputbox "Enter Preset Frequency 3 in MHz" 8 78 $PFREQ3 --title "SET TOUCHSCREEN PRESETS" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var pfreq3 "$PFREQ3" $PATH_PPRESETS
  fi

  PFREQ4=$(get_config_var pfreq4 $PATH_PPRESETS)
  PFREQ4=$(whiptail --inputbox "Enter Preset Frequency 4 in MHz" 8 78 $PFREQ4 --title "SET TOUCHSCREEN PRESETS" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var pfreq4 "$PFREQ4" $PATH_PPRESETS
  fi

  PFREQ5=$(get_config_var pfreq5 $PATH_PPRESETS)
  PFREQ5=$(whiptail --inputbox "Enter Preset Frequency 5 in MHz" 8 78 $PFREQ5 --title "SET TOUCHSCREEN PRESETS" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var pfreq5 "$PFREQ5" $PATH_PPRESETS
  fi

  PFREQ6=$(get_config_var pfreq6 $PATH_PPRESETS)
  PFREQ6=$(whiptail --inputbox "Enter Preset Frequency 6 in MHz" 8 78 $PFREQ6 --title "SET TOUCHSCREEN PRESETS" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var pfreq6 "$PFREQ6" $PATH_PPRESETS
  fi

  PFREQ7=$(get_config_var pfreq7 $PATH_PPRESETS)
  PFREQ7=$(whiptail --inputbox "Enter Preset Frequency 7 in MHz" 8 78 $PFREQ7 --title "SET TOUCHSCREEN PRESETS" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var pfreq7 "$PFREQ7" $PATH_PPRESETS
  fi

  PFREQ8=$(get_config_var pfreq8 $PATH_PPRESETS)
  PFREQ8=$(whiptail --inputbox "Enter Preset Frequency 8 in MHz" 8 78 $PFREQ8 --title "SET TOUCHSCREEN PRESETS" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var pfreq8 "$PFREQ8" $PATH_PPRESETS
  fi

  PFREQ9=$(get_config_var pfreq9 $PATH_PPRESETS)
  PFREQ9=$(whiptail --inputbox "Enter Preset Frequency 9 in MHz" 8 78 $PFREQ9 --title "SET TOUCHSCREEN PRESETS" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var pfreq9 "$PFREQ9" $PATH_PPRESETS
  fi
}

do_preset_SRs()
{
  PSR1=$(get_config_var psr1 $PATH_PPRESETS)
  PSR1=$(whiptail --inputbox "Enter Preset Symbol Rate 1 in KS/s" 8 78 $PSR1 --title "SET TOUCHSCREEN PRESETS" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var psr1 "$PSR1" $PATH_PPRESETS
  fi

  PSR2=$(get_config_var psr2 $PATH_PPRESETS)
  PSR2=$(whiptail --inputbox "Enter Preset Symbol Rate 2 in KS/s" 8 78 $PSR2 --title "SET TOUCHSCREEN PRESETS" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var psr2 "$PSR2" $PATH_PPRESETS
  fi

  PSR3=$(get_config_var psr3 $PATH_PPRESETS)
  PSR3=$(whiptail --inputbox "Enter Preset Symbol Rate 3 in KS/s" 8 78 $PSR3 --title "SET TOUCHSCREEN PRESETS" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var psr3 "$PSR3" $PATH_PPRESETS
  fi

  PSR4=$(get_config_var psr4 $PATH_PPRESETS)
  PSR4=$(whiptail --inputbox "Enter Preset Symbol Rate 4 in KS/s" 8 78 $PSR4 --title "SET TOUCHSCREEN PRESETS" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var psr4 "$PSR4" $PATH_PPRESETS
  fi

  PSR5=$(get_config_var psr5 $PATH_PPRESETS)
  PSR5=$(whiptail --inputbox "Enter Preset Symbol Rate 5 in KS/s" 8 78 $PSR5 --title "SET TOUCHSCREEN PRESETS" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var psr5 "$PSR5" $PATH_PPRESETS
  fi

  PSR6=$(get_config_var psr6 $PATH_PPRESETS)
  PSR6=$(whiptail --inputbox "Enter Preset Symbol Rate 6 in KS/s" 8 78 $PSR6 --title "SET TOUCHSCREEN PRESETS" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var psr6 "$PSR6" $PATH_PPRESETS
  fi

  PSR7=$(get_config_var psr7 $PATH_PPRESETS)
  PSR7=$(whiptail --inputbox "Enter Preset Symbol Rate 7 in KS/s" 8 78 $PSR7 --title "SET TOUCHSCREEN PRESETS" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var psr7 "$PSR7" $PATH_PPRESETS
  fi

  PSR8=$(get_config_var psr8 $PATH_PPRESETS)
  PSR8=$(whiptail --inputbox "Enter Preset Symbol Rate 8 in KS/s" 8 78 $PSR8 --title "SET TOUCHSCREEN PRESETS" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var psr8 "$PSR8" $PATH_PPRESETS
  fi

  PSR9=$(get_config_var psr9 $PATH_PPRESETS)
  PSR9=$(whiptail --inputbox "Enter Preset Symbol Rate 9 in KS/s" 8 78 $PSR9 --title "SET TOUCHSCREEN PRESETS" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var psr9 "$PSR9" $PATH_PPRESETS
  fi
}

do_4351_ref()
{
  ADFREF=$(get_config_var adfref $PCONFIGFILE)
  ADFREF=$(whiptail --inputbox "Enter oscillator frequency in Hz" 8 78 $ADFREF --title "SET ADF4351 REFERENCE OSCILLATOR" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var adfref "$ADFREF" $PCONFIGFILE
  fi
}

do_atten_levels()
{
  ATTENLEVEL0=$(get-config_var d1attenlevel $PATH_PPRESETS)
  ATTENLEVEL0=$(whiptail --inputbox "Enter 0 to 31.5 dB" 8 78 $ATTENLEVEL0 --title "SET ATTENUATOR LEVEL FOR THE 71 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var d1attenlevel "-""$ATTENLEVEL0" $PATH_PPRESETS
  fi

  ATTENLEVEL1=$(get-config_var d2attenlevel $PATH_PPRESETS)
  ATTENLEVEL1=$(whiptail --inputbox "Enter 0 to 31.5 dB" 8 78 $ATTENLEVEL1 --title "SET ATTENUATOR LEVEL FOR THE 146 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var d2attenlevel "-""$ATTENLEVEL1" $PATH_PPRESETS
  fi

  ATTENLEVEL2=$(get-config_var d3attenlevel $PATH_PPRESETS)
  ATTENLEVEL2=$(whiptail --inputbox "Enter 0 to 31.5 dB" 8 78 $ATTENLEVEL2 --title "SET ATTENUATOR LEVEL FOR THE 437MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var d3attenlevel "-""$ATTENLEVEL2" $PATH_PPRESETS
  fi

  ATTENLEVEL3=$(get-config_var d4attenlevel $PATH_PPRESETS)
  ATTENLEVEL3=$(whiptail --inputbox "Enter 0 to 31.5 dB" 8 78 $ATTENLEVEL3 --title "SET ATTENUATOR LEVEL FOR THE 1255 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var d4attenlevel "-""$ATTENLEVEL3" $PATH_PPRESETS
  fi

  ATTENLEVEL4=$(get-config_var d5attenlevel $PATH_PPRESETS)
  ATTENLEVEL4=$(whiptail --inputbox "Enter 0 to 31.5 dB" 8 78 $ATTENLEVEL4 --title "SET ATTENUATOR LEVEL FOR THE 2400 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var d5attenlevel "-""$ATTENLEVEL4" $PATH_PPRESETS
  fi

  ATTENLEVEL5=$(get-config_var t1attenlevel $PATH_PPRESETS)
  ATTENLEVEL5=$(whiptail --inputbox "Enter 0 to 31.5 dB" 8 78 $ATTENLEVEL5 --title "SET ATTENUATOR LEVEL FOR TRANSVERTER 1" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var t1attenlevel "-""$ATTENLEVEL5" $PATH_PPRESETS
  fi

  ATTENLEVEL6=$(get-config_var t2attenlevel $PATH_PPRESETS)
  ATTENLEVEL6=$(whiptail --inputbox "Enter 0 to 31.5 dB" 8 78 $ATTENLEVEL6 --title "SET ATTENUATOR LEVEL FOR TRANSVERTER 2" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var t2attenlevel "-""$ATTENLEVEL6" $PATH_PPRESETS
  fi

  ATTENLEVEL7=$(get-config_var t3attenlevel $PATH_PPRESETS)
  ATTENLEVEL7=$(whiptail --inputbox "Enter 0 to 31.5 dB" 8 78 $ATTENLEVEL7 --title "SET ATTENUATOR LEVEL FOR TRANSVERTER 3" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var t3attenlevel "-""$ATTENLEVEL7" $PATH_PPRESETS
  fi

  ATTENLEVEL8=$(get-config_var t4attenlevel $PATH_PPRESETS)
  ATTENLEVEL8=$(whiptail --inputbox "Enter 0 to 31.5 dB" 8 78 $ATTENLEVEL8 --title "SET ATTENUATOR LEVEL FOR TRANSVERTER 4" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var t4attenlevel "-""$ATTENLEVEL8" $PATH_PPRESETS
  fi

  ## Now set the Attenuator Level for the current band
  BAND=$(get_config_var band $PCONFIGFILE)

  case "$BAND" in
  d1)
    set_config_var attenlevel "$ATTENLEVEL0" $PCONFIGFILE
  ;;
  d2)
    set_config_var attenlevel "$ATTENLEVEL1" $PCONFIGFILE
  ;;
  d3)
    set_config_var attenlevel "$ATTENLEVEL2" $PCONFIGFILE
  ;;
  d4)
    set_config_var attenlevel "$ATTENLEVEL3" $PCONFIGFILE
  ;;
  d5)
    set_config_var attenlevel "$ATTENLEVEL4" $PCONFIGFILE
  ;;
  t1)
    set_config_var attenlevel "$ATTENLEVEL5" $PCONFIGFILE
  ;;
  t2)
    set_config_var attenlevel "$ATTENLEVEL6" $PCONFIGFILE
  ;;
  t3)
    set_config_var attenlevel "$ATTENLEVEL7" $PCONFIGFILE
  ;;
  t4)
    set_config_var attenlevel "$ATTENLEVEL8" $PCONFIGFILE
  ;;
  *)
    set_config_var attenlevel "$ATTENLEVEL0" $PCONFIGFILE
  ;;
  esac
}


do_set_limegain()
{
  LIMEGAIN0=$(get-config_var d1limegain $PATH_PPRESETS)
  LIMEGAIN0=$(whiptail --inputbox "Enter 0 to 100" 8 78 $LIMEGAIN0 --title "SET LIME GAIN FOR THE 71 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var d1limegain "$LIMEGAIN0" $PATH_PPRESETS
  fi

  LIMEGAIN1=$(get-config_var d2limegain $PATH_PPRESETS)
  LIMEGAIN1=$(whiptail --inputbox "Enter 0 to 100" 8 78 $LIMEGAIN1 --title "SET LIME GAIN FOR THE 146 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var d2limegain "$LIMEGAIN1" $PATH_PPRESETS
  fi

  LIMEGAIN2=$(get-config_var d3limegain $PATH_PPRESETS)
  LIMEGAIN2=$(whiptail --inputbox "Enter 0 to 100" 8 78 $LIMEGAIN2 --title "SET LIME GAIN FOR THE 437MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var d3limegain "$LIMEGAIN2" $PATH_PPRESETS
  fi

  LIMEGAIN3=$(get-config_var d4limegain $PATH_PPRESETS)
  LIMEGAIN3=$(whiptail --inputbox "Enter 0 to 100" 8 78 $LIMEGAIN3 --title "SET LIME GAIN FOR THE 1255 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var d4limegain "$LIMEGAIN3" $PATH_PPRESETS
  fi

  LIMEGAIN4=$(get-config_var d5limegain $PATH_PPRESETS)
  LIMEGAIN4=$(whiptail --inputbox "Enter 0 to 100" 8 78 $LIMEGAIN4 --title "SET LIME GAIN FOR THE 2400 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var d5limegain "$LIMEGAIN4" $PATH_PPRESETS
  fi

  LIMEGAIN5=$(get-config_var t1limegain $PATH_PPRESETS)
  LIMEGAIN5=$(whiptail --inputbox "Enter 0 to 100" 8 78 $LIMEGAIN5 --title "SET LIME GAIN FOR TRANSVERTER 1" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var t1limegain "$LIMEGAIN5" $PATH_PPRESETS
  fi

  LIMEGAIN6=$(get-config_var t2limegain $PATH_PPRESETS)
  LIMEGAIN6=$(whiptail --inputbox "Enter 0 to 100" 8 78 $LIMEGAIN6 --title "SET LIME GAIN FOR TRANSVERTER 2" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var t2limegain "$LIMEGAIN6" $PATH_PPRESETS
  fi

  LIMEGAIN7=$(get-config_var t3limegain $PATH_PPRESETS)
  LIMEGAIN7=$(whiptail --inputbox "Enter 0 to 100" 8 78 $LIMEGAIN7 --title "SET LIME GAIN FOR TRANSVERTER 3" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var t3limegain "$LIMEGAIN7" $PATH_PPRESETS
  fi

  LIMEGAIN8=$(get-config_var t4limegain $PATH_PPRESETS)
  LIMEGAIN8=$(whiptail --inputbox "Enter 0 to 100" 8 78 $LIMEGAIN8 --title "SET LIME GAIN FOR TRANSVERTER 4" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var t4limegain "$LIMEGAIN8" $PATH_PPRESETS
  fi

  ## Now set the Lime Gain for the current band
  BAND=$(get_config_var band $PCONFIGFILE)

  case "$BAND" in
  d1)
    set_config_var limegain "$LIMEGAIN0" $PCONFIGFILE
  ;;
  d2)
    set_config_var limegain "$LIMEGAIN1" $PCONFIGFILE
  ;;
  d3)
    set_config_var limegain "$LIMEGAIN2" $PCONFIGFILE
  ;;
  d4)
    set_config_var limegain "$LIMEGAIN3" $PCONFIGFILE
  ;;
  d5)
    set_config_var limegain "$LIMEGAIN4" $PCONFIGFILE
  ;;
  t1)
    set_config_var limegain "$LIMEGAIN5" $PCONFIGFILE
  ;;
  t2)
    set_config_var limegain "$LIMEGAIN6" $PCONFIGFILE
  ;;
  t3)
    set_config_var limegain "$LIMEGAIN7" $PCONFIGFILE
  ;;
  t4)
    set_config_var limegain "$LIMEGAIN8" $PCONFIGFILE
  ;;
  *)
    set_config_var limegain "$LIMEGAIN0" $PCONFIGFILE
  ;;
  esac

}


do_set_express()
{
  EXPLEVEL0=$(get_config_var d1explevel $PATH_PPRESETS)
  EXPLEVEL0=$(whiptail --inputbox "Enter 0 to 47" 8 78 $EXPLEVEL0 --title "SET DATV EXPRESS OUTPUT LEVEL FOR THE 71 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var d1explevel "$EXPLEVEL0" $PATH_PPRESETS
  fi

  Check1=OFF
  Check2=OFF
  Check3=OFF
  Check4=OFF
  EXPPORTS0=$(get_config_var d1expports $PATH_PPRESETS)
  TESTPORT=$EXPPORTS0
  if [ "$TESTPORT" -gt 7 ]; then
    Check4=ON
    TESTPORT=$[$TESTPORT-8]
  fi
  if [ "$TESTPORT" -gt 3 ]; then
    Check3=ON
    TESTPORT=$[$TESTPORT-4]
  fi
  if [ "$TESTPORT" -gt 1 ]; then
    Check2=ON
    TESTPORT=$[$TESTPORT-2]
  fi
  if [ "$TESTPORT" -gt 0 ]; then
    Check1=ON
  fi
  TESTPORT=$(whiptail --title "SET DATV EXPRESS PORTS FOR THE 71 MHz BAND" --checklist \
    "Select or deselect the active ports using the spacebar" 20 78 4 \
    "Port A" "J6 Pin 5" $Check1 \
    "Port B" "J6 Pin 6" $Check2 \
    "Port C" "J6 Pin 7" $Check3 \
    "Port D" "J6 Pin 10" $Check4 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    EXPPORTS0=0
    if (echo $TESTPORT | grep -q A); then
      EXPPORTS0=1
    fi
    if (echo $TESTPORT | grep -q B); then
      EXPPORTS0=$[$EXPPORTS0+2]
    fi
    if (echo $TESTPORT | grep -q C); then
      EXPPORTS0=$[$EXPPORTS0+4]
    fi
    if (echo $TESTPORT | grep -q D); then
      EXPPORTS0=$[$EXPPORTS0+8]
    fi
    set_config_var d1expports "$EXPPORTS0" $PATH_PPRESETS
  fi

  EXPLEVEL1=$(get_config_var d2explevel $PATH_PPRESETS)
  EXPLEVEL1=$(whiptail --inputbox "Enter 0 to 47" 8 78 $EXPLEVEL1 --title "SET DATV EXPRESS OUTPUT LEVEL FOR THE 146 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var d2explevel "$EXPLEVEL1" $PATH_PPRESETS
  fi

  Check1=OFF
  Check2=OFF
  Check3=OFF
  Check4=OFF
  EXPPORTS1=$(get_config_var d2expports $PATH_PPRESETS)
  TESTPORT=$EXPPORTS1
  if [ "$TESTPORT" -gt 7 ]; then
    Check4=ON
    TESTPORT=$[$TESTPORT-8]
  fi
  if [ "$TESTPORT" -gt 3 ]; then
    Check3=ON
    TESTPORT=$[$TESTPORT-4]
  fi
  if [ "$TESTPORT" -gt 1 ]; then
    Check2=ON
    TESTPORT=$[$TESTPORT-2]
  fi
  if [ "$TESTPORT" -gt 0 ]; then
    Check1=ON
  fi
  TESTPORT=$(whiptail --title "SET DATV EXPRESS PORTS FOR THE 146 MHz BAND" --checklist \
    "Select or deselect the active ports using the spacebar" 20 78 4 \
    "Port A" "J6 Pin 5" $Check1 \
    "Port B" "J6 Pin 6" $Check2 \
    "Port C" "J6 Pin 7" $Check3 \
    "Port D" "J6 Pin 10" $Check4 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    EXPPORTS1=0
    if (echo $TESTPORT | grep -q A); then
      EXPPORTS1=1
    fi
    if (echo $TESTPORT | grep -q B); then
      EXPPORTS1=$[$EXPPORTS1+2]
    fi
    if (echo $TESTPORT | grep -q C); then
      EXPPORTS1=$[$EXPPORTS1+4]
    fi
    if (echo $TESTPORT | grep -q D); then
      EXPPORTS1=$[$EXPPORTS1+8]
    fi
    set_config_var d2expports "$EXPPORTS1" $PATH_PPRESETS
  fi

  EXPLEVEL2=$(get_config_var d3explevel $PATH_PPRESETS)
  EXPLEVEL2=$(whiptail --inputbox "Enter 0 to 47" 8 78 $EXPLEVEL2 --title "SET DATV EXPRESS OUTPUT LEVEL FOR THE 437 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var d3explevel "$EXPLEVEL2" $PATH_PPRESETS
  fi

  Check1=OFF
  Check2=OFF
  Check3=OFF
  Check4=OFF
  EXPPORTS2=$(get_config_var d3expports $PATH_PPRESETS)
  TESTPORT=$EXPPORTS2
  if [ "$TESTPORT" -gt 7 ]; then
    Check4=ON
    TESTPORT=$[$TESTPORT-8]
  fi
  if [ "$TESTPORT" -gt 3 ]; then
    Check3=ON
    TESTPORT=$[$TESTPORT-4]
  fi
  if [ "$TESTPORT" -gt 1 ]; then
    Check2=ON
    TESTPORT=$[$TESTPORT-2]
  fi
  if [ "$TESTPORT" -gt 0 ]; then
    Check1=ON
  fi
  TESTPORT=$(whiptail --title "SET DATV EXPRESS PORTS FOR THE 437 MHz BAND" --checklist \
    "Select or deselect the active ports using the spacebar" 20 78 4 \
    "Port A" "J6 Pin 5" $Check1 \
    "Port B" "J6 Pin 6" $Check2 \
    "Port C" "J6 Pin 7" $Check3 \
    "Port D" "J6 Pin 10" $Check4 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    EXPPORTS2=0
    if (echo $TESTPORT | grep -q A); then
      EXPPORTS2=1
    fi
    if (echo $TESTPORT | grep -q B); then
      EXPPORTS2=$[$EXPPORTS2+2]
    fi
    if (echo $TESTPORT | grep -q C); then
      EXPPORTS2=$[$EXPPORTS2+4]
    fi
    if (echo $TESTPORT | grep -q D); then
      EXPPORTS2=$[$EXPPORTS2+8]
    fi
    set_config_var d3expports "$EXPPORTS2" $PATH_PPRESETS
  fi

  EXPLEVEL3=$(get_config_var d4explevel $PATH_PPRESETS)
  EXPLEVEL3=$(whiptail --inputbox "Enter 0 to 47" 8 78 $EXPLEVEL3 --title "SET DATV EXPRESS OUTPUT LEVEL FOR THE 1255 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var d4explevel "$EXPLEVEL3" $PATH_PPRESETS
  fi

  Check1=OFF
  Check2=OFF
  Check3=OFF
  Check4=OFF
  EXPPORTS3=$(get_config_var d4expports $PATH_PPRESETS)
  TESTPORT=$EXPPORTS3
  if [ "$TESTPORT" -gt 7 ]; then
    Check4=ON
    TESTPORT=$[$TESTPORT-8]
  fi
  if [ "$TESTPORT" -gt 3 ]; then
    Check3=ON
    TESTPORT=$[$TESTPORT-4]
  fi
  if [ "$TESTPORT" -gt 1 ]; then
    Check2=ON
    TESTPORT=$[$TESTPORT-2]
  fi
  if [ "$TESTPORT" -gt 0 ]; then
    Check1=ON
  fi
  TESTPORT=$(whiptail --title "SET DATV EXPRESS PORTS FOR THE 1255 MHz BAND" --checklist \
    "Select or deselect the active ports using the spacebar" 20 78 4 \
    "Port A" "J6 Pin 5" $Check1 \
    "Port B" "J6 Pin 6" $Check2 \
    "Port C" "J6 Pin 7" $Check3 \
    "Port D" "J6 Pin 10" $Check4 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    EXPPORTS3=0
    if (echo $TESTPORT | grep -q A); then
      EXPPORTS3=1
    fi
    if (echo $TESTPORT | grep -q B); then
      EXPPORTS3=$[$EXPPORTS3+2]
    fi
    if (echo $TESTPORT | grep -q C); then
      EXPPORTS3=$[$EXPPORTS3+4]
    fi
    if (echo $TESTPORT | grep -q D); then
      EXPPORTS3=$[$EXPPORTS3+8]
    fi
    set_config_var d4expports "$EXPPORTS3" $PATH_PPRESETS
  fi

  EXPLEVEL4=$(get_config_var d5explevel $PATH_PPRESETS)
  EXPLEVEL4=$(whiptail --inputbox "Enter 0 to 47" 8 78 $EXPLEVEL4 --title "SET DATV EXPRESS OUTPUT LEVEL FOR THE 2400 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var d5explevel "$EXPLEVEL4" $PATH_PPRESETS
  fi

  Check1=OFF
  Check2=OFF
  Check3=OFF
  Check4=OFF
  EXPPORTS4=$(get_config_var d5expports $PATH_PPRESETS)
  TESTPORT=$EXPPORTS4
  if [ "$TESTPORT" -gt 7 ]; then
    Check4=ON
    TESTPORT=$[$TESTPORT-8]
  fi
  if [ "$TESTPORT" -gt 3 ]; then
    Check3=ON
    TESTPORT=$[$TESTPORT-4]
  fi
  if [ "$TESTPORT" -gt 1 ]; then
    Check2=ON
    TESTPORT=$[$TESTPORT-2]
  fi
  if [ "$TESTPORT" -gt 0 ]; then
    Check1=ON
  fi
  TESTPORT=$(whiptail --title "SET DATV EXPRESS PORTS FOR THE 2400 MHz BAND" --checklist \
    "Select or deselect the active ports using the spacebar" 20 78 4 \
    "Port A" "J6 Pin 5" $Check1 \
    "Port B" "J6 Pin 6" $Check2 \
    "Port C" "J6 Pin 7" $Check3 \
    "Port D" "J6 Pin 10" $Check4 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    EXPPORTS4=0
    if (echo $TESTPORT | grep -q A); then
      EXPPORTS4=1
    fi
    if (echo $TESTPORT | grep -q B); then
      EXPPORTS4=$[$EXPPORTS4+2]
    fi
    if (echo $TESTPORT | grep -q C); then
      EXPPORTS4=$[$EXPPORTS4+4]
    fi
    if (echo $TESTPORT | grep -q D); then
      EXPPORTS4=$[$EXPPORTS4+8]
    fi
    set_config_var d5expports "$EXPPORTS4" $PATH_PPRESETS
  fi

  EXPLEVEL5=$(get_config_var t1explevel $PATH_PPRESETS)
  EXPLEVEL5=$(whiptail --inputbox "Enter 0 to 47" 8 78 $EXPLEVEL5 --title "SET DATV EXPRESS OUTPUT LEVEL FOR TRANSVERTER 1" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var t1explevel "$EXPLEVEL5" $PATH_PPRESETS
  fi

  Check1=OFF
  Check2=OFF
  Check3=OFF
  Check4=OFF
  EXPPORTS5=$(get_config_var t1expports $PATH_PPRESETS)
  TESTPORT=$EXPPORTS5
  if [ "$TESTPORT" -gt 7 ]; then
    Check4=ON
    TESTPORT=$[$TESTPORT-8]
  fi
  if [ "$TESTPORT" -gt 3 ]; then
    Check3=ON
    TESTPORT=$[$TESTPORT-4]
  fi
  if [ "$TESTPORT" -gt 1 ]; then
    Check2=ON
    TESTPORT=$[$TESTPORT-2]
  fi
  if [ "$TESTPORT" -gt 0 ]; then
    Check1=ON
  fi
  TESTPORT=$(whiptail --title "SET DATV EXPRESS PORTS FOR TRANSVERTER 1" --checklist \
    "Select or deselect the active ports using the spacebar" 20 78 4 \
    "Port A" "J6 Pin 5" $Check1 \
    "Port B" "J6 Pin 6" $Check2 \
    "Port C" "J6 Pin 7" $Check3 \
    "Port D" "J6 Pin 10" $Check4 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    EXPPORTS5=0
    if (echo $TESTPORT | grep -q A); then
      EXPPORTS5=1
    fi
    if (echo $TESTPORT | grep -q B); then
      EXPPORTS5=$[$EXPPORTS5+2]
    fi
    if (echo $TESTPORT | grep -q C); then
      EXPPORTS5=$[$EXPPORTS5+4]
    fi
    if (echo $TESTPORT | grep -q D); then
      EXPPORTS5=$[$EXPPORTS5+8]
    fi
    set_config_var t1expports "$EXPPORTS5" $PATH_PPRESETS
  fi

  EXPLEVEL6=$(get_config_var t2explevel $PATH_PPRESETS)
  EXPLEVEL6=$(whiptail --inputbox "Enter 0 to 47" 8 78 $EXPLEVEL6 --title "SET DATV EXPRESS OUTPUT LEVEL FOR TRANSVERTER 2" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var t2explevel "$EXPLEVEL6" $PATH_PPRESETS
  fi

  Check1=OFF
  Check2=OFF
  Check3=OFF
  Check4=OFF
  EXPPORTS6=$(get_config_var t2expports $PATH_PPRESETS)
  TESTPORT=$EXPPORTS6
  if [ "$TESTPORT" -gt 7 ]; then
    Check4=ON
    TESTPORT=$[$TESTPORT-8]
  fi
  if [ "$TESTPORT" -gt 3 ]; then
    Check3=ON
    TESTPORT=$[$TESTPORT-4]
  fi
  if [ "$TESTPORT" -gt 1 ]; then
    Check2=ON
    TESTPORT=$[$TESTPORT-2]
  fi
  if [ "$TESTPORT" -gt 0 ]; then
    Check1=ON
  fi
  TESTPORT=$(whiptail --title "SET DATV EXPRESS PORTS FOR TRANSVERTER 2" --checklist \
    "Select or deselect the active ports using the spacebar" 20 78 4 \
    "Port A" "J6 Pin 5" $Check1 \
    "Port B" "J6 Pin 6" $Check2 \
    "Port C" "J6 Pin 7" $Check3 \
    "Port D" "J6 Pin 10" $Check4 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    EXPPORTS6=0
    if (echo $TESTPORT | grep -q A); then
      EXPPORTS6=1
    fi
    if (echo $TESTPORT | grep -q B); then
      EXPPORTS6=$[$EXPPORTS6+2]
    fi
    if (echo $TESTPORT | grep -q C); then
      EXPPORTS6=$[$EXPPORTS6+4]
    fi
    if (echo $TESTPORT | grep -q D); then
      EXPPORTS6=$[$EXPPORTS6+8]
    fi
    set_config_var t2expports "$EXPPORTS6" $PATH_PPRESETS
  fi

  EXPLEVEL7=$(get_config_var t3explevel $PATH_PPRESETS)
  EXPLEVEL7=$(whiptail --inputbox "Enter 0 to 47" 8 78 $EXPLEVEL7 --title "SET DATV EXPRESS OUTPUT LEVEL FOR TRANSVERER 3" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var t3explevel "$EXPLEVEL7" $PATH_PPRESETS
  fi

  Check1=OFF
  Check2=OFF
  Check3=OFF
  Check4=OFF
  EXPPORTS7=$(get_config_var t3expports $PATH_PPRESETS)
  TESTPORT=$EXPPORTS7
  if [ "$TESTPORT" -gt 7 ]; then
    Check4=ON
    TESTPORT=$[$TESTPORT-8]
  fi
  if [ "$TESTPORT" -gt 3 ]; then
    Check3=ON
    TESTPORT=$[$TESTPORT-4]
  fi
  if [ "$TESTPORT" -gt 1 ]; then
    Check2=ON
    TESTPORT=$[$TESTPORT-2]
  fi
  if [ "$TESTPORT" -gt 0 ]; then
    Check1=ON
  fi
  TESTPORT=$(whiptail --title "SET DATV EXPRESS PORTS FOR TRANSVERTER 3" --checklist \
    "Select or deselect the active ports using the spacebar" 20 78 4 \
    "Port A" "J6 Pin 5" $Check1 \
    "Port B" "J6 Pin 6" $Check2 \
    "Port C" "J6 Pin 7" $Check3 \
    "Port D" "J6 Pin 10" $Check4 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    EXPPORTS7=0
    if (echo $TESTPORT | grep -q A); then
      EXPPORTS7=1
    fi
    if (echo $TESTPORT | grep -q B); then
      EXPPORTS7=$[$EXPPORTS7+2]
    fi
    if (echo $TESTPORT | grep -q C); then
      EXPPORTS7=$[$EXPPORTS7+4]
    fi
    if (echo $TESTPORT | grep -q D); then
      EXPPORTS7=$[$EXPPORTS7+8]
    fi
    set_config_var t3expports "$EXPPORTS7" $PATH_PPRESETS
  fi

  EXPLEVEL8=$(get_config_var t4explevel $PATH_PPRESETS)
  EXPLEVEL8=$(whiptail --inputbox "Enter 0 to 47" 8 78 $EXPLEVEL8 --title "SET DATV EXPRESS OUTPUT LEVEL FOR TRANSVERTER 4" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var t4explevel "$EXPLEVEL8" $PATH_PPRESETS
  fi

  Check1=OFF
  Check2=OFF
  Check3=OFF
  Check4=OFF
  EXPPORTS8=$(get_config_var t4expports $PATH_PPRESETS)
  TESTPORT=$EXPPORTS8
  if [ "$TESTPORT" -gt 7 ]; then
    Check4=ON
    TESTPORT=$[$TESTPORT-8]
  fi
  if [ "$TESTPORT" -gt 3 ]; then
    Check3=ON
    TESTPORT=$[$TESTPORT-4]
  fi
  if [ "$TESTPORT" -gt 1 ]; then
    Check2=ON
    TESTPORT=$[$TESTPORT-2]
  fi
  if [ "$TESTPORT" -gt 0 ]; then
    Check1=ON
  fi
  TESTPORT=$(whiptail --title "SET DATV EXPRESS PORTS FOR TRANSVERTER 4" --checklist \
    "Select or deselect the active ports using the spacebar" 20 78 4 \
    "Port A" "J6 Pin 5" $Check1 \
    "Port B" "J6 Pin 6" $Check2 \
    "Port C" "J6 Pin 7" $Check3 \
    "Port D" "J6 Pin 10" $Check4 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    EXPPORTS8=0
    if (echo $TESTPORT | grep -q A); then
      EXPPORTS8=1
    fi
    if (echo $TESTPORT | grep -q B); then
      EXPPORTS8=$[$EXPPORTS8+2]
    fi
    if (echo $TESTPORT | grep -q C); then
      EXPPORTS8=$[$EXPPORTS8+4]
    fi
    if (echo $TESTPORT | grep -q D); then
      EXPPORTS8=$[$EXPPORTS8+8]
    fi
    set_config_var t4expports "$EXPPORTS8" $PATH_PPRESETS
  fi
}

do_numbers()
{
  NUMBERS0=$(get_config_var d1numbers $PATH_PPRESETS)
  NUMBERS0=$(whiptail --inputbox "Enter 4 digits" 8 78 $NUMBERS0 --title "SET CONTEST NUMBERS FOR THE 71 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var d1numbers "$NUMBERS0" $PATH_PPRESETS
  fi

  NUMBERS1=$(get_config_var d2numbers $PATH_PPRESETS)
  NUMBERS1=$(whiptail --inputbox "Enter 4 digits" 8 78 $NUMBERS1 --title "SET CONTEST NUMBERS FOR THE 146 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var d2numbers "$NUMBERS1" $PATH_PPRESETS
  fi

  NUMBERS2=$(get_config_var d3numbers $PATH_PPRESETS)
  NUMBERS2=$(whiptail --inputbox "Enter 4 digits" 8 78 $NUMBERS2 --title "SET CONTEST NUMBERS FOR THE 437 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var d3numbers "$NUMBERS2" $PATH_PPRESETS
  fi

  NUMBERS3=$(get_config_var d4numbers $PATH_PPRESETS)
  NUMBERS3=$(whiptail --inputbox "Enter 4 digits" 8 78 $NUMBERS3 --title "SET CONTEST NUMBERS FOR THE 1255 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var d4numbers "$NUMBERS3" $PATH_PPRESETS
  fi

  NUMBERS4=$(get_config_var d5numbers $PATH_PPRESETS)
  NUMBERS4=$(whiptail --inputbox "Enter 4 digits" 8 78 $NUMBERS4 --title "SET CONTEST NUMBERS FOR THE 2400 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var d5numbers "$NUMBERS4" $PATH_PPRESETS
  fi

  NUMBERS5=$(get_config_var t1numbers $PATH_PPRESETS)
  NUMBERS5=$(whiptail --inputbox "Enter 4 digits" 8 78 $NUMBERS5 --title "SET CONTEST NUMBERS FOR TRANSVERTER 1" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var t1numbers "$NUMBERS5" $PATH_PPRESETS
  fi

  NUMBERS6=$(get_config_var t2numbers $PATH_PPRESETS)
  NUMBERS6=$(whiptail --inputbox "Enter 4 digits" 8 78 $NUMBERS6 --title "SET CONTEST NUMBERS FOR TRANSVERTER 2" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var t2numbers "$NUMBERS6" $PATH_PPRESETS
  fi

  NUMBERS7=$(get_config_var t3numbers $PATH_PPRESETS)
  NUMBERS7=$(whiptail --inputbox "Enter 4 digits" 8 78 $NUMBERS7 --title "SET CONTEST NUMBERS FOR TRANSVERTER 3" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var t3numbers "$NUMBERS7" $PATH_PPRESETS
  fi

  NUMBERS8=$(get_config_var t4numbers $PATH_PPRESETS)
  NUMBERS8=$(whiptail --inputbox "Enter 4 digits" 8 78 $NUMBERS8 --title "SET CONTEST NUMBERS FOR TRANSVERTER 4" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var t4numbers "$NUMBERS8" $PATH_PPRESETS
  fi
}

do_vfinder()
{
  V_FINDER=$(get_config_var vfinder $PCONFIGFILE)
  case "$V_FINDER" in
  on)
    Radio1=ON
    Radio2=OFF
  ;;
  off)
    Radio1=OFF
    Radio2=ON
  esac

  V_FINDER=$(whiptail --title "SET VIEWFINDER ON OR OFF" --radiolist \
    "Select one" 20 78 8 \
    "on" "Transmitted image displayed on Touchscreen" $Radio1 \
    "off" "Buttons displayed on touchscreen during transmit" $Radio2 \
    3>&2 2>&1 1>&3)

  if [ $? -eq 0 ]; then                     ## If the selection has changed
    set_config_var vfinder "$V_FINDER" $PCONFIGFILE
  fi
}

do_SD_info()
{
  $PATHSCRIPT"/sd_card_info.sh"
}

do_factory()
{
  FACTORY=""
  FACTORY=$(whiptail --inputbox "Enter y or n" 8 78 $FACTORY --title "RESET TO INITIAL SETTINGS?" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    if [[ "$FACTORY" == "y" || "$FACTORY" == "Y" ]]; then
      mv $PATHSCRIPT"/portsdown_config.txt" $PATHSCRIPT"/portsdown_config.txt.bak"
      cp $PATHSCRIPT"/configs/portsdown_config.txt.factory" $PATHSCRIPT"/portsdown_config.txt"
      mv $PATHSCRIPT"/portsdown_presets.txt" $PATHSCRIPT"/portsdown_presets.txt.bak"
      cp $PATHSCRIPT"/configs/portsdown_presets.txt.factory" $PATHSCRIPT"/portsdown_presets.txt"
      whiptail --title "Message" --msgbox "Factory Configuration Restored.  Please press enter to continue" 8 78
    else
      whiptail --title "Message" --msgbox "Current Configuration Retained.  Please press enter to continue" 8 78
    fi
  fi
}

do_touch_factory()
{
  TOUCH_FACTORY=""
  TOUCH_FACTORY=$(whiptail --inputbox "Enter y or n" 8 78 $TOUCH_FACTORY --title "RESET TOUCHSCREEN CALIBRATION?" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    if [[ "$TOUCH_FACTORY" == "y" || "$TOUCH_FACTORY" == "Y" ]]; then
      mv $PATHSCRIPT"/touchcal.txt" $PATHSCRIPT"/touchcal.txt.bak"
      cp $PATHSCRIPT"/configs/touchcal.txt.factory" $PATHSCRIPT"/touchcal.txt"
      whiptail --title "Message" --msgbox "Touchscreen calibration reset to zero.  Please press enter to continue" 8 78
    else
      whiptail --title "Message" --msgbox "Current Calibration Retained.  Please press enter to continue" 8 78
    fi
  fi
}

do_back_up()
{
  BACKUP=""
  BACKUP=$(whiptail --inputbox "Enter y or n" 8 78 $BACKUP --title "SAVE TO USB? EXISTING FILES WILL BE OVER-WRITTEN" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    if [[ "$BACKUP" == "y" || "$BACKUP" == "Y" ]]; then
      ls -l /dev/disk/by-uuid|grep -q sda  # returns 0 if USB drive connected
      if [ $? -eq 0 ]; then
        source /home/pi/rpidatv/scripts/copy_settings_to_usb.sh
        sudo umount /media/usb0 >/dev/null 2>/dev/null
        whiptail --title "Message" --msgbox "Config files copied to USB.  USB Drive unmounted.  Press enter to continue" 8 78
      else
        whiptail --title "Message" --msgbox "No USB Drive found.  Please press enter to continue" 8 78
      fi
    else
      whiptail --title "Message" --msgbox "Configuration files not copied.  Please press enter to continue" 8 78
    fi
  fi
}

do_load_settings()
{
  BACKUP=""
  BACKUP=$(whiptail --inputbox "Enter y or n" 8 78 $BACKUP --title "LOAD CONFIG FROM USB? EXISTING FILE WILL BE OVER-WRITTEN" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    if [[ "$BACKUP" == "y" || "$BACKUP" == "Y" ]]; then
      ls -l /dev/disk/by-uuid|grep -q sda  # returns 0 if USB drive connected
      if [ $? -eq 0 ]; then
        if [ -f /media/usb0/portsdown_settings/portsdown_config.txt ]; then
          source /home/pi/rpidatv/scripts/restore_from_USB.sh
          whiptail --title "Message" --msgbox "Configuration files copied from USB.  Please press enter to continue" 8 78
        else
          whiptail --title "Message" --msgbox "File portsdown_config.txt not found.  Please press enter to continue" 8 78
        fi
      else
        whiptail --title "Message" --msgbox "No USB Drive found.  Please press enter to continue" 8 78
      fi
    else
      whiptail --title "Message" --msgbox "Configuration files not copied.  Please press enter to continue" 8 78
    fi
  fi
}


do_Reboots()
{
  sudo crontab -l | grep -E -q '^0 3'
  if [ $? == 0 ]; then                # 12-hourly reboot enabled
    Radio1=OFF
    Radio2=ON
  else
    Radio1=ON
    Radio2=OFF
  fi

  REBOOTS=$(whiptail --title "SET 12-Hourly Reboots ON OR OFF" --radiolist \
    "Select one" 20 78 8 \
    "off" "No 12-hourly reboots scheduled" $Radio1 \
    "on" "Reboots scheduled for 0300 and 1500 UTC" $Radio2 \
    3>&2 2>&1 1>&3)

  if [ $? -eq 0 ]; then                     ## If the selection has changed
    if [[ "$REBOOTS" == "on" ]]; then
      sudo crontab /home/pi/rpidatv/scripts/configs/rptrcron
    else
      sudo crontab /home/pi/rpidatv/scripts/configs/blankcron
    fi
  fi
}

do_system_setup()
{
menuchoice=$(whiptail --title "$StrSystemTitle" --menu "$StrSystemContext" 20 78 13 \
    "1 Autostart" "$StrAutostartMenu"  \
    "2 Display" "$StrDisplayMenu" \
    "3 Show IP" "$StrIPMenu" \
    "4 WiFi Set-up" "SSID and password"  \
    "5 WiFi Off" "Turn the WiFi Off" \
    "6 Web Control" "Enable or Disable Web Control" \
    "7 Set-up EasyCap" "Set input socket and PAL/NTSC"  \
    "8 Audio Input" "Select USB Dongle or EasyCap"  \
    "9 Attenuator" "Select Output Attenuator Type"  \
    "10 Lime Status" "Check the LimeSDR Firmware Version"  \
    "11 Lime Update" "Update the LimeSDR Firmware Version"  \
    "12 Reboots" "Enable/disable 12-hourly Reboot"  \
    "13 Update" "Check for Updated Portsdown Software"  \
    3>&2 2>&1 1>&3)
    case "$menuchoice" in
      1\ *) do_autostart_setup ;;
      2\ *) do_display_setup   ;;
	  3\ *) do_IP_setup ;;
      4\ *) do_WiFi_setup ;;
      5\ *) do_WiFi_Off   ;;
      6\ *) do_Web_Control ;;
      7\ *) do_EasyCap ;;
      8\ *) do_audio_switch;;
      9\ *) do_attenuator;;
      10\ *) do_LimeStatus;;
      11\ *) do_LimeUpdate;;
      12\ *) do_Reboots ;;
      13\ *) do_Update ;;
    esac
}

do_system_setup_2()
{
  menuchoice=$(whiptail --title "$StrSystemTitle 2" --menu "$StrSystemContext" 20 78 13 \
    "1 Set Freq Presets" "For Touchscreen Frequencies"  \
    "2 Set SR Presets" "For Touchscreen Symbol Rates"  \
    "3 ADF4351 Ref Freq" "Set ADF4351 Reference Freq and Cal" \
    "4 Attenuator Levels" "Set Attenuator Levels for Each Band" \
    "5 Lime Gain" "Set the LimeGain for Each Band" \
    "6 DATV Express" "Configure DATV Express Settings for each band" \
    "7 Contest Numbers" "Set Contest Numbers for each band" \
    "8 Viewfinder" "Disable or Enable Viewfinder on Touchscreen" \
    "9 SD Card Info" "Show SD Card Information"  \
    "10 Factory Settings" "Restore Initial Configuration" \
    "11 Reset Touch Cal" "Reset Touchscreen Calibration to zero" \
    "12 Back-up Settings" "Save Settings to a USB drive" \
    "13 Load Settings" "Load settings from a USB Drive" \
    3>&2 2>&1 1>&3)
  case "$menuchoice" in
    1\ *) do_presets ;;
    2\ *) do_preset_SRs ;;
    3\ *) do_4351_ref  ;;
    4\ *) do_atten_levels ;;
    5\ *) do_set_limegain ;;
    6\ *) do_set_express ;;
    7\ *) do_numbers ;;
    8\ *) do_vfinder ;;
    9\ *) do_SD_info ;;
    10\ *) do_factory;;
    11\ *) do_touch_factory;;
    12\ *) do_back_up;;
    13\ *) do_load_settings;;
  esac
}

do_lmsver()
{
  reset
  LimeUtil --info
  printf "\nPress any key to return to the main menu\n"
  read -n 1
}

do_lfwver()
{
  reset
  LimeUtil --make
  printf "\nPress any key to return to the main menu\n"
  read -n 1
}

do_lqt()
{
  reset
  LimeQuickTest
  printf "\nPress any key to return to the main menu\n"
  read -n 1
}


do_ud130()
{
  UD130=""
  UD130=$(whiptail --inputbox "Enter y or n" 8 78 $UD130 --title "Load LimeSDR Mini Firmware 1.30?" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    if [[ "$UD130" == "y" || "$UD130" == "Y" ]]; then
      reset
      sudo LimeUtil --fpga=/home/pi/.local/share/LimeSuite/images/22.09/LimeSDR-Mini_HW_1.2_r1.30.rpd
      printf "\nPress any key to return to the main menu\n"
      read -n 1
    else
      whiptail --title "Message" --msgbox "Current LimeSDR Configuration Retained.  Please press enter to continue" 8 78
    fi
  fi
}

do_uddvb()
{
  UDDVB=""
  UDDVB=$(whiptail --inputbox "Enter y or n" 8 78 $UDDVB --title "Load LimeSDR Mini Custom DVB Firmware?" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    if [[ "$UDDVB" == "y" || "$UDDVB" == "Y" ]]; then
      reset
      sudo LimeUtil --force --fpga=/home/pi/.local/share/LimeSuite/images/v0.3/LimeSDR-Mini_lms7_trx_HW_1.2_auto.rpd
      printf "\nPress any key to return to the main menu\n"
      read -n 1
    else
      whiptail --title "Message" --msgbox "Current LimeSDR Configuration Retained.  Please press enter to continue" 8 78
    fi
  fi
}


do_lg()
{
  BAND=$(get_config_var band $PCONFIGFILE)
  LIMEGAIN=$(get-config_var limegain $PCONFIGFILE)
  case "$BAND" in
  d1)
    BAND_NAME=$(get_config_var d1label $PATH_PPRESETS)
  ;;
  d2)
    BAND_NAME=$(get_config_var d2label $PATH_PPRESETS)
  ;;
  d3)
    BAND_NAME=$(get_config_var d3label $PATH_PPRESETS)
  ;;
  d4)
    BAND_NAME=$(get_config_var d4label $PATH_PPRESETS)
  ;;
  d5)
    BAND_NAME=$(get_config_var d5label $PATH_PPRESETS)
  ;;
  t1)
    BAND_NAME=$(get_config_var t1label $PATH_PPRESETS)
  ;;
  t2)
    BAND_NAME=$(get_config_var t2label $PATH_PPRESETS)
  ;;
  t3)
    BAND_NAME=$(get_config_var t3label $PATH_PPRESETS)
  ;;
  t4)
    BAND_NAME=$(get_config_var t4label $PATH_PPRESETS)
  ;;
  esac

  LIMEGAIN=$(whiptail --inputbox "Current gain = "$LIMEGAIN".  Enter 0 to 100" 8 78 $LIMEGAIN --title "SET LIME GAIN FOR THE "$BAND_NAME" BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
 
  set_config_var limegain "$LIMEGAIN" $PCONFIGFILE
  case "$BAND" in
  d1)
    set_config_var d1limegain "$LIMEGAIN" $PATH_PPRESETS
  ;;
  d2)
    set_config_var d2limegain "$LIMEGAIN" $PATH_PPRESETS
  ;;
  d3)
    set_config_var d3limegain "$LIMEGAIN" $PATH_PPRESETS
  ;;
  d4)
    set_config_var d4limegain "$LIMEGAIN" $PATH_PPRESETS
  ;;
  d5)
    set_config_var d5limegain "$LIMEGAIN" $PATH_PPRESETS
  ;;
  t1)
    set_config_var t1limegain "$LIMEGAIN" $PATH_PPRESETS
  ;;
  t2)
    set_config_var t2limegain "$LIMEGAIN" $PATH_PPRESETS
  ;;
  t3)
    set_config_var t3limegain "$LIMEGAIN" $PATH_PPRESETS
  ;;
  t4)
    set_config_var t4limegain "$LIMEGAIN" $PATH_PPRESETS
  ;;
  esac
  fi
}

do_lupsample()
{
  UPSAMPLE=$(get-config_var upsample $PCONFIGFILE)
  UPSAMPLE=$(whiptail --inputbox "Current upsample = "$UPSAMPLE".  Enter 1, 2, 4 or 8" 8 78 $UPSAMPLE --title "SET UPSAMPLE for the LimeSDR" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var upsample "$UPSAMPLE" $PCONFIGFILE
  fi
}


do_lime_setup()
{
  menuchoice=$(whiptail --title "LimeSDR Configuration Menu" --menu "$StrSystemContext" 20 78 9 \
    "1 Show LimeSuite Version" "LimeSDR Driver Software"  \
    "2 Show Lime FW Version" "Firmware and Gateware Version"  \
    "3 Lime Quick Test" "Basic Test of LimeSDR" \
    "4 Update to FW 1.30" "Update LimeSDRMini to Firmware 1.30" \
    "5 Update to DVB FW" "Update LimeSDRMini to custom DVB Firmware" \
    "6 Set Lime Gain" "Set the Lime Gain for the current Band" \
    "7 Set Lime Upsampling" "Set the Lime Upsampling for cleaner TX" \
    3>&2 2>&1 1>&3)
  case "$menuchoice" in
    1\ *) do_lmsver ;;
    2\ *) do_lfwver ;;
    3\ *) do_lqt ;;
    4\ *) do_ud130 ;;
    5\ *) do_uddvb ;;
    6\ *) do_lg ;;
    7\ *) do_lupsample ;;
  esac
}

do_language_setup()
{
  menuchoice=$(whiptail --title "$StrLanguageTitle" --menu "$StrOutputContext" 16 78 6 \
    "1 French Menus" "Menus Francais"  \
    "2 English Menus" "Change Menus to English" \
    "3 German Menus" "Menüs auf Deutsch wechseln" \
    "4 French Keyboard" "$StrKeyboardChange" \
    "5 UK Keyboard" "$StrKeyboardChange" \
    "6 US Keyboard" "$StrKeyboardChange" \
      3>&2 2>&1 1>&3)
    case "$menuchoice" in
      1\ *) set_config_var menulanguage "fr" $PCONFIGFILE ;;
      2\ *) set_config_var menulanguage "en" $PCONFIGFILE ;;
      3\ *) set_config_var menulanguage "de" $PCONFIGFILE ;;
      4\ *) sudo cp $PATHCONFIGS"/keyfr" /etc/default/keyboard ;;
      5\ *) sudo cp $PATHCONFIGS"/keygb" /etc/default/keyboard ;;
      6\ *) sudo cp $PATHCONFIGS"/keyus" /etc/default/keyboard ;;
    esac

  # Check Language

  MENU_LANG=$(get_config_var menulanguage $PCONFIGFILE)

  # Set Language

  case "$MENU_LANG" in
  en)
    source $PATHSCRIPT"/langgb.sh"
  ;;
  fr)
    source $PATHSCRIPT"/langfr.sh"
  ;;
  de)
    source $PATHSCRIPT"/langde.sh"
  ;;
  *)
    source $PATHSCRIPT"/langgb.sh"
  ;;
  esac
}

do_Exit()
{
  exit
}

do_Reboot()
{
  sudo swapoff -a
  sudo reboot now
}

do_Shutdown()
{
  sudo swapoff -a
  sudo shutdown now
}

do_TouchScreen()
{
  reset
  /home/pi/rpidatv/scripts/scheduler.sh
}

do_KTransmit()
{
  /home/pi/rpidatv/bin/keyedtx 1 7
}

do_KStreamer()
{
  /home/pi/rpidatv/bin/keyedstream 1 7
}

do_CStreamer()
{
  /home/pi/rpidatv/bin/keyedstream 0
}

do_EnableButtonSD()
{
  cp $PATHCONFIGS"/text.pi-sdn" /home/pi/.pi-sdn  ## Load it at logon
  /home/pi/.pi-sdn                                ## Load it now
}

do_DisableButtonSD()
{
  rm /home/pi/.pi-sdn             ## Stop it being loaded at log-on
  sudo pkill -x pi-sdn            ## kill the current process
} 

do_shutdown_menu()
{
menuchoice=$(whiptail --title "Shutdown Menu" --menu "Select Choice" 16 78 10 \
    "1 Shutdown now" "Immediate Shutdown"  \
    "2 Reboot now" "Immediate reboot" \
    "3 Exit to Linux" "Exit menu to Command Prompt" \
    "4 Restore TouchScreen" "Exit to LCD.  Use ctrl-C to return" \
    "5 Start Keyed TX" "Start the GPIO keyed Transmitter" \
    "6 Start Keyed Streamer" "Start the keyed Repeater Streamer" \
    "7 Start Constant Streamer" "Start the constant Repeater Streamer" \
    "8 Button Enable" "Enable Shutdown Button" \
    "9 Button Disable" "Disable Shutdown Button" \
      3>&2 2>&1 1>&3)
    case "$menuchoice" in
        1\ *) do_Shutdown ;;
        2\ *) do_Reboot ;;
        3\ *) do_Exit ;;
        4\ *) do_TouchScreen ;;
        5\ *) do_KTransmit ;;
        6\ *) do_KStreamer ;;
        7\ *) do_CStreamer ;;
        8\ *) do_EnableButtonSD ;;
        9\ *) do_DisableButtonSD ;;
    esac
}

display_splash()
{
  sudo fbi -T 1 -noverbose -a $PATHSCRIPT"/images/BATC_Black.png" >/dev/null 2>/dev/null
  (sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work
}

OnStartup()
{
  CALL=$(get_config_var call $PCONFIGFILE)
  MODE_INPUT=$(get_config_var modeinput $PCONFIGFILE)
  MODE_OUTPUT=$(get_config_var modeoutput $PCONFIGFILE)
  SYMBOLRATEK=$(get_config_var symbolrate $PCONFIGFILE)
  FEC=$(get_config_var fec $PCONFIGFILE)
  MODULATION=$(get_config_var modulation $PCONFIGFILE)
  PATHTS=$(get_config_var pathmedia $PCONFIGFILE)
  FREQ_OUTPUT=$(get_config_var freqoutput $PCONFIGFILE)
  GAIN_OUTPUT=$(get_config_var rfpower $PCONFIGFILE)
  do_fec_lookup
  V_FINDER=$(get_config_var vfinder $PCONFIGFILE)
  MODE_STARTUP=$(get_config_var startup $PCONFIGFILE)

  INFO=$CALL":"$MODE_INPUT"-->"$MODE_OUTPUT" "$FREQ_OUTPUT" MHz "$SYMBOLRATEK" KS, "$MODULATION", FEC "$FECNUM"/"$FECDEN""

  do_transmit
}

#********************************************* MAIN MENU *********************************
#************************* Execution of Console Menu starts here *************************

# Check Language
MENU_LANG=$(get_config_var menulanguage $PCONFIGFILE)

# Set Language
  case "$MENU_LANG" in
  en)
    source $PATHSCRIPT"/langgb.sh"
  ;;
  fr)
    source $PATHSCRIPT"/langfr.sh"
  ;;
  de)
    source $PATHSCRIPT"/langde.sh"
  ;;
  *)
    source $PATHSCRIPT"/langgb.sh"
  ;;
  esac


#if [ "$MENU_LANG" == "en" ]; then
#  source $PATHSCRIPT"/langgb.sh"
#else
#  source $PATHSCRIPT"/langfr.sh"
#fi

# Display Splash on Touchscreen if fitted
# display_splash
status="0"

# Start DATV Express Server if required
MODE_OUTPUT=$(get_config_var modeoutput $PCONFIGFILE)
SYMBOLRATEK=$(get_config_var symbolrate $PCONFIGFILE)
if [ "$MODE_OUTPUT" == "DATVEXPRESS" ]; then
  if pgrep -x "express_server" > /dev/null; then
    # Express already running so do nothing
    :
  else
    # Not running and needed, so start it
    echo "Starting the DATV Express Server.  Please wait."
    # Make sure that the Control file is not locked
    sudo rm /tmp/expctrl >/dev/null 2>/dev/null
    # From its own folder otherwise it doesn't read the config file
    cd /home/pi/express_server
    sudo nice -n -40 /home/pi/express_server/express_server  >/dev/null 2>/dev/null &
    cd /home/pi
    sleep 5                # Give it time to start
    reset                  # Clear message from screen
  fi
fi

# Set Band, Filter and Port Switching
$PATHSCRIPT"/ctlfilter.sh"

# Check whether to go straight to transmit or display the menu
if [ "$1" != "menu" ]; then                                     # if tx on boot
  OnStartup                                                     # go straight to transmit
fi

sleep 0.2

# Loop round main menu
while [ "$status" -eq 0 ] 
  do

    # Lookup parameters for Menu Info Message
    CALL=$(get_config_var call $PCONFIGFILE)
    MODE_INPUT=$(get_config_var modeinput $PCONFIGFILE)
    MODE_OUTPUT=$(get_config_var modeoutput $PCONFIGFILE)
    SYMBOLRATEK=$(get_config_var symbolrate $PCONFIGFILE)
    FEC=$(get_config_var fec $PCONFIGFILE)
    MODULATION=$(get_config_var modulation $PCONFIGFILE)
    PATHTS=$(get_config_var pathmedia $PCONFIGFILE)
    FREQ_OUTPUT=$(get_config_var freqoutput $PCONFIGFILE)
    GAIN_OUTPUT=$(get_config_var rfpower $PCONFIGFILE)
    do_fec_lookup
    INFO=$CALL": "$MODE_INPUT" -> "$MODE_OUTPUT" "$FREQ_OUTPUT" MHz "$SYMBOLRATEK" KS, "$MODULATION", FEC "$FECNUM"/"$FECDEN""
    V_FINDER=$(get_config_var vfinder $PCONFIGFILE)

    # Display main menu

    menuchoice=$(whiptail --title "$StrMainMenuTitle" --menu "$INFO" 16 82 10 \
	"0 Transmit" $FREQ_OUTPUT" MHz, "$SYMBOLRATEK" KS, "$MODULATION", FEC "$FECNUM"/"$FECDEN"" \
    "1 Source" "$StrMainMenuSource"" ("$MODE_INPUT" selected)" \
	"2 Output" "$StrMainMenuOutput"" ("$MODE_OUTPUT" selected)" \
	"3 Station" "$StrMainMenuCall" \
	"4 Receive" "$StrMainMenuReceive" \
	"5 System" "$StrMainMenuSystem" \
    "6 System 2" "$StrMainMenuSystem2" \
    "7 Lime Config  " "LimeSDR Mini Info and Configuration" \
	"8 Language" "$StrMainMenuLanguage" \
    "9 Shutdown" "$StrMainMenuShutdown" \
 	3>&2 2>&1 1>&3)

        case "$menuchoice" in
	    0\ *) do_transmit   ;;
        1\ *) do_input_setup   ;;
	    2\ *) do_output_setup ;;
   	    3\ *) do_station_setup ;;
	    4\ *) do_receive_menu ;;
	    5\ *) do_system_setup ;;
	    6\ *) do_system_setup_2 ;;
	    7\ *) do_lime_setup ;;
        8\ *) do_language_setup ;;
        9\ *) do_shutdown_menu ;;
           *)

        # Display exit message if user jumps out of menu
        whiptail --title "$StrMainMenuExitTitle" --msgbox "$StrMainMenuExitContext" 8 78

        # Set status to exit
        status=1

        # Sleep while user reads message, then exit
        sleep 1
      exit ;;
    esac
    exitstatus1=$status1
  done
exit
