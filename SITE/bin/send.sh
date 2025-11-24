SITE_NAME=pqtr.ai
if [ $# -eq 1 ]; then
	    SITE_NAME=$1
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

ssh -T $SUDO_USER@$SITE_NAME "bash -s" <<END_SSH
sudo rm -fr $OPEN_AREA
mkdir -p $OPEN_AREA
END_SSH
echo "tidy done"

scp $HERE/tmp/$WORK_NAME.tar.gz $SUDO_USER@$SITE_NAME:$OPEN_AREA
echo "copy done"

ssh -T $SUDO_USER@$SITE_NAME "bash -s" <<END_SSH
sudo tar xvf $OPEN_AREA/$WORK_NAME.tar.gz -C $OPEN_AREA
sudo cp -r $OPEN_AREA/etc/* /etc/nginx/sites-enabled/
sudo mkdir -p /var/www/$SITE_NAME
sudo cp -r $OPEN_AREA/www/* /var/www/$SITE_NAME
sudo systemctl restart nginx.service
END_SSH
echo "host send done"

