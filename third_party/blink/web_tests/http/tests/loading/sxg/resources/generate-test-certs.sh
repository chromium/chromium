#!/bin/sh

# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

topdir=$(git rev-parse --show-toplevel)
cd $topdir/third_party/blink/tools/blinkpy/third_party/wpt/certs

openssl ecparam -out 127.0.0.1.sxg.key -name prime256v1 -genkey;
openssl req -new -sha256 -key 127.0.0.1.sxg.key -out 127.0.0.1.sxg.csr \
  --subj '/CN=127.0.0.1/O=Test/C=US' \
  -config 127.0.0.1.sxg.cnf;
openssl x509 -req -days 3650 \
  -in 127.0.0.1.sxg.csr -extfile 127.0.0.1.sxg.ext \
  -CA cacert.pem -CAkey cakey.pem -passin pass:web-platform-tests \
  -set_serial 3 -out 127.0.0.1.sxg.pem
