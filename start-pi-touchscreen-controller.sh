#!/usr/bin/env bash
# Original by Jon Eskdale edited and modified by Olaf Sonderegger 2021-01-01
# Run program to start slide (picture slideshow), dim or turn off RPi backlight after a period of time,
# turn on and stop slide when the touchscreen is touched.
# Best to run this script from /etc/rc.local to start at boot.

#EDIT THIS VALUE to set the period before slide will start
slide_timeout=1 # minute

#EDIT THIS VALUE to set the path, where slide find pictures
slide_pictures=/mnt/nas_photoframe

#EDIT THIS VALUE to set the period before it will dim
dimmer_timeout=30 # minutes

#EDIT THIS VALUE to set the brightness it dims the display to.
#50-253, minimum value is 50, blanking the screen will be done after blank timeout
min_brightness=100

#EDIT THIS VALUE to set the period before it will blank the screen
blank_period="23:00-07:00"

# Find the device the touchscreen uses.  This can change depending on
# other input devices (keyboard, mouse) are connected at boot time.
# for line in $(lsinput); do
#        if [[ $line == *"/dev/input"* ]]; then
#                word=$(echo $line | tr "/" "\n")
#                for dev in $word; do
#                        if [[ $dev == "event"* ]]; then
#                                break
#                       fi
#              done
#        fi
#        if [[ $line == *"FT5406"* ]] ; then
#               break
#        fi
#done

pi-touchscreen-controller $slide_timeout $slide_pictures $dimmer_timeout $min_brightness $blank_period event0
