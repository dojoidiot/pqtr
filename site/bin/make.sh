set -e
HERE_=$(
    cd "$(dirname "$0")"
    pwd -P
)
HERE=$HERE_/..
MAKE_AREA=$HERE/target
PACK_AREA=$MAKE_AREA/pack
WORK_NAME=pqtr

rm -fr $PACK_AREA
mkdir -p $PACK_AREA
cp -r $HERE/etc/ $PACK_AREA
cp -r $HERE/www/ $PACK_AREA
cd $PACK_AREA
tar -cvzf $MAKE_AREA/$WORK_NAME.tar.gz .