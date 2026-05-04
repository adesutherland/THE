#!/usr/bin/env sh
pdir=$1
cdir=`pwd`
abssrc=`realpath $2`
ver=$3
dist=$4

shift
shift
shift
shift

apks=$*

rem=`echo $cdir | sed -e s#$abssrc##`
base=`echo $rem | cut -f2 -d/`
machine=`cat /etc/apk/arch`
srcdir=$pdir/$base
srcdir=$pdir/$base/$machine

for a in $apks; do
  fname=`ls $srcdir/$a-$ver-*.apk`
  bname=`basename $fname .apk`
  dname=`dirname $fname`
  newname=$pdir/$bname-$machine-$dist.apk
  /bin/cp $fname $newname
  done;
ls -l $pdir/*.apk
