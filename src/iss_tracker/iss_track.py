#!/usr/bin/python3
"""
ISS Tracker for Portsdown (rotctl etc) - A cobble by Paul M0EYT, extended by G8GKQ.
pip3 install ephem requests
install hamlib to give you rotctl then run python3 iss_tracker.py

Options:
  --lat FLOAT       Observer latitude  in decimal degrees
  --lon FLOAT       Observer longitude in decimal degrees
  --alt FLOAT       Observer altitude  in metres            (default: 0)
  --device STR      Serial device for rotctl  (default: /dev/ttyUSB2)
  --model INT       Hamlib rotator model number (default: 603  G6LVB EasycommII)
  --baud INT        Serial baud rate            (default: 9600)
  --min-el FLOAT    Minimum elevation to track  (default: 0)
  --tmp-path STR    Path for RamDisk            (default: /home/pi/tmp/)
  --park-az FLOAT   Azimuth  to park at after each pass (default: 180.0  = North)
  --park-el FLOAT   Elevation to park at after each pass (default: 90.0 = Up)
  --no-park         Disable parking; just stop the rotator at LOS
  --park-now        Don't track, just park
  --stop            Don't track, just stop
  --dry-run         Print commands instead of sending to rotctl
  --sun             Track sun, not ISS
  --moon            Track moon, not ISS
  --g5500pi         Control g5500pi controller, not rotctl
  --g55pi_ip STR    g5500pi control address (default: 192.168.2.140:8008)
  --offs-az INT     Azimuth offset in degrees
  --offs-el INT     Elevation offset in degrees
  --flip            Flip mode for passes North of zenith with North stop rotator
  --half-flip       Half-flip mode for passes through Zenith
"""

import argparse
import ephem
import math
import os
import subprocess
import sys
import time
import logging
import requests
from datetime import datetime, timezone, timedelta

# Configuration
TLE_URL     = "https://live.ariss.org/iss.txt"
TLE_MAX_AGE = 86400          # seconds (24 h)
POLL_HZ     = 0.5              # position updates per second
LOG_FMT     = "%(asctime)s %(levelname)s %(message)s"

# Argument parsing
def parse_args():
    p = argparse.ArgumentParser(description="ISS -> rotctl tracker")
    p.add_argument("--lat",       type=float, default=50.7787,        help="Observer latitude  (decimal deg)")
    p.add_argument("--lon",       type=float, default=-2.021,         help="Observer longitude (decimal deg)")
    p.add_argument("--alt",       type=float, default=0.0,            help="Observer altitude  (metres)")
    p.add_argument("--device",    type=str,   default="/dev/ttyUSB2", help="default id /dev/ttyUSB2")
    p.add_argument("--model",     type=int,   default=603,            help="Hamlib model (603 use for LVB)")
    p.add_argument("--baud",      type=int,   default=9600,           help="default=9600")
    p.add_argument("--min-el",    type=float, default=5.0,            help="Min elevation to begin tracking")
    p.add_argument("--tmp-path",  type=str,   default="/home/pi/tmp/")
    p.add_argument("--park-az",   type=float, default=180.0,          help="Park azimuth  after pass (deg, default 0 = North)")
    p.add_argument("--park-el",   type=float, default=90.0,           help="Park elevation after pass (deg, default 0 = Horizon)")
    p.add_argument("--no-park",   action="store_true",                help="Disable auto-park; just stop at LOS")
    p.add_argument("--park-now",  action="store_true",                help="Stop tracking, go to park")
    p.add_argument("--stop",      action="store_true",                help="Stop tracking, just stop")
    p.add_argument("--dry-run",   action="store_true",                help="Print commands, don't send")
    p.add_argument("--verbose",   action="store_true")
    p.add_argument("--sun",       action="store_true",                help="Track sun, not ISS")
    p.add_argument("--moon",      action="store_true",                help="Track moon, not ISS")
    p.add_argument("--g5500pi",   action="store_true",                help="Control g5500pi")
    p.add_argument("--g55pi_ip",  type=str,   default="192.168.2.140:8008")
    p.add_argument("--offs-az",   type=int, default=0,                help="Azimuth offset in degrees")
    p.add_argument("--offs-el",   type=int, default=0,                help="Elevation offset in degrees")
    p.add_argument("--flip",      action="store_true",                help="For passes North of zenith")
    p.add_argument("--half-flip", action="store_true",                help="For passes through zenith")

    return p.parse_args()

# TLE fetching update once a day
def fetch_tle(cache_path, max_age=TLE_MAX_AGE):
    """Return (name, line1, line2). Download only if cache is stale."""
    try:
        age   = time.time() - os.path.getmtime(cache_path)
        fresh = age < max_age
    except OSError:
        fresh = False

    if not fresh:
        logging.info("Fetching TLE from %s", TLE_URL)
        try:
            r = requests.get(TLE_URL, timeout=15)
            r.raise_for_status()
            with open(cache_path, "w") as f:
                f.write(r.text)
            logging.info("TLE cached to %s", cache_path)
        except Exception as e:
            if os.path.exists(cache_path):
                logging.warning("TLE fetch failed (%s); using cached copy", e)
            else:
                logging.error("TLE fetch failed and no cache: %s", e)
                sys.exit(1)

    with open(cache_path) as f:
        lines = [l.strip() for l in f if l.strip()]

    for i, line in enumerate(lines):
        if line.startswith("1 25544"):    # NORAD ID for ISS
            return lines[i - 1], lines[i], lines[i + 1]
    return lines[0], lines[1], lines[2]

# Observer setup
def make_observer(lat, lon, alt):
    obs          = ephem.Observer()
    obs.lat      = str(lat)
    obs.lon      = str(lon)
    obs.elev     = alt
    obs.pressure = 0     # disable atmospheric refraction for accuracy
    return obs

# rotctl process (stdin pipe mode)
def start_rotctl(model, device, baud, dry_run):
    if dry_run:
        return None
    cmd = ["rotctl", "-m", str(model), "-r", device, "-s", str(baud), "-"]
    logging.info("Starting rotctl: %s", " ".join(cmd))
    print(cmd)
    proc = subprocess.Popen(
        cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        text=True,
        bufsize=1,
    )
    return proc

# Send position using rotctl
def send_position(proc, az, el, dry_run):
    cmd = f"P {az:.1f} {el:.1f}\n"
    if dry_run:
        print(cmd, end="", flush=True)
    else:
        try:
            proc.stdin.write(cmd)
            proc.stdin.flush()
        except BrokenPipeError:
            logging.error("rotctl process died unexpectedly")
            sys.exit(1)

# Send position using g5500pi
def send_g5500pi(g55pi_ip, az, el, dry_run):
    cmd = "curl \"http://"+g55pi_ip+f"/set_pos?az={az:.1f}&el={el:.1f}\""
    if dry_run:
        print(cmd)
    else:
        subprocess.run(cmd, shell=True)

# Query G5500 rotator position
def check_g5500pi(g55pi_ip):
    query_pos = "curl \"http://"+g55pi_ip+"/get_pos\""
    print(query_pos)
    position = subprocess.run(query_pos, shell=True, stdout=subprocess.PIPE).stdout.decode('utf-8')
    return position.rstrip()

# Stop rotator using rotctl     
def stop_rotctl(proc):
    if proc is None:
        return
    try:
        proc.stdin.write("S\n")
        proc.stdin.flush()
        proc.stdin.close()
        proc.wait(timeout=5)
    except Exception:
        proc.kill()

# Park rotator using rotctl
def park_rotator(proc, az, el, dry_run):
    logging.info("Parking antenna at AZ=%.1f EL=%.1f", az, el)
    cmd = f"P {az:.1f} {el:.1f}\n"
    if dry_run:
        print(cmd, end="", flush=True)
    else:
        try:
            proc.stdin.write(cmd)
            proc.stdin.flush()
        except BrokenPipeError:
            logging.error("rotctl process died during park")

# Initialise and then run tracking loop every second
def main():

    # Read the arguments and set up logging
    args = parse_args()
    logging.basicConfig(
        level   = logging.DEBUG if args.verbose else logging.INFO,
        format  = LOG_FMT,
        stream  = sys.stdout,
    )

    # Set station position
    obs      = make_observer(args.lat, args.lon, args.alt)
    tle_time = 0
    iss      = None
    above    = False

    # Initialise half-flip state variables
    ascending = True
    pre_zenith = False
    post_zenith = False
    descending = False

    # Start the Rotator Controller
    if not args.g5500pi:
        proc = start_rotctl(args.model, args.device, args.baud, args.dry_run)
    logging.info(
        "Observer: lat=%.4f lon=%.4f alt=%.0fm  min-el=%.1f deg",
        args.lat, args.lon, args.alt, args.min_el,
    )

    # Immediate stop if required
    if args.stop:
        logging.info("Commanded to stop.  Not tracking.")
        if args.g5500pi:
            cmd = "curl \"http://"+args.g55pi_ip+"/stop\""
            if args.dry_run:
                print(cmd)
            else:
                subprocess.run(cmd, shell=True)
        else:
            stop_rotctl(proc)

    # Immediate park if required, otherwise set position between passes
    if args.park_now:
        logging.info("Commanded to park.  Not tracking.")
        if args.g5500pi:
            send_g5500pi(args.g55pi_ip, args.park_az, args.park_el, args.dry_run)
        else:
            send_position(proc, args.park_az, args.park_el, args.dry_run)
        # Write desired position to file for display
        file_write = "echo \"Demanded azimuth "+f"{args.park_az:.1f} elevation {args.park_el:.1f}\" > /home/pi/tmp/demand.txt"
        subprocess.run(file_write, shell=True)

    elif (not args.stop):
        if args.no_park:
            logging.info("Auto-park disabled; rotator will stop at LOS.")
        else:
            logging.info("Park position: AZ=%.1f  EL=%.1f", args.park_az, args.park_el)

    if (not args.park_now) and (not args.stop):
        logging.info("Tracking started. Press Ctrl-C to stop.")

    # Calculate where to point
    try:
        while (not args.park_now) and (not args.stop):
            now = time.time()
 
            obs.date = datetime.now(timezone.utc).strftime("%Y/%m/%d %H:%M:%S.%f")
            
            if args.sun:
                sun = ephem.Sun()
                sun.compute(obs)
                sunaz=math.degrees(sun.az)
                sunalt=math.degrees(sun.alt)
                #print(sunaz,sunalt)
                el=sunalt
                az=sunaz
                rng=10000

            elif args.moon:
                moon = ephem.Moon()
                moon.compute(obs)
                moonaz=math.degrees(moon.az)
                moonalt=math.degrees(moon.alt)
                #print(moonaz,moonalt)
                el=moonalt
                az=moonaz
                rng=10000

            else:
                # Refresh ISS TLE once per day
                if now - tle_time >= TLE_MAX_AGE:
                    tle_file = args.tmp_path + "iss.tle"
                    name, l1, l2 = fetch_tle(tle_file)
                    iss          = ephem.readtle(name, l1, l2)
                    tle_time     = now
                    logging.info("Loaded TLE: %s", name)
                iss.compute(obs)
                az  = math.degrees(iss.az)     # 0-360 N=0 clockwise
                el  = math.degrees(iss.alt)    # -90 to +90
                rng = iss.range / 1000         # km

            logging.info("Calculated Position az=%.1f el=%.1f", az, el)

            # Write desired position to file for display
            file_write = "echo \"Demanded azimuth "+f"{az:.1f} elevation {el:.1f}\" > /home/pi/tmp/demand.txt"
            subprocess.run(file_write, shell=True)

            # Apply offsets here
            az = az + args.offs_az
            el = el + args.offs_el

            logging.info("After Offsets Pos   az=%.1f el=%.1f", az, el)

            # Apply flip modifications here
            if args.flip:
                if az <= 180.0:
                    az=az+180.0
                else:
                    az=az-180.0
                el=180.0-el

            if args.half_flip:
                # States below happen in sequence during a half-flip pass
                # Two states are true in sequence during transitions
                if ascending:
                    if el > 85.0:
                        # Pre-zenith, so freeze azimuth, but track in elev
                        zenith_az=az
                        ascending = False
                        pre_zenith = True
                if pre_zenith:
                    if az < 180.0:
                        # Post-zenith so flip elevation with frozen azimuth
                        pre_zenith = False
                        post_zenith = True
                    else:
                        # Pre-zenith, so freeze azimuth, but track in elev
                        az=zenith_az
                if post_zenith:
                    if el < 85.0:
                        # Descending, so flip elevation and azimuth
                        post_zenith = False
                        descending = True
                    else:
                        # Post-zenith, so freeze azimuth, but flip in elev
                        az=zenith_az
                        el=180.0-el
                if descending:
                    # Descending, so flip elevation and azimuth
                    if az <= 180.0:
                        az=az+180.0
                    else:
                        az=az-180.0
                    el=180.0-el

            logging.info("After Flip Pos      az=%.1f el=%.1f", az, el)

            # Check above horizon
            if el >= args.min_el and el <= (180 - args.min_el):
                # Check for AOS
                if not above:
                    logging.info("*** ISS AOS ***")
                    above = True
                    # Initialise half-flip state variables
                    ascending = True
                    pre_zenith = False
                    post_zenth = False
                    descending = False
                # And point towards it
                if not args.g5500pi:
                    send_position(proc, az, el, args.dry_run)
                else:
                    send_g5500pi(args.g55pi_ip, az, el, args.dry_run)
                logging.info("AZ=%6.1f  EL=%5.1f  Range=%s km", az, el, f"{rng:,.0f}")
            else:
                # Below Horizon
                half_flip_active = False
                if above:
                    logging.info("*** ISS LOS el=%.1f deg ***", el)
                    above = False
                    if args.no_park:
                        if not args.dry_run and proc:
                            # Stop rotator
                            if not args.g5500pi:
                                proc.stdin.write("S\n")
                                proc.stdin.flush()
                            else:
                                cmd = "curl \"http://"+g55pi_ip+"/stop\""
                                print(cmd)
                                subprocess.run(cmd, shell=True)

                    else:
                        if not args.g5500pi:
                            park_rotator(proc, args.park_az, args.park_el, args.dry_run)
                        else:
                            send_g5500pi(args.g55pi_ip, args.park_az, args.park_el, args.dry_run)
                else:
                    logging.debug("ISS/Sun/Moon is underground: el=%.1f deg", el)

            # Report actual position to ramdisk file
            if args.g5500pi:
                position = check_g5500pi(args.g55pi_ip)
                file2_write = "echo \"Actual Position " + position + "\" > /home/pi/tmp/actual.txt"
                subprocess.run(file2_write, shell=True)

            else:
                file2_write = "something else"

            # Refresh after 1 second
            time.sleep(1.0 / POLL_HZ)

    # Exit on keyboard interrupt
    except KeyboardInterrupt:
        logging.info("Interrupt  -park and stop LVB")

    finally:
        if ((not args.no_park) or (not args.stop)) and not args.park_now:
            if not args.g5500pi:
                park_rotator(proc, args.park_az, args.park_el, args.dry_run)
                stop_rotctl(proc)
            else:
                #send_g5500pi(args.g55pi_ip, args.park_az, args.park_el, args.dry_run)
                cmd = "curl \"http://"+args.g55pi_ip+"/stop\""
                print(cmd)
                #subprocess.run(cmd, shell=True)

if __name__ == "__main__":
    main()

