// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_WEBVIEW_WEB_CONTENTS_DELEGATE_VIEWS_H_
#define UI_VIEWS_CONTROLS_WEBVIEW_WEB_CONTENTS_DELEGATE_VIEWS_H_

#include <concepts>
#include <optional>
#include <type_traits>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview_export.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"

namespace views {

// Specifies how keyboard accelerators are handled when the WebContents is
// focused.
enum class WebContentsAcceleratorHandling {
  // Accelerators are only sent to the web contents and will not be processed by
  // the browser (default).
  kDisabled,
  // Accelerators sent to the web contents will be processed after the renderer
  // has had a chance to process them.
  //
  // This is the recommended setting for most surfaces.
  kEnabled,
  // Accelerators sent to the web contents will be processed before the renderer
  // on non-mac platforms (equivalent to kEnabled on Mac due to a different
  // processing pathway).
  //
  // TODO(https://crbug.com/576327952): This is provided for backwards-
  // compatibility and should not generally be used in new code.
  kProcessBeforeRenderer,
};

// Process the ESC key being pressed; returns whether the event was consumed by
// the callback (if so, it will not receive further processing). Can be used to
// close a dialog or panel, or just to block processing of the ESC key.
using WebContentsOnEscapeCallback = base::RepeatingCallback<bool()>;

namespace internal {

// Base class that provides functionality for accelerator handling in WebUI
// hosted in Views. See `WebContentsDelegateViews` for full documentation.
class WEBVIEW_EXPORT WebContentsDelegateViewsBase {
 public:
  WebContentsDelegateViewsBase();
  WebContentsDelegateViewsBase(const WebContentsDelegateViewsBase&);
  void operator=(const WebContentsDelegateViewsBase&);
  virtual ~WebContentsDelegateViewsBase();

  // Sets the view which provides the focus manager to handle accelerators, the
  // widget to close with ESC, etc.; not necessary to set this if `this` is a
  // View.
  //
  // The view is only weakly stored so there is no need to reset this to null if
  // the view goes away before the delegate. However, the reference view
  // *should* be updated if the web contents moves to a new view/surface.
  void SetReferenceView(views::View* reference_view);

  // Set a callback that will be called whenever the user presses ESC. See
  // `OnEscCallback` for details.
  //
  // The callback will happen before the renderer sees the event and if it
  // handles the event the event will not receive further processing.
  //
  // Be aware that in many situations, `SetOnEscCloseWidget()` is simpler and
  // may do what you need.
  void SetOnEscCallback(WebContentsOnEscapeCallback on_esc_callback);

  // Convenience method which sets the on-ESC callback to close the widget
  // associated with the reference view (which might be `this` if `this` is a
  // view).
  void SetOnEscCloseWidget();

  // Set how accelerator handling should work for the associated WebContents.
  // If `this` is not a view, `SetReferenceView()` must be called first.
  void SetAcceleratorHandling(
      WebContentsAcceleratorHandling accelerator_handling);

  // Returns the accelerator handling for the given `contents`, or `kDisabled`
  // if none.
  static WebContentsAcceleratorHandling GetAcceleratorHandlingFor(
      content::WebContents& contents);

  // Returns the delegate for the given `contents`.
  static WebContentsDelegateViewsBase* FromWebContents(
      content::WebContents& contents);

 protected:
  bool MaybeProcessEscape(const input::NativeWebKeyboardEvent& event);
  bool MaybeProcessKeypress(const input::NativeWebKeyboardEvent& event);
  void OnAttached(content::WebContents* contents);
  void OnDetaching(content::WebContents* contents);

  // Returns the reference view, which might be `this` if no reference view is
  // set and `this` is a View.
  virtual views::View* GetReferenceView() = 0;

  // Returns the _explicitly set_ reference view; does not return `this`.
  std::optional<views::ViewTracker>& reference_view() {
    return reference_view_;
  }

 private:
  WebContentsAcceleratorHandling accelerator_handling_ =
      WebContentsAcceleratorHandling::kDisabled;
  UnhandledKeyboardEventHandler event_handler_;
  WebContentsOnEscapeCallback on_esc_callback_;
  std::optional<views::ViewTracker> reference_view_;
  base::WeakPtrFactory<WebContentsDelegateViewsBase> weak_ptr_factory_{this};
};

}  // namespace internal

// CRTP base class that adds the `content::WebContentsDelegate` interface along
// with convenience methods for configuring Views-based accelerator handling.
//
// Note that `WebView` implements this class already, and is the default
// delegate for its `content::WebContents`. If you only need WebView +
// accelerator handling, you can configure the WebView directly:
//
// ```
//  MyController::Init() {
//    web_view_->SetAcceleratorHandling(
//        WebContentsAcceleratorHandling)::kEnabled);
//
//    // Optional: if `web_view_` is on a dialog that needs to close when ESC
//    // is pressed, this handles it.
//    web_view_->SetOnEscCloseWidget();
//  }
// ```
//
// If you need a custom WebContentsDelegate, derive your own implementation, as
// shown below:
//
// ```
//  class MyController : public WebContentsDelegateViews<MyController> {
//  };
//
//  MyController::Init() {
//    // Only required if MyController isn't also a View. Must be set first.
//    // This helps us route accelerator events.
//    set_reference_view(web_view_);
//
//    // Make sure accelerators are handled.
//    SetAcceleratorHandling(
//        WebContentsAcceleratorHandling::kEnabled);
//
//    // Optional: if you need it, set an ESC handler.
//    //
//    // Note that if the reference view is in a dialog, you might still use
//    // SetOnEscCloseWidget() here instead.
//    SetOnEscCallback(&MyClass::OnEscPressed, GetWeakPtr());
//
//    // Set the webcontents to use `this` as its delegate. This can be done
//    // before or after configuration.
//    web_contents_->SetDelegate(this);
//  }
// ```
template <typename T, typename Delegate = content::WebContentsDelegate>
  requires std::derived_from<Delegate, content::WebContentsDelegate>
class WebContentsDelegateViews : public internal::WebContentsDelegateViewsBase,
                                 public Delegate {
 public:
  template <typename... Args>
  explicit WebContentsDelegateViews(Args&&... args)
      : Delegate(std::forward<Args>(args)...) {}
  ~WebContentsDelegateViews() override = default;

 protected:
  // content::WebContentsDelegate:
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override {
    return WebContentsDelegateViewsBase::MaybeProcessEscape(event)
               ? content::KeyboardEventProcessingResult::HANDLED
               : Delegate::PreHandleKeyboardEvent(source, event);
  }
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override {
    return WebContentsDelegateViewsBase::MaybeProcessKeypress(event) ||
           Delegate::HandleKeyboardEvent(source, event);
  }

  // WebContentsDelegate:
  void OnAttached(content::WebContents* contents) override {
    internal::WebContentsDelegateViewsBase::OnAttached(contents);
    Delegate::OnAttached(contents);
  }

  void OnDetaching(content::WebContents* contents) override {
    Delegate::OnDetaching(contents);
    internal::WebContentsDelegateViewsBase::OnDetaching(contents);
  }

 private:
  // Make this private.
  using WebContentsDelegateViewsBase::reference_view;

  // internal::WebContentsDelegateViewsBase:
  views::View* GetReferenceView() override {
    views::View* view = reference_view() ? reference_view()->view() : nullptr;
    if constexpr (std::derived_from<T, views::View>) {
      if (!view) {
        view = static_cast<T*>(this);
      }
    } else {
      CHECK(reference_view())
          << "Accelerator and/or close-on-ESC is enabled for delegate which is "
             "not a view, but no reference view was set, so accelerator "
             "processing will never actually happen.";
    }
    return view;
  }
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_WEBVIEW_WEB_CONTENTS_DELEGATE_VIEWS_H_
