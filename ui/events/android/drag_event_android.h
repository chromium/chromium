// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ANDROID_DRAG_EVENT_ANDROID_H_
#define UI_EVENTS_ANDROID_DRAG_EVENT_ANDROID_H_

#include <jni.h>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ref.h"
#include "ui/events/events_export.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace gfx {
class PointF;
}

namespace ui {

// Event class used to carry the info from Java DragEvent through native.
// All coordinates are in DIPs.
class EVENTS_EXPORT DragEventAndroid {
 public:
  DragEventAndroid(JNIEnv* env,
                   int action,
                   const gfx::PointF& location,
                   const gfx::PointF& screen_location,
                   const std::vector<std::u16string>& mime_types,
                   jstring content);

  DragEventAndroid(const DragEventAndroid&) = delete;
  DragEventAndroid& operator=(const DragEventAndroid&) = delete;

  ~DragEventAndroid();

  int action() const { return action_; }
  const gfx::PointF& location() const { return location_; }
  const gfx::PointF& screen_location() const { return screen_location_; }
  const std::vector<std::u16string>& mime_types() const { return *mime_types_; }

  base::android::ScopedJavaLocalRef<jstring> GetJavaContent() const;

  // Creates a new DragEventAndroid instance different from |this| only by
  // its location.
  std::unique_ptr<DragEventAndroid> CreateFor(
      const gfx::PointF& new_location) const;

 private:
  int action_;
  // Location relative to the view which the event is targeted.
  gfx::PointF location_;
  // Location relative to the screen coordinate.
  gfx::PointF screen_location_;
  const raw_ref<const std::vector<std::u16string>> mime_types_;
  // The Java reference to the drop content to avoid unnecessary copying.
  base::android::ScopedJavaGlobalRef<jstring> content_;
};

}  // namespace ui

#endif  // UI_EVENTS_ANDROID_DRAG_EVENT_ANDROID_H_
