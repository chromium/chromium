// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEB_DIALOGS_WEB_DIALOG_DELEGATE_H_
#define UI_WEB_DIALOGS_WEB_DIALOG_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/web_dialogs/web_dialogs_export.h"

class GURL;

namespace content {
class RenderFrameHost;
class WebContents;
class WebUI;
class WebUIMessageHandler;
struct ContextMenuParams;
struct OpenURLParams;
}

namespace gfx {
class Size;
}

namespace ui {

// Implement this class to receive notifications.
class WEB_DIALOGS_EXPORT WebDialogDelegate {
 public:
  enum class FrameKind {
    kDialog,     // Does not include a title bar or frame caption buttons.
    kNonClient,  // Includes a non client frame view with title & buttons.
  };

  WebDialogDelegate();
  virtual ~WebDialogDelegate();

  // Returns the modal type for this dialog. Only called once, during
  // WebDialogView creation. If you can, prefer using set_modal_type() to
  // overriding GetDialogModalType().
  virtual mojom::ModalType GetDialogModalType() const;
  void set_dialog_modal_type(mojom::ModalType modal_type) {
    modal_type_ = modal_type;
  }

  // Returns the title of the dialog. If you can, prefer to use set_title()
  // rather than overriding GetDialogTitle().
  virtual std::u16string GetDialogTitle() const;
  void set_dialog_title(std::u16string title) { title_ = title; }

  // Returns the title to be read with screen readers. If you can, prefer to use
  // set_accessible_title() rather than overriding GetAccessibleDialogTitle().
  virtual std::u16string GetAccessibleDialogTitle() const;
  void set_accessible_dialog_title(std::u16string accessible_title) {
    accessible_title_ = accessible_title;
  }

  // Returns the dialog's name identifier. Used to identify this dialog for
  // state restoration. If you can, prefer to use set_dialog_name() rather than
  // overriding GetDialogName().
  virtual std::string GetDialogName() const;
  void set_dialog_name(std::string dialog_name) { name_ = dialog_name; }

  // Get the HTML file path for the content to load in the dialog.
  virtual GURL GetDialogContentURL() const;
  void set_dialog_content_url(GURL content_url) { content_url_ = content_url; }

  // Get WebUIMessageHandler objects to handle messages from the HTML/JS page.
  // The handlers are used to send and receive messages from the page while it
  // is still open.  Ownership of each handler is taken over by the WebUI
  // hosting the page. By default this method adds no handlers.
  virtual void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers);
  void AddWebUIMessageHandler(
      std::unique_ptr<content::WebUIMessageHandler> handler);

  // Get the size of the dialog. Implementations can safely assume |size| is a
  // valid pointer. Callers should be able to handle the case where
  // implementations do not write into |size|.
  virtual void GetDialogSize(gfx::Size* size) const;
  void set_dialog_size(gfx::Size size) { size_ = size; }

  // Get the minimum size of the dialog. The default implementation just calls
  // GetDialogSize().
  virtual void GetMinimumDialogSize(gfx::Size* size) const;
  void set_minimum_dialog_size(gfx::Size minimum_size) {
    minimum_size_ = minimum_size;
  }

  // Gets the JSON string input to use when showing the dialog.
  // TODO: should this just be a base::Value representing the JSON value
  // directly?
  virtual std::string GetDialogArgs() const;
  void set_dialog_args(std::string dialog_args) { args_ = dialog_args; }

  // Returns true if the dialog can ever be resized.
  bool can_resize() const { return can_resize_; }
  void set_can_resize(bool can_resize) { can_resize_ = can_resize; }

  // Returns true if the dialog can ever be maximized.
  virtual bool CanMaximizeDialog() const;
  void set_can_maximize(bool can_maximize) { can_maximize_ = can_maximize; }

  // Returns true if the dialog can ever be minimized.
  bool can_minimize() const { return can_minimize_; }
  void set_can_minimize(bool can_minimize) { can_minimize_ = can_minimize; }

  // Gets the element identifier that should be used for the web view that
  // `this` is a delegate of.
  ui::ElementIdentifier web_view_element_id() const {
    return web_view_element_id_;
  }
  void set_web_view_element_id(
      const ui::ElementIdentifier web_view_element_id) {
    web_view_element_id_ = web_view_element_id;
  }

  // A callback to notify the delegate that |source|'s loading state has
  // changed.
  virtual void OnLoadingStateChanged(content::WebContents* source) {}

  // A callback to notify the delegate that a web dialog has been shown.
  // |webui| is the WebUI with which the dialog is associated.
  virtual void OnDialogShown(content::WebUI* webui) {}

  // A callback to notify the delegate that the window is requesting to be
  // closed.  If this returns true, the dialog is closed, otherwise the
  // dialog remains open. Default implementation returns true.
  virtual bool OnDialogCloseRequested();

  // Called when the dialog's window is certainly about to close, but teardown
  // has not started yet. This differs from OnDialogCloseRequested in that
  // OnDialogCloseRequested is part of the process of deciding whether to close
  // a window, while OnDialogWillClose is called as soon as it is known for
  // certain that the window is about to be closed.
  virtual void OnDialogWillClose() {}

  // A callback to notify the delegate that the dialog is about to close due to
  // the user pressing the ESC key.
  virtual void OnDialogClosingFromKeyEvent() {}

  // A callback to notify the delegate that the dialog closed.
  // IMPORTANT: Implementations should delete |this| here (unless they've
  // arranged for the delegate to be deleted in some other way, e.g. by
  // registering it as a message handler in the WebUI object).
  //
  // The default behavior of this method is to delete |this| and return.
  // Do not add new overrides of this method; instead use
  // RegisterOnDialogClosedCallback() (if you need to do things during dialog
  // close) or set_delete_on_close() (if you need to control lifetime).
  // TODO(ellyjones): Get rid of all overrides of this method.
  virtual void OnDialogClosed(const std::string& json_retval);

  void set_delete_on_close(bool delete_on_close) {
    delete_on_close_ = delete_on_close;
  }
  bool delete_on_close() const { return delete_on_close_; }

  using OnDialogClosedCallback = base::OnceCallback<void(const std::string&)>;
  void RegisterOnDialogClosedCallback(OnDialogClosedCallback callback);

  // A callback to notify the delegate that the dialog is being closed in
  // response to a "dialogClose" message from WebUI.
  virtual void OnDialogCloseFromWebUI(const std::string& json_retval);

  // A callback to notify the delegate that the contents are requesting
  // to be closed.  This could be in response to a number of events
  // that are handled by the WebContents.  If the output parameter
  // is set to true, then the dialog is closed.  The default is false.
  // |out_close_dialog| is never NULL.
  virtual void OnCloseContents(content::WebContents* source,
                               bool* out_close_dialog);
  void set_can_close(bool can_close) { can_close_ = can_close; }

  // Returns true if escape should immediately close the dialog. Default is
  // true.
  virtual bool ShouldCloseDialogOnEscape() const;
  void set_close_dialog_on_escape(bool close_dialog_on_escape) {
    close_on_escape_ = close_dialog_on_escape;
  }

  // A callback to allow the delegate to dictate that the window should not
  // have a title bar.  This is useful when presenting branded interfaces.
  virtual bool ShouldShowDialogTitle() const;
  void set_show_dialog_title(bool show_dialog_title) {
    show_title_ = show_dialog_title;
  }

  // A callback to allow the delegate to center title text. Default is
  // false.
  virtual bool ShouldCenterDialogTitleText() const;
  void set_center_dialog_title_text(bool center_dialog_title_text) {
    center_title_text_ = center_dialog_title_text;
  }

  // Returns true if the dialog should show a close button in the title bar.
  // Default implementation returns true.
  virtual bool ShouldShowCloseButton() const;
  void set_show_close_button(bool show_close_button) {
    show_close_button_ = show_close_button;
  }

  // A callback to allow the delegate to inhibit context menu or show
  // customized menu.
  //
  // The `render_frame_host` represents the frame that requests the context menu
  // (typically this frame is focused, but this is not necessarily the case -
  // see https://crbug.com/1257907#c14).
  //
  // Returns true iff you do NOT want the standard context menu to be
  // shown (because you want to handle it yourself).
  virtual bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                                 const content::ContextMenuParams& params);
  void set_allow_default_context_menu(bool allow_default_context_menu) {
    allow_default_context_menu_ = allow_default_context_menu;
  }

  // A callback to allow the delegate to open a new URL inside |source|.
  // On return |out_new_contents| should contain the WebContents the URL
  // is opened in. Return false to use the default handler.
  // If a `navigation_handle_callback` function is provided, it should be called
  // with the pending navigation (if any) when the navigation handle become
  // available. This allows callers to observe or attach their specific data.
  // `navigation_handle_callback` may not be called if the navigation fails for
  // any reason.
  virtual bool HandleOpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback,
      content::WebContents** out_new_contents);

  // A callback to control whether a WebContents will be created. Returns
  // true to disallow the creation. Return false to use the default handler.
  virtual bool HandleShouldOverrideWebContentsCreation();
  void set_allow_web_contents_creation(bool allow_web_contents_creation) {
    allow_web_contents_creation_ = allow_web_contents_creation;
  }

  // Accelerator handling: there are two ways to supply accelerators. You can
  // register individual accelerators using RegisterAccelerator(), or you can
  // get more flexibility by overriding GetAccelerators() and
  // AcceleratorPressed() to provide arbitrary handling.
  using AcceleratorHandler =
      base::RepeatingCallback<bool(WebDialogDelegate&, const Accelerator&)>;
  void RegisterAccelerator(Accelerator accelerator, AcceleratorHandler handler);

  // The default behavior of these methods is to return/invoke accelerators
  // registered with RegisterAccelerator().
  // TODO(ellyjones): Remove these.
  virtual std::vector<Accelerator> GetAccelerators();
  virtual bool AcceleratorPressed(const Accelerator& accelerator);

  virtual void OnWebContentsFinishedLoad() {}

  // TODO(ellyjones): Document what these do and when they are called.
  // Especially document what CheckMediaAccessPermission() is supposed to
  // return.
  virtual void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) {}
  virtual bool CheckMediaAccessPermission(
      content::RenderFrameHost* render_frame_host,
      const url::Origin& security_origin,
      blink::mojom::MediaStreamType type);

  // Whether to use dialog frame view for non client frame view.
  virtual FrameKind GetWebDialogFrameKind() const;
  void set_dialog_frame_kind(FrameKind frame_kind) { frame_kind_ = frame_kind; }

 private:
  base::flat_map<Accelerator, AcceleratorHandler> accelerators_;
  std::optional<std::u16string> accessible_title_;
  bool allow_default_context_menu_ = true;
  bool allow_web_contents_creation_ = true;
  std::string args_;
  bool can_close_ = false;
  bool can_maximize_ = false;
  bool can_minimize_ = false;
  bool can_resize_ = true;
  bool center_title_text_ = false;
  GURL content_url_;
  bool close_on_escape_ = true;
  // TODO(ellyjones): Make this default to false.
  bool delete_on_close_ = true;
  FrameKind frame_kind_ = FrameKind::kNonClient;
  std::optional<gfx::Size> minimum_size_;
  mojom::ModalType modal_type_ = mojom::ModalType::kNone;
  std::string name_;
  bool show_close_button_ = true;
  bool show_title_ = true;
  gfx::Size size_;
  std::u16string title_;
  // The value that should be used for the element ID of the web view that this
  // is a delegate for.
  ui::ElementIdentifier web_view_element_id_;

  OnDialogClosedCallback closed_callback_;

  std::vector<std::unique_ptr<content::WebUIMessageHandler>>
      added_message_handlers_;
};

}  // namespace ui

#endif  // UI_WEB_DIALOGS_WEB_DIALOG_DELEGATE_H_
