// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/url_canon_stdstring.h"

namespace url {

StdStringCanonOutput::StdStringCanonOutput(std::string* str) : str_(str) {
  cur_len_ = str_->size();  // Append to existing data.
  buffer_ = str_->empty() ? nullptr : &(*str_)[0];
  buffer_len_ = str_->size();
}

StdStringCanonOutput::~StdStringCanonOutput() {
  // Nothing to do, we don't own the string.
}

void StdStringCanonOutput::Complete() {
  str_->resize(cur_len_);
  buffer_len_ = cur_len_;
}

void StdStringCanonOutput::Resize(size_t sz) {
  str_->resize(sz);
  buffer_ = str_->empty() ? nullptr : &(*str_)[0];
  buffer_len_ = sz;
}

}  // namespace url
