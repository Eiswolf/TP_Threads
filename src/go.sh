#!/bin/bash

cd ../build
make clean
make
make test
echo "Bonjour, on passe à make check\n"
make check
#./ensitsp 10 32 4 -s
cd ../src