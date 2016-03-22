FROM=$(realpath .)
TO=$(realpath $OSCA)
echo "from: $FROM"
echo "  to: $TO"
rsync --delete -vrt --exclude "*/.*" --exclude "*/.*/" --exclude=Debug $FROM $TO

