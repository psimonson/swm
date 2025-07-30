#!/bin/sh

set -e

make clean all

XEPHYR=$(command -v Xephyr) # Absolute path of Xephyr
xinit ./xinitrc -- \
	"$XEPHYR" \
	:1 \
	-ac \
	-seat :1 \
	-fullscreen \
	-screen 1920x1080
