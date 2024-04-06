// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_EXAMPLES_BROWSER_DEVTOOLS_DEVTOOLS_FRONTEND_H_
#define UI_WEBUI_EXAMPLES_BROWSER_DEVTOOLS_DEVTOOLS_FRONTEND_H_

#include "base/memory/raw_ptr.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace webui_examples {

class DevToolsFrontend {
 public:
  DevToolsFrontend(const DevToolsFrontend&) = delete;
  DevToolsFrontend& operator=(const DevToolsFrontend&) = delete;
  ~DevToolsFrontend();

  static DevToolsFrontend* CreateAndGet(
      content::WebContents* inspected_contents);

  const GURL& frontend_url() { return frontend_url_; }

  void SetDevtoolsWebContents(content::WebContents* devtools_contents);

 private:
  class AgentHostClient;
  class Pointer;
  DevToolsFrontend(content::WebContents* inspected_contents);

  const GURL frontend_url_;
  raw_ptr<content::WebContents> inspected_contents_;
  raw_ptr<content::WebContents> devtools_contents_;
  std::unique_ptr<AgentHostClient> agent_host_client_;
};

}  // namespace webui_examples

#endif  // UI_WEBUI_EXAMPLES_BROWSER_DEVTOOLS_DEVTOOLS_FRONTEND_H_
