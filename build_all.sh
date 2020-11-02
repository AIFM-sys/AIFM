#!/bin/bash

# Shenango
cd shenango
./build.sh || { echo 'Failed to build Shenango.'; exit 1; }
cd ..

# AIFM
cd aifm
./build.sh || { echo 'Failed to build AIFM.'; exit 1; }
cd ..
