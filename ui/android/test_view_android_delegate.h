// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_TEST_VIEW_ANDROID_DELEGATE_H_
#define UI_ANDROID_TEST_VIEW_ANDROID_DELEGATE_H_

#include "base/android/scoped_java_ref.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

class ViewAndroid;

// Provides a test version of the ViewAndroidDelegate with a native interface
// that connects to the Java TestViewAndroidDelegate, used to test insetting on
// an RWHVA.
class TestViewAndroidDelegate {
 public:
  TestViewAndroidDelegate();

  TestViewAndroidDelegate(const TestViewAndroidDelegate&) = delete;
  TestViewAndroidDelegate& operator=(const TestViewAndroidDelegate&) = delete;

  ~TestViewAndroidDelegate();
  // Sets up the test delegate.
  // |view_android| is the ViewAndroid to use.
  // This setup must be called before calling any other methods on this
  // delegate.
  void SetupTestDelegate(ui::ViewAndroid* view_android);
  // Insets the Visual Viewport bottom. |SetupTestDelegate| must be called
  // first.
  void InsetViewportBottom(int bottom);

  int GetLastCursorType() const;
  gfx::Size GetLastCustomCursorSize() const;
  gfx::Point GetLastCustomCursorHotspot() const;

  // Sets the container view coordinates. `SetupTestDelegate()` must be called
  // first, and this must not be called after `ClearContainerView()`.
  void SetContainerLocation(const gfx::Point& window_location,
                            const gfx::Point& screen_location);

  // Clears the container view (detaching the underlying test view).
  // `SetupTestDelegate()` must be called first.
  void ClearContainerView();

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_delegate_;
};

}  // namespace ui

#endif  // UI_ANDROID_TEST_VIEW_ANDROID_DELEGATE_H_
