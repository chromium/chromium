// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface to SHA1 digest computation.

#ifndef GOOGLE_CACHEINVALIDATION_DEPS_SHA1_DIGEST_FUNCTION_H_
#define GOOGLE_CACHEINVALIDATION_DEPS_SHA1_DIGEST_FUNCTION_H_

#include <string>

#include "base/hash/sha1.h"
#include "google/cacheinvalidation/deps/digest-function.h"
#include "google/cacheinvalidation/deps/stl-namespace.h"

namespace invalidation {

using ::INVALIDATION_STL_NAMESPACE::string;

class Sha1DigestFunction : public DigestFunction {
 public:
  Sha1DigestFunction() : reset_needed_(false) {}

  virtual void Reset() {
    reset_needed_ = false;
    buffer_.clear();
  }

  virtual void Update(const string& s) {
    CHECK(!reset_needed_);
    buffer_.append(s);
  }

  virtual string GetDigest() {
    CHECK(!reset_needed_);
    reset_needed_ = true;
    return base::SHA1HashString(buffer_);
  }

 private:
  string buffer_;
  bool reset_needed_;
};

}  // namespace invalidation

#endif  // GOOGLE_CACHEINVALIDATION_DEPS_SHA1_DIGEST_FUNCTION_H_
