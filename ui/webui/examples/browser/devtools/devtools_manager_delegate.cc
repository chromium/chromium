// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/examples/browser/devtools/devtools_manager_delegate.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_agent_host.h"

namespace webui_examples {

DevToolsManagerDelegate::DevToolsManagerDelegate(
    content::BrowserContext* browser_context,
    CreateContentWindowFunc create_content_window_func)
    : browser_context_(browser_context),
      create_content_window_func_(std::move(create_content_window_func)) {}

DevToolsManagerDelegate::~DevToolsManagerDelegate() = default;

content::BrowserContext* DevToolsManagerDelegate::GetDefaultBrowserContext() {
  return browser_context_;
}

scoped_refptr<content::DevToolsAgentHost>
DevToolsManagerDelegate::CreateNewTarget(
    const GURL& url,
    content::DevToolsManagerDelegate::TargetType target_type) {
  content::WebContents* web_content =
      create_content_window_func_.Run(browser_context_.get(), url);
  return target_type == content::DevToolsManagerDelegate::kTab
             ? content::DevToolsAgentHost::GetOrCreateForTab(web_content)
             : content::DevToolsAgentHost::GetOrCreateFor(web_content);
}

std::string DevToolsManagerDelegate::GetDiscoveryPageHTML() {
  return std::string();
}

bool DevToolsManagerDelegate::HasBundledFrontendResources() {
  return true;
}

}  // namespace webui_examples
