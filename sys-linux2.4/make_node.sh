#!

N=0

while test -c /dev/ise$N
do
    N=`expr $N + 1`
done

test $N -ge 8 && exit

mknod --mode=0666 /dev/ise$N c 114 $N
mknod --mode=0666 /dev/isex$N c 114 `expr 128 + $N`
