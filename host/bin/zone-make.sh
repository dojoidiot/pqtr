if [ $# -ne 1 ]; then
    echo "args: <zone-name>"
    exit 1
fi
ZONE=$1
HERE=$(
    cd "$(dirname "$0")"
    pwd -P
)
ssh-keygen -q -t ed25519 -f $HERE/../etc/ssh/$ZONE.skey -C "$ZONE zone"
