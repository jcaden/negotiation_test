#!/bin/bash

PROGRAM=build/negotiation_test

if [ ! -e ${PROGRAM} ]
then
  ./build.sh
fi

${PROGRAM} --gst-debug=negotiation_test:4 $*
