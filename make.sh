BIN=clonky
SRC=src/*.c
#OPTS=-Os
#WARNINGS="-Wall -Wextra"
LIBS="-lX11 -lXft"
INCLUDES=-I/usr/include/freetype2/

echo &&
gcc  -o $BIN  $SRC $INCLUDES $LIBS $OPTS $WARNINGS && 
echo    "             lines   words  chars" &&
echo -n "       wc:" &&
cat $SRC|wc
echo -n "wc zipped:" &&
cat $SRC|gzip|wc &&
echo && ls -o --color $BIN &&
echo
