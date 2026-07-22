#!/bin/sh
# Purge GtkBuilder UI files: strip XML comments and collapse whitespace
# between tags. Mirrors the .glade.ui rule from the old autotools build.
set -e
sed 's/<!--.*-->//' < "$1" | sed ':a;N;$!ba;s/ *\n *</</g' > "$2"
