#!/usr/bin/sh
#
# Image loader utility for use with webtoon-viewer.
# This takes a filename as an argument and prints the result, converted to
# the farbfeld file format, to stdout.
#
# Author:   Alastair Hughes
# Contact:  hobbitalastair at yandex dot com

if [ "$#" -ne 1 ]; then
    printf "usage: %s <image>\n", "$0"
    exit 1
fi

exec 2ff < "$1"
