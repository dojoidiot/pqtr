#!/bin/bash
if [ $# -ne 1 ]; then
    echo 'args: exec|logs|stop'
    exit 1
fi
TASK=$1
HERE=$(
    cd "$(dirname "$0")"
    pwd -P
)
NAME=jwta
ACCT=$(whoami)
EXEC_FILE=$HERE/bin/$NAME
INFO_FILE=$HERE/etc/$NAME.json
DATA_AREA=$HERE/var
PROC_AREA=$HERE/run
PROC_FILE=$PROC_AREA/$NAME.pid
LOGS_FILE=$PROC_AREA/$NAME.log

if [ ! -d $DATA_AREA ]; then
    mkdir -p $DATA_AREA
fi

if [ ! -d $PROC_AREA ]; then
    mkdir -p $PROC_AREA
fi

if [ "$TASK" = "exec" ]; then
    if [ -e $PROC_FILE ]; then
        if [ -e /proc/$(cat $PROC_FILE) ]; then
            kill -15 $(cat $PROC_FILE)
        fi
        rm $PROC_FILE
    fi
    nohup $EXEC_FILE --info-file $INFO_FILE --data-area $DATA_AREA >$LOGS_FILE 2>&1 &
    echo $! >$PROC_FILE
    echo "[info] $TASK: started $NAME"
    exit 0
elif [ "$TASK" = "logs" ]; then
    if [ ! -f $LOGS_FILE ]; then
        echo "[fail] $TASK: no $NAME log file"
        exit 0
    fi
    tail -n 100 $LOGS_FILE
    exit 0
elif [ "$TASK" = "stop" ]; then
    if [ ! -e $PROC_FILE ]; then
        echo "[fail] $TASK: no $NAME pid file"
        exit 0
    fi
    if [ ! -e /proc/$(cat $PROC_FILE) ]; then
        echo "[warn] $TASK: $NAME pid not there; tidying up"
        rm $PROC_FILE
        exit 0
    else
        kill -15 $(cat $PROC_FILE)
        echo "[info] $TASK: ended $NAME"
    fi
else
    echo "[fail] bad task: $TASK"
    exit 1
fi
