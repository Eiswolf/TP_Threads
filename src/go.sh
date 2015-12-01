#!/bin/bash

cd ../build
make clean
make
make test
make check
./ensitsp 10 32 4 -s
