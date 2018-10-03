#!/bin/sh

OUTPUT="$1"

echo "#define LIBINSANE_VERSION \"$(git describe --always)\"" > ${OUTPUT}
