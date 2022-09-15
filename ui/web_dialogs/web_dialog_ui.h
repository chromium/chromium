// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEB_DIALOGS_WEB_DIALOG_UI_H_
#define UI_WEB_DIALOGS_WEB_DIALOG_UI_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/base/ui_base_types.h"
#include "ui/web_dialogs/web_dialogs_export.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace ui {

class WebDialogDelegate;

class WEB_DIALOGS_EXPORT WebDialogUIBase {
 public:
  // Sets the delegate on the WebContents.
  static void SetDelegate(content::WebContents* web_contents,
                          WebDialogDelegate* delegate);

  WebDialogUIBase(content::WebUI* web_ui);

  WebDialogUIBase(const WebDialogUIBase&) = delete;
  WebDialogUIBase& operator=(const WebDialogUIBase&) = delete;

  // Close the dialog, passing the specified arguments to the close handler.
  void CloseDialog(const base::Value::List& args);

 protected:
  virtual ~WebDialogUIBase();

  // Prepares |render_frame_host| to host a dialog.
  void HandleRenderFrameCreated(content::RenderFrameHost* render_frame_host);

 private:
  // Gets the delegate for the WebContent set with SetDelegate.
  static WebDialogDelegate* GetDelegate(content::WebContents* web_contents);

  // JS message handler.
  void OnDialogClosed(const base::Value::List& args);

  raw_ptr<content::WebUI> web_ui_;
};

// Displays file URL contents inside a modal web dialog.
//
// This application really should not use WebContents + WebUI. It should instead
// just embed a RenderView in a dialog and be done with it.
//
// Before loading a URL corresponding to this WebUI, the caller should set its
// delegate as user data on the WebContents by calling SetDelegate(). This WebUI
// will pick it up from there and call it back. This is a bit of a hack to allow
// the dialog to pass its delegate to the Web UI without having nasty accessors
// on the WebContents. The correct design using RVH directly would avoid all of
// this.
class WEB_DIALOGS_EXPORT WebDialogUI : public WebDialogUIBase,
                                       public content::WebUIController {
 public:
  // When created, the delegate should already be set as user data on the
  // WebContents.
  explicit WebDialogUI(content::WebUI* web_ui);
  ~WebDialogUI() override;
  WebDialogUI(const WebDialogUI&) = delete;
  WebDialogUI& operator=(const WebDialogUI&) = delete;

 private:
  // content::WebUIController:
  void WebUIRenderFrameCreated(
      content::RenderFrameHost* render_frame_host) override;
};

// Displays file URL contents inside a modal web dialog while also enabling
// Mojo calls to be made from within the dialog.
class WEB_DIALOGS_EXPORT MojoWebDialogUI : public WebDialogUIBase,
                                           public MojoWebUIController {
 public:
  // When created, the delegate should already be set as user data on the
  // WebContents.
  explicit MojoWebDialogUI(content::WebUI* web_ui);
  ~MojoWebDialogUI() override;
  MojoWebDialogUI(const MojoWebDialogUI&) = delete;
  MojoWebDialogUI& operator=(const MojoWebDialogUI&) = delete;

 private:
  // content::WebUIController:
  void WebUIRenderFrameCreated(
      content::RenderFrameHost* render_frame_host) override;
};

}  // namespace ui

#endif  // UI_WEB_DIALOGS_WEB_DIALOG_UI_H_
