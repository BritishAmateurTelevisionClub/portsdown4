#!/bin/bash
# Check for the presence of a Muntjac
# Echo 0 if present, echo 1 if not detected

lsusb | grep -E -q "2e8a:000a"
if [ $? == 0 ]; then   ## Present
  echo "0"
else
  echo "1"
fi 

exit
