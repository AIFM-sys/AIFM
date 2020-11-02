#!/bin/sh

# Description:
#
# Runs the benchmark with a minimal problem size in a multiprocessor
# simulator.
#
# Usage:
#
# ./run.sh [NUMPROCS]
#

#Some default values
TARGET=radix

#Arguments
if [ -n "$1" ]
then
  NUMPROCS="$1"
fi

if [ -n "$2" ]
then
  INPUTSIZE="$2"
fi


if [ -n "$3" ]
then
  echo "Error: Too many arguments!"
  echo 
  head -n11 $0 | tail -n9 | sed 's/#//'
  exit 1
fi


#Determine program name, file names & arguments
case "${INPUTSIZE}" in 
"test"	) 
	PROGARGS="-p${NUMPROCS} -r4096 -n262144 -m524288";;
"simdev"	) 
	PROGARGS="-p${NUMPROCS} -r4096 -n262144 -m524288";;
"simsmall"	) 
	PROGARGS="-p${NUMPROCS} -r4096 -n4194304 -m2147483647";;
"simmedium"	) 
	PROGARGS="-p${NUMPROCS} -r4096 -n16777216 -m2147483647";;
"simlarge"	) 
	PROGARGS="-p${NUMPROCS} -r4096 -n67108864 -m2147483647";;
"native"	) 
	PROGARGS="-p${NUMPROCS} -r4096 -n268435456 -m2147483647";;
*)  
	echo "Input size error"
	exit 1;;
esac
 

PROG="${PARSECDIR}/ext/splash2x/kernels/${TARGET}/inst/${PARSECPLAT}/bin/${TARGET}"


#Some tests
if [ ! -x "$PROG" ]
then
  echo "Error: Binary ${PROG} does not exist!"
  exit 1
fi


#Execution
echo Generating input file ${INPUTFILE}...

RUN="$PROG $PROGARGS"

echo "Running $RUN:"
eval $RUN


