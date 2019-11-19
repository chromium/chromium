// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/view_android.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/containers/adapters.h"
#include "base/stl_util.h"
#include "cc/layers/layer.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "third_party/blink/public/platform/web_cursor_info.h"
#include "ui/android/event_forwarder.h"
#include "ui/android/ui_android_jni_headers/ViewAndroidDelegate_jni.h"
#include "ui/android/window_android.h"
#include "ui/base/layout.h"
#include "ui/events/android/drag_event_android.h"
#include "ui/events/android/event_handler_android.h"
#include "ui/events/android/gesture_event_android.h"
#include "ui/events/android/key_event_android.h"
#include "ui/events/android/motion_event_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/gurl.h"

namespace ui {

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

ViewAndroid::ScopedAnchorView::ScopedAnchorView(
    JNIEnv* env,
    const JavaRef<jobject>& jview,
    const JavaRef<jobject>& jdelegate)
    : view_(env, jview.obj()), delegate_(env, jdelegate.obj()) {
  // If there's a view, then we need a delegate to remove it.
  DCHECK(!jdelegate.is_null() || jview.is_null());
}

ViewAndroid::ScopedAnchorView::ScopedAnchorView() { }

ViewAndroid::ScopedAnchorView::ScopedAnchorView(ScopedAnchorView&& other) {
  view_ = other.view_;
  other.view_.reset();
  delegate_ = other.delegate_;
  other.delegate_.reset();
}

ViewAndroid::ScopedAnchorView&
ViewAndroid::ScopedAnchorView::operator=(ScopedAnchorView&& other) {
  if (this != &other) {
    view_ = other.view_;
    other.view_.reset();
    delegate_ = other.delegate_;
    other.delegate_.reset();
  }
  return *this;
}

ViewAndroid::ScopedAnchorView::~ScopedAnchorView() {
  Reset();
}

void ViewAndroid::ScopedAnchorView::Reset() {
  JNIEnv* env = base::android::AttachCurrentThread();
  const ScopedJavaLocalRef<jobject> view = view_.get(env);
  const ScopedJavaLocalRef<jobject> delegate = delegate_.get(env);
  if (!view.is_null() && !delegate.is_null()) {
    Java_ViewAndroidDelegate_removeView(env, delegate, view);
  }
  view_.reset();
  delegate_.reset();
}

const base::android::ScopedJavaLocalRef<jobject>
ViewAndroid::ScopedAnchorView::view() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  return view_.get(env);
}

ViewAndroid::ViewAndroid(LayoutType layout_type)
    : parent_(nullptr), layout_type_(layout_type) {}

ViewAndroid::ViewAndroid() : ViewAndroid(LayoutType::NORMAL) {}

ViewAndroid::~ViewAndroid() {
  RemoveAllChildren(GetWindowAndroid() != nullptr);
  observer_list_.Clear();
  RemoveFromParent();
}

void ViewAndroid::SetDelegate(const JavaRef<jobject>& delegate) {
  // A ViewAndroid may have its own delegate or otherwise will use the next
  // available parent's delegate.
  JNIEnv* env = base::android::AttachCurrentThread();
  delegate_ = JavaObjectWeakGlobalRef(env, delegate);
}

void ViewAndroid::UpdateFrameInfo(const FrameInfo& frame_info) {
  frame_info_ = frame_info;
}

float ViewAndroid::GetDipScale() {
  return ui::GetScaleFactorForNativeView(this);
}

ScopedJavaLocalRef<jobject> ViewAndroid::GetEventForwarder() {
  if (!event_forwarder_) {
    DCHECK(!RootPathHasEventForwarder(parent_))
        << "The view tree path already has an event forwarder.";
    DCHECK(!SubtreeHasEventForwarder(this))
        << "The view tree path already has an event forwarder.";
    event_forwarder_.reset(new EventForwarder(this));
  }
  return event_forwarder_->GetJavaObject();
}

void ViewAndroid::AddChild(ViewAndroid* child) {
  DCHECK(child);
  DCHECK(!base::Contains(children_, child));
  DCHECK(!RootPathHasEventForwarder(this) || !SubtreeHasEventForwarder(child))
      << "Some view tree path will have more than one event forwarder "
         "if the child is added.";

  // The new child goes to the top, which is the end of the list.
  children_.push_back(child);
  if (child->parent_)
    child->RemoveFromParent();
  child->parent_ = this;

  // Empty physical backing size need not propagating down since it can
  // accidentally overwrite the valid ones in the children.
  if (!physical_size_.IsEmpty())
    child->OnPhysicalBackingSizeChanged(physical_size_);

  // Empty view size also need not propagating down in order to prevent
  // spurious events with empty size from being sent down.
  if (child->match_parent() && !bounds_.IsEmpty() &&
      child->GetSize() != bounds_.size()) {
    child->OnSizeChangedInternal(bounds_.size());
    child->DispatchOnSizeChanged();
  }

  if (GetWindowAndroid())
    child->OnAttachedToWindow();
}

// static
bool ViewAndroid::RootPathHasEventForwarder(ViewAndroid* view) {
  while (view) {
    if (view->has_event_forwarder())
      return true;
    view = view->parent_;
  }

  return false;
}

// static
bool ViewAndroid::SubtreeHasEventForwarder(ViewAndroid* view) {
  if (view->has_event_forwarder())
    return true;

  for (auto* child : view->children_) {
    if (SubtreeHasEventForwarder(child))
      return true;
  }
  return false;
}

void ViewAndroid::MoveToFront(ViewAndroid* child) {
  DCHECK(child);
  auto it = std::find(children_.begin(), children_.end(), child);
  DCHECK(it != children_.end());

  // Top element is placed at the end of the list.
  if (*it != children_.back())
    children_.splice(children_.end(), children_, it);
}

void ViewAndroid::MoveToBack(ViewAndroid* child) {
  DCHECK(child);
  auto it = std::find(children_.begin(), children_.end(), child);
  DCHECK(it != children_.end());

  // Bottom element is placed at the beginning of the list.
  if (*it != children_.front())
    children_.splice(children_.begin(), children_, it);
}

void ViewAndroid::RemoveFromParent() {
  if (parent_)
    parent_->RemoveChild(this);
}

ViewAndroid::ScopedAnchorView ViewAndroid::AcquireAnchorView() {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return ViewAndroid::ScopedAnchorView();

  JNIEnv* env = base::android::AttachCurrentThread();
  return ViewAndroid::ScopedAnchorView(
      env, Java_ViewAndroidDelegate_acquireView(env, delegate), delegate);
}

void ViewAndroid::SetAnchorRect(const JavaRef<jobject>& anchor,
                                const gfx::RectF& bounds_dip) {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return;

  float dip_scale = GetDipScale();
  int left_margin = std::round(bounds_dip.x() * dip_scale);
  // Note that content_offset() is in CSS scale and bounds_dip is in DIP scale
  // (i.e., CSS pixels * page scale factor), but the height of browser control
  // is not affected by page scale factor. Thus, content_offset() in CSS scale
  // is also in DIP scale.
  int top_margin = std::round((content_offset() + bounds_dip.y()) * dip_scale);
  const gfx::RectF bounds_px = gfx::ScaleRect(bounds_dip, dip_scale);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ViewAndroidDelegate_setViewPosition(
      env, delegate, anchor, bounds_px.x(), bounds_px.y(), bounds_px.width(),
      bounds_px.height(), left_margin, top_margin);
}

ScopedJavaLocalRef<jobject> ViewAndroid::GetContainerView() {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return nullptr;

  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_ViewAndroidDelegate_getContainerView(env, delegate);
}

gfx::Point ViewAndroid::GetLocationOfContainerViewInWindow() {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return gfx::Point();

  JNIEnv* env = base::android::AttachCurrentThread();
  gfx::Point result(
      Java_ViewAndroidDelegate_getXLocationOfContainerViewInWindow(env,
                                                                   delegate),
      Java_ViewAndroidDelegate_getYLocationOfContainerViewInWindow(env,
                                                                   delegate));

  return result;
}

gfx::PointF ViewAndroid::GetLocationOnScreen(float x, float y) {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return gfx::PointF();

  JNIEnv* env = base::android::AttachCurrentThread();
  float loc_x = Java_ViewAndroidDelegate_getXLocationOnScreen(env, delegate);
  float loc_y = Java_ViewAndroidDelegate_getYLocationOnScreen(env, delegate);
  return gfx::PointF(x + loc_x, y + loc_y);
}

void ViewAndroid::RemoveAllChildren(bool attached_to_window) {
  auto it = children_.begin();
  while (it != children_.end()) {
    if (attached_to_window)
      (*it)->OnDetachedFromWindow();
    (*it)->parent_ = nullptr;
    // erase returns a new iterator for the element following the ereased one.
    it = children_.erase(it);
  }
}

void ViewAndroid::RemoveChild(ViewAndroid* child) {
  DCHECK(child);
  DCHECK_EQ(child->parent_, this);

  if (GetWindowAndroid())
    child->OnDetachedFromWindow();
  std::list<ViewAndroid*>::iterator it =
      std::find(children_.begin(), children_.end(), child);
  DCHECK(it != children_.end());
  children_.erase(it);
  child->parent_ = nullptr;
}

void ViewAndroid::AddObserver(ViewAndroidObserver* observer) {
  observer_list_.AddObserver(observer);
}

void ViewAndroid::RemoveObserver(ViewAndroidObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void ViewAndroid::RequestDisallowInterceptTouchEvent() {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ViewAndroidDelegate_requestDisallowInterceptTouchEvent(env, delegate);
}

void ViewAndroid::RequestUnbufferedDispatch(const MotionEventAndroid& event) {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ViewAndroidDelegate_requestUnbufferedDispatch(env, delegate,
                                                     event.GetJavaObject());
}

void ViewAndroid::SetCopyOutputCallback(CopyViewCallback callback) {
  copy_view_callback_ = std::move(callback);
}

// If view does not support copy request, return back the request.
std::unique_ptr<viz::CopyOutputRequest> ViewAndroid::MaybeRequestCopyOfView(
    std::unique_ptr<viz::CopyOutputRequest> request) {
  if (copy_view_callback_.is_null())
    return request;
  copy_view_callback_.Run(std::move(request));
  return nullptr;
}

void ViewAndroid::OnAttachedToWindow() {
  for (auto& observer : observer_list_)
    observer.OnAttachedToWindow();
  for (auto* child : children_)
    child->OnAttachedToWindow();
}

void ViewAndroid::OnDetachedFromWindow() {
  for (auto& observer : observer_list_)
    observer.OnDetachedFromWindow();
  for (auto* child : children_)
    child->OnDetachedFromWindow();
}

WindowAndroid* ViewAndroid::GetWindowAndroid() const {
  return parent_ ? parent_->GetWindowAndroid() : nullptr;
}

const ScopedJavaLocalRef<jobject> ViewAndroid::GetViewAndroidDelegate()
    const {
  JNIEnv* env = base::android::AttachCurrentThread();
  const ScopedJavaLocalRef<jobject> delegate = delegate_.get(env);
  if (!delegate.is_null())
    return delegate;

  return parent_ ? parent_->GetViewAndroidDelegate() : delegate;
}

cc::Layer* ViewAndroid::GetLayer() const {
  return layer_.get();
}

bool ViewAndroid::HasFocus() {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return false;
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_ViewAndroidDelegate_hasFocus(env, delegate);
}

void ViewAndroid::RequestFocus() {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ViewAndroidDelegate_requestFocus(env, delegate);
}

void ViewAndroid::SetLayer(scoped_refptr<cc::Layer> layer) {
  layer_ = layer;
}

bool ViewAndroid::StartDragAndDrop(const JavaRef<jstring>& jtext,
                                   const JavaRef<jobject>& jimage) {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return false;
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_ViewAndroidDelegate_startDragAndDrop(env, delegate, jtext,
                                                   jimage);
}

void ViewAndroid::OnCursorChanged(int type,
                                  const SkBitmap& custom_image,
                                  const gfx::Point& hotspot) {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  if (type == static_cast<int>(ui::CursorType::kCustom)) {
    if (custom_image.drawsNothing()) {
      Java_ViewAndroidDelegate_onCursorChanged(
          env, delegate, static_cast<int>(ui::CursorType::kPointer));
      return;
    }
    ScopedJavaLocalRef<jobject> java_bitmap =
        gfx::ConvertToJavaBitmap(&custom_image);
    Java_ViewAndroidDelegate_onCursorChangedToCustom(env, delegate, java_bitmap,
                                                     hotspot.x(), hotspot.y());
  } else {
    Java_ViewAndroidDelegate_onCursorChanged(env, delegate, type);
  }
}

void ViewAndroid::OnBackgroundColorChanged(unsigned int color) {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ViewAndroidDelegate_onBackgroundColorChanged(env, delegate, color);
}

void ViewAndroid::OnTopControlsChanged(float top_controls_offset,
                                       float top_content_offset) {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ViewAndroidDelegate_onTopControlsChanged(env, delegate,
                                                std::round(top_controls_offset),
                                                std::round(top_content_offset));
}

void ViewAndroid::OnBottomControlsChanged(float bottom_controls_offset,
                                          float bottom_content_offset) {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ViewAndroidDelegate_onBottomControlsChanged(
      env, delegate, std::round(bottom_controls_offset),
      std::round(bottom_content_offset));
}

int ViewAndroid::GetViewportInsetBottom() {
  ScopedJavaLocalRef<jobject> delegate(GetViewAndroidDelegate());
  if (delegate.is_null())
    return 0;
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_ViewAndroidDelegate_getViewportInsetBottom(env, delegate);
}

void ViewAndroid::OnSizeChanged(int width, int height) {
  // Match-parent view must not receive size events.
  DCHECK(!match_parent());

  float scale = GetDipScale();
  gfx::Size size(std::ceil(width / scale), std::ceil(height / scale));
  if (bounds_.size() == size)
    return;

  OnSizeChangedInternal(size);

  // Signal resize event after all the views in the tree get the updated size.
  DispatchOnSizeChanged();
}

void ViewAndroid::OnSizeChangedInternal(const gfx::Size& size) {
  if (bounds_.size() == size)
    return;

  bounds_.set_size(size);
  for (auto* child : children_) {
    if (child->match_parent())
      child->OnSizeChangedInternal(size);
  }
}

void ViewAndroid::DispatchOnSizeChanged() {
  if (event_handler_)
    event_handler_->OnSizeChanged();
  for (auto* child : children_) {
    if (child->match_parent())
      child->DispatchOnSizeChanged();
  }
}

void ViewAndroid::OnPhysicalBackingSizeChanged(const gfx::Size& size) {
  if (physical_size_ == size)
    return;
  physical_size_ = size;
  if (event_handler_)
    event_handler_->OnPhysicalBackingSizeChanged();

  for (auto* child : children_)
    child->OnPhysicalBackingSizeChanged(size);
}

gfx::Size ViewAndroid::GetPhysicalBackingSize() const {
  return physical_size_;
}

gfx::Size ViewAndroid::GetSize() const {
  return bounds_.size();
}

bool ViewAndroid::OnDragEvent(const DragEventAndroid& event) {
  return HitTest(base::BindRepeating(&ViewAndroid::SendDragEventToHandler),
                 event, event.location_f());
}

// static
bool ViewAndroid::SendDragEventToHandler(EventHandlerAndroid* handler,
                                         const DragEventAndroid& event) {
  return handler->OnDragEvent(event);
}

bool ViewAndroid::OnTouchEvent(const MotionEventAndroid& event) {
  return HitTest(base::BindRepeating(&ViewAndroid::SendTouchEventToHandler),
                 event, event.GetPoint());
}

// static
bool ViewAndroid::SendTouchEventToHandler(EventHandlerAndroid* handler,
                                          const MotionEventAndroid& event) {
  return handler->OnTouchEvent(event);
}

bool ViewAndroid::OnMouseEvent(const MotionEventAndroid& event) {
  return HitTest(base::BindRepeating(&ViewAndroid::SendMouseEventToHandler),
                 event, event.GetPoint());
}

// static
bool ViewAndroid::SendMouseEventToHandler(EventHandlerAndroid* handler,
                                          const MotionEventAndroid& event) {
  return handler->OnMouseEvent(event);
}

bool ViewAndroid::OnMouseWheelEvent(const MotionEventAndroid& event) {
  return HitTest(
      base::BindRepeating(&ViewAndroid::SendMouseWheelEventToHandler), event,
      event.GetPoint());
}

// static
bool ViewAndroid::SendMouseWheelEventToHandler(
    EventHandlerAndroid* handler,
    const MotionEventAndroid& event) {
  return handler->OnMouseWheelEvent(event);
}

bool ViewAndroid::OnGestureEvent(const GestureEventAndroid& event) {
  return HitTest(base::BindRepeating(&ViewAndroid::SendGestureEventToHandler),
                 event, event.location());
}

// static
bool ViewAndroid::SendGestureEventToHandler(EventHandlerAndroid* handler,
                                            const GestureEventAndroid& event) {
  return handler->OnGestureEvent(event);
}

bool ViewAndroid::OnGenericMotionEvent(const MotionEventAndroid& event) {
  if (event_handler_ && event_handler_->OnGenericMotionEvent(event))
    return true;

  for (auto* child : children_) {
    if (child->OnGenericMotionEvent(event))
      return true;
  }
  return false;
}

bool ViewAndroid::OnKeyUp(const KeyEventAndroid& event) {
  if (event_handler_ && event_handler_->OnKeyUp(event))
    return true;

  for (auto* child : children_) {
    if (child->OnKeyUp(event))
      return true;
  }
  return false;
}

bool ViewAndroid::DispatchKeyEvent(const KeyEventAndroid& event) {
  if (event_handler_ && event_handler_->DispatchKeyEvent(event))
    return true;

  for (auto* child : children_) {
    if (child->DispatchKeyEvent(event))
      return true;
  }
  return false;
}

bool ViewAndroid::ScrollBy(float delta_x, float delta_y) {
  if (event_handler_ && event_handler_->ScrollBy(delta_x, delta_y))
    return true;

  for (auto* child : children_) {
    if (child->ScrollBy(delta_x, delta_y))
      return true;
  }
  return false;
}

bool ViewAndroid::ScrollTo(float x, float y) {
  if (event_handler_ && event_handler_->ScrollTo(x, y))
    return true;

  for (auto* child : children_) {
    if (child->ScrollTo(x, y))
      return true;
  }
  return false;
}

template <typename E>
bool ViewAndroid::HitTest(EventHandlerCallback<E> handler_callback,
                          const E& event,
                          const gfx::PointF& point) {
  if (event_handler_) {
    if (bounds_.origin().IsOrigin()) {  // (x, y) == (0, 0)
      if (handler_callback.Run(event_handler_, event))
        return true;
    } else {
      std::unique_ptr<E> e(event.CreateFor(point));
      if (handler_callback.Run(event_handler_, *e))
        return true;
    }
  }

  if (!children_.empty()) {
    gfx::PointF offset_point(point);
    offset_point.Offset(-bounds_.x(), -bounds_.y());
    gfx::Point int_point = gfx::ToFlooredPoint(offset_point);

    // Match from back to front for hit testing.
    for (auto* child : base::Reversed(children_)) {
      bool matched = child->match_parent();
      if (!matched)
        matched = child->bounds_.Contains(int_point);
      if (matched && child->HitTest(handler_callback, event, offset_point))
        return true;
    }
  }
  return false;
}

void ViewAndroid::SetLayoutForTesting(int x, int y, int width, int height) {
  bounds_.SetRect(x, y, width, height);
}

bool ViewAndroid::HasTouchlessEventHandler() {
  JNIEnv* env = base::android::AttachCurrentThread();
  static bool s_has_touchless_event_handler =
      Java_ViewAndroidDelegate_hasTouchlessEventHandler(env);

  return s_has_touchless_event_handler;
}

bool ViewAndroid::OnUnconsumedKeyboardEventAck(int native_code) {
  if (!HasTouchlessEventHandler())
    return false;

  JNIEnv* env = base::android::AttachCurrentThread();

  return Java_ViewAndroidDelegate_onUnconsumedKeyboardEventAck(env,
                                                               native_code);
}

void ViewAndroid::FallbackCursorModeLockCursor(bool left,
                                               bool right,
                                               bool up,
                                               bool down) {
  if (!HasTouchlessEventHandler())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();

  Java_ViewAndroidDelegate_fallbackCursorModeLockCursor(env, left, right, up,
                                                        down);
}

void ViewAndroid::FallbackCursorModeSetCursorVisibility(bool visible) {
  if (!HasTouchlessEventHandler())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();

  Java_ViewAndroidDelegate_fallbackCursorModeSetCursorVisibility(env, visible);
}

}  // namespace ui
