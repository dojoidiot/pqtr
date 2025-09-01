if [ $# -ne 2 ]; then
    echo "args: <user> <zone-name>"
    exit 1
fi
USER=$1
ZONE=$2
HERE=$(
    cd "$(dirname "$0")"
    pwd -P
)
ssh-keygen -q -t ed25519 -f $HERE/../etc/$USER-$ZONE.skey -C "$ZONE zone"
