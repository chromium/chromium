#!/bin/bash
# Do something that somewhat resembles a roll of closure-library.

die() {
 echo $* >&2
 exit 1
}

prompt() {
  echo $*
  echo press enter to continue, or interrupt.
  read ignored
}

CHROMIUM=`pwd`
prompt using $CHROMIUM as closure-library.

# TODO: remove $CHROMIUM/everything except our own files

CLOSURE=/tmp/closure
mkdir $CLOSURE || die cannot make $CLOSURE does it already exist?

cd $CLOSURE || die cannot cd to $CLOSURE after making it.  that is really unexpected.  good luck.
git clone "https://github.com/google/closure-library.git" || die cannot clone upstream repo
cd closure-library || die cannot cd to closure-library after cloning it.  did they rename it?

CLOSURE_VERSION=`cat package.json  |grep version |head -1 |sed 's/^.*": "//' |sed 's/".*//'`
prompt version is ${CLOSURE_VERSION}.  hopefully this looks sane to you.

SHA1=`git log --format=%H -1`
prompt sha1 of head is ${SHA1}.  hopefully this looks sane to you.

tar cvf - . |(cd $CHROMIUM && tar xvf - ) || die cannot tar.  or untar.  or maybe cd.  im a script, not a door.

cd $CHROMIUM || die cannot cd to $CHROMIUM after copying new library
# I was going to do this automatically, but ran out of time.
die Please update readme file.

prompt about to add and upload.  press enter if okay, or interrupt if not.
# This will add new files too.
git add --all .
git commit -m "Rolled closure-libary"
git cl upload
git cl try

echo Congratulations.  Now land it.
echo if the trybots fail with unknon deps, then add them to:
echo //third_party/protobuf/BUILD.gn
exit 0
