#!/bin/bash -eu
# Copyright 2019 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################################

# This script is meant to be run by
# https://github.com/google/oss-fuzz/blob/master/projects/libzip/Dockerfile


mkdir build
cd build
cmake -DBUILD_SHARED_LIBS=OFF -DENABLE_GNUTLS=OFF -DENABLE_OPENSSL=ON -DBUILD_TOOLS=OFF -DHAVE_CRYPTO=ON ..
make -j$(nproc)

for fuzzer in $(make list-fuzzers | sed -n 's/^FUZZERS: //p')
do
  $CXX $CFLAGS -I. -I../lib \
    $SRC/libzip/ossfuzz/$fuzzer.c \
      -o $OUT/$fuzzer \
      $LIB_FUZZING_ENGINE $SRC/libzip/build/lib/libzip.a -lbz2 -llzma -lz -lzstd -v -lssl -lcrypto
done

find $SRC/libzip/regress -name "*zip" | \
     xargs zip $OUT/zip_read_fuzzer_seed_corpus.zip
cp $SRC/libzip/ossfuzz/zip_read_fuzzer.dict $OUT/

cp $SRC/libzip/ossfuzz/zip_write_encrypt_aes256_file_fuzzer_seed_corpus.zip $OUT/

for pair in \
  zip_read_fuzzer:zip_read_file_fuzzer \
  zip_write_encrypt_aes256_file_fuzzer:zip_write_encrypt_pkware_file_fuzzer
do
  source=${pair%%:*}
  target=${pair##*:}
  for file in .dict _seed_corpus.zip
  do
    if [ -f $OUT/$source$file ]
    then
      cp $OUT/$source$file $OUT/$target$file
    fi
  done
done


