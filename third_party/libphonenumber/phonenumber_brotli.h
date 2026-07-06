// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_LIBPHONENUMBER_PHONENUMBER_BROTLI_H_
#define THIRD_PARTY_LIBPHONENUMBER_PHONENUMBER_BROTLI_H_

#include <cstddef>

#include "phonenumbers/metadata_bytes.h"

namespace i18n::phonenumbers {

// Brotli-decompresses |compressed_size| bytes at |compressed| into a freshly
// allocated buffer of exactly |uncompressed_size| bytes and returns it as an
// owning MetadataBytes. The buffer is released when the MetadataBytes goes out
// of scope, i.e. once its caller (PhoneNumberUtil) has parsed the metadata.
//
// The compressed input is produced at build time by
// gen_compressed_metadata.py, so decompression cannot fail in practice; the
// process is terminated if it ever does, rather than handing back partially
// initialized metadata.
MetadataBytes PhonenumberBrotliDecompress(const unsigned char* compressed,
                                          size_t compressed_size,
                                          int uncompressed_size);

}  // namespace i18n::phonenumbers

#endif  // THIRD_PARTY_LIBPHONENUMBER_PHONENUMBER_BROTLI_H_
