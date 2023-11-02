// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/scoped_ignore_errors.h"

namespace x11 {

namespace {

void IgnoreErrors(const Error* error, const char* request_name) {}

}  // namespace

ScopedIgnoreErrors::ScopedIgnoreErrors(Connection* connection)
    : connection_(connection) {
  old_error_handler_ =
      connection_->SetErrorHandler(base::BindRepeating(IgnoreErrors));
}

ScopedIgnoreErrors::~ScopedIgnoreErrors() {
  connection_->SetErrorHandler(old_error_handler_);
}

}  // namespace x11
