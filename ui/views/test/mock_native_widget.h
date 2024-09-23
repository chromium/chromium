//, Copyright 2023 The Chromium Authors
//, Use of this source code is governed by a BSD-style license that can be
//, found in the LICENSE file.

#ifndef UI_VIEWS_TEST_MOCK_NATIVE_WIDGET_H_
#define UI_VIEWS_TEST_MOCK_NATIVE_WIDGET_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/views/widget/native_widget_private.h"

namespace views {

class MockNativeWidget : public internal::NativeWidgetPrivate {
 public:
  explicit MockNativeWidget(Widget*);
  ~MockNativeWidget() override;

  MOCK_METHOD(void, InitNativeWidget, (Widget::InitParams), (override));
  MOCK_METHOD(void, OnWidgetInitDone, (), (override));

  MOCK_METHOD(void, ReparentNativeViewImpl, (gfx::NativeView), (override));

  MOCK_METHOD(std::unique_ptr<NonClientFrameView>,
              CreateNonClientFrameView,
              (),
              (override));

  MOCK_METHOD(bool, ShouldUseNativeFrame, (), (const override));
  MOCK_METHOD(bool, ShouldWindowContentsBeTransparent, (), (const override));
  MOCK_METHOD(void, FrameTypeChanged, (), (override));
  MOCK_METHOD(Widget*, GetWidget, (), (override));
  MOCK_METHOD(const Widget*, GetWidget, (), (const override));
  MOCK_METHOD(gfx::NativeView, GetNativeView, (), (const override));
  MOCK_METHOD(gfx::NativeWindow, GetNativeWindow, (), (const override));
  MOCK_METHOD(Widget*, GetTopLevelWidget, (), (override));
  MOCK_METHOD(const ui::Compositor*, GetCompositor, (), (const override));
  MOCK_METHOD(const ui::Layer*, GetLayer, (), (const override));
  MOCK_METHOD(void, ReorderNativeViews, (), (override));
  MOCK_METHOD(void, ViewRemoved, (View * view), (override));
  MOCK_METHOD(void,
              SetNativeWindowProperty,
              (const char* name, void* value),
              (override));
  MOCK_METHOD(void*,
              GetNativeWindowProperty,
              (const char* name),
              (const override));
  MOCK_METHOD(TooltipManager*, GetTooltipManager, (), (const override));
  MOCK_METHOD(void, SetCapture, (), (override));
  MOCK_METHOD(void, ReleaseCapture, (), (override));
  MOCK_METHOD(bool, HasCapture, (), (const override));
  MOCK_METHOD(ui::InputMethod*, GetInputMethod, (), (override));
  MOCK_METHOD(void, CenterWindow, (const gfx::Size& size), (override));
  MOCK_METHOD(void,
              GetWindowPlacement,
              (gfx::Rect * bounds, ui::mojom::WindowShowState* show_state),
              (const override));
  MOCK_METHOD(bool, SetWindowTitle, (const std::u16string& title), (override));
  MOCK_METHOD(void,
              SetWindowIcons,
              (const gfx::ImageSkia& window_icon,
               const gfx::ImageSkia& app_icon),
              (override));
  MOCK_METHOD(const gfx::ImageSkia*, GetWindowIcon, (), (override));
  MOCK_METHOD(const gfx::ImageSkia*, GetWindowAppIcon, (), (override));
  MOCK_METHOD(void,
              InitModalType,
              (ui::mojom::ModalType modal_type),
              (override));
  MOCK_METHOD(gfx::Rect, GetWindowBoundsInScreen, (), (const override));
  MOCK_METHOD(gfx::Rect, GetClientAreaBoundsInScreen, (), (const override));
  MOCK_METHOD(gfx::Rect, GetRestoredBounds, (), (const override));
  MOCK_METHOD(std::string, GetWorkspace, (), (const override));
  MOCK_METHOD(void, SetBounds, (const gfx::Rect& bounds), (override));
  MOCK_METHOD(void,
              SetBoundsConstrained,
              (const gfx::Rect& bounds),
              (override));
  MOCK_METHOD(void, SetSize, (const gfx::Size& size), (override));
  MOCK_METHOD(void, StackAbove, (gfx::NativeView native_view), (override));
  MOCK_METHOD(void, StackAtTop, (), (override));
  MOCK_METHOD(bool, IsStackedAbove, (gfx::NativeView native_view), (override));
  MOCK_METHOD(void,
              SetShape,
              (std::unique_ptr<Widget::ShapeRects> shape),
              (override));
  MOCK_METHOD(void, Close, (), (override));
  MOCK_METHOD(void, CloseNow, (), (override));
  MOCK_METHOD(void,
              Show,
              (ui::mojom::WindowShowState show_state,
               const gfx::Rect& restore_bounds),
              (override));
  MOCK_METHOD(void, Hide, (), (override));
  MOCK_METHOD(bool, IsVisible, (), (const override));
  MOCK_METHOD(void, Activate, (), (override));
  MOCK_METHOD(void, Deactivate, (), (override));
  MOCK_METHOD(bool, IsActive, (), (const override));
  MOCK_METHOD(void, SetZOrderLevel, (ui::ZOrderLevel order), (override));
  MOCK_METHOD(ui::ZOrderLevel, GetZOrderLevel, (), (const override));
  MOCK_METHOD(void,
              SetVisibleOnAllWorkspaces,
              (bool always_visible),
              (override));
  MOCK_METHOD(bool, IsVisibleOnAllWorkspaces, (), (const override));
  MOCK_METHOD(void, Maximize, (), (override));
  MOCK_METHOD(void, Minimize, (), (override));
  MOCK_METHOD(bool, IsMaximized, (), (const override));
  MOCK_METHOD(bool, IsMinimized, (), (const override));
  MOCK_METHOD(void, Restore, (), (override));
  MOCK_METHOD(void,
              SetFullscreen,
              (bool fullscreen, int64_t target_display_id),
              (override));
  MOCK_METHOD(bool, IsFullscreen, (), (const override));
  MOCK_METHOD(void,
              SetCanAppearInExistingFullscreenSpaces,
              (bool can_appear_in_existing_fullscreen_spaces),
              (override));
  MOCK_METHOD(void, SetOpacity, (float opacity), (override));
  MOCK_METHOD(void,
              SetAspectRatio,
              (const gfx::SizeF& aspect_ratio,
               const gfx::Size& excluded_margin),
              (override));
  MOCK_METHOD(void, FlashFrame, (bool flash), (override));
  MOCK_METHOD(void,
              RunShellDrag,
              (std::unique_ptr<ui::OSExchangeData> data,
               const gfx::Point& location,
               int operation,
               ui::mojom::DragEventSource source),
              (override));
  MOCK_METHOD(void, CancelShellDrag, (View * view), (override));
  MOCK_METHOD(void, SchedulePaintInRect, (const gfx::Rect& rect), (override));
  MOCK_METHOD(void, ScheduleLayout, (), (override));
  MOCK_METHOD(void, SetCursor, (const ui::Cursor& cursor), (override));
  MOCK_METHOD(void, ShowEmojiPanel, (), (override));
  MOCK_METHOD(bool, IsMouseEventsEnabled, (), (const override));
  MOCK_METHOD(bool, IsMouseButtonDown, (), (const override));
  MOCK_METHOD(void, ClearNativeFocus, (), (override));
  MOCK_METHOD(gfx::Rect, GetWorkAreaBoundsInScreen, (), (const override));
  MOCK_METHOD(bool, IsMoveLoopSupported, (), (const override));
  MOCK_METHOD(Widget::MoveLoopResult,
              RunMoveLoop,
              (const gfx::Vector2d& drag_offset,
               Widget::MoveLoopSource source,
               Widget::MoveLoopEscapeBehavior escape_behavior),
              (override));
  MOCK_METHOD(void, EndMoveLoop, (), (override));
  MOCK_METHOD(void,
              SetVisibilityChangedAnimationsEnabled,
              (bool value),
              (override));
  MOCK_METHOD(void,
              SetVisibilityAnimationDuration,
              (const base::TimeDelta& duration),
              (override));
  MOCK_METHOD(void,
              SetVisibilityAnimationTransition,
              (Widget::VisibilityTransition transition),
              (override));
  MOCK_METHOD(ui::GestureRecognizer*, GetGestureRecognizer, (), (override));
  MOCK_METHOD(ui::GestureConsumer*, GetGestureConsumer, (), (override));
  MOCK_METHOD(void, OnSizeConstraintsChanged, (), (override));
  MOCK_METHOD(void, OnNativeViewHierarchyWillChange, (), (override));
  MOCK_METHOD(void, OnNativeViewHierarchyChanged, (), (override));
  MOCK_METHOD(bool, SetAllowScreenshots, (bool allow), (override));
  MOCK_METHOD(bool, AreScreenshotsAllowed, (), (override));
  MOCK_METHOD(std::string, GetName, (), (const override));

  base::WeakPtr<NativeWidgetPrivate> GetWeakPtr() override;

 private:
  raw_ptr<Widget> widget_;
  base::WeakPtrFactory<MockNativeWidget> weak_factory_{this};
};

}  // namespace views

#endif  // UI_VIEWS_TEST_MOCK_NATIVE_WIDGET_H_
