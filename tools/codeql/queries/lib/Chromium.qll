// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import cpp
import semmle.code.cpp.dataflow.DataFlow

module Chromium {
  predicate isChromiumCode(Element e) { isChromiumPath(e.getFile()) }

  predicate isChromiumPath(Container c) {
    exists(string path |
        path = c.getAbsolutePath() and
        not path.matches("%buildtools%") and
        not path.matches("%include/c++%") and
        not path.matches("/usr/include%") and
        not path.matches("%native_client%")
    )
  }

  predicate isUbiquitousChromiumPath(Container c) {
    exists(string path |
        path = c.getAbsolutePath() and
        not path.matches("%ios_internal%") and
        not path.matches("%ios%") and
        not path.matches("%android_webview%") and
        not path.matches("%chromecast%")
    )
  }

  predicate isUbiquitousChromiumCode(Element e) {
    isChromiumPath(e.getFile()) and
    isUbiquitousChromiumPath(e.getFile())
  }

  predicate isBlinkCode(Element e) {
    isChromiumCode(e.getFile()) and
    isBlinkPath(e.getFile())
  }

  predicate isBlinkPath(Container c) {
    exists(string path |
      path = c.getAbsolutePath() and
      path.matches("%third_party/blink%")
    )
  }
}
