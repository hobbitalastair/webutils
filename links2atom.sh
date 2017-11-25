#!/usr/bin/sh
#
# Create an atom feed from a list of links.
#
# Author:   Alastair Hughes
# Contact:  hobbitalastair at yandex dot com

title="$1"
id="$2"
date="$(date "+%FT%TZ")"

printf '<feed xmlns="http://www.w3.org/2005/Atom">\n'
printf '    <title>%s</title>\n' "${title}"
printf '    <id>%s</id>\n' "${id}"
printf '    <link href="%s"></link>\n' "${id}"
printf '    <updated>%s</updated>\n' "${date}"

while IFS='' read link; do
    printf '    <entry>\n'
    printf '        <title>%s</title>\n' "${link}"
    printf '        <content></content>\n'
    printf '        <id>%s</id>\n' "${link}"
    printf '        <link href="%s"></link>\n' "${link}"
    printf '        <updated>%s</updated>\n' "${date}"
    printf '    </entry>\n'
done

printf '</feed>\n'
