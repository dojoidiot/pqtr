HOST_NAME=pqtr.ai
if [ $# -eq 1 ]; then
	    HOST_NAME=$1
fi

set -e
HERE=$(
    cd "$(dirname "$0")"
        pwd -P
)
ROOT=$HERE/..
NAME=jwta
HOST_NAME=pqtr.ai
SUDO_USER=env
TASK_USER=svc
HOST_USER=ops
OPEN_AREA=/home/$SUDO_USER/$NAME/
TASK_AREA=/home/$TASK_USER/$NAME/

ssh -T $SUDO_USER@$HOST_NAME "bash -s" <<END_SSH
sudo rm -fr $OPEN_AREA
mkdir -p $OPEN_AREA
END_SSH
echo "$NAME: tidy done"

scp $ROOT/tmp/$NAME.tar.gz $SUDO_USER@$HOST_NAME:$OPEN_AREA
echo "$NAME: copy done"

# Open the pack into svc account for running
ssh -T $SUDO_USER@$HOST_NAME "bash -s" <<END_SSH
sudo mkdir -p $TASK_AREA/var
sudo mkdir -p $TASK_AREA/run
sudo tar xvf $OPEN_AREA/$NAME.tar.gz -C $TASK_AREA
sudo chown -R $TASK_USER:$TASK_USER $TASK_AREA 

END_SSH
echo "host send done" 

