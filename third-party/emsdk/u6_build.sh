#!/bin/bash
set -e
cd "$(dirname "$0")"

./emsdk update
./emsdk install sdk-fastcomp-1.38.30-64bit
./emsdk activate sdk-fastcomp-1.38.30-64bit
