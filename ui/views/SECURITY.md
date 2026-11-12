# Views Re-entrancy and Lifetime Guidance

Consult [content/browser/SECURITY.md][content-browser-sec] and [Security for
Agents][sec-agents] first for canonical UI-thread message pump, nested loop,
platform callback, threat-model, and report-validation rules.

## `Widget` and `View` Lifetime Hazards

In `//ui/views`, re-entrancy bugs occur when a `View` or `Widget` operation
triggers a synchronous native callback or nested message loop (under the
mechanisms and constraints described in
[content/browser/SECURITY.md][content-browser-sec]) that destroys the `Widget`
or mutates the `View` hierarchy while outer stack frames still hold raw `this`
or child pointers:

* **Deferred vs. synchronous `Widget` close:**
  * `Widget::Close()` / `Widget::CloseWithReason()` normally hides the window
    and posts an asynchronous task to destroy the native widget after returning
    to the message loop (queued tasks do not execute inside native OS nested
    loops by default).
  * **Synchronous destruction** occurs on the current call stack when:
    * a `CLIENT_OWNS_WIDGET` owner has registered
      `Widget::MakeCloseSynchronous()` and resets its `unique_ptr<Widget>`
      inside that callback when `Widget::Close()` / `Widget::CloseWithReason()`
      runs (instead of posting an asynchronous close task), or
    * a caller invokes `Widget::CloseNow()` or the native window is destroyed
      directly by the OS (e.g., `WM_NCDESTROY`): under
      `NATIVE_WIDGET_OWNS_WIDGET`, this synchronously deletes the `Widget` (and
      `RootView`), whereas under `CLIENT_OWNS_WIDGET` (or
      `WIDGET_OWNS_NATIVE_WIDGET`) it synchronously destroys the
      `NativeWidget`—firing `WidgetObserver::OnWidgetDestroying`,
      `WidgetDelegate::WindowClosing()`, `WidgetObserver::OnWidgetDestroyed`,
      `WidgetDelegate::DeleteDelegate()`, and any `MakeCloseSynchronous()`
      callback—and leaves the `Widget` alive in a zombie state unless the owner
      resets its `unique_ptr<Widget>` in one of those callbacks (the same
      callbacks also run if the owner resets `unique_ptr<Widget>` later when
      `Close()`'s asynchronous task destroys the `NativeWidget`).
* **Views observer and delegate callbacks:** Synchronous native windowing
  notifications (such as focus, activation, or bounds changes) can invoke
  `WidgetObserver`, `ViewObserver`, `FocusChangeListener`, or `WidgetDelegate`
  methods directly on the current call stack without invoking Chrome's message
  pump.

[content-browser-sec]: /content/browser/SECURITY.md
[sec-agents]: /docs/security/security-for-agents.md
