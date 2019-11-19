// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <vector>

// Include views_test_base.h first because the definition of None in X.h
// conflicts with the definition of None in gtest-type-util.h
#include "ui/views/test/views_test_base.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_aurax11.h"
#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/x11_move_loop.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

class TestDragDropClient;

// Collects messages which would otherwise be sent to |xid_| via
// SendXClientEvent().
class ClientMessageEventCollector {
 public:
  ClientMessageEventCollector(::Window xid, TestDragDropClient* client);
  virtual ~ClientMessageEventCollector();

  // Returns true if |events_| is non-empty.
  bool HasEvents() const {
    return !events_.empty();
  }

  // Pops all of |events_| and returns the popped events in the order that they
  // were on the stack
  std::vector<XClientMessageEvent> PopAllEvents();

  // Adds |event| to the stack.
  void RecordEvent(const XClientMessageEvent& event);

 private:
  ::Window xid_;

  // Not owned.
  TestDragDropClient* client_;

  std::vector<XClientMessageEvent> events_;

  DISALLOW_COPY_AND_ASSIGN(ClientMessageEventCollector);
};

// An implementation of X11MoveLoop where RunMoveLoop() always starts the move
// loop.
class TestMoveLoop : public X11MoveLoop {
 public:
  explicit TestMoveLoop(X11MoveLoopDelegate* delegate);
  ~TestMoveLoop() override;

  // Returns true if the move loop is running.
  bool IsRunning() const;

  // X11MoveLoop:
  bool RunMoveLoop(aura::Window* window, gfx::NativeCursor cursor) override;
  void UpdateCursor(gfx::NativeCursor cursor) override;
  void EndMoveLoop() override;

 private:
  // Not owned.
  X11MoveLoopDelegate* delegate_;

  // Ends the move loop.
  base::OnceClosure quit_closure_;

  bool is_running_ = false;
};

// Implementation of DesktopDragDropClientAuraX11 which short circuits
// FindWindowFor().
class SimpleTestDragDropClient : public DesktopDragDropClientAuraX11 {
 public:
  SimpleTestDragDropClient(aura::Window*,
                           DesktopNativeCursorManager* cursor_manager);
  ~SimpleTestDragDropClient() override;

  // Sets |xid| as the topmost window for all mouse positions.
  void SetTopmostXWindow(XID xid);

  // Returns true if the move loop is running.
  bool IsMoveLoopRunning();

  Widget* drag_widget() { return DesktopDragDropClientAuraX11::drag_widget(); }

 private:
  // DesktopDragDropClientAuraX11:
  std::unique_ptr<X11MoveLoop> CreateMoveLoop(
      X11MoveLoopDelegate* delegate) override;
  XID FindWindowFor(const gfx::Point& screen_point) override;

  // The XID of the window which is simulated to be the topmost window.
  XID target_xid_ = x11::None;

  // The move loop. Not owned.
  TestMoveLoop* loop_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SimpleTestDragDropClient);
};

// Implementation of DesktopDragDropClientAuraX11 which works with a fake
// |DesktopDragDropClientAuraX11::source_current_window_|.
class TestDragDropClient : public SimpleTestDragDropClient {
 public:
  // The location in screen coordinates used for the synthetic mouse moves
  // generated in SetTopmostXWindowAndMoveMouse().
  static constexpr int kMouseMoveX = 100;
  static constexpr int kMouseMoveY = 200;

  TestDragDropClient(aura::Window* window,
                     DesktopNativeCursorManager* cursor_manager);
  ~TestDragDropClient() override;

  // Returns the XID of the window which initiated the drag.
  ::Window source_xwindow() {
    return source_xid_;
  }

  // Returns the Atom with |name|.
  Atom GetAtom(const char* name);

  // Returns true if the event's message has |type|.
  bool MessageHasType(const XClientMessageEvent& event,
                      const char* type);

  // Sets |collector| to collect XClientMessageEvents which would otherwise
  // have been sent to the drop target window.
  void SetEventCollectorFor(::Window xid,
                            ClientMessageEventCollector* collector);

  // Builds an XdndStatus message and sends it to
  // DesktopDragDropClientAuraX11.
  void OnStatus(XID target_window,
                bool will_accept_drop,
                ::Atom accepted_action);

  // Builds an XdndFinished message and sends it to
  // DesktopDragDropClientAuraX11.
  void OnFinished(XID target_window,
                  bool accepted_drop,
                  ::Atom performed_action);

  // Sets |xid| as the topmost window at the current mouse position and
  // generates a synthetic mouse move.
  void SetTopmostXWindowAndMoveMouse(::Window xid);

 private:
  // DesktopDragDropClientAuraX11:
  void SendXClientEvent(::Window xid, XEvent* event) override;

  // The XID of the window which initiated the drag.
  ::Window source_xid_;

  // Map of ::Windows to the collector which intercepts XClientMessageEvents
  // for that window.
  std::map< ::Window, ClientMessageEventCollector*> collectors_;

  DISALLOW_COPY_AND_ASSIGN(TestDragDropClient);
};

///////////////////////////////////////////////////////////////////////////////
// ClientMessageEventCollector

ClientMessageEventCollector::ClientMessageEventCollector(
    ::Window xid,
    TestDragDropClient* client)
    : xid_(xid),
      client_(client) {
  client->SetEventCollectorFor(xid, this);
}

ClientMessageEventCollector::~ClientMessageEventCollector() {
  client_->SetEventCollectorFor(xid_, nullptr);
}

std::vector<XClientMessageEvent> ClientMessageEventCollector::PopAllEvents() {
  std::vector<XClientMessageEvent> to_return;
  to_return.swap(events_);
  return to_return;
}

void ClientMessageEventCollector::RecordEvent(
    const XClientMessageEvent& event) {
  events_.push_back(event);
}

///////////////////////////////////////////////////////////////////////////////
// TestMoveLoop

TestMoveLoop::TestMoveLoop(X11MoveLoopDelegate* delegate)
    : delegate_(delegate) {}

TestMoveLoop::~TestMoveLoop() = default;

bool TestMoveLoop::IsRunning() const {
  return is_running_;
}

bool TestMoveLoop::RunMoveLoop(
    aura::Window* window,
    gfx::NativeCursor cursor) {
  is_running_ = true;
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
  return true;
}

void TestMoveLoop::UpdateCursor(gfx::NativeCursor cursor) {
}

void TestMoveLoop::EndMoveLoop() {
  if (is_running_) {
    delegate_->OnMoveLoopEnded();
    is_running_ = false;
    std::move(quit_closure_).Run();
  }
}

///////////////////////////////////////////////////////////////////////////////
// SimpleTestDragDropClient

SimpleTestDragDropClient::SimpleTestDragDropClient(
    aura::Window* window,
    DesktopNativeCursorManager* cursor_manager)
    : DesktopDragDropClientAuraX11(window,
                                   cursor_manager,
                                   gfx::GetXDisplay(),
                                   window->GetHost()->GetAcceleratedWidget()) {}

SimpleTestDragDropClient::~SimpleTestDragDropClient() = default;

void SimpleTestDragDropClient::SetTopmostXWindow(XID xid) {
  target_xid_ = xid;
}

bool SimpleTestDragDropClient::IsMoveLoopRunning() {
  return loop_->IsRunning();
}

std::unique_ptr<X11MoveLoop> SimpleTestDragDropClient::CreateMoveLoop(
    X11MoveLoopDelegate* delegate) {
  loop_ = new TestMoveLoop(delegate);
  return base::WrapUnique(loop_);
}

XID SimpleTestDragDropClient::FindWindowFor(const gfx::Point& screen_point) {
  return target_xid_;
}

///////////////////////////////////////////////////////////////////////////////
// TestDragDropClient

TestDragDropClient::TestDragDropClient(
    aura::Window* window,
    DesktopNativeCursorManager* cursor_manager)
    : SimpleTestDragDropClient(window, cursor_manager),
      source_xid_(window->GetHost()->GetAcceleratedWidget()) {}

TestDragDropClient::~TestDragDropClient() = default;

Atom TestDragDropClient::GetAtom(const char* name) {
  return gfx::GetAtom(name);
}

bool TestDragDropClient::MessageHasType(const XClientMessageEvent& event,
                                        const char* type) {
  return event.message_type == GetAtom(type);
}

void TestDragDropClient::SetEventCollectorFor(
    ::Window xid,
    ClientMessageEventCollector* collector) {
  if (collector)
    collectors_[xid] = collector;
  else
    collectors_.erase(xid);
}

void TestDragDropClient::OnStatus(XID target_window,
                                  bool will_accept_drop,
                                  ::Atom accepted_action) {
  XClientMessageEvent event;
  event.message_type = GetAtom("XdndStatus");
  event.format = 32;
  event.window = source_xid_;
  event.data.l[0] = target_window;
  event.data.l[1] = will_accept_drop ? 1 : 0;
  event.data.l[2] = 0;
  event.data.l[3] = 0;
  event.data.l[4] = accepted_action;
  OnXdndStatus(event);
}

void TestDragDropClient::OnFinished(XID target_window,
                                    bool accepted_drop,
                                    ::Atom performed_action) {
  XClientMessageEvent event;
  event.message_type = GetAtom("XdndFinished");
  event.format = 32;
  event.window = source_xid_;
  event.data.l[0] = target_window;
  event.data.l[1] = accepted_drop ? 1 : 0;
  event.data.l[2] = performed_action;
  event.data.l[3] = 0;
  event.data.l[4] = 0;
  OnXdndFinished(event);
}

void TestDragDropClient::SetTopmostXWindowAndMoveMouse(::Window xid) {
  SetTopmostXWindow(xid);
  OnMouseMovement(gfx::Point(kMouseMoveX, kMouseMoveY), ui::EF_NONE,
                  ui::EventTimeForNow());
}

void TestDragDropClient::SendXClientEvent(::Window xid, XEvent* event) {
  auto it = collectors_.find(xid);
  if (it != collectors_.end())
    it->second->RecordEvent(event->xclient);
}

}  // namespace

class DesktopDragDropClientAuraX11Test : public ViewsTestBase {
 public:
  DesktopDragDropClientAuraX11Test() = default;
  ~DesktopDragDropClientAuraX11Test() override = default;

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
        ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);
  }

  // ViewsTestBase:
  void SetUp() override {
    set_native_widget_type(NativeWidgetType::kDesktop);

    ViewsTestBase::SetUp();

    // Create widget to initiate the drags.
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(100, 100);
    widget_->Init(std::move(params));
    widget_->Show();

    cursor_manager_ = std::make_unique<DesktopNativeCursorManager>();

    client_ = std::make_unique<TestDragDropClient>(widget_->GetNativeWindow(),
                                                   cursor_manager_.get());
    client_->Init();
  }

  void TearDown() override {
    client_.reset();
    cursor_manager_.reset();
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  TestDragDropClient* client() {
    return client_.get();
  }

 private:
  std::unique_ptr<TestDragDropClient> client_;
  std::unique_ptr<DesktopNativeCursorManager> cursor_manager_;

  // The widget used to initiate drags.
  std::unique_ptr<Widget> widget_;

  DISALLOW_COPY_AND_ASSIGN(DesktopDragDropClientAuraX11Test);
};

namespace {

void BasicStep2(TestDragDropClient* client, XID toplevel) {
  EXPECT_TRUE(client->IsMoveLoopRunning());

  ClientMessageEventCollector collector(toplevel, client);
  client->SetTopmostXWindowAndMoveMouse(toplevel);

  // XdndEnter should have been sent to |toplevel| before the XdndPosition
  // message.
  std::vector<XClientMessageEvent> events = collector.PopAllEvents();
  ASSERT_EQ(2u, events.size());

  EXPECT_TRUE(client->MessageHasType(events[0], "XdndEnter"));
  EXPECT_EQ(client->source_xwindow(),
            static_cast<XID>(events[0].data.l[0]));
  EXPECT_EQ(1, events[0].data.l[1] & 1);
  std::vector<Atom> targets;
  ui::GetAtomArrayProperty(client->source_xwindow(), "XdndTypeList", &targets);
  EXPECT_FALSE(targets.empty());

  EXPECT_TRUE(client->MessageHasType(events[1], "XdndPosition"));
  EXPECT_EQ(client->source_xwindow(),
            static_cast<XID>(events[0].data.l[0]));
  const long kCoords =
      TestDragDropClient::kMouseMoveX << 16 | TestDragDropClient::kMouseMoveY;
  EXPECT_EQ(kCoords, events[1].data.l[2]);
  EXPECT_EQ(client->GetAtom("XdndActionCopy"),
            static_cast<Atom>(events[1].data.l[4]));

  client->OnStatus(toplevel, true, client->GetAtom("XdndActionCopy"));

  // Because there is no unprocessed XdndPosition, the drag drop client should
  // send XdndDrop immediately after the mouse is released.
  client->OnMouseReleased();

  events = collector.PopAllEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndDrop"));
  EXPECT_EQ(client->source_xwindow(),
            static_cast<XID>(events[0].data.l[0]));

  // Send XdndFinished to indicate that the drag drop client can cleanup any
  // data related to this drag. The move loop should end only after the
  // XdndFinished message was received.
  EXPECT_TRUE(client->IsMoveLoopRunning());
  client->OnFinished(toplevel, true, client->GetAtom("XdndActionCopy"));
  EXPECT_FALSE(client->IsMoveLoopRunning());
}

void BasicStep3(TestDragDropClient* client, XID toplevel) {
  EXPECT_TRUE(client->IsMoveLoopRunning());

  ClientMessageEventCollector collector(toplevel, client);
  client->SetTopmostXWindowAndMoveMouse(toplevel);

  std::vector<XClientMessageEvent> events = collector.PopAllEvents();
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
  client->OnMouseReleased();
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

TEST_F(DesktopDragDropClientAuraX11Test, Basic) {
  XID toplevel = 1;

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

void HighDPIStep(TestDragDropClient* client) {
  float scale =
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor();
  // Start dragging at 100, 100 in native coordinates.
  gfx::Point mouse_position_in_screen_pixel(100, 100);
  client->OnMouseMovement(mouse_position_in_screen_pixel, 0,
                          ui::EventTimeForNow());

  EXPECT_EQ(gfx::ScaleToFlooredPoint(gfx::Point(100, 100), 1.f / scale),
            client->drag_widget()->GetWindowBoundsInScreen().origin());

  // Drag the mouse down 200 pixels.
  mouse_position_in_screen_pixel.Offset(200, 0);
  client->OnMouseMovement(mouse_position_in_screen_pixel, 0,
                          ui::EventTimeForNow());
  EXPECT_EQ(gfx::ScaleToFlooredPoint(gfx::Point(300, 100), 1.f / scale),
            client->drag_widget()->GetWindowBoundsInScreen().origin());

  client->OnMouseReleased();
}

TEST_F(DesktopDragDropClientAuraX11Test, HighDPI200) {
  aura::TestScreen* screen =
      static_cast<aura::TestScreen*>(display::Screen::GetScreen());
  screen->SetDeviceScaleFactor(2.0f);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&HighDPIStep, client()));
  int result = StartDragAndDrop();
  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE, result);
}

TEST_F(DesktopDragDropClientAuraX11Test, HighDPI150) {
  aura::TestScreen* screen =
      static_cast<aura::TestScreen*>(display::Screen::GetScreen());
  screen->SetDeviceScaleFactor(1.5f);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&HighDPIStep, client()));
  int result = StartDragAndDrop();
  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE, result);
}

namespace {

void TargetDoesNotRespondStep2(TestDragDropClient* client) {
  EXPECT_TRUE(client->IsMoveLoopRunning());

  XID toplevel = 1;
  ClientMessageEventCollector collector(toplevel, client);
  client->SetTopmostXWindowAndMoveMouse(toplevel);

  std::vector<XClientMessageEvent> events = collector.PopAllEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndEnter"));
  EXPECT_TRUE(client->MessageHasType(events[1], "XdndPosition"));

  client->OnMouseReleased();
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
TEST_F(DesktopDragDropClientAuraX11Test, TargetDoesNotRespond) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&TargetDoesNotRespondStep2, client()));
  int result = StartDragAndDrop();
  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE, result);
}

namespace {

void QueuePositionStep2(TestDragDropClient* client) {
  EXPECT_TRUE(client->IsMoveLoopRunning());

  XID toplevel = 1;
  ClientMessageEventCollector collector(toplevel, client);
  client->SetTopmostXWindowAndMoveMouse(toplevel);
  client->SetTopmostXWindowAndMoveMouse(toplevel);
  client->SetTopmostXWindowAndMoveMouse(toplevel);

  std::vector<XClientMessageEvent> events = collector.PopAllEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndEnter"));
  EXPECT_TRUE(client->MessageHasType(events[1], "XdndPosition"));

  client->OnStatus(toplevel, true, client->GetAtom("XdndActionCopy"));
  events = collector.PopAllEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndPosition"));

  client->OnStatus(toplevel, true, client->GetAtom("XdndActionCopy"));
  EXPECT_FALSE(collector.HasEvents());

  client->OnMouseReleased();
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
TEST_F(DesktopDragDropClientAuraX11Test, QueuePosition) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&QueuePositionStep2, client()));
  int result = StartDragAndDrop();
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY, result);
}

namespace {

void TargetChangesStep2(TestDragDropClient* client) {
  EXPECT_TRUE(client->IsMoveLoopRunning());

  XID toplevel1 = 1;
  ClientMessageEventCollector collector1(toplevel1, client);
  client->SetTopmostXWindowAndMoveMouse(toplevel1);

  std::vector<XClientMessageEvent> events1 = collector1.PopAllEvents();
  ASSERT_EQ(2u, events1.size());
  EXPECT_TRUE(client->MessageHasType(events1[0], "XdndEnter"));
  EXPECT_TRUE(client->MessageHasType(events1[1], "XdndPosition"));

  XID toplevel2 = 2;
  ClientMessageEventCollector collector2(toplevel2, client);
  client->SetTopmostXWindowAndMoveMouse(toplevel2);

  // It is possible for |toplevel1| to send XdndStatus after the source has sent
  // XdndLeave but before |toplevel1| has received the XdndLeave message. The
  // XdndStatus message should be ignored.
  client->OnStatus(toplevel1, true, client->GetAtom("XdndActionCopy"));
  events1 = collector1.PopAllEvents();
  ASSERT_EQ(1u, events1.size());
  EXPECT_TRUE(client->MessageHasType(events1[0], "XdndLeave"));

  std::vector<XClientMessageEvent> events2 = collector2.PopAllEvents();
  ASSERT_EQ(2u, events2.size());
  EXPECT_TRUE(client->MessageHasType(events2[0], "XdndEnter"));
  EXPECT_TRUE(client->MessageHasType(events2[1], "XdndPosition"));

  client->OnStatus(toplevel2, true, client->GetAtom("XdndActionCopy"));
  client->OnMouseReleased();
  events2 = collector2.PopAllEvents();
  ASSERT_EQ(1u, events2.size());
  EXPECT_TRUE(client->MessageHasType(events2[0], "XdndDrop"));

  EXPECT_TRUE(client->IsMoveLoopRunning());
  client->OnFinished(toplevel2, true, client->GetAtom("XdndActionCopy"));
  EXPECT_FALSE(client->IsMoveLoopRunning());
}

}  // namespace

// Test the behavior when the target changes during a drag.
TEST_F(DesktopDragDropClientAuraX11Test, TargetChanges) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&TargetChangesStep2, client()));
  int result = StartDragAndDrop();
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY, result);
}

namespace {

void RejectAfterMouseReleaseStep2(TestDragDropClient* client) {
  EXPECT_TRUE(client->IsMoveLoopRunning());

  XID toplevel = 1;
  ClientMessageEventCollector collector(toplevel, client);
  client->SetTopmostXWindowAndMoveMouse(toplevel);

  std::vector<XClientMessageEvent> events = collector.PopAllEvents();
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

  client->OnMouseReleased();
  // Reject the drop.
  client->OnStatus(toplevel, false, x11::None);

  events = collector.PopAllEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndLeave"));
  EXPECT_FALSE(client->IsMoveLoopRunning());
}

void RejectAfterMouseReleaseStep3(TestDragDropClient* client) {
  EXPECT_TRUE(client->IsMoveLoopRunning());

  XID toplevel = 2;
  ClientMessageEventCollector collector(toplevel, client);
  client->SetTopmostXWindowAndMoveMouse(toplevel);

  std::vector<XClientMessageEvent> events = collector.PopAllEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndEnter"));
  EXPECT_TRUE(client->MessageHasType(events[1], "XdndPosition"));

  client->OnStatus(toplevel, true, client->GetAtom("XdndActionCopy"));
  EXPECT_FALSE(collector.HasEvents());

  client->OnMouseReleased();
  events = collector.PopAllEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndDrop"));

  EXPECT_TRUE(client->IsMoveLoopRunning());
  client->OnFinished(toplevel, false, x11::None);
  EXPECT_FALSE(client->IsMoveLoopRunning());
}

}  // namespace

// Test that the source sends XdndLeave instead of XdndDrop if the drag
// operation is rejected after the mouse is released.
TEST_F(DesktopDragDropClientAuraX11Test, RejectAfterMouseRelease) {
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

namespace {

// DragDropDelegate which counts the number of each type of drag-drop event and
// keeps track of the most recent drag-drop event.
class TestDragDropDelegate : public aura::client::DragDropDelegate {
 public:
  TestDragDropDelegate() = default;
  ~TestDragDropDelegate() override = default;

  int num_enters() const { return num_enters_; }
  int num_updates() const { return num_updates_; }
  int num_exits() const { return num_exits_; }
  int num_drops() const { return num_drops_; }
  gfx::Point last_event_mouse_position() const {
    return last_event_mouse_position_;
  }
  int last_event_flags() const { return last_event_flags_; }

 private:
  // aura::client::DragDropDelegate:
  void OnDragEntered(const ui::DropTargetEvent& event) override {
    ++num_enters_;
    last_event_mouse_position_ = event.location();
    last_event_flags_ = event.flags();
  }

  int OnDragUpdated(const ui::DropTargetEvent& event) override {
    ++num_updates_;
    last_event_mouse_position_ = event.location();
    last_event_flags_ = event.flags();
    return ui::DragDropTypes::DRAG_COPY;
  }

  void OnDragExited() override {
    ++num_exits_;
  }

  int OnPerformDrop(const ui::DropTargetEvent& event,
                    std::unique_ptr<OSExchangeData> data) override {
    ++num_drops_;
    last_event_mouse_position_ = event.location();
    last_event_flags_ = event.flags();
    return ui::DragDropTypes::DRAG_COPY;
  }

  int num_enters_ = 0;
  int num_updates_ = 0;
  int num_exits_ = 0;
  int num_drops_ = 0;

  gfx::Point last_event_mouse_position_;
  int last_event_flags_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestDragDropDelegate);
};

}  // namespace

// Test harness for tests where the drag and drop source and target are both
// Chrome windows.
class DesktopDragDropClientAuraX11ChromeSourceTargetTest
    : public ViewsTestBase {
 public:
  DesktopDragDropClientAuraX11ChromeSourceTargetTest() = default;

  ~DesktopDragDropClientAuraX11ChromeSourceTargetTest() override = default;

  int StartDragAndDrop() {
    auto data(std::make_unique<ui::OSExchangeData>());
    data->SetString(base::ASCIIToUTF16("Test"));

    return client_->StartDragAndDrop(
        std::move(data), widget_->GetNativeWindow()->GetRootWindow(),
        widget_->GetNativeWindow(), gfx::Point(), ui::DragDropTypes::DRAG_COPY,
        ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);
  }

  // ViewsTestBase:
  void SetUp() override {
    ViewsTestBase::SetUp();

    // Create widget to initiate the drags.
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.native_widget = new DesktopNativeWidgetAura(widget_.get());
    params.bounds = gfx::Rect(100, 100);
    widget_->Init(std::move(params));
    widget_->Show();

    cursor_manager_ = std::make_unique<DesktopNativeCursorManager>();

    client_ = std::make_unique<SimpleTestDragDropClient>(
        widget_->GetNativeWindow(), cursor_manager_.get());
    client_->Init();
  }

  void TearDown() override {
    client_.reset();
    cursor_manager_.reset();
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  SimpleTestDragDropClient* client() {
    return client_.get();
  }

 private:
  std::unique_ptr<SimpleTestDragDropClient> client_;
  std::unique_ptr<DesktopNativeCursorManager> cursor_manager_;

  // The widget used to initiate drags.
  std::unique_ptr<Widget> widget_;

  DISALLOW_COPY_AND_ASSIGN(DesktopDragDropClientAuraX11ChromeSourceTargetTest);
};

namespace {

void ChromeSourceTargetStep2(SimpleTestDragDropClient* client,
                             int modifier_flags) {
  EXPECT_TRUE(client->IsMoveLoopRunning());

  std::unique_ptr<Widget> target_widget(new Widget);
  Widget::InitParams target_params(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  target_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  target_params.native_widget =
      new DesktopNativeWidgetAura(target_widget.get());
  target_params.bounds = gfx::Rect(100, 100);
  target_widget->Init(std::move(target_params));
  target_widget->Show();

  std::unique_ptr<TestDragDropDelegate> delegate(new TestDragDropDelegate);
  aura::client::SetDragDropDelegate(target_widget->GetNativeWindow(),
                                    delegate.get());

  client->SetTopmostXWindow(
      target_widget->GetNativeView()->GetHost()->GetAcceleratedWidget());

  gfx::Rect target_widget_bounds_in_screen =
      target_widget->GetWindowBoundsInScreen();
  gfx::Point point1_in_screen = target_widget_bounds_in_screen.CenterPoint();
  gfx::Point point1_in_target_widget(
      target_widget_bounds_in_screen.width() / 2,
      target_widget_bounds_in_screen.height() / 2);
  gfx::Point point2_in_screen = point1_in_screen + gfx::Vector2d(1, 0);
  gfx::Point point2_in_target_widget =
      point1_in_target_widget + gfx::Vector2d(1, 0);

  client->OnMouseMovement(point1_in_screen, modifier_flags,
                          ui::EventTimeForNow());
  EXPECT_EQ(1, delegate->num_enters());
  EXPECT_EQ(1, delegate->num_updates());
  EXPECT_EQ(0, delegate->num_exits());
  EXPECT_EQ(0, delegate->num_drops());
  EXPECT_EQ(point1_in_target_widget.ToString(),
            delegate->last_event_mouse_position().ToString());
  EXPECT_EQ(modifier_flags, delegate->last_event_flags());

  client->OnMouseMovement(point2_in_screen, modifier_flags,
                          ui::EventTimeForNow());
  EXPECT_EQ(1, delegate->num_enters());
  EXPECT_EQ(2, delegate->num_updates());
  EXPECT_EQ(0, delegate->num_exits());
  EXPECT_EQ(0, delegate->num_drops());
  EXPECT_EQ(point2_in_target_widget.ToString(),
            delegate->last_event_mouse_position().ToString());
  EXPECT_EQ(modifier_flags, delegate->last_event_flags());

  client->OnMouseReleased();
  EXPECT_EQ(1, delegate->num_enters());
  EXPECT_EQ(2, delegate->num_updates());
  EXPECT_EQ(0, delegate->num_exits());
  EXPECT_EQ(1, delegate->num_drops());
  EXPECT_EQ(point2_in_target_widget.ToString(),
            delegate->last_event_mouse_position().ToString());
  EXPECT_EQ(modifier_flags, delegate->last_event_flags());

  EXPECT_FALSE(client->IsMoveLoopRunning());
}

}  // namespace

TEST_F(DesktopDragDropClientAuraX11ChromeSourceTargetTest, Basic) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ChromeSourceTargetStep2, client(), ui::EF_NONE));
  int result = StartDragAndDrop();
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY, result);
}

// Test that if 'Ctrl' is pressed during a drag and drop operation, that
// the aura::client::DragDropDelegate is properly notified.
TEST_F(DesktopDragDropClientAuraX11ChromeSourceTargetTest, CtrlPressed) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ChromeSourceTargetStep2, client(), ui::EF_CONTROL_DOWN));
  int result = StartDragAndDrop();
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY, result);
}

}  // namespace views
