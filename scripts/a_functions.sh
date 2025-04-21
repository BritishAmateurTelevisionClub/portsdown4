# Functions called by a.sh.  First used in Buster Build 201911230

############ FUNCTION TO READ CONFIG FILE #############################

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

############ FUNCTION TO IDENTIFY AUDIO DEVICES #############################

detect_audio()
{
  # Returns AUDIO_CARD=1 if any audio dongle, or video dongle with
  # audio, detected.  Else AUDIO_CARD=0

  # Then, depending on the values of  $AUDIO_PREF and $MODE_INPUT it sets
  # AUDIO_CARD_NUMBER (typically 0, 1 2 or 3)
  # AUDIO_CHANNELS (0 for no audio required, 1 for mic, 2 for video stereo capture)
  # AUDIO_SAMPLE (44100 for mic, 48000 for video stereo capture, and ?? for Webcam)

  # Set initial conditions for later testing
  MIC=9
  USBTV=9
  WCAM=9
  VID_WEBCAM="nk"

  printf "Audio selection = $AUDIO_PREF \n"

  # Check on picture input type
  case "$MODE_INPUT" in
  "ANALOGCAM" | "ANALOGMPEG-2" | "ANALOG16MPEG-2" )
     PIC_INPUT="ANALOG"
  printf "Video Input is $PIC_INPUT \n"
  ;;
  "WEBCAMH264" | "WEBCAMMPEG-2" | "WEBCAM16MPEG-2" | "WEBCAMHDMPEG-2" \
  | "C920H264" | "C920HDH264" | "C920FHDH264")
    PIC_INPUT="WEBCAM"
  ;;
  *)
    PIC_INPUT="DIGITAL"
    printf "Mode Input is $MODE_INPUT \n"
  ;;
  esac
  printf "Video Input is $PIC_INPUT \n"

  # First check if any audio card is present
  arecord -l | grep -q 'card'
  if [ $? != 0 ]; then   ## not present
    printf "Audio card not present\n"
    # No known card detected so take the safe option and go for beeps
    # unless noaudio selected:
    if [ "$AUDIO_PREF" == "no_audio" ]; then  # no audio
      AUDIO_CARD=0
      AUDIO_CHANNELS=0
      AUDIO_SAMPLE=44100
    else                                      # bleeps
      AUDIO_CARD=0
      AUDIO_CHANNELS=1
      AUDIO_SAMPLE=44100
    fi
  else                   ## card detected
    printf "Audio card present\n"
    # Check for the presence of a dedicated audio device
    arecord -l | grep -E -q "USB Audio Device|USB AUDIO|Head|Sound Device"
    if [ $? == 0 ]; then   ## Present
      # Look for the dedicated USB Audio Device, select the line and take
      # the 6th character.  Max card number = 8 !!
      MIC="$(arecord -l | grep -E "USB Audio Device|USB AUDIO|Head|Sound Device" \
        | head -c 6 | tail -c 1)"
    fi

    # Check for the presence of a Video dongle with audio
    arecord -l | grep -E -q \
      "usbtv|U0x534d0x21|DVC90|Cx231xxAudio|STK1160|U0xeb1a0x2861|AV TO USB|Grabby"
    if [ $? == 0 ]; then   ## Present
      # Look for the video dongle, select the line and take
      # the 6th character.  Max card number = 8 !!
      USBTV="$(arecord -l | grep -E \
        "usbtv|U0x534d0x21|DVC90|Cx231xxAudio|STK1160|U0xeb1a0x2861|AV TO USB|Grabby" \
        | head -c 6 | tail -c 1)"
    fi

    CamLink4KPresent=0
    # Check for the presence of an Elgato CamLink4K with stereo audio
    arecord -l | grep -E -q \
      "C4K"
    if [ $? == 0 ]; then   ## Present
      CamLink4KPresent=1
      # Look for the video dongle, select the line and take
      # the 6th character.  Max card number = 8 !!
      WCAM="$(arecord -l | grep -E \
        "C4K" \
        | head -c 6 | tail -c 1)"
      WC_AUDIO_CHANNELS=2
      WC_AUDIO_SAMPLE=48000
      WC_VIDEO_FPS=29.97
    fi
 
    C920Present=0
    # Check for the presence of a C920 Webcam with stereo audio
    arecord -l | grep -E -q \
      "Webcam C920"
    if [ $? == 0 ]; then   ## Present
      C920Present=1
      # Look for the video dongle, select the line and take
      # the 6th character.  Max card number = 8 !!
      WCAM="$(arecord -l | grep -E \
        "Webcam C920" \
        | head -c 6 | tail -c 1)"
      WC_AUDIO_CHANNELS=2
      WC_AUDIO_SAMPLE=48000
      WC_VIDEO_FPS=30
    fi

    C930ePresent=0
    # Check for the presence of a C930e Webcam with stereo audio
    arecord -l | grep -E -q \
      "Webcam C930e"
    if [ $? == 0 ]; then   ## Present
      C930ePresent=1
      # Look for the webcam's audio card, select the line and take
      # the 6th character.  Max card number = 8 !!
      WCAM="$(arecord -l | grep -E \
        "Webcam C930e" \
        | head -c 6 | tail -c 1)"
      WC_AUDIO_CHANNELS=2
      WC_AUDIO_SAMPLE=48000
      WC_VIDEO_FPS=30
    fi

    C910Present=0
    # Check for the presence of an old C910 Webcam with stereo audio
    arecord -l | grep -E -q \
      "U0x46d0x821"
    if [ $? == 0 ]; then   ## Present
      C910Present=1
      # Look for the video dongle, select the line and take
      # the 6th character.  Max card number = 8 !!
      WCAM="$(arecord -l | grep -E \
        "U0x46d0x821" \
        | head -c 6 | tail -c 1)"
      WC_AUDIO_CHANNELS=2
      WC_AUDIO_SAMPLE=48000
      WC_VIDEO_FPS=30
    fi

    # Check for the presence of a C525 or C170 with mono audio
    C170Present=0
    arecord -l | grep -E -q \
      "Webcam C525|Webcam C170"
    if [ $? == 0 ]; then   ## Present
      C170Present=1
      # Look for the video dongle, select the line and take
      # the 6th character.  Max card number = 8 !!
      WCAM="$(arecord -l | grep -E \
        "Webcam C525|Webcam C170" \
        | head -c 6 | tail -c 1)"
      WC_AUDIO_CHANNELS=1
      WC_AUDIO_SAMPLE=48000
      WC_VIDEO_FPS=15
    fi

    # Check for the presence of a new C910 Webcam with stereo audio
    arecord -l | grep -E -q \
      "U0x46d0x823"
    if [ $? == 0 ]; then   ## Present
      C910Present=1
      # Look for the video dongle, select the line and take
      # the 6th character.  Max card number = 8 !!
      WCAM="$(arecord -l | grep -E \
        "U0x46d0x823" \
        | head -c 6 | tail -c 1)"
      WC_AUDIO_CHANNELS=2
      WC_AUDIO_SAMPLE=32000
      WC_VIDEO_FPS=30
    fi

    # Check for the presence of a C270 with mono audio
    C270Present=0
    arecord -l | grep -E -q \
      "U0x46d0x825"
    if [ $? == 0 ]; then   ## Present
      C270Present=1
      # Look for the video dongle, select the line and take
      # the 6th character.  Max card number = 8 !!
      WCAM="$(arecord -l | grep -E \
        "U0x46d0x825" \
        | head -c 6 | tail -c 1)"
      WC_AUDIO_CHANNELS=1
      WC_AUDIO_SAMPLE=48000
      WC_VIDEO_FPS=25
    fi

    # Check for the presence of a C310 with mono audio
    C310Present=0
    arecord -l | grep -E -q \
      "U0x46d0x81b"
    if [ $? == 0 ]; then   ## Present
      C310Present=1
      # Look for the video dongle, select the line and take
      # the 6th character.  Max card number = 8 !!
      WCAM="$(arecord -l | grep -E \
        "U0x46d0x81b" \
        | head -c 6 | tail -c 1)"
      WC_AUDIO_CHANNELS=1
      WC_AUDIO_SAMPLE=48000
      WC_VIDEO_FPS=25
    fi

    Logi4KPresent=0
    # Check for the presence of an Logi 4K Stream Edition with stereo audio
    arecord -l | grep -E -q \
      "Logi 4K"
    if [ $? == 0 ]; then   ## Present
      Logi4KPresent=1

      WCAM="$(arecord -l | grep -E \
        "Logi 4K" \
        | head -c 6 | tail -c 1)"
      WC_AUDIO_CHANNELS=2
      WC_AUDIO_SAMPLE=32000
      WC_VIDEO_FPS=30
    fi

    Ali4KPresent=0
    # Check for the presence of PA3FBX's Ali Express 4K UHD Webcam
    arecord -l | grep -E -q \
      "WEBCAM [USB WEBCAM], device 0: USB Audio [USB Audio]"
    if [ $? == 0 ]; then   ## Present
      Ali4KPresent=1

      WCAM="$(arecord -l | grep -E \
        "WEBCAM [USB WEBCAM], device 0: USB Audio [USB Audio]" \
        | head -c 6 | tail -c 1)"
      WC_AUDIO_CHANNELS=1
      WC_AUDIO_SAMPLE=16000
      WC_VIDEO_FPS=30
    fi

    printf "MIC = $MIC\n"
    printf "USBTV = $USBTV\n"
    printf "WCAM = $WCAM\n"
    printf "AUDIO_PREF = $PIC_INPUT\n"

    # At least one card detected, so sort out what card parameters are used
    case "$AUDIO_PREF" in
      auto)                                                 # Auto selected
        case "$PIC_INPUT" in
          "DIGITAL")                                        # Using Pi Cam video
            if [ "$MIC" != "9" ] && [ "$CamLink4KPresent" != "1" ]; then                      # Mic available
              AUDIO_CARD=1                                  # so use mic audio
              AUDIO_CARD_NUMBER=$MIC
              AUDIO_CHANNELS=1
              AUDIO_SAMPLE=48000
            elif [ "$USBTV" != "9" ] && [ "$MIC" == "9" ] && [ "$CamLink4KPresent" != "1" ]; then # Mic not available, but EasyCap is
              AUDIO_CARD=1                                  # so use EasyCap audio (correct for old and new EasyCaps)
              AUDIO_CARD_NUMBER=$USBTV
              AUDIO_CHANNELS=2
              AUDIO_SAMPLE=48000
            elif [ "$CamLink4KPresent" == "1" ]; then
              AUDIO_CARD=1                                  # so use CamLink Audio
              AUDIO_CARD_NUMBER=$WCAM
              AUDIO_CHANNELS=2
              AUDIO_SAMPLE=48000
            else                                            # Neither available
              AUDIO_CARD=0                                  # So no audio
              AUDIO_CHANNELS=0
              AUDIO_SAMPLE=44100
            fi
          ;;
          "ANALOG")                                         # Using Easycap video
            if [ "$USBTV" != "9" ]; then                    # Easycap Audio available
              AUDIO_CARD=1                                  # so use EasyCap audio
              AUDIO_CARD_NUMBER=$USBTV
              AUDIO_CHANNELS=2
              AUDIO_SAMPLE=48000
            elif [ "$MIC" != "9" ] && [ "$USBTV" == "9" ]; then # EasyCap not available, but Mic is
              AUDIO_CARD=1                                  # so use Mic audio
              AUDIO_CARD_NUMBER=$MIC
              AUDIO_CHANNELS=1
              AUDIO_SAMPLE=48000
            else                                            # Neither available
              AUDIO_CARD=0                                  # So no audio
              AUDIO_CHANNELS=0
              AUDIO_SAMPLE=44100
            fi
          ;;
          "WEBCAM")                                         # Using Webcam video
            if [ "$WCAM" != "9" ]; then                     # Webcam Audio available
              AUDIO_CARD=1                                  # So use Webcam audio
              AUDIO_CARD_NUMBER=$WCAM
              AUDIO_CHANNELS=$WC_AUDIO_CHANNELS
              AUDIO_SAMPLE=$WC_AUDIO_SAMPLE
            elif [ "$MIC" != "9" ] && [ "$WCAM" == "9" ]; then # Webcam not available, but Mic is
              AUDIO_CARD=1                                  # so use Mic audio
              AUDIO_CARD_NUMBER=$MIC
              AUDIO_CHANNELS=1
              AUDIO_SAMPLE=48000
            else                                            # Neither available
              AUDIO_CARD=0                                  # So no audio
              AUDIO_CHANNELS=0
              AUDIO_SAMPLE=44100
            fi
          ;;
        esac
      ;;
      mic)                                                  # Mic selected
        if [ "$MIC" != "9" ]; then                          # Mic card detected
          AUDIO_CARD=1                                      # so use mic
          AUDIO_CARD_NUMBER=$MIC
          AUDIO_CHANNELS=1
          AUDIO_SAMPLE=48000
        else                                                # No mic card
          AUDIO_CARD=0                                      # So no audio
          AUDIO_CHANNELS=0
          AUDIO_SAMPLE=44100
        fi
      ;;
      video)                                                # EasyCap selected
        if [ "$USBTV" != "9" ]; then                        # EasyCap detected
          AUDIO_CARD=1                                      # So use EasyCap
          AUDIO_CARD_NUMBER=$USBTV
          AUDIO_CHANNELS=2
          AUDIO_SAMPLE=48000
        else                                                # No EasyCap
          AUDIO_CARD=0                                      # So no audio
          AUDIO_CHANNELS=0
          AUDIO_SAMPLE=44100
        fi
      ;;
      webcam)                                               # Webcam selected
        if [ "$WCAM" != "9" ]; then                         # Webcam detected
          AUDIO_CARD=1                                      # So use Webcam
          AUDIO_CARD_NUMBER=$WCAM
          AUDIO_CHANNELS=$WC_AUDIO_CHANNELS
          AUDIO_SAMPLE=$WC_AUDIO_SAMPLE
        else                                                # No Webcam Audio
          AUDIO_CARD=1                                      # so use mic
          AUDIO_CARD_NUMBER=$MIC
          AUDIO_CHANNELS=1
          AUDIO_SAMPLE=48000
        fi
      ;;
      bleeps)
        AUDIO_CARD=0
        AUDIO_CHANNELS=1
        AUDIO_SAMPLE=44100
      ;;
      no_audio)
        AUDIO_CARD=0
        AUDIO_CARD_NUMBER=9
        AUDIO_CHANNELS=0
        AUDIO_SAMPLE=44100
      ;;
      *)                                                    # Unidentified option
        AUDIO_CARD=0                                        # No Audio
        AUDIO_CARD_NUMBER=9
        AUDIO_CHANNELS=0
        AUDIO_SAMPLE=44100
      ;;
    esac
  fi

  printf "AUDIO_CARD = $AUDIO_CARD\n"
  printf "AUDIO_CARD_NUMBER = $AUDIO_CARD_NUMBER \n"
  printf "AUDIO_CHANNELS = $AUDIO_CHANNELS \n"
  printf "AUDIO_SAMPLE = $AUDIO_SAMPLE \n"
}

############ FUNCTION TO IDENTIFY VIDEO DEVICES #############################

detect_video()
{
  # List the video devices, select the 2 lines for any listed device, then
  # select the line with the device details and delete the leading tab
 
  #                                                         Check for any device with "Webcam" in its description
  VID_WEBCAM="$(v4l2-ctl --list-devices 2> /dev/null | \
    sed -n '/Webcam/,/dev/p' | grep 'dev' | tr -d '\t')"

  if [ "${#VID_WEBCAM}" -lt "10" ]; then #                 Check for Logitech C270

    # List the video devices, select the 2 lines for a C270 device, then
    # select the line with the device details and delete the leading tab
    VID_WEBCAM="$(v4l2-ctl --list-devices 2> /dev/null | \
      sed -n '/046d:0825/,/dev/p' | grep 'dev' | tr -d '\t')"
  fi

  if [ "${#VID_WEBCAM}" -lt "10" ]; then #                 Check for Logitech C910

    # List the video devices, select the 2 lines for a C910 device, then
    # select the line with the device details and delete the leading tab
    VID_WEBCAM="$(v4l2-ctl --list-devices 2> /dev/null | \
      sed -n '/046d:0821/,/dev/p' | grep 'dev' | tr -d '\t')"
  fi

  if [ "${#VID_WEBCAM}" -lt "10" ]; then #                 Check for Logitech B910

    # List the video devices, select the 2 lines for a new C910 (B910) device, then
    # select the line with the device details and delete the leading tab
    VID_WEBCAM="$(v4l2-ctl --list-devices 2> /dev/null | \
      sed -n '/046d:0823/,/dev/p' | grep 'dev' | tr -d '\t')"
  fi

  if [ "${#VID_WEBCAM}" -lt "10" ]; then #                 Check for Logitech C310

    # List the video devices, select the 2 lines for a C310 device, then
    # select the line with the device details and delete the leading tab
    VID_WEBCAM="$(v4l2-ctl --list-devices 2> /dev/null | \
      sed -n '/046d:081b/,/dev/p' | grep 'dev' | tr -d '\t')"
  fi

  if [ "${#VID_WEBCAM}" -lt "10" ]; then #                 Check for Polycom EagleEye

    # List the video devices, select the 2 lines for a Polycom EagleEye device, then
    # select the line with the device details and delete the leading tab
    VID_WEBCAM="$(v4l2-ctl --list-devices 2> /dev/null | \
      sed -n '/EagleEye/,/dev/p' | grep 'dev' | tr -d '\t')"
  fi

  if [ "${#VID_WEBCAM}" -lt "10" ]; then #                 Check for CamLink 4K

    # List the video devices, select the 2 lines for a CamLink 4K device, then
    # select the line with the device details and delete the leading tab
    VID_WEBCAM="$(v4l2-ctl --list-devices 2> /dev/null | \
      sed -n '/Cam Link 4K/,/dev/p' | grep 'dev' | tr -d '\t')"
  fi

  if [ "${#VID_WEBCAM}" -lt "10" ]; then #                 Check for BRIO 4K Stream Edition 
    # List the video devices, select the 2 lines for a BRIO 4K Stream Edition device, then
    # select the line with the device details and delete the leading tab
    VID_WEBCAM="$(v4l2-ctl --list-devices 2> /dev/null | \
      sed -n '/Logi 4K Stream Edition/,/dev/p' | grep 'dev' | tr -d '\t')"
  fi

  if [ "${#VID_WEBCAM}" -lt "10" ]; then #                 Check for Ali Express 4K UHD 
    # List the video devices, select the 2 lines for an Ali Express 4K UHD device, then
    # select the line with the device details and delete the leading tab
    VID_WEBCAM="$(v4l2-ctl --list-devices 2> /dev/null | \
      sed -n '/USB WEBCAM: USB WEBCAM/,/dev/p' | grep 'dev' | tr -d '\t')"
  fi

  printf "The first Webcam device string is $VID_WEBCAM\n"

  # List the video devices, select the 2 lines for any usb device, then
  # select the line with the device details and delete the leading tab
  VID_USB1="$(v4l2-ctl --list-devices 2> /dev/null | \
    sed -n '/usb/,/dev/p' | grep 'dev' | tr -d '\t' | head -n1)"
  
  printf "The first USB device string is $VID_USB1\n"
 
  VID_USB2="$(v4l2-ctl --list-devices 2> /dev/null | \
    sed -n '/usb/,/dev/p' | grep 'dev' | tr -d '\t' | tail -n1)"
  printf "The second USB device string is $VID_USB2\n"

  if [ "$VID_USB1" != "$VID_WEBCAM" ]; then
    VID_USB=$VID_USB1
  printf "The first test passed"
  fi

  if [ "$VID_USB2" != "$VID_WEBCAM" ]; then
    VID_USB=$VID_USB2
  printf "The second test passed"
  fi  
  printf "The final USB device string is $VID_USB\n"

  # List the video devices, select the 2 lines for any mmal device, then
  # select the line with the device details and delete the leading tab
  VID_PICAM="$(v4l2-ctl --list-devices 2> /dev/null | \
    sed -n '/mmal/,/dev/p' | grep 'dev' | tr -d '\t')"

  if [ "$VID_USB" == '' ]; then
    printf "VID_USB was not found, setting to /dev/video0\n"
    VID_USB="/dev/video0"
  fi
  if [ "$VID_PICAM" == '' ]; then
    printf "VID_PICAM was not found, setting to /dev/video0\n"
    VID_PICAM="/dev/video0"
  fi
  if [ "$VID_WEBCAM" == '' ]; then
    printf "VID_WEBCAM was not found, setting to /dev/video2\n"
    VID_WEBCAM="/dev/video2"
  fi

  printf "The PI-CAM device string is $VID_PICAM\n"
  printf "The USB device string is $VID_USB\n"
  printf "The Webcam device string is $VID_WEBCAM\n"

  # Check for the presence of a new C920 without H264 encoder
  NEWC920Present=0
  lsusb | grep -q "046d:0892"
  if [ $? == 0 ]; then   ## New C920 connected
    NEWC920Present=1
  fi
  # Note that C920 present will also be set to 1
}

  ################ Check Webcam Type (not all are compatible) #########################
  ################ Only one can be connected at any one time  #########################
  # Detects:
  # OldC920
  # OrbicamC920
  # NewerC920
  # C930e
  # C170
  # C270
  # C910
  # B910
  # C310
  # BRIO4K
  # Webcam

  WEBCAM_TYPE="None"

  lsusb | grep -q "046d:082d"
  if [ $? == 0 ]; then
    WEBCAM_TYPE="OldC920"
  fi

  if [ "$WEBCAM_TYPE" == "None" ]; then
    lsusb | grep -q "046d:0892"
    if [ $? == 0 ]; then
      WEBCAM_TYPE="OrbicamC920"
    fi
  fi

  if [ "$WEBCAM_TYPE" == "None" ]; then
    lsusb | grep -q "046d:08e5"
    if [ $? == 0 ]; then
      WEBCAM_TYPE="NewerC920"
    fi
  fi

  if [ "$WEBCAM_TYPE" == "None" ]; then
    lsusb | grep -q "046d:0843"
    if [ $? == 0 ]; then
      WEBCAM_TYPE="C930e"
    fi
  fi

  if [ "$WEBCAM_TYPE" == "None" ]; then
    lsusb | grep -q "046d:082b"
    if [ $? == 0 ]; then
      WEBCAM_TYPE="C170"
    fi
  fi

  if [ "$WEBCAM_TYPE" == "None" ]; then
    lsusb | grep -q "046d:0825"
    if [ $? == 0 ]; then
      WEBCAM_TYPE="C270"
    fi
  fi

  if [ "$WEBCAM_TYPE" == "None" ]; then
    lsusb | grep -q "046d:0821"
    if [ $? == 0 ]; then
      WEBCAM_TYPE="C910"
    fi
  fi

  if [ "$WEBCAM_TYPE" == "None" ]; then
    lsusb | grep -q "046d:0823"
    if [ $? == 0 ]; then
      WEBCAM_TYPE="B910"
    fi
  fi

  if [ "$WEBCAM_TYPE" == "None" ]; then
    lsusb | grep -q "046d:0826"
    if [ $? == 0 ]; then
      WEBCAM_TYPE="C525"
    fi
  fi

  if [ "$WEBCAM_TYPE" == "None" ]; then
    lsusb | grep -q "046d:0823"
    if [ $? == 0 ]; then
      WEBCAM_TYPE="C310"
    fi
  fi

  if [ "$WEBCAM_TYPE" == "None" ]; then
    lsusb | grep -q "095d:3001"
    if [ $? == 0 ]; then
      WEBCAM_TYPE="EagleEye"
    fi
  fi

  if [ "$WEBCAM_TYPE" == "None" ]; then
    lsusb | grep -q "0fd9:0066"
    if [ $? == 0 ]; then
      WEBCAM_TYPE="CamLink4K"
    fi
  fi

  if [ "$WEBCAM_TYPE" == "None" ]; then
    lsusb | grep -q "0fd9:0067"
    if [ $? == 0 ]; then
      WEBCAM_TYPE="CamLink4K"
    fi
  fi

  if [ "$WEBCAM_TYPE" == "None" ]; then
    lsusb | grep -q "046d:086b"
    if [ $? == 0 ]; then
      WEBCAM_TYPE="BRIO4K"
    fi
  fi

  if [ "$WEBCAM_TYPE" == "None" ]; then
    lsusb | grep -q "1b3f:1167"
    if [ $? == 0 ]; then
      WEBCAM_TYPE="ALI4K"
    fi
  fi

  if [ "$WEBCAM_TYPE" == "None" ]; then
    lsusb | grep -q "Webcam"
    if [ $? == 0 ]; then
      WEBCAM_TYPE="Webcam"
    fi
  fi

  if [ "$WEBCAM_TYPE" == "None" ]; then
    printf "No Webcam identified\n"
  else 
    printf "Found Webcam of type $WEBCAM_TYPE\n"
  fi


