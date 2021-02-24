// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_drag_drop_client.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_loader.h"
#include "ui/base/cursor/mojom/cursor_type.mojom.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/x/x11_cursor.h"
#include "ui/base/x/x11_move_loop.h"
#include "ui/base/x/x11_move_loop_delegate.h"
#include "ui/base/x/x11_os_exchange_data_provider.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xproto_util.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

class TestDragDropClient;

// Collects messages which would otherwise be sent to |window_| via
// SendXClientEvent().
class ClientMessageEventCollector {
 public:
  ClientMessageEventCollector(x11::Window window, TestDragDropClient* client);
  virtual ~ClientMessageEventCollector();

  // Returns true if |events_| is non-empty.
  bool HasEvents() const { return !events_.empty(); }

  // Pops all of |events_| and returns the popped events in the order that they
  // were on the stack
  std::vector<x11::ClientMessageEvent> PopAllEvents();

  // Adds |event| to the stack.
  void RecordEvent(const x11::ClientMessageEvent& event);

 private:
  x11::Window window_;

  // Not owned.
  TestDragDropClient* client_;

  std::vector<x11::ClientMessageEvent> events_;

  DISALLOW_COPY_AND_ASSIGN(ClientMessageEventCollector);
};

// An implementation of ui::X11MoveLoop where RunMoveLoop() always starts the
// move loop.
class TestMoveLoop : public ui::X11MoveLoop {
 public:
  explicit TestMoveLoop(ui::X11MoveLoopDelegate* delegate);
  ~TestMoveLoop() override;

  // Returns true if the move loop is running.
  bool IsRunning() const;

  // ui::X11MoveLoop:
  bool RunMoveLoop(bool can_grab_pointer,
                   scoped_refptr<ui::X11Cursor> old_cursor,
                   scoped_refptr<ui::X11Cursor> new_cursor) override;
  void UpdateCursor(scoped_refptr<ui::X11Cursor> cursor) override;
  void EndMoveLoop() override;

 private:
  // Not owned.
  ui::X11MoveLoopDelegate* delegate_;

  // Ends the move loop.
  base::OnceClosure quit_closure_;

  bool is_running_ = false;
};

// Implementation of XDragDropClient which short circuits FindWindowFor().
class SimpleTestDragDropClient : public aura::client::DragDropClient,
                                 public ui::XDragDropClient,
                                 public ui::XDragDropClient::Delegate,
                                 public ui::X11MoveLoopDelegate {
 public:
  explicit SimpleTestDragDropClient(aura::Window*);
  ~SimpleTestDragDropClient() override;

  // Sets |window| as the topmost window for all mouse positions.
  void SetTopmostXWindow(x11::Window window);

  // Returns true if the move loop is running.
  bool IsMoveLoopRunning();

  // aura::client::DragDropClient:
  int StartDragAndDrop(std::unique_ptr<ui::OSExchangeData> data,
                       aura::Window* root_window,
                       aura::Window* source_window,
                       const gfx::Point& screen_location,
                       int operation,
                       ui::mojom::DragEventSource source) override;
  void DragCancel() override;
  bool IsDragDropInProgress() override;
  void AddObserver(aura::client::DragDropClientObserver* observer) override;
  void RemoveObserver(aura::client::DragDropClientObserver* observer) override;

 private:
  // ui::XDragDropClient::Delegate:
  std::unique_ptr<ui::XTopmostWindowFinder> CreateWindowFinder() override;
  int UpdateDrag(const gfx::Point& screen_point) override;
  void UpdateCursor(
      ui::DragDropTypes::DragOperation negotiated_operation) override;
  void OnBeginForeignDrag(x11::Window window) override;
  void OnEndForeignDrag() override;
  void OnBeforeDragLeave() override;
  int PerformDrop() override;
  void EndDragLoop() override;

  // XDragDropClient:
  x11::Window FindWindowFor(const gfx::Point& screen_point) override;

  // ui::X11MoveLoopDelegate:
  void OnMouseMovement(const gfx::Point& screen_point,
                       int flags,
                       base::TimeTicks event_time) override;
  void OnMouseReleased() override;
  void OnMoveLoopEnded() override;

  std::unique_ptr<ui::X11MoveLoop> CreateMoveLoop(
      ui::X11MoveLoopDelegate* delegate);

  // The x11::Window of the window which is simulated to be the topmost window.
  x11::Window target_window_ = x11::Window::None;

  // The move loop. Not owned.
  TestMoveLoop* loop_ = nullptr;

  base::OnceClosure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(SimpleTestDragDropClient);
};

// Implementation of XDragDropClient which works with a fake
// |XDragDropClient::source_current_window_|.
class TestDragDropClient : public SimpleTestDragDropClient {
 public:
  // The location in screen coordinates used for the synthetic mouse moves
  // generated in SetTopmostXWindowAndMoveMouse().
  static constexpr int kMouseMoveX = 100;
  static constexpr int kMouseMoveY = 200;

  explicit TestDragDropClient(aura::Window* window);
  ~TestDragDropClient() override;

  // Returns the x11::Window of the window which initiated the drag.
  x11::Window source_xwindow() { return source_window_; }

  // Returns the Atom with |name|.
  x11::Atom GetAtom(const char* name);

  // Returns true if the event's message has |type|.
  bool MessageHasType(const x11::ClientMessageEvent& event, const char* type);

  // Sets |collector| to collect x11::ClientMessageEvents which would otherwise
  // have been sent to the drop target window.
  void SetEventCollectorFor(x11::Window window,
                            ClientMessageEventCollector* collector);

  // Builds an XdndStatus message and sends it to
  // XDragDropClient.
  void OnStatus(x11::Window target_window,
                bool will_accept_drop,
                x11::Atom accepted_action);

  // Builds an XdndFinished message and sends it to
  // XDragDropClient.
  void OnFinished(x11::Window target_window,
                  bool accepted_drop,
                  x11::Atom performed_action);

  // Sets |window| as the topmost window at the current mouse position and
  // generates a synthetic mouse move.
  void SetTopmostXWindowAndMoveMouse(x11::Window window);

 private:
  // XDragDropClient:
  void SendXClientEvent(x11::Window window,
                        const x11::ClientMessageEvent& event) override;

  // The x11::Window of the window which initiated the drag.
  x11::Window source_window_;

  // Map of x11::Windows to the collector which intercepts
  // x11::ClientMessageEvents for that window.
  std::map<x11::Window, ClientMessageEventCollector*> collectors_;

  DISALLOW_COPY_AND_ASSIGN(TestDragDropClient);
};

///////////////////////////////////////////////////////////////////////////////
// ClientMessageEventCollector

ClientMessageEventCollector::ClientMessageEventCollector(
    x11::Window window,
    TestDragDropClient* client)
    : window_(window), client_(client) {
  client->SetEventCollectorFor(window, this);
}

ClientMessageEventCollector::~ClientMessageEventCollector() {
  client_->SetEventCollectorFor(window_, nullptr);
}

std::vector<x11::ClientMessageEvent>
ClientMessageEventCollector::PopAllEvents() {
  std::vector<x11::ClientMessageEvent> to_return;
  to_return.swap(events_);
  return to_return;
}

void ClientMessageEventCollector::RecordEvent(
    const x11::ClientMessageEvent& event) {
  events_.push_back(event);
}

///////////////////////////////////////////////////////////////////////////////
// TestMoveLoop

TestMoveLoop::TestMoveLoop(ui::X11MoveLoopDelegate* delegate)
    : delegate_(delegate) {}

TestMoveLoop::~TestMoveLoop() = default;

bool TestMoveLoop::IsRunning() const {
  return is_running_;
}

bool TestMoveLoop::RunMoveLoop(bool can_grab_pointer,
                               scoped_refptr<ui::X11Cursor> old_cursor,
                               scoped_refptr<ui::X11Cursor> new_cursor) {
  is_running_ = true;
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
  return true;
}

void TestMoveLoop::UpdateCursor(scoped_refptr<ui::X11Cursor> cursor) {}

void TestMoveLoop::EndMoveLoop() {
  if (is_running_) {
    delegate_->OnMoveLoopEnded();
    is_running_ = false;
    std::move(quit_closure_).Run();
  }
}

///////////////////////////////////////////////////////////////////////////////
// SimpleTestDragDropClient

SimpleTestDragDropClient::SimpleTestDragDropClient(aura::Window* window)
    : ui::XDragDropClient(
          this,
          static_cast<x11::Window>(window->GetHost()->GetAcceleratedWidget())) {
}

SimpleTestDragDropClient::~SimpleTestDragDropClient() = default;

void SimpleTestDragDropClient::SetTopmostXWindow(x11::Window window) {
  target_window_ = window;
}

bool SimpleTestDragDropClient::IsMoveLoopRunning() {
  return loop_->IsRunning();
}

std::unique_ptr<ui::X11MoveLoop> SimpleTestDragDropClient::CreateMoveLoop(
    ui::X11MoveLoopDelegate* delegate) {
  loop_ = new TestMoveLoop(delegate);
  return base::WrapUnique(loop_);
}

int SimpleTestDragDropClient::StartDragAndDrop(
    std::unique_ptr<ui::OSExchangeData> data,
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& screen_location,
    int operation,
    ui::mojom::DragEventSource source) {
  InitDrag(operation, data.get());

  auto loop = CreateMoveLoop(this);

  // Windows has a specific method, DoDragDrop(), which performs the entire
  // drag. We have to emulate this, so we spin off a nested runloop which will
  // track all cursor movement and reroute events to a specific handler.
  ui::CursorLoader cursor_loader;
  ui::Cursor grabbing = ui::mojom::CursorType::kGrabbing;
  cursor_loader.SetPlatformCursor(&grabbing);
  auto* last_cursor = static_cast<ui::X11Cursor*>(
      source_window->GetHost()->last_cursor().platform());
  loop_->RunMoveLoop(!source_window->HasCapture(), last_cursor,
                     static_cast<ui::X11Cursor*>(grabbing.platform()));

  auto resulting_operation = negotiated_operation();
  CleanupDrag();
  return resulting_operation;
}

void SimpleTestDragDropClient::DragCancel() {}
bool SimpleTestDragDropClient::IsDragDropInProgress() {
  return false;
}
void SimpleTestDragDropClient::AddObserver(
    aura::client::DragDropClientObserver* observer) {}
void SimpleTestDragDropClient::RemoveObserver(
    aura::client::DragDropClientObserver* observer) {}

int SimpleTestDragDropClient::UpdateDrag(const gfx::Point& screen_point) {
  return 0;
}

std::unique_ptr<ui::XTopmostWindowFinder>
SimpleTestDragDropClient::CreateWindowFinder() {
  return {};
}
void SimpleTestDragDropClient::UpdateCursor(
    ui::DragDropTypes::DragOperation negotiated_operation) {}
void SimpleTestDragDropClient::OnBeginForeignDrag(x11::Window window) {}
void SimpleTestDragDropClient::OnEndForeignDrag() {}
void SimpleTestDragDropClient::OnBeforeDragLeave() {}
int SimpleTestDragDropClient::PerformDrop() {
  return 0;
}
void SimpleTestDragDropClient::EndDragLoop() {
  // std::move(quit_closure_).Run();
  loop_->EndMoveLoop();
}

x11::Window SimpleTestDragDropClient::FindWindowFor(
    const gfx::Point& screen_point) {
  return target_window_;
}

void SimpleTestDragDropClient::OnMouseMovement(const gfx::Point& screen_point,
                                               int flags,
                                               base::TimeTicks event_time) {
  HandleMouseMovement(screen_point, flags, event_time);
}

void SimpleTestDragDropClient::OnMouseReleased() {
  HandleMouseReleased();
}

void SimpleTestDragDropClient::OnMoveLoopEnded() {
  HandleMoveLoopEnded();
}

///////////////////////////////////////////////////////////////////////////////
// TestDragDropClient

TestDragDropClient::TestDragDropClient(aura::Window* window)
    : SimpleTestDragDropClient(window),
      source_window_(
          static_cast<x11::Window>(window->GetHost()->GetAcceleratedWidget())) {
}

TestDragDropClient::~TestDragDropClient() = default;

x11::Atom TestDragDropClient::GetAtom(const char* name) {
  return x11::GetAtom(name);
}

bool TestDragDropClient::MessageHasType(const x11::ClientMessageEvent& event,
                                        const char* type) {
  return event.type == GetAtom(type);
}

void TestDragDropClient::SetEventCollectorFor(
    x11::Window window,
    ClientMessageEventCollector* collector) {
  if (collector)
    collectors_[window] = collector;
  else
    collectors_.erase(window);
}

void TestDragDropClient::OnStatus(x11::Window target_window,
                                  bool will_accept_drop,
                                  x11::Atom accepted_action) {
  x11::ClientMessageEvent event;
  event.type = GetAtom("XdndStatus");
  event.format = 32;
  event.window = source_window_;
  event.data.data32[0] = static_cast<uint32_t>(target_window);
  event.data.data32[1] = will_accept_drop ? 1 : 0;
  event.data.data32[2] = 0;
  event.data.data32[3] = 0;
  event.data.data32[4] = static_cast<uint32_t>(accepted_action);
  HandleXdndEvent(event);
}

void TestDragDropClient::OnFinished(x11::Window target_window,
                                    bool accepted_drop,
                                    x11::Atom performed_action) {
  x11::ClientMessageEvent event;
  event.type = GetAtom("XdndFinished");
  event.format = 32;
  event.window = source_window_;
  event.data.data32[0] = static_cast<uint32_t>(target_window);
  event.data.data32[1] = accepted_drop ? 1 : 0;
  event.data.data32[2] = static_cast<uint32_t>(performed_action);
  event.data.data32[3] = 0;
  event.data.data32[4] = 0;
  HandleXdndEvent(event);
}

void TestDragDropClient::SetTopmostXWindowAndMoveMouse(x11::Window window) {
  SetTopmostXWindow(window);
  HandleMouseMovement(gfx::Point(kMouseMoveX, kMouseMoveY), ui::EF_NONE,
                      ui::EventTimeForNow());
}

void TestDragDropClient::SendXClientEvent(
    x11::Window window,
    const x11::ClientMessageEvent& event) {
  auto it = collectors_.find(window);
  if (it != collectors_.end())
    it->second->RecordEvent(event);
}

}  // namespace

class X11DragDropClientTest : public ViewsTestBase {
 public:
  X11DragDropClientTest() = default;
  ~X11DragDropClientTest() override = default;

  int StartDragAndDrop() {
    auto data(std::make_unique<ui::OSExchangeData>());
    data->SetString(base::ASCIIToUTF16("Test"));
    SkBitmap drag_bitmap;
    drag_bitmap.allocN32Pixels(10, 10);
    drag_bitmap.eraseARGB(0xFF, 0, 0, 0);
    gfx::ImageSkia drag_image(gfx::ImageSkia::CreateFrom1xBitmap(drag_bitmap));
    data->provider().SetDragImage(drag_image, gfx::Vector2d());

    return client_->StartDragAndDrop(
        std::move(data), widget_->GetNativeWindow()->GetRootWindow(),
        widget_->GetNativeWindow(), gfx::Point(), ui::DragDropTypes::DRAG_COPY,
        ui::mojom::DragEventSource::kMouse);
  }

  // ViewsTestBase:
  void SetUp() override {
    set_native_widget_type(NativeWidgetType::kDesktop);

    ViewsTestBase::SetUp();
    // TODO(crbug.com/1096425): Once non-Ozone X11 is deprecated, re-work this.
    if (features::IsUsingOzonePlatform())
      GTEST_SKIP();

    // Create widget to initiate the drags.
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(100, 100);
    widget_->Init(std::move(params));
    widget_->Show();

    client_ = std::make_unique<TestDragDropClient>(widget_->GetNativeWindow());
    // client_->Init();
  }

  void TearDown() override {
    client_.reset();
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  TestDragDropClient* client() { return client_.get(); }

 private:
  std::unique_ptr<TestDragDropClient> client_;

  // The widget used to initiate drags.
  std::unique_ptr<Widget> widget_;

  DISALLOW_COPY_AND_ASSIGN(X11DragDropClientTest);
};

namespace {

void BasicStep2(TestDragDropClient* client, x11::Window toplevel) {
  EXPECT_TRUE(client->IsMoveLoopRunning());

  ClientMessageEventCollector collector(toplevel, client);
  client->SetTopmostXWindowAndMoveMouse(toplevel);

  // XdndEnter should have been sent to |toplevel| before the XdndPosition
  // message.
  std::vector<x11::ClientMessageEvent> events = collector.PopAllEvents();
  ASSERT_EQ(2u, events.size());

  EXPECT_TRUE(client->MessageHasType(events[0], "XdndEnter"));
  EXPECT_EQ(client->source_xwindow(),
            static_cast<x11::Window>(events[0].data.data32[0]));
  EXPECT_EQ(1u, events[0].data.data32[1] & 1);
  std::vector<x11::Atom> targets;
  GetArrayProperty(client->source_xwindow(), x11::GetAtom("XdndTypeList"),
                   &targets);
  EXPECT_FALSE(targets.empty());

  EXPECT_TRUE(client->MessageHasType(events[1], "XdndPosition"));
  EXPECT_EQ(client->source_xwindow(),
            static_cast<x11::Window>(events[0].data.data32[0]));
  const uint32_t kCoords =
      TestDragDropClient::kMouseMoveX << 16 | TestDragDropClient::kMouseMoveY;
  EXPECT_EQ(kCoords, events[1].data.data32[2]);
  EXPECT_EQ(client->GetAtom("XdndActionCopy"),
            static_cast<x11::Atom>(events[1].data.data32[4]));

  client->OnStatus(toplevel, true, client->GetAtom("XdndActionCopy"));

  // Because there is no unprocessed XdndPosition, the drag drop client should
  // send XdndDrop immediately after the mouse is released.
  client->HandleMouseReleased();

  events = collector.PopAllEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndDrop"));
  EXPECT_EQ(client->source_xwindow(),
            static_cast<x11::Window>(events[0].data.data32[0]));

  // Send XdndFinished to indicate that the drag drop client can cleanup any
  // data related to this drag. The move loop should end only after the
  // XdndFinished message was received.
  EXPECT_TRUE(client->IsMoveLoopRunning());
  client->OnFinished(toplevel, true, client->GetAtom("XdndActionCopy"));
  EXPECT_FALSE(client->IsMoveLoopRunning());
}

void BasicStep3(TestDragDropClient* client, x11::Window toplevel) {
  EXPECT_TRUE(client->IsMoveLoopRunning());

  ClientMessageEventCollector collector(toplevel, client);
  client->SetTopmostXWindowAndMoveMouse(toplevel);

  std::vector<x11::ClientMessageEvent> events = collector.PopAllEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndEnter"));
  EXPECT_TRUE(client->MessageHasType(events[1], "XdndPosition"));

  client->OnStatus(toplevel, true, client->GetAtom("XdndActionCopy"));
  client->SetTopmostXWindowAndMoveMouse(toplevel);
  events = collector.PopAllEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndPosition"));

  // We have not received an XdndStatus ack for the second XdndPosition message.
  // Test that sending XdndDrop is delayed till the XdndStatus ack is received.
  client->HandleMouseReleased();
  EXPECT_FALSE(collector.HasEvents());

  client->OnStatus(toplevel, true, client->GetAtom("XdndActionCopy"));
  events = collector.PopAllEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndDrop"));

  EXPECT_TRUE(client->IsMoveLoopRunning());
  client->OnFinished(toplevel, true, client->GetAtom("XdndActionCopy"));
  EXPECT_FALSE(client->IsMoveLoopRunning());
}

}  // namespace

TEST_F(X11DragDropClientTest, Basic) {
  x11::Window toplevel = static_cast<x11::Window>(1);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&BasicStep2, client(), toplevel));
  int result = StartDragAndDrop();
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY, result);

  // Do another drag and drop to test that the data is properly cleaned up as a
  // result of the XdndFinished message.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&BasicStep3, client(), toplevel));
  result = StartDragAndDrop();
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY, result);
}

namespace {

void TargetDoesNotRespondStep2(TestDragDropClient* client) {
  EXPECT_TRUE(client->IsMoveLoopRunning());

  x11::Window toplevel = static_cast<x11::Window>(1);
  ClientMessageEventCollector collector(toplevel, client);
  client->SetTopmostXWindowAndMoveMouse(toplevel);

  std::vector<x11::ClientMessageEvent> events = collector.PopAllEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndEnter"));
  EXPECT_TRUE(client->MessageHasType(events[1], "XdndPosition"));

  client->HandleMouseReleased();
  events = collector.PopAllEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndLeave"));
  EXPECT_FALSE(client->IsMoveLoopRunning());
}

}  // namespace

// Test that we do not wait for the target to send XdndStatus if we have not
// received any XdndStatus messages at all from the target. The Unity
// DNDCollectionWindow is an example of an XdndAware target which does not
// respond to XdndPosition messages at all.
TEST_F(X11DragDropClientTest, TargetDoesNotRespond) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&TargetDoesNotRespondStep2, client()));
  int result = StartDragAndDrop();
  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE, result);
}

namespace {

void QueuePositionStep2(TestDragDropClient* client) {
  EXPECT_TRUE(client->IsMoveLoopRunning());

  x11::Window toplevel = static_cast<x11::Window>(1);
  ClientMessageEventCollector collector(toplevel, client);
  client->SetTopmostXWindowAndMoveMouse(toplevel);
  client->SetTopmostXWindowAndMoveMouse(toplevel);
  client->SetTopmostXWindowAndMoveMouse(toplevel);

  std::vector<x11::ClientMessageEvent> events = collector.PopAllEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndEnter"));
  EXPECT_TRUE(client->MessageHasType(events[1], "XdndPosition"));

  client->OnStatus(toplevel, true, client->GetAtom("XdndActionCopy"));
  events = collector.PopAllEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndPosition"));

  client->OnStatus(toplevel, true, client->GetAtom("XdndActionCopy"));
  EXPECT_FALSE(collector.HasEvents());

  client->HandleMouseReleased();
  events = collector.PopAllEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndDrop"));

  EXPECT_TRUE(client->IsMoveLoopRunning());
  client->OnFinished(toplevel, true, client->GetAtom("XdndActionCopy"));
  EXPECT_FALSE(client->IsMoveLoopRunning());
}

}  // namespace

// Test that XdndPosition messages are queued till the pending XdndPosition
// message is acked via an XdndStatus message.
TEST_F(X11DragDropClientTest, QueuePosition) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&QueuePositionStep2, client()));
  int result = StartDragAndDrop();
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY, result);
}

namespace {

void TargetChangesStep2(TestDragDropClient* client) {
  EXPECT_TRUE(client->IsMoveLoopRunning());

  x11::Window toplevel1 = static_cast<x11::Window>(1);
  ClientMessageEventCollector collector1(toplevel1, client);
  client->SetTopmostXWindowAndMoveMouse(toplevel1);

  std::vector<x11::ClientMessageEvent> events1 = collector1.PopAllEvents();
  ASSERT_EQ(2u, events1.size());
  EXPECT_TRUE(client->MessageHasType(events1[0], "XdndEnter"));
  EXPECT_TRUE(client->MessageHasType(events1[1], "XdndPosition"));

  x11::Window toplevel2 = static_cast<x11::Window>(2);
  ClientMessageEventCollector collector2(toplevel2, client);
  client->SetTopmostXWindowAndMoveMouse(toplevel2);

  // It is possible for |toplevel1| to send XdndStatus after the source has sent
  // XdndLeave but before |toplevel1| has received the XdndLeave message. The
  // XdndStatus message should be ignored.
  client->OnStatus(toplevel1, true, client->GetAtom("XdndActionCopy"));
  events1 = collector1.PopAllEvents();
  ASSERT_EQ(1u, events1.size());
  EXPECT_TRUE(client->MessageHasType(events1[0], "XdndLeave"));

  std::vector<x11::ClientMessageEvent> events2 = collector2.PopAllEvents();
  ASSERT_EQ(2u, events2.size());
  EXPECT_TRUE(client->MessageHasType(events2[0], "XdndEnter"));
  EXPECT_TRUE(client->MessageHasType(events2[1], "XdndPosition"));

  client->OnStatus(toplevel2, true, client->GetAtom("XdndActionCopy"));
  client->HandleMouseReleased();
  events2 = collector2.PopAllEvents();
  ASSERT_EQ(1u, events2.size());
  EXPECT_TRUE(client->MessageHasType(events2[0], "XdndDrop"));

  EXPECT_TRUE(client->IsMoveLoopRunning());
  client->OnFinished(toplevel2, true, client->GetAtom("XdndActionCopy"));
  EXPECT_FALSE(client->IsMoveLoopRunning());
}

}  // namespace

// Test the behavior when the target changes during a drag.
TEST_F(X11DragDropClientTest, TargetChanges) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&TargetChangesStep2, client()));
  int result = StartDragAndDrop();
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY, result);
}

namespace {

void RejectAfterMouseReleaseStep2(TestDragDropClient* client) {
  EXPECT_TRUE(client->IsMoveLoopRunning());

  x11::Window toplevel = static_cast<x11::Window>(1);
  ClientMessageEventCollector collector(toplevel, client);
  client->SetTopmostXWindowAndMoveMouse(toplevel);

  std::vector<x11::ClientMessageEvent> events = collector.PopAllEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndEnter"));
  EXPECT_TRUE(client->MessageHasType(events[1], "XdndPosition"));

  client->OnStatus(toplevel, true, client->GetAtom("XdndActionCopy"));
  EXPECT_FALSE(collector.HasEvents());

  // Send another mouse move such that there is a pending XdndPosition.
  client->SetTopmostXWindowAndMoveMouse(toplevel);
  events = collector.PopAllEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndPosition"));

  client->HandleMouseReleased();
  // Reject the drop.
  client->OnStatus(toplevel, false, x11::Atom::None);

  events = collector.PopAllEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndLeave"));
  EXPECT_FALSE(client->IsMoveLoopRunning());
}

void RejectAfterMouseReleaseStep3(TestDragDropClient* client) {
  EXPECT_TRUE(client->IsMoveLoopRunning());

  x11::Window toplevel = static_cast<x11::Window>(2);
  ClientMessageEventCollector collector(toplevel, client);
  client->SetTopmostXWindowAndMoveMouse(toplevel);

  std::vector<x11::ClientMessageEvent> events = collector.PopAllEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndEnter"));
  EXPECT_TRUE(client->MessageHasType(events[1], "XdndPosition"));

  client->OnStatus(toplevel, true, client->GetAtom("XdndActionCopy"));
  EXPECT_FALSE(collector.HasEvents());

  client->HandleMouseReleased();
  events = collector.PopAllEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndDrop"));

  EXPECT_TRUE(client->IsMoveLoopRunning());
  client->OnFinished(toplevel, false, x11::Atom::None);
  EXPECT_FALSE(client->IsMoveLoopRunning());
}

}  // namespace

// Test that the source sends XdndLeave instead of XdndDrop if the drag
// operation is rejected after the mouse is released.
TEST_F(X11DragDropClientTest, RejectAfterMouseRelease) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&RejectAfterMouseReleaseStep2, client()));
  int result = StartDragAndDrop();
  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE, result);

  // Repeat the test but reject the drop in the XdndFinished message instead.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&RejectAfterMouseReleaseStep3, client()));
  result = StartDragAndDrop();
  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE, result);
}

}  // namespace views
