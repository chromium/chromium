# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
protoc --python_out . \
  ../../chrome/browser/media/media_engagement_preload.proto \
  --proto_path ../../chrome/browser/media
