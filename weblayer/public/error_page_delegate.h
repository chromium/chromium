// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_ERROR_PAGE_DELEGATE_H_
#define WEBLAYER_PUBLIC_ERROR_PAGE_DELEGATE_H_

#include <memory>

namespace weblayer {

// An interface that allows handling of interactions with error pages (such as
// SSL interstitials). If this interface is not used, default actions will be
// taken.
class ErrorPageDelegate {
 public:
  // The user has pressed "back to safety" on a blocking page. A return value of
  // true will cause WebLayer to skip the default action.
  virtual bool OnBackToSafety() = 0;

 protected:
  virtual ~ErrorPageDelegate() = default;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_ERROR_PAGE_DELEGATE_H_
