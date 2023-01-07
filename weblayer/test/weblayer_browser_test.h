// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_TEST_WEBLAYER_BROWSER_TEST_H_
#define WEBLAYER_TEST_WEBLAYER_BROWSER_TEST_H_

#include "base/memory/raw_ptr.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"

namespace content {
class BrowserContext;
}

namespace weblayer {
class ProfileImpl;
class Shell;

class WebLayerBrowserTest : public content::BrowserTestBase {
 public:
  WebLayerBrowserTest();

  WebLayerBrowserTest(const WebLayerBrowserTest&) = delete;
  WebLayerBrowserTest& operator=(const WebLayerBrowserTest&) = delete;

  ~WebLayerBrowserTest() override;

  // content::BrowserTestBase implementation.
  void SetUp() override;
  void PreRunTestOnMainThread() override;
  void PostRunTestOnMainThread() override;

  // Configures this object such that when it starts the shell it does so in
  // incognito mode. Must be invoked before SetUp() has been called.
  void SetShellStartsInIncognitoMode();

  // Returns the window for the test.
  Shell* shell() const { return shell_; }

  ProfileImpl* GetProfile();
  content::BrowserContext* GetBrowserContext();

 private:
  raw_ptr<Shell> shell_ = nullptr;
  bool start_in_incognito_mode_ = false;
};

}  // namespace weblayer

#endif  // WEBLAYER_TEST_WEBLAYER_BROWSER_TEST_H_
