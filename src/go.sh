#!/bin/bash

cd ../build
make clean
make
#make test
#echo "Bonjour, on passe à make check\n"
make check
#rm image.svg & ./ensitsp 30 32 4 -s >> image.svg & eog image.svg
cd ../src