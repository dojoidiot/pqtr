#!/bin/bash
# send.sh - Build and deploy to production
# Usage: ./send.sh [hostname]
set -e

HOST_NAME=${1:-pqtr.ai}
NAME=base
SUDO_USER=env
TASK_USER=svc
OPEN_AREA=/home/$SUDO_USER/$NAME
TASK_AREA=/home/$TASK_USER/$NAME

HERE=$(cd "$(dirname "$0")" && pwd -P)
PACK_NAME=$HERE/tmp/$NAME.tar.gz

echo "=== Building ==="
make

echo "=== Creating tarball ==="
tar czf "$PACK_NAME" -C tmp/pack .

echo "=== Deploying $NAME to $HOST_NAME ==="

# Clean staging area
ssh -T $SUDO_USER@$HOST_NAME "bash -s" <<END_SSH
sudo rm -rf $OPEN_AREA
mkdir -p $OPEN_AREA
END_SSH
echo "[info] staging cleaned"

# Copy tarball
scp "$PACK_NAME" $SUDO_USER@$HOST_NAME:$OPEN_AREA/
echo "[info] tarball copied"

# Extract to service account
ssh -T $SUDO_USER@$HOST_NAME "bash -s" <<END_SSH
sudo mkdir -p $TASK_AREA/var/BASE $TASK_AREA/var/LABS $TASK_AREA/run
sudo tar xzf $OPEN_AREA/$NAME.tar.gz -C $TASK_AREA
sudo chown -R $TASK_USER:$TASK_USER $TASK_AREA
END_SSH
echo "[info] extracted to $TASK_AREA"

echo "=== Deploy complete ==="
echo "On server: sudo -u $TASK_USER $TASK_AREA/bin/base.sh exec"
