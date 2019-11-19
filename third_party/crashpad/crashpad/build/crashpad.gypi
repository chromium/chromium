# Copyright 2015 The Crashpad Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

{
  'variables': {
    # When building as a part of Chromium, this variable sets up the build to
    # treat Crashpad as Chromium code. This enables warnings at an appropriate
    # level and applies Chromium’s build/filename_rules.gypi. In a standalone
    # build, this variable has no effect.
    'chromium_code': 1,
  },
  'target_defaults': {
    'msvs_disabled_warnings': [
      4201,  # nonstandard extension used : nameless struct/union.
      4324,  # structure was padded due to __declspec(align()).
    ],
    'conditions': [
      ['OS=="linux" or OS=="android"', {
        'conditions': [
          ['clang==0', {
            'cflags': [
              '-Wno-multichar',
            ],
          }],
        ],
      }],
      ['OS=="android"', {
        'ldflags': [
          '-static-libstdc++',
        ],
      }],
    ],
  },
}
