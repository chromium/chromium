// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/snapshot/snapshot.h"

#include <stddef.h>
#include <stdint.h>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/compositor/test/test_context_factories.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/image/image.h"
#include "ui/gl/gl_implementation.h"

namespace ui {
namespace {

SkColor GetExpectedColorForPoint(int x, int y) {
  return SkColorSetRGB(std::min(x, 255), std::min(y, 255), 0);
}

// Paint simple rectangle on the specified aura window.
class TestPaintingWindowDelegate : public aura::test::TestWindowDelegate {
 public:
  explicit TestPaintingWindowDelegate(const gfx::Size& window_size)
      : window_size_(window_size) {
  }

  TestPaintingWindowDelegate(const TestPaintingWindowDelegate&) = delete;
  TestPaintingWindowDelegate& operator=(const TestPaintingWindowDelegate&) =
      delete;

  ~TestPaintingWindowDelegate() override {}

  void OnPaint(const ui::PaintContext& context) override {
    ui::PaintRecorder recorder(context, window_size_);
    for (int y = 0; y < window_size_.height(); ++y) {
      for (int x = 0; x < window_size_.width(); ++x) {
        recorder.canvas()->FillRect(gfx::Rect(x, y, 1, 1),
                                    GetExpectedColorForPoint(x, y));
      }
    }
  }

 private:
  gfx::Size window_size_;
};

size_t GetFailedPixelsCountWithScaleFactor(const gfx::Image& image,
                                           int scale_factor) {
  const SkBitmap* bitmap = image.ToSkBitmap();
  uint32_t* bitmap_data =
      reinterpret_cast<uint32_t*>(bitmap->pixelRef()->pixels());
  size_t result = 0;
  for (int y = 0; y < bitmap->height(); y += scale_factor) {
    for (int x = 0; x < bitmap->width(); x += scale_factor) {
      if (static_cast<SkColor>(bitmap_data[x + y * bitmap->width()]) !=
          GetExpectedColorForPoint(x / scale_factor, y / scale_factor)) {
        ++result;
      }
    }
  }
  return result;
}

size_t GetFailedPixelsCount(const gfx::Image& image) {
  return GetFailedPixelsCountWithScaleFactor(image, 1);
}

}  // namespace

class SnapshotAuraTest : public testing::Test {
 public:
  SnapshotAuraTest() = default;

  SnapshotAuraTest(const SnapshotAuraTest&) = delete;
  SnapshotAuraTest& operator=(const SnapshotAuraTest&) = delete;

  ~SnapshotAuraTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();

    task_environment_ = std::make_unique<base::test::TaskEnvironment>(
        base::test::TaskEnvironment::MainThreadType::UI);

    // The ContextFactory must exist before any Compositors are created.
    // Snapshot test tests real drawing and readback, so needs pixel output.
    const bool enable_pixel_output = true;
    context_factories_ =
        std::make_unique<ui::TestContextFactories>(enable_pixel_output);

    helper_ = std::make_unique<aura::test::AuraTestHelper>(
        context_factories_->GetContextFactory());
    helper_->SetUp();
  }

  void TearDown() override {
    test_window_.reset();
    delegate_.reset();
    helper_->RunAllPendingInMessageLoop();
    helper_.reset();
    context_factories_.reset();
    task_environment_.reset();
    testing::Test::TearDown();
  }

 protected:
  aura::Window* test_window() { return test_window_.get(); }
  aura::Window* root_window() { return helper_->GetContext(); }
  aura::TestScreen* test_screen() { return helper_->GetTestScreen(); }

  void WaitForDraw() {
    helper_->GetHost()->compositor()->ScheduleDraw();
    ui::DrawWaiterForTest::WaitForCompositingEnded(
        helper_->GetHost()->compositor());
  }

  void SetupTestWindow(const gfx::Rect& window_bounds) {
    delegate_ =
        std::make_unique<TestPaintingWindowDelegate>(window_bounds.size());
    test_window_.reset(aura::test::CreateTestWindowWithDelegate(
        delegate_.get(), 0, window_bounds, root_window()));
  }

  gfx::Image GrabSnapshotForTestWindow() {
    gfx::Rect source_rect(test_window_->bounds().size());
    aura::Window::ConvertRectToTarget(
        test_window(), root_window(), &source_rect);

    scoped_refptr<SnapshotHolder> holder(new SnapshotHolder);
    ui::GrabWindowSnapshot(
        root_window(), source_rect,
        base::BindOnce(&SnapshotHolder::SnapshotCallback, holder));

    holder->WaitForSnapshot();
    DCHECK(holder->completed());
    return holder->image();
  }

 private:
  class SnapshotHolder : public base::RefCountedThreadSafe<SnapshotHolder> {
   public:
    SnapshotHolder() : completed_(false) {}

    void SnapshotCallback(gfx::Image image) {
      DCHECK(!completed_);
      image_ = image;
      completed_ = true;
      run_loop_.Quit();
    }
    void WaitForSnapshot() { run_loop_.Run(); }
    bool completed() const { return completed_; }
    const gfx::Image& image() const { return image_; }

   private:
    friend class base::RefCountedThreadSafe<SnapshotHolder>;

    virtual ~SnapshotHolder() {}

    base::RunLoop run_loop_;
    gfx::Image image_;
    bool completed_;
  };

  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
  std::unique_ptr<ui::TestContextFactories> context_factories_;
  std::unique_ptr<aura::test::AuraTestHelper> helper_;
  std::unique_ptr<aura::Window> test_window_;
  std::unique_ptr<TestPaintingWindowDelegate> delegate_;
  std::vector<unsigned char> png_representation_;
};

#if BUILDFLAG(IS_WIN) && !defined(NDEBUG)
// https://crbug.com/852512
#define MAYBE_FullScreenWindow DISABLED_FullScreenWindow
#elif BUILDFLAG(IS_LINUX)
// https://crbug.com/1143031
#define MAYBE_FullScreenWindow DISABLED_FullScreenWindow
#else
#define MAYBE_FullScreenWindow FullScreenWindow
#endif
TEST_F(SnapshotAuraTest, MAYBE_FullScreenWindow) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
  // TODO(crbug.com/40154923): Fix this test to run in < action_timeout()
  // on the Linux Debug & TSAN bots.
  const base::test::ScopedRunLoopTimeout increased_run_timeout(
      FROM_HERE, TestTimeouts::action_max_timeout());
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/41393300): Make work on Windows.
  if (::testing::internal::AlwaysTrue()) {
    GTEST_SKIP();
  }
#endif
  SetupTestWindow(root_window()->bounds());
  WaitForDraw();

  gfx::Image snapshot = GrabSnapshotForTestWindow();
  EXPECT_EQ(test_window()->bounds().size().ToString(),
            snapshot.Size().ToString());
  EXPECT_EQ(0u, GetFailedPixelsCount(snapshot));
}

TEST_F(SnapshotAuraTest, PartialBounds) {
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/41393300): Make work on Windows.
  if (::testing::internal::AlwaysTrue()) {
    GTEST_SKIP();
  }
#endif
  gfx::Rect test_bounds(100, 100, 300, 200);
  SetupTestWindow(test_bounds);
  WaitForDraw();

  gfx::Image snapshot = GrabSnapshotForTestWindow();
  EXPECT_EQ(test_bounds.size().ToString(), snapshot.Size().ToString());
  EXPECT_EQ(0u, GetFailedPixelsCount(snapshot));
}

TEST_F(SnapshotAuraTest, Rotated) {
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/41393300): Make work on Windows.
  if (::testing::internal::AlwaysTrue()) {
    GTEST_SKIP();
  }
#endif
  test_screen()->SetDisplayRotation(display::Display::ROTATE_90);

  gfx::Rect test_bounds(100, 100, 300, 200);
  SetupTestWindow(test_bounds);
  WaitForDraw();

  gfx::Image snapshot = GrabSnapshotForTestWindow();
  EXPECT_EQ(test_bounds.size().ToString(), snapshot.Size().ToString());
  EXPECT_EQ(0u, GetFailedPixelsCount(snapshot));
}

TEST_F(SnapshotAuraTest, UIScale) {
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/41393300): Make work on Windows.
  if (::testing::internal::AlwaysTrue()) {
    GTEST_SKIP();
  }
#endif
  const float kUIScale = 0.5f;
  test_screen()->SetUIScale(kUIScale);

  gfx::Rect test_bounds(100, 100, 300, 200);
  SetupTestWindow(test_bounds);
  WaitForDraw();

  // Snapshot always captures the physical pixels.
  gfx::SizeF snapshot_size(test_bounds.size());
  snapshot_size.InvScale(kUIScale);

  gfx::Image snapshot = GrabSnapshotForTestWindow();
  EXPECT_EQ(gfx::ToRoundedSize(snapshot_size).ToString(),
            snapshot.Size().ToString());
  EXPECT_EQ(0u, GetFailedPixelsCountWithScaleFactor(snapshot, 1 / kUIScale));
}

TEST_F(SnapshotAuraTest, DeviceScaleFactor) {
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/41393300): Make work on Windows.
  if (::testing::internal::AlwaysTrue()) {
    GTEST_SKIP();
  }
#endif
  test_screen()->SetDeviceScaleFactor(2.0f);

  gfx::Rect test_bounds(100, 100, 150, 100);
  SetupTestWindow(test_bounds);
  WaitForDraw();

  // Snapshot always captures the physical pixels.
  gfx::SizeF snapshot_size(test_bounds.size());
  snapshot_size.Scale(2.0f);

  gfx::Image snapshot = GrabSnapshotForTestWindow();
  EXPECT_EQ(gfx::ToRoundedSize(snapshot_size).ToString(),
            snapshot.Size().ToString());
  EXPECT_EQ(0u, GetFailedPixelsCountWithScaleFactor(snapshot, 2));
}

TEST_F(SnapshotAuraTest, RotateAndUIScale) {
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/41393300): Make work on Windows.
  if (::testing::internal::AlwaysTrue()) {
    GTEST_SKIP();
  }
#endif
  const float kUIScale = 0.5f;
  test_screen()->SetUIScale(kUIScale);
  test_screen()->SetDisplayRotation(display::Display::ROTATE_90);

  gfx::Rect test_bounds(100, 100, 200, 300);
  SetupTestWindow(test_bounds);
  WaitForDraw();

  // Snapshot always captures the physical pixels.
  gfx::SizeF snapshot_size(test_bounds.size());
  snapshot_size.InvScale(kUIScale);

  gfx::Image snapshot = GrabSnapshotForTestWindow();
  EXPECT_EQ(gfx::ToRoundedSize(snapshot_size).ToString(),
            snapshot.Size().ToString());
  EXPECT_EQ(0u, GetFailedPixelsCountWithScaleFactor(snapshot, 1 / kUIScale));
}

TEST_F(SnapshotAuraTest, RotateAndUIScaleAndScaleFactor) {
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/41393300): Make work on Windows.
  if (::testing::internal::AlwaysTrue()) {
    GTEST_SKIP();
  }
#endif
  test_screen()->SetDeviceScaleFactor(2.0f);
  const float kUIScale = 0.5f;
  test_screen()->SetUIScale(kUIScale);
  test_screen()->SetDisplayRotation(display::Display::ROTATE_90);

  gfx::Rect test_bounds(20, 30, 100, 150);
  SetupTestWindow(test_bounds);
  WaitForDraw();

  // Snapshot always captures the physical pixels.
  gfx::SizeF snapshot_size(test_bounds.size());
  snapshot_size.Scale(2.0f / kUIScale);

  gfx::Image snapshot = GrabSnapshotForTestWindow();
  EXPECT_EQ(gfx::ToRoundedSize(snapshot_size).ToString(),
            snapshot.Size().ToString());
  EXPECT_EQ(0u, GetFailedPixelsCountWithScaleFactor(snapshot, 2 / kUIScale));
}

}  // namespace ui
