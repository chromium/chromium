// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "base/numerics/safe_math.h"
#include "third_party/hunspell/google/bdict.h"

// static
bool hunspell::BDict::Verify(base::span<const uint8_t> bdict) {
  if (bdict.size() <= sizeof(hunspell::BDict::Header)) {
    return false;
  }

  const BDict::Header* header =
      reinterpret_cast<const hunspell::BDict::Header*>(bdict.data());
  if (header->signature != hunspell::BDict::SIGNATURE ||
      header->major_version > hunspell::BDict::MAJOR_VERSION ||
      header->aff_offset > bdict.size() || header->dic_offset > bdict.size()) {
    return false;
  }

  {
    // Make sure there is enough room for the affix header.
    base::CheckedNumeric<uint32_t> aff_offset(header->aff_offset);
    aff_offset += sizeof(hunspell::BDict::AffHeader);
    if (!aff_offset.IsValid() || aff_offset.ValueOrDie() > bdict.size()) {
      return false;
    }
  }

  const hunspell::BDict::AffHeader* aff_header =
      reinterpret_cast<const hunspell::BDict::AffHeader*>(
          &bdict[header->aff_offset]);

  // Make sure there is enough room for the affix group count dword.
  {
    base::CheckedNumeric<uint32_t> affix_group_offset(
        aff_header->affix_group_offset);
    affix_group_offset += sizeof(uint32_t);
    if (!affix_group_offset.IsValid() ||
        affix_group_offset.ValueOrDie() > bdict.size()) {
      return false;
    }
  }

  // The new BDICT header has a MD5 digest of the dictionary data. Compare the
  // MD5 digest of the data with the one in the BDICT header.
  if (header->major_version >= 2) {
    base::MD5Digest digest;
    base::MD5Sum(bdict.subspan(header->aff_offset), &digest);
    if (memcmp(&digest, &header->digest, sizeof(digest)))
      return false;
  }

  return true;
}
