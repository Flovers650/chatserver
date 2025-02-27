#! /bin/bash

set -x

cd `pwd`/build/

rm -rf *

cmake .. && make
