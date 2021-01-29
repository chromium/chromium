// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_MAIN_DELEGATE_H_
#define UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_MAIN_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "content/public/app/content_main_delegate.h"
#include "content/shell/common/shell_content_client.h"

namespace ui {

class ViewsContentBrowserClient;
class ViewsContentClient;

class ViewsContentMainDelegate : public content::ContentMainDelegate {
 public:
  explicit ViewsContentMainDelegate(ViewsContentClient* views_content_client);
  ~ViewsContentMainDelegate() override;

  // content::ContentMainDelegate implementation
  bool BasicStartupComplete(int* exit_code) override;
  void PreSandboxStartup() override;
  void PreCreateMainMessageLoop() override;
  content::ContentClient* CreateContentClient() override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;

 private:
  std::unique_ptr<ViewsContentBrowserClient> browser_client_;
  content::ShellContentClient content_client_;
  ViewsContentClient* views_content_client_;

  DISALLOW_COPY_AND_ASSIGN(ViewsContentMainDelegate);
};

}  // namespace ui

#endif  // UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_MAIN_DELEGATE_H_
