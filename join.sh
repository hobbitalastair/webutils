#!/usr/bin/env sh
# This uses imagemagick's convert to join the images in the given directory
# together. It assumes that the images are numerically ordered.

pushd "$1" > /dev/null
convert $(echo * | tr ' ' '\n' | rev | cut -d/ -f1 | rev | sort -n) -append \
    "joined.jpg"
