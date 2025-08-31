if [ $# -ne 3 ]; then
    echo "args: <host-name> <site-name> <site-mail>"
    exit 1
fi
HOST_NAME=$1
SITE_NAME=$2
SITE_MAIL=$3
HERE=$(
    cd "$(dirname "$0")"
    pwd -P
)

ssh env@$HOST_NAME "bash -s" << END_INIT
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                
if [ -f "/etc/nginx/sites-enabled/$SITE_NAME" ] ; then
    echo "[warn] already done."
    exit 0
fi

sudo apt update
sudo apt upgrade
sudo apt -y install nginx certbot
sudo systemctl stop nginx
sudo ufw allow http
sudo ufw allow https
sudo certbot certonly --non-interactive --agree-tos --standalone --preferred-challenges http -d $SITE_NAME -m $SITE_MAIL
sudo rm /etc/nginx/sites-available/*
sudo rm /etc/nginx/sites-enabled/*
sudo openssl dhparam -out /etc/nginx/dhparam.pem 2048

                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           
echo "
    server {
            server_tokens off;
            listen  80;
            server_name $SITE_NAME; 
            return 302 https://$SITE_NAME;
    }

    # Panic server when upstream does not work.
    server {
            server_tokens off;
            listen  8080;
            server_name $SITE_NAME; 
            ssl off;
            root /var/www/easyvibes.app/;
            index index.html;
    }

    server {

            merge_slashes on;
            server_tokens off;
            listen  443;
            server_name $SITE_NAME;

            ssl on;
            ssl_certificate         /etc/letsencrypt/live/$SITE_NAME/fullchain.pem;
            ssl_certificate_key     /etc/letsencrypt/live/$SITE_NAME/privkey.pem;

            gzip on;

            location / {
                proxy_set_header Host \\\$host;
                proxy_set_header X-Real-IP \\\$remote;
                proxy_set_header Upgrade \\\$http_upgrade;
                proxy_next_upstream error timeout http_500;
                proxy_connect_timeout 5s;
                proxy_pass http://host;
                error_page 403 500 502 504 = @fail;
            }
    }
" | sudo tee -a /etc/nginx/sites-enabled/$SITE_NAME;

echo "
    upstream host {
        server localhost:4040;
    }
    upstream fail {
        server localhost:8080;
    }
" | sudo tee -a /etc/nginx/sites-enabled/$SITE_NAME.conf;

sudo mkdir -p /var/www/$SITE_NAME
sudo bash -c 'echo $SITE_NAME  > /var/www/$SITE_NAME/index.html'
sudo chown -R www-data:www-data /var/www/$SITE_NAME
sudo systemctl restart nginx

END_INIT