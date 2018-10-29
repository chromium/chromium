// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/mojo_web_ui_controller.h"

#include "content/public/browser/render_process_host.h"
#include "content/public/common/bindings_policy.h"

namespace ui {

MojoWebUIController::MojoWebUIController(content::WebUI* contents,
                                         bool enable_chrome_send)
    : content::WebUIController(contents),
      content::WebContentsObserver(contents->GetWebContents()) {
  int bindings = content::BINDINGS_POLICY_MOJO_WEB_UI;
  if (enable_chrome_send)
    bindings |= content::BINDINGS_POLICY_WEB_UI;
  contents->SetBindings(bindings);
}
MojoWebUIController::~MojoWebUIController() = default;

void MojoWebUIController::OnInterfaceRequestFromFrame(
    content::RenderFrameHost* render_frame_host,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  if (!registry_.CanBindInterface(interface_name))
    return;

  // Right now, this is expected to be called only for main frames.
  if (render_frame_host->GetParent()) {
    LOG(ERROR) << "Terminating renderer for requesting " << interface_name
               << " interface from subframe";
    render_frame_host->GetProcess()->ShutdownForBadMessage(
        content::RenderProcessHost::CrashReportMode::GENERATE_CRASH_DUMP);
    return;
  }

  registry_.TryBindInterface(interface_name, interface_pipe);
}

}  // namespace ui
