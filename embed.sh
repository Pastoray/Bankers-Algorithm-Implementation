#!/bin/sh
OUTPUT="$1"
HTML="src/main.html"

if [ ! -f "$HTML" ]; then
    echo "Error: $HTML not found!" >&2
    exit 1
fi

printf "static const char* s_html_page =\n" > "$OUTPUT"
sed 's/"/\\"/g; s/.*/  "&\\n"/' "$HTML" >> "$OUTPUT"
printf ";\n" >> "$OUTPUT"