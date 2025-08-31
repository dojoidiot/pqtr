if [ $# -ne 3 ]; then
    echo "args: <zone-skey> <role-name> <user-pkey>"
    exit 1
fi
ZONE_SKEY=$1
ROLE_NAME=$2
USER_PKEY=$3
HERE=$(
    cd "$(dirname "$0")"
    pwd -P
)
ssh-keygen -s $ZONE_SKEY -n $ROLE_NAME -I $ROLE_NAME-role $USER_PKEY
