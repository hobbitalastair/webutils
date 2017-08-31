#!/usr/bin/env sh
#
# Add a new RSS feed corresponding to the given artist's name.
#
# Author:   Alastair Hughes
# Contact:  hobbitalastair at yandex dot com

set -e

if [ "$#" -ne 1 ]; then
    printf "usage: %s <name>\n" "$0" 1>&2
    exit 1
fi

[ -z "${FEED_DIR}" ] && FEED_DIR="${XDG_CONFIG_DIR:-${HOME}/.config}/feeds/"
if [ ! -d "${FEED_DIR}" ]; then
    printf "%s: feed dir '%s' does not exist\n" "$0" "${FEED_DIR}" 1>&2
    exit 1
fi

printf "%s: adding feed for '%s'\n" "$0" "$1"

mkdir "${FEED_DIR}/$1/"

cat > "${FEED_DIR}/$1/fetch" << EOF
#!/usr/bin/env sh
curl -L -o - 'https://backend.deviantart.com/rss.xml?q=gallery%3A$1&type=deviation' | rss2atom
EOF
chmod +x "${FEED_DIR}/$1/fetch"

ln -s "../deviant-open.sh" "${FEED_DIR}/$1/open"
