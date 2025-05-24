#!/bin/bash
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

if [[ ! -d third_party/grpc ]]; then
  echo 'Please set the current working directory to chromium root first.'
  exit 1
fi

if [[ -d /tmp/grpc ]]; then
  rm -rf /tmp/grpc
fi
mkdir /tmp/grpc
version="$(grep -oP 'Revision: \K[0-9a-f]+' third_party/grpc/README.chromium)"
echo "Downloading grpc ${version}..."
curl -s -L "https://github.com/grpc/grpc/archive/${version}.tar.gz" | tar xz --strip=1 -C /tmp/grpc

echo 'Replacing third_party/grpc/source...'
rm -rf third_party/grpc/source
mkdir third_party/grpc/source
mv /tmp/grpc/src /tmp/grpc/include /tmp/grpc/third_party \
  third_party/grpc/source
find third_party/grpc/source \
  -type f ! \( -name '*.h' -o -name '*.c' -o -name '*.cc' -o -name '*.hpp' \
  -o -name '*.inc' \) -delete
# Keep the top-level LICENSE because it's used by README.chromium.
mv /tmp/grpc/LICENSE third_party/grpc/source/

# No quotes here on purpose.
for patch in $(ls third_party/grpc/patches/) ; do
  echo "Applying ${patch}..."
  git apply "third_party/grpc/patches/${patch}"
done

echo 'Done'
