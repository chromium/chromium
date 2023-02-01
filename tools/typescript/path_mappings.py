# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def GetDepToPathMappings(root_gen_dir):
  # TODO(crbug.com/1402829): Replace the following entry, with dedicated entries
  # for each shared ts_library() target.
  return {
      '//ui/webui/resources:library': [(
          f'//resources/*',
          f'{root_gen_dir}/ui/webui/resources/tsc/*',
      )]
  }
