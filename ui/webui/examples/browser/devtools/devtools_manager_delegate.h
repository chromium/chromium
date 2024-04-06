// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_EXAMPLES_BROWSER_DEVTOOLS_DEVTOOLS_MANAGER_DELEGATE_H_
#define UI_WEBUI_EXAMPLES_BROWSER_DEVTOOLS_DEVTOOLS_MANAGER_DELEGATE_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace webui_examples {

class DevToolsManagerDelegate : public content::DevToolsManagerDelegate {
 public:
  using CreateContentWindowFunc =
      base::RepeatingCallback<content::WebContents*(content::BrowserContext*,
                                                    const GURL&)>;

  DevToolsManagerDelegate(content::BrowserContext* browser_context,
                          CreateContentWindowFunc create_content_window_func);
  DevToolsManagerDelegate(const DevToolsManagerDelegate&) = delete;
  DevToolsManagerDelegate& operator=(const DevToolsManagerDelegate&) = delete;
  ~DevToolsManagerDelegate() override;

  // DevToolsManagerDelegate:
  content::BrowserContext* GetDefaultBrowserContext() override;
  scoped_refptr<content::DevToolsAgentHost> CreateNewTarget(
      const GURL& url,
      TargetType target_type) override;
  std::string GetDiscoveryPageHTML() override;
  bool HasBundledFrontendResources() override;

 private:
  const raw_ptr<content::BrowserContext> browser_context_;
  CreateContentWindowFunc create_content_window_func_;
};

}  // namespace webui_examples

#endif  // UI_WEBUI_EXAMPLES_BROWSER_DEVTOOLS_DEVTOOLS_MANAGER_DELEGATE_H_
