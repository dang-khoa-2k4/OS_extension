#!/bin/bash

make all

if [ ! -d out ]; then
    mkdir out
else 
    rm -f out/*

fi

SRC_FILE=$(ls input -p | grep -v / )
echo "RUN TC: $SRC_FILE"

for fsrc in $SRC_FILE
do 
  ./os $fsrc >"out/$fsrc.txt"
done
