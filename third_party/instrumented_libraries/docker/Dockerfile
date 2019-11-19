# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

FROM ubuntu:trusty

ENV DEBIAN_FRONTEND noninteractive

# Don't gzip indexes. `apt-cache dumpavail` (used by install-build-deps.sh)
# does not understand gzipped indexes.
RUN echo "ACQUIRE { GzipIndexes \"false\"; }" > /etc/apt/apt.conf.d/no-gzip.conf

RUN apt-get update && apt-get upgrade -y && \
    apt-get install -y curl build-essential python

RUN echo ttf-mscorefonts-installer msttcorefonts/accepted-mscorefonts-eula select true | debconf-set-selections && \
    curl 'https://chromium.googlesource.com/chromium/src/+/master/build/install-build-deps.sh?format=TEXT' | \
    base64 -d | bash -s -- --no-prompt --no-chromeos-fonts --syms

COPY run.sh /bin
RUN chmod +x /bin/run.sh

ENTRYPOINT ["run.sh"]
