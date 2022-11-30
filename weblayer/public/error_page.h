// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_ERROR_PAGE_H_
#define WEBLAYER_PUBLIC_ERROR_PAGE_H_

#include <string>

namespace weblayer {

// Contains the html to show when an error is encountered.
struct ErrorPage {
  std::string html;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_ERROR_PAGE_H_
