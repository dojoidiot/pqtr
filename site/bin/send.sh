HOST=i2xp.com
if [ $# -eq 1 ]; then
    HOST=$1
fi

set -e
HERE_=$(
    cd "$(dirname "$0")"
    pwd -P
)
HERE=$HERE_/..

WORK_NAME=pqtr
SUDO_USER=env
HOST_USER=ops
OPEN_AREA=/home/$SUDO_USER/$WORK_NAME/

. $HERE_/make.sh 

ssh -T $SUDO_USER@$HOST "bash -s" <<END_SSH
sudo rm -fr $OPEN_AREA
mkdir -p $OPEN_AREA
END_SSH
echo "tidy done"

scp $HERE/target/$WORK_NAME.tar.gz $SUDO_USER@$HOST:$OPEN_AREA
echo "copy done"

ssh -T $SUDO_USER@$HOST "bash -s" <<END_SSH
#sudo mkdir -p /home/$HOST_USER/$OPEN_AREA
sudo tar xvf $OPEN_AREA/$WORK_NAME.tar.gz -C $OPEN_AREA
sudo cp -r $OPEN_AREA/etc/* /etc/nginx/sites-enabled/
sudo cp -r $OPEN_AREA/www/* /var/www
sudo systemctl restart nginx.service
END_SSH
echo "host send done"
