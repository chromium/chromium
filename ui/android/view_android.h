// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_VIEW_ANDROID_H_
#define UI_ANDROID_VIEW_ANDROID_H_

#include <list>
#include <memory>

#include "base/android/jni_array.h"
#include "base/android/jni_weak_ref.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "ui/android/ui_android_export.h"
#include "ui/android/view_android_observer.h"
#include "ui/gfx/geometry/rect_f.h"

class SkBitmap;

namespace cc {
class Layer;
}

namespace gfx {
class Point;
class Size;
}

namespace viz {
class CopyOutputRequest;
}

namespace ui {
class DragEventAndroid;
class EventForwarder;
class EventHandlerAndroid;
class GestureEventAndroid;
class KeyEventAndroid;
class MotionEventAndroid;
class WindowAndroid;
class ViewAndroidObserver;

// View-related parameters from frame updates.
struct FrameInfo {
  gfx::SizeF viewport_size;  // In dip.

  // Content offset from the top. Used to translate snapshots to
  // the correct part of the view. In dip.
  float content_offset;
};

// A simple container for a UI layer.
// At the root of the hierarchy is a WindowAndroid, when attached.
// Dispatches input/view events coming from Java layer. Hit testing against
// those events is implemented so that the |ViewClient| will be invoked
// when the event got hit on the area defined by |layout_params_|.
// Hit testing is done in the order of parent -> child, and from top
// of the stack to back among siblings.
class UI_ANDROID_EXPORT ViewAndroid {
 public:
  using CopyViewCallback =
      base::RepeatingCallback<void(std::unique_ptr<viz::CopyOutputRequest>)>;

  // Stores an anchored view to delete itself at the end of its lifetime
  // automatically. This helps manage the lifecyle without the dependency
  // on |ViewAndroid|.
  class ScopedAnchorView {
   public:
    ScopedAnchorView(JNIEnv* env,
                     const base::android::JavaRef<jobject>& jview,
                     const base::android::JavaRef<jobject>& jdelegate);

    ScopedAnchorView();
    ScopedAnchorView(ScopedAnchorView&& other);
    ScopedAnchorView& operator=(ScopedAnchorView&& other);

    // Calls JNI removeView() on the delegate for cleanup.
    ~ScopedAnchorView();

    void Reset();

    const base::android::ScopedJavaLocalRef<jobject> view() const;

   private:
    // TODO(jinsukkim): Following weak refs can be cast to strong refs which
    //     cannot be garbage-collected and leak memory. Rewrite not to use them.
    //     see comments in crrev.com/2103243002.
    JavaObjectWeakGlobalRef view_;
    JavaObjectWeakGlobalRef delegate_;

    // Default copy/assign disabled by move constructor.
  };

  enum class LayoutType {
    // Can have its own size given by |OnSizeChanged| events.
    NORMAL,
    // Always follows its parent's size.
    MATCH_PARENT
  };

  explicit ViewAndroid(LayoutType layout_type);

  ViewAndroid();
  virtual ~ViewAndroid();

  void UpdateFrameInfo(const FrameInfo& frame_info);
  // content_offset is in dip.
  float content_offset() const { return frame_info_.content_offset; }
  gfx::SizeF viewport_size() const { return frame_info_.viewport_size; }

  // Returns the window at the root of this hierarchy, or |null|
  // if disconnected.
  virtual WindowAndroid* GetWindowAndroid() const;

  // Virtual for testing.
  virtual float GetDipScale();

  // Used to return and set the layer for this view. May be |null|.
  cc::Layer* GetLayer() const;
  void SetLayer(scoped_refptr<cc::Layer> layer);

  void SetDelegate(const base::android::JavaRef<jobject>& delegate);

  // Gets (creates one if not present) Java object of the EventForwarder
  // for a view tree in the view hierarchy including this node.
  // Only one instance per the view tree is allowed.
  base::android::ScopedJavaLocalRef<jobject> GetEventForwarder();

  // Adds a child to this view.
  void AddChild(ViewAndroid* child);

  // Moves the give child ViewAndroid to the front of the list so that it can be
  // the first responder of events.
  void MoveToFront(ViewAndroid* child);
  // Moves the given child ViewAndroid to the back of the list so that any other
  // view may respond to events first.
  void MoveToBack(ViewAndroid* child);

  // Detaches this view from its parent.
  void RemoveFromParent();

  bool HasFocus();
  void RequestFocus();

  bool StartDragAndDrop(const base::android::JavaRef<jstring>& jtext,
                        const base::android::JavaRef<jobject>& jimage);

  gfx::Size GetPhysicalBackingSize() const;
  gfx::Size GetSize() const;
  gfx::Rect bounds() const { return bounds_; }

  void OnSizeChanged(int width, int height);
  void OnPhysicalBackingSizeChanged(const gfx::Size& size);
  void OnCursorChanged(int type,
                       const SkBitmap& custom_image,
                       const gfx::Point& hotspot);
  void OnBackgroundColorChanged(unsigned int color);
  void OnTopControlsChanged(float top_controls_offset,
                            float top_content_offset);
  void OnBottomControlsChanged(float bottom_controls_offset,
                               float bottom_content_offset);

  // Gets the Visual Viewport inset to apply in physical pixels.
  int GetViewportInsetBottom();

  ScopedAnchorView AcquireAnchorView();
  void SetAnchorRect(const base::android::JavaRef<jobject>& anchor,
                     const gfx::RectF& bounds);

  // This may return null.
  base::android::ScopedJavaLocalRef<jobject> GetContainerView();

  // Return the location of the container view in physical pixels.
  gfx::Point GetLocationOfContainerViewInWindow();

  // Return the location of the point relative to screen coordinate in pixels.
  gfx::PointF GetLocationOnScreen(float x, float y);

  // ViewAndroid does not own |observer|s.
  void AddObserver(ViewAndroidObserver* observer);
  void RemoveObserver(ViewAndroidObserver* observer);

  void RequestDisallowInterceptTouchEvent();
  void RequestUnbufferedDispatch(const MotionEventAndroid& event);

  void SetCopyOutputCallback(CopyViewCallback callback);
  // Return the CopyOutputRequest back if view cannot perform readback.
  std::unique_ptr<viz::CopyOutputRequest> MaybeRequestCopyOfView(
      std::unique_ptr<viz::CopyOutputRequest> request);

  void set_event_handler(EventHandlerAndroid* handler) {
    event_handler_ = handler;
  }

  ViewAndroid* parent() const { return parent_; }

  bool OnTouchEventForTesting(const MotionEventAndroid& event) {
    return OnTouchEvent(event);
  }

  bool OnUnconsumedKeyboardEventAck(int native_code);
  void FallbackCursorModeLockCursor(bool left, bool right, bool up, bool down);
  void FallbackCursorModeSetCursorVisibility(bool visible);

 protected:
  void RemoveAllChildren(bool attached_to_window);

  ViewAndroid* parent_;

 private:
  FRIEND_TEST_ALL_PREFIXES(ViewAndroidBoundsTest, MatchesViewInFront);
  FRIEND_TEST_ALL_PREFIXES(ViewAndroidBoundsTest, MatchesViewArea);
  FRIEND_TEST_ALL_PREFIXES(ViewAndroidBoundsTest, MatchesViewAfterMove);
  FRIEND_TEST_ALL_PREFIXES(ViewAndroidBoundsTest,
                           MatchesViewSizeOfkMatchParent);
  FRIEND_TEST_ALL_PREFIXES(ViewAndroidBoundsTest, MatchesViewsWithOffset);
  FRIEND_TEST_ALL_PREFIXES(ViewAndroidBoundsTest, OnSizeChanged);
  friend class EventForwarder;
  friend class ViewAndroidBoundsTest;

  bool OnDragEvent(const DragEventAndroid& event);
  bool OnTouchEvent(const MotionEventAndroid& event);
  bool OnMouseEvent(const MotionEventAndroid& event);
  bool OnMouseWheelEvent(const MotionEventAndroid& event);
  bool OnGestureEvent(const GestureEventAndroid& event);
  bool OnGenericMotionEvent(const MotionEventAndroid& event);
  bool OnKeyUp(const KeyEventAndroid& event);
  bool DispatchKeyEvent(const KeyEventAndroid& event);
  bool ScrollBy(float delta_x, float delta_y);
  bool ScrollTo(float x, float y);

  void RemoveChild(ViewAndroid* child);

  void OnAttachedToWindow();
  void OnDetachedFromWindow();

  void SetLayoutForTesting(int x, int y, int width, int height);

  template <typename E>
  using EventHandlerCallback =
      const base::RepeatingCallback<bool(EventHandlerAndroid*, const E&)>;
  template <typename E>
  bool HitTest(EventHandlerCallback<E> handler_callback,
               const E& event,
               const gfx::PointF& point);

  static bool SendDragEventToHandler(EventHandlerAndroid* handler,
                                     const DragEventAndroid& event);
  static bool SendTouchEventToHandler(EventHandlerAndroid* handler,
                                      const MotionEventAndroid& event);
  static bool SendMouseEventToHandler(EventHandlerAndroid* handler,
                                      const MotionEventAndroid& event);
  static bool SendMouseWheelEventToHandler(EventHandlerAndroid* handler,
                                           const MotionEventAndroid& event);
  static bool SendGestureEventToHandler(EventHandlerAndroid* handler,
                                        const GestureEventAndroid& event);

  bool has_event_forwarder() const { return !!event_forwarder_; }

  bool match_parent() const { return layout_type_ == LayoutType::MATCH_PARENT; }

  // Checks if there is any event forwarder in any node up to root.
  static bool RootPathHasEventForwarder(ViewAndroid* view);

  // Checks if there is any event forwarder in the node paths down to
  // each leaf of subtree.
  static bool SubtreeHasEventForwarder(ViewAndroid* view);

  void OnSizeChangedInternal(const gfx::Size& size);
  void DispatchOnSizeChanged();

  bool HasTouchlessEventHandler();

  // Returns the Java delegate for this view. This is used to delegate work
  // up to the embedding view (or the embedder that can deal with the
  // implementation details).
  const base::android::ScopedJavaLocalRef<jobject> GetViewAndroidDelegate()
      const;

  std::list<ViewAndroid*> children_;
  base::ObserverList<ViewAndroidObserver>::Unchecked observer_list_;
  scoped_refptr<cc::Layer> layer_;
  JavaObjectWeakGlobalRef delegate_;

  EventHandlerAndroid* event_handler_ = nullptr;  // Not owned

  // Basic view layout information. Used to do hit testing deciding whether
  // the passed events should be processed by the view. Unit in DIP.
  gfx::Rect bounds_;
  const LayoutType layout_type_;

  // In physical pixel.
  gfx::Size physical_size_;

  FrameInfo frame_info_;

  std::unique_ptr<EventForwarder> event_forwarder_;

  // Copy output of View rather than window.
  CopyViewCallback copy_view_callback_;

  DISALLOW_COPY_AND_ASSIGN(ViewAndroid);
};

}  // namespace ui

#endif  // UI_ANDROID_VIEW_ANDROID_H_
