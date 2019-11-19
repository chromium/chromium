// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_TEST_WEBLAYER_BROWSER_TEST_UTILS_H_
#define WEBLAYER_TEST_WEBLAYER_BROWSER_TEST_UTILS_H_

#include "base/strings/string16.h"
#include "base/values.h"

class GURL;

namespace weblayer {
class Shell;

// Navigates |shell| to |url| and wait for completed navigation.
void NavigateAndWaitForCompletion(const GURL& url, Shell* shell);

// Navigates |shell| to |url| and wait for failed navigation.
void NavigateAndWaitForFailure(const GURL& url, Shell* shell);

// Executes |script| in |shell| and returns the result.
base::Value ExecuteScript(Shell* shell,
                          const std::string& script,
                          bool use_separate_isolate);

// Gets the title of the current webpage in |shell|.
const base::string16& GetTitle(Shell* shell);

}  // namespace weblayer

#endif  // WEBLAYER_TEST_WEBLAYER_BROWSER_TEST_UTILS_H_
