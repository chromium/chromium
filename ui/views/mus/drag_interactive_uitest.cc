// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "services/ws/public/mojom/constants.mojom.h"
#include "services/ws/public/mojom/event_injector.mojom.h"
#include "services/ws/public/mojom/window_server_test.mojom.h"
#include "services/ws/public/mojom/window_tree_constants.mojom.h"
#include "ui/aura/mus/in_flight_change.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/test/mus/change_completion_waiter.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace test {
namespace {

// A view used in DragTestInteractive.DragTest as a drag source.
class DraggableView : public views::View {
 public:
  DraggableView() {}
  ~DraggableView() override {}

  // views::View overrides:
  int GetDragOperations(const gfx::Point& press_pt) override {
    return ui::DragDropTypes::DRAG_MOVE;
  }
  void WriteDragData(const gfx::Point& press_pt,
                     OSExchangeData* data) override {
    data->SetString(base::UTF8ToUTF16("test"));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DraggableView);
};

// A view used in DragTestInteractive.DragTest as a drop target.
class TargetView : public views::View {
 public:
  TargetView() : dropped_(false) {}
  ~TargetView() override {}

  void WaitForDropped(base::Closure quit_closure) {
    if (dropped_) {
      quit_closure.Run();
      return;
    }

    quit_closure_ = quit_closure;
  }

  // views::View overrides:
  bool GetDropFormats(
      int* formats,
      std::set<ui::Clipboard::FormatType>* format_types) override {
    *formats = ui::OSExchangeData::STRING;
    return true;
  }
  bool AreDropTypesRequired() override { return false; }
  bool CanDrop(const OSExchangeData& data) override { return true; }
  int OnDragUpdated(const ui::DropTargetEvent& event) override {
    return ui::DragDropTypes::DRAG_MOVE;
  }
  int OnPerformDrop(const ui::DropTargetEvent& event) override {
    dropped_ = true;
    if (quit_closure_)
      quit_closure_.Run();
    return ui::DragDropTypes::DRAG_MOVE;
  }

  bool dropped() const { return dropped_; }

 private:
  bool dropped_;

  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(TargetView);
};

std::unique_ptr<ui::MouseEvent> CreateMouseMoveEvent(int x, int y) {
  return std::make_unique<ui::MouseEvent>(
      ui::ET_MOUSE_MOVED, gfx::Point(x, y), gfx::Point(x, y),
      ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_NONE);
}

std::unique_ptr<ui::MouseEvent> CreateMouseDownEvent(int x, int y) {
  return std::make_unique<ui::MouseEvent>(
      ui::ET_MOUSE_PRESSED, gfx::Point(x, y), gfx::Point(x, y),
      ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
      ui::EF_LEFT_MOUSE_BUTTON);
}

std::unique_ptr<ui::MouseEvent> CreateMouseUpEvent(int x, int y) {
  return std::make_unique<ui::MouseEvent>(
      ui::ET_MOUSE_RELEASED, gfx::Point(x, y), gfx::Point(x, y),
      ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
      ui::EF_LEFT_MOUSE_BUTTON);
}

}  // namespace

using DragTestInteractive = WidgetTest;

// Dispatch of events is asynchronous so most of DragTestInteractive.DragTest
// consists of callback functions which will perform an action after the
// previous action has completed.
void DragTest_Part3(int64_t display_id,
                    const base::Closure& quit_closure,
                    bool result) {
  EXPECT_TRUE(result);
  quit_closure.Run();
}

void DragTest_Part2(ws::mojom::EventInjector* event_injector,
                    int64_t display_id,
                    const base::Closure& quit_closure,
                    bool result) {
  EXPECT_TRUE(result);
  if (!result)
    quit_closure.Run();

  event_injector->InjectEvent(
      display_id, CreateMouseUpEvent(30, 30),
      base::BindOnce(&DragTest_Part3, display_id, quit_closure));
}

void DragTest_Part1(ws::mojom::EventInjector* event_injector,
                    int64_t display_id,
                    const base::Closure& quit_closure,
                    bool result) {
  EXPECT_TRUE(result);
  if (!result)
    quit_closure.Run();

  event_injector->InjectEvent(
      display_id, CreateMouseMoveEvent(30, 30),
      base::BindOnce(&DragTest_Part2, base::Unretained(event_injector),
                     display_id, quit_closure));
}

TEST_F(DragTestInteractive, DragTest) {
  ws::mojom::EventInjectorPtr event_injector;
  MusClient::Get()->window_tree_client()->connector()->BindInterface(
      ws::mojom::kServiceName, &event_injector);
  Widget* source_widget = CreateTopLevelFramelessPlatformWidget();
  View* source_view = new DraggableView;
  source_widget->SetContentsView(source_view);
  source_widget->Show();

  aura::test::ChangeCompletionWaiter source_waiter(aura::ChangeType::BOUNDS,
                                                   false);
  source_widget->SetBounds(gfx::Rect(0, 0, 20, 20));
  ASSERT_TRUE(source_waiter.Wait());

  Widget* target_widget = CreateTopLevelFramelessPlatformWidget();
  TargetView* target_view = new TargetView;
  target_widget->SetContentsView(target_view);
  target_widget->Show();

  aura::test::ChangeCompletionWaiter target_waiter(aura::ChangeType::BOUNDS,
                                                   false);
  target_widget->SetBounds(gfx::Rect(20, 20, 20, 20));
  ASSERT_TRUE(target_waiter.Wait());

  auto* dnwa =
      static_cast<DesktopNativeWidgetAura*>(source_widget->native_widget());
  ASSERT_TRUE(dnwa);
  auto* wth = static_cast<aura::WindowTreeHostMus*>(dnwa->host());
  ASSERT_TRUE(wth);
  int64_t display_id = wth->display_id();

  {
    base::RunLoop run_loop;
    event_injector->InjectEvent(
        display_id, CreateMouseDownEvent(10, 10),
        base::BindOnce(&DragTest_Part1, base::Unretained(event_injector.get()),
                       display_id, run_loop.QuitClosure()));

    run_loop.Run();
  }

  // Wait for the event dispatch to cause the final drop signal.
  {
    base::RunLoop run_loop;
    target_view->WaitForDropped(run_loop.QuitClosure());
    run_loop.Run();
  }

  EXPECT_TRUE(target_view->dropped());

  target_widget->Close();
  source_widget->Close();
  RunPendingMessages();
}

}  // namespace test
}  // namespace views
