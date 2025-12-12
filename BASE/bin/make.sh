#!/bin/bash
set -e

WORK_NAME=jwta
HERE=$(cd "$(dirname "$0")" && pwd -P)
ROOT=$HERE/..
TEMP_AREA=$ROOT/tmp
MAKE_AREA=$TEMP_AREA/make
SEND_AREA=$TEMP_AREA/send
SEND_FILE=$TEMP_AREA/$WORK_NAME.tar.gz

rm -rf $TEMP_AREA

cd $ROOT
make

mkdir -p $SEND_AREA

# Explicit copy of deployable files
mkdir -p $SEND_AREA/etc
cp  etc/$WORK_NAME.json $SEND_AREA/etc
mkdir -p $SEND_AREA/bin
cp -r bin/$WORK_NAME.sh $SEND_AREA
cp $MAKE_AREA/$WORK_NAME $SEND_AREA/bin

cd $SEND_AREA
tar -cvzf $SEND_FILE .