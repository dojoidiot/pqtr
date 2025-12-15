#!/bin/bash
# base.sh - Run BASE server (production)
# Usage: ./bin/base.sh exec|stop|status|logs [-f]
set -e

HERE=$(cd "$(dirname "$0")/.." && pwd -P)
NAME=base

EXEC_FILE="$HERE/bin/$NAME"
INFO_FILE="$HERE/etc/$NAME.json"
DATA_AREA="$HERE/var"
WASM_ROOT="$HERE/www"
PROC_AREA="$HERE/run"
PROC_FILE="$PROC_AREA/$NAME.pid"
LOGS_FILE="$PROC_AREA/$NAME.log"

mkdir -p "$DATA_AREA/BASE" "$DATA_AREA/LABS" "$PROC_AREA"

is_running() {
    [ -e "$PROC_FILE" ] && [ -e "/proc/$(cat "$PROC_FILE")" ]
}

case "${1:-}" in
    exec)
        if ! [ -x "$EXEC_FILE" ]; then
            echo "[fail] executable not found: $EXEC_FILE"
            exit 1
        fi
        if ! [ -f "$INFO_FILE" ]; then
            echo "[fail] config not found: $INFO_FILE"
            exit 1
        fi
        if is_running; then
            kill -15 "$(cat "$PROC_FILE")"
            sleep 1
        fi
        rm -f "$PROC_FILE"
        nohup "$EXEC_FILE" --info-file "$INFO_FILE" --data-area "$DATA_AREA" --wasm-root "$WASM_ROOT" >"$LOGS_FILE" 2>&1 &
        echo $! >"$PROC_FILE"
        echo "[info] started $NAME (pid $(cat "$PROC_FILE"))"
        ;;
    stop)
        if ! is_running; then
            echo "[info] $NAME not running"
            rm -f "$PROC_FILE"
            exit 0
        fi
        kill -15 "$(cat "$PROC_FILE")"
        rm -f "$PROC_FILE"
        echo "[info] stopped $NAME"
        ;;
    info)
        if is_running; then
            echo "[info] $NAME running (pid $(cat "$PROC_FILE"))"
        else
            echo "[info] $NAME not running"
            rm -f "$PROC_FILE"
        fi
        ;;
    logs)
        if ! [ -f "$LOGS_FILE" ]; then
            echo "[fail] no log file"
            exit 1
        fi
        if [ "${2:-}" = "-f" ]; then
            tail -f "$LOGS_FILE"
        else
            tail -n 100 "$LOGS_FILE"
        fi
        ;;
    *)
        echo "usage: $0 exec|stop|info|logs [-f]"
        exit 1
        ;;
esac
