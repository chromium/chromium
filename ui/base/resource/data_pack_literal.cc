// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "ui/base/resource/data_pack_literal.h"
#include "ui/base/resource/resource_bundle.h"

namespace ui {

const char kSamplePakContentsV4[] = {
    0x04, 0x00, 0x00, 0x00,              // header(version
    0x04, 0x00, 0x00, 0x00,              //        no. entries
    0x01,                                //        encoding)
    0x01, 0x00, 0x27, 0x00, 0x00, 0x00,  // index entry 1
    0x04, 0x00, 0x27, 0x00, 0x00, 0x00,  // index entry 4
    0x06, 0x00, 0x33, 0x00, 0x00, 0x00,  // index entry 6
    0x0a, 0x00, 0x3f, 0x00, 0x00, 0x00,  // index entry 10
    0x00, 0x00, 0x3f, 0x00, 0x00, 0x00,  // extra entry for the size of last
    't',  'h',  'i',  's',  ' ',  'i',  's', ' ', 'i', 'd', ' ', '4',
    't',  'h',  'i',  's',  ' ',  'i',  's', ' ', 'i', 'd', ' ', '6'};

const size_t kSamplePakSizeV4 = sizeof(kSamplePakContentsV4);

const char kSampleCompressPakContentsV5[] = {
    0x05, 0x00, 0x00, 0x00,              // version
    0x01, 0x00, 0x00, 0x00,              // encoding + padding
    0x03, 0x00, 0x01, 0x00,              // num_resources, num_aliases
    0x04, 0x00, 0x28, 0x00, 0x00, 0x00,  // index entry 4
    0x06, 0x00, 0x34, 0x00, 0x00, 0x00,  // index entry 6
    0x08, 0x00, 0x4c, 0x00, 0x00, 0x00,  // index entry 8
    0x00, 0x00, 0x69, 0x00, 0x00, 0x00,  // extra entry for the size of last
    0x0a, 0x00, 0x00, 0x00,              // alias table

    't', 'h', 'i', 's', ' ', 'i', 's', ' ', 'i', 'd', ' ', '4',
    // "this is id 6" brotli compressed with
    // echo -n "this is id 6" |{brotli_executable_path}/brotli - -f|
    // hexdump -C where brotli_executable_path is the directory of the brotli
    // executable dependent on the build device. For example, on Android builds,
    // the executable path is out/Build_name/clang_x64/brotli. Also added header
    // with kBrotliConst and the length of the decompressed data.
    ResourceBundle::kBrotliConst[0], ResourceBundle::kBrotliConst[1], 0x0c,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x8f, 0x05, 0x80, 0x74, 0x68, 0x69, 0x73,
    0x20, 0x69, 0x73, 0x20, 0x69, 0x64, 0x20, 0x36, 0x03,
    // "this is id 8" gzipped with
    // echo -n "this is id 8" | gzip -f -c | hexdump -C
    0x1f, 0x8b, 0x08, 0x00, 0x53, 0x99, 0x1f, 0x5d, 0x00, 0x03, 0x2b, 0xc9,
    0xc8, 0x2c, 0x56, 0x00, 0xa1, 0x14, 0x05, 0x0b, 0x00, 0x71, 0xa7, 0x0b,
    0x4d, 0x0c, 0x00, 0x00, 0x00};

const size_t kSampleCompressPakSizeV5 = sizeof(kSampleCompressPakContentsV5);

const char kSampleCompressScaledPakContents[] = {
    0x05, 0x00, 0x00, 0x00,              // version
    0x01, 0x00, 0x00, 0x00,              // encoding + padding
    0x03, 0x00, 0x01, 0x00,              // num_resources, num_aliases
    0x01, 0x00, 0x28, 0x00, 0x00, 0x00,  // index entry 1
    0x04, 0x00, 0x28, 0x00, 0x00, 0x00,  // index entry 4
    0x06, 0x00, 0x34, 0x00, 0x00, 0x00,  // index entry 6
    0x00, 0x00, 0x4f, 0x00, 0x00, 0x00,  // extra entry for the size of last
    0x0a, 0x00, 0x01, 0x00,              // alias table

    't', 'h', 'i', 's', ' ', 'i', 's', ' ', 'i', 'd', ' ', '4',
    // "this is id 6 x2" brotli compressed with
    // echo -n "this is id 6 x2" |{brotli_executable_path}/brotli - -f|
    // hexdump -C where brotli_executable_path is the directory of the brotli
    // executable dependent on the build device. For example, on Android builds,
    // the executable path is out/Build_name/clang_x64/brotli. Also added header
    // with kBrotliConst and the length of the decompressed data. See
    // tools/grit/grit/node/base.py for grit brotli compression.
    ResourceBundle::kBrotliConst[0], ResourceBundle::kBrotliConst[1], 0x0f,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x07, 0x80, 0x74, 0x68, 0x69, 0x73,
    0x20, 0x69, 0x73, 0x20, 0x69, 0x64, 0x20, 0x36, 0x20, 0x78, 0x32, 0x03};

const size_t kSampleCompressScaledPakSize =
    sizeof(kSampleCompressScaledPakContents);

const char kSampleCorruptPakContents[] = {
    0x04, 0x00, 0x00, 0x00,              // header(version
    0x04, 0x00, 0x00, 0x00,              //        no. entries
    0x01,                                //        encoding)
    0x01, 0x00, 0x27, 0x00, 0x00, 0x00,  // index entry 1
    0x04, 0x00, 0x27, 0x00, 0x00, 0x00,  // index entry 4
    0x06, 0x00, 0x33, 0x00, 0x00, 0x00,  // index entry 6
    0x0a, 0x00, 0x3f, 0x00, 0x00, 0x00,  // index entry 10
    0x00, 0x00, 0x40, 0x00, 0x00, 0x00,  // extra entry for the size of last,
                                         // extends past END OF FILE.
    't', 'h', 'i', 's', ' ', 'i', 's', ' ', 'i', 'd', ' ', '4', 't', 'h', 'i',
    's', ' ', 'i', 's', ' ', 'i', 'd', ' ', '6'};

const size_t kSampleCorruptPakSize = sizeof(kSampleCorruptPakContents);

const char kSamplePakContents2x[] = {
    0x04, 0x00, 0x00, 0x00,              // header(version
    0x01, 0x00, 0x00, 0x00,              //        no. entries
    0x01,                                //        encoding)
    0x04, 0x00, 0x15, 0x00, 0x00, 0x00,  // index entry 4
    0x00, 0x00, 0x24, 0x00, 0x00, 0x00,  // extra entry for the size of last
    't',  'h',  'i',  's',  ' ',  'i',  's', ' ',
    'i',  'd',  ' ',  '4',  ' ',  '2',  'x'};

const size_t kSamplePakSize2x = sizeof(kSamplePakContents2x);

const char kEmptyPakContents[] = {
    0x04, 0x00, 0x00, 0x00,             // header(version
    0x00, 0x00, 0x00, 0x00,             //        no. entries
    0x01,                               //        encoding)
    0x00, 0x00, 0x0f, 0x00, 0x00, 0x00  // extra entry for the size of last
};

const size_t kEmptyPakSize = sizeof(kEmptyPakContents);

}  // namespace ui
