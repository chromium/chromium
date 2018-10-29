// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBRUNNER_BROWSER_TEST_COMMON_H_
#define WEBRUNNER_BROWSER_TEST_COMMON_H_

#include "content/public/browser/web_contents_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "webrunner/fidl/chromium/web/cpp/fidl.h"

namespace webrunner {

// Defines mock methods used by tests to observe NavigationStateChangeEvents
// and lower-level WebContentsObserver events.
class MockNavigationObserver : public chromium::web::NavigationEventObserver,
                               public content::WebContentsObserver {
 public:
  using content::WebContentsObserver::Observe;

  MockNavigationObserver();
  ~MockNavigationObserver() override;

  // Acknowledges processing of the most recent OnNavigationStateChanged call.
  void Acknowledge();

  MOCK_METHOD1(MockableOnNavigationStateChanged,
               void(chromium::web::NavigationEvent change));

  // chromium::web::NavigationEventObserver implementation.
  // Proxies calls to MockableOnNavigationStateChanged(), because GMock does
  // not work well with fit::Callbacks inside mocked actions.
  void OnNavigationStateChanged(
      chromium::web::NavigationEvent change,
      OnNavigationStateChangedCallback callback) override;

  // WebContentsObserver implementation.
  MOCK_METHOD2(DidFinishLoad,
               void(content::RenderFrameHost* render_frame_host,
                    const GURL& validated_url));

 private:
  OnNavigationStateChangedCallback navigation_ack_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockNavigationObserver);
};

}  // namespace webrunner

#endif  // WEBRUNNER_BROWSER_TEST_COMMON_H_
