#!/bin/sh

tarball=`realpath "$1"`
nb_tests="$2"
oldvectors=`realpath "$3"`
newvectors=`realpath "$4"`
base=`basename "$tarball" .tar.gz`

tar xvf "$tarball" > /dev/null 2>&1
cd "$base"

if [ $? -ne 0 ]
then
        echo cannot go to "$base"
        exit 1
fi

mkdir build_tests

configure_dir=`pwd`
seq -w "$nb_tests" | parallel --halt now,fail=10 -j +2 -q ../random_config.sh "build_tests/run_{}" "$configure_dir" "$oldvectors" "$newvectors"

if [ $? -ne 0 ]
then
        echo Check found errors
        exit 1
else
        echo No error found
fi
