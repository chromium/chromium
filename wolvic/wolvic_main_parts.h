// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_WOLVIC_MAIN_PARTS_H_
#define WOLVIC_WOLVIC_MAIN_PARTS_H_

#include "content/public/browser/browser_main_parts.h"

namespace wolvic {

class WolvicBrowserContext;

class WolvicMainParts : public content::BrowserMainParts {
 public:
  WolvicMainParts();

  WolvicMainParts(const WolvicMainParts&) = delete;
  WolvicMainParts& operator=(const WolvicMainParts&) = delete;

  ~WolvicMainParts() override;

  // BrowserMainParts overrides.
  int PreEarlyInitialization() override;
  int PreMainMessageLoopRun() override;
  void PostMainMessageLoopRun() override;

  // Inspired from the //chrome ChromeBrowserMainParts, but we don't need such complex Parts achitectuture in Wolvic.
  // TODO(jfernandez): Do we need to implement an actual Profile feature ?
  void PreProfileInit();
  void PostBrowserStart();

  WolvicBrowserContext* browser_context() { return browser_context_.get(); }
  WolvicBrowserContext* off_the_record_browser_context() {
    return off_the_record_browser_context_.get();
  }

 private:
  void set_browser_context(WolvicBrowserContext*);
  void set_off_the_record_browser_context(WolvicBrowserContext*);

  // Instantiates all the KeyedService factories used in Wolvic, which is
  // especially important for services that should be created at profile
  // creation time as compared to lazily on first access.
  // TODO(jfernandez): Inspired by //chrome/profiles ChromeBrowserMainExtraPartProfiles, but we may need to simplify
  // this logic if we don't need to depend on a Profiles logic.
  static void EnsureBrowserContextKeyedServiceFactoriesBuilt();

  std::unique_ptr<WolvicBrowserContext> browser_context_;
  std::unique_ptr<WolvicBrowserContext> off_the_record_browser_context_;
};

}  // namespace wolvic

#endif  // WOLVIC_WOLVIC_MAIN_PARTS_H_
