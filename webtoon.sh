#!/usr/bin/sh
#
# Basic webtoon page viewer (wraps webtoon-scrape and webtoon-viewer).
#
# Author:   Alastair Hughes
# Contact:  hobbitalastair at yandex dot com

if [ "$#" -ne 1 ]; then
    printf "usage: %s <url>\n" "$0" 1>&2
    exit 1
fi

dir="$(mktemp --tmpdir -d "webtoon-XXXXXXXX")"
if [ "$?" -ne 0 ]; then
    printf "%s: failed to create temporary dir\n" "$0" 1>&2
    exit 1
fi
trap "rm -rf '${dir}'" EXIT

"$(dirname "$0")/webtoon-scrape" "$1" "${dir}"
if [ "$?" -ne 0 ]; then
    printf "%s: download failed\n" "$0" 1>&2
    exit 1
fi

if [ ! -e "${dir}/000.jpg" ]; then
    printf "%s: no files downloaded\n" "$0" 1>&2
    exit 1
fi

"$(dirname "$0")/webtoon-viewer" "${dir}/"*.jpg
