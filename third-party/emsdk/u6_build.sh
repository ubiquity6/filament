#!/bin/bash
set -e
cd "$(dirname "$0")"

#./emsdk update
./emsdk install sdk-1.38.33-64bit
./emsdk activate sdk-1.38.33-64bit
