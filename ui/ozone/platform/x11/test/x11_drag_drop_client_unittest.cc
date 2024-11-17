// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_drag_drop_client.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_factory_ozone.h"
#include "ui/base/x/x11_cursor.h"
#include "ui/base/x/x11_move_loop.h"
#include "ui/base/x/x11_move_loop_delegate.h"
#include "ui/base/x/x11_os_exchange_data_provider.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto.h"
#include "ui/ozone/platform/x11/os_exchange_data_provider_x11.h"
#include "ui/ozone/platform/x11/x11_window.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {
namespace {

using ::ui::mojom::DragOperation;

class TestDragDropClient;

// Collects messages which would otherwise be sent to |window_| via
// SendXClientEvent().
class ClientMessageEventCollector {
 public:
  ClientMessageEventCollector(x11::Window window, TestDragDropClient* client);

  ClientMessageEventCollector(const ClientMessageEventCollector&) = delete;
  ClientMessageEventCollector& operator=(const ClientMessageEventCollector&) =
      delete;

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
  raw_ptr<TestDragDropClient> client_;

  std::vector<x11::ClientMessageEvent> events_;
};

// An implementation of X11MoveLoop where RunMoveLoop() always starts the
// move loop.
class TestMoveLoop : public X11MoveLoop {
 public:
  explicit TestMoveLoop(X11MoveLoopDelegate* delegate);
  ~TestMoveLoop() override;

  // Returns true if the move loop is running.
  bool IsRunning() const;

  // X11MoveLoop:
  bool RunMoveLoop(bool can_grab_pointer,
                   scoped_refptr<X11Cursor> old_cursor,
                   scoped_refptr<X11Cursor> new_cursor,
                   base::OnceClosure started_callback) override;
  void UpdateCursor(scoped_refptr<X11Cursor> cursor) override;
  void EndMoveLoop() override;

 private:
  // Not owned.
  raw_ptr<X11MoveLoopDelegate> delegate_;

  // Ends the move loop.
  base::OnceClosure quit_closure_;

  bool is_running_ = false;
};

// Implementation of XDragDropClient which short circuits FindWindowFor().
class SimpleTestDragDropClient : public XDragDropClient,
                                 public XDragDropClient::Delegate,
                                 public X11MoveLoopDelegate {
 public:
  explicit SimpleTestDragDropClient(X11Window* window);

  SimpleTestDragDropClient(const SimpleTestDragDropClient&) = delete;
  SimpleTestDragDropClient& operator=(const SimpleTestDragDropClient&) = delete;

  ~SimpleTestDragDropClient() override;

  // Sets |window| as the topmost window for all mouse positions.
  void SetTopmostXWindow(x11::Window window);

  // Returns true if the move loop is running.
  bool IsMoveLoopRunning();

  // Starts the move loop.
  DragOperation StartDragAndDrop(std::unique_ptr<OSExchangeData> data,
                                 X11Window* source_window,
                                 int allowed_operations,
                                 mojom::DragEventSource source);

 private:
  // XDragDropClient::Delegate:
  std::optional<gfx::AcceleratedWidget> GetDragWidget() override;
  int UpdateDrag(const gfx::Point& screen_point) override;
  void UpdateCursor(DragOperation negotiated_operation) override;
  void OnBeginForeignDrag(x11::Window window) override;
  void OnEndForeignDrag() override;
  void OnBeforeDragLeave() override;
  DragOperation PerformDrop() override;
  void EndDragLoop() override;

  // XDragDropClient:
  x11::Window FindWindowFor(const gfx::Point& screen_point) override;

  // X11MoveLoopDelegate:
  void OnMouseMovement(const gfx::Point& screen_point,
                       int flags,
                       base::TimeTicks event_time) override;
  void OnMouseReleased() override;
  void OnMoveLoopEnded() override;

  // The x11::Window of the window which is simulated to be the topmost window.
  x11::Window target_window_ = x11::Window::None;

  // The move loop.
  std::unique_ptr<TestMoveLoop> loop_;

  base::OnceClosure quit_closure_;
};

// Implementation of XDragDropClient which works with a fake
// |XDragDropClient::source_current_window_|.
class TestDragDropClient : public SimpleTestDragDropClient {
 public:
  // The location in screen coordinates used for the synthetic mouse moves
  // generated in SetTopmostXWindowAndMoveMouse().
  static constexpr int kMouseMoveX = 100;
  static constexpr int kMouseMoveY = 200;

  explicit TestDragDropClient(X11Window* window);

  TestDragDropClient(const TestDragDropClient&) = delete;
  TestDragDropClient& operator=(const TestDragDropClient&) = delete;

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
  std::map<x11::Window, raw_ptr<ClientMessageEventCollector, CtnExperimental>>
      collectors_;
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

TestMoveLoop::TestMoveLoop(X11MoveLoopDelegate* delegate)
    : delegate_(delegate) {}

TestMoveLoop::~TestMoveLoop() = default;

bool TestMoveLoop::IsRunning() const {
  return is_running_;
}

bool TestMoveLoop::RunMoveLoop(bool can_grab_pointer,
                               scoped_refptr<X11Cursor> old_cursor,
                               scoped_refptr<X11Cursor> new_cursor,
                               base::OnceClosure started_callback) {
  is_running_ = true;
  std::move(started_callback).Run();
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
  return true;
}

void TestMoveLoop::UpdateCursor(scoped_refptr<X11Cursor> cursor) {}

void TestMoveLoop::EndMoveLoop() {
  if (is_running_) {
    delegate_->OnMoveLoopEnded();
    is_running_ = false;
    std::move(quit_closure_).Run();
  }
}

///////////////////////////////////////////////////////////////////////////////
// SimpleTestDragDropClient

SimpleTestDragDropClient::SimpleTestDragDropClient(X11Window* window)
    : XDragDropClient(this, static_cast<x11::Window>(window->GetWidget())) {}

SimpleTestDragDropClient::~SimpleTestDragDropClient() = default;

void SimpleTestDragDropClient::SetTopmostXWindow(x11::Window window) {
  target_window_ = window;
}

bool SimpleTestDragDropClient::IsMoveLoopRunning() {
  return loop_->IsRunning();
}

DragOperation SimpleTestDragDropClient::StartDragAndDrop(
    std::unique_ptr<OSExchangeData> data,
    X11Window* source_window,
    int allowed_operations,
    mojom::DragEventSource source) {
  InitDrag(allowed_operations, data.get());

  loop_ = std::make_unique<TestMoveLoop>(this);

  // Cursors are not set. Thus, pass nothing.
  loop_->RunMoveLoop(!source_window->HasCapture(), {}, {}, base::DoNothing());

  auto resulting_operation = negotiated_operation();
  CleanupDrag();
  loop_.reset();
  return resulting_operation;
}

std::optional<gfx::AcceleratedWidget>
SimpleTestDragDropClient::GetDragWidget() {
  return std::nullopt;
}

int SimpleTestDragDropClient::UpdateDrag(const gfx::Point& screen_point) {
  return 0;
}

void SimpleTestDragDropClient::UpdateCursor(
    DragOperation negotiated_operation) {}
void SimpleTestDragDropClient::OnBeginForeignDrag(x11::Window window) {}
void SimpleTestDragDropClient::OnEndForeignDrag() {}
void SimpleTestDragDropClient::OnBeforeDragLeave() {}
DragOperation SimpleTestDragDropClient::PerformDrop() {
  return DragOperation::kNone;
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

TestDragDropClient::TestDragDropClient(X11Window* window)
    : SimpleTestDragDropClient(window),
      source_window_(static_cast<x11::Window>(window->GetWidget())) {}

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
  HandleMouseMovement(gfx::Point(kMouseMoveX, kMouseMoveY), EF_NONE,
                      EventTimeForNow());
}

void TestDragDropClient::SendXClientEvent(
    x11::Window window,
    const x11::ClientMessageEvent& event) {
  auto it = collectors_.find(window);
  if (it != collectors_.end())
    it->second->RecordEvent(event);
}

class TestPlatformWindowDelegate : public PlatformWindowDelegate {
 public:
  TestPlatformWindowDelegate() = default;
  TestPlatformWindowDelegate(const TestPlatformWindowDelegate&) = delete;
  TestPlatformWindowDelegate& operator=(const TestPlatformWindowDelegate&) =
      delete;
  ~TestPlatformWindowDelegate() override = default;

  // PlatformWindowDelegate:
  void OnBoundsChanged(
      const PlatformWindowDelegate::BoundsChange& change) override {}
  void OnDamageRect(const gfx::Rect& damaged_region) override {}
  void DispatchEvent(Event* event) override {}
  void OnCloseRequest() override {}
  void OnClosed() override {}
  void OnWindowStateChanged(PlatformWindowState old_state,
                            PlatformWindowState new_state) override {}
  void OnLostCapture() override {}
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override {}
  void OnWillDestroyAcceleratedWidget() override {}
  void OnAcceleratedWidgetDestroyed() override {}
  void OnActivationChanged(bool active) override {}
  void OnMouseEnter() override {}
  SkPath GetWindowMaskForWindowShapeInPixels() override { return {}; }
};

class TestOSExchangeDataProvideFactory
    : public OSExchangeDataProviderFactoryOzone {
 public:
  TestOSExchangeDataProvideFactory() { SetInstance(this); }
  ~TestOSExchangeDataProvideFactory() override = default;

  std::unique_ptr<OSExchangeDataProvider> CreateProvider() override {
    return std::make_unique<OSExchangeDataProviderX11>();
  }
};

}  // namespace

class X11DragDropClientTest : public testing::Test {
 public:
  X11DragDropClientTest()
      : task_env_(std::make_unique<base::test::TaskEnvironment>(
            base::test::TaskEnvironment::MainThreadType::UI)) {}

  X11DragDropClientTest(const X11DragDropClientTest&) = delete;
  X11DragDropClientTest& operator=(const X11DragDropClientTest&) = delete;

  ~X11DragDropClientTest() override = default;

  DragOperation StartDragAndDrop() {
    auto data(std::make_unique<OSExchangeData>());
    data->SetString(u"Test");
    SkBitmap drag_bitmap;
    drag_bitmap.allocN32Pixels(10, 10);
    drag_bitmap.eraseARGB(0xFF, 0, 0, 0);
    gfx::ImageSkia drag_image(gfx::ImageSkia::CreateFrom1xBitmap(drag_bitmap));
    data->provider().SetDragImage(drag_image, gfx::Vector2d());

    return client_->StartDragAndDrop(std::move(data), window_.get(),
                                     DragDropTypes::DRAG_COPY,
                                     mojom::DragEventSource::kMouse);
  }

  // testing::Test:
  void SetUp() override {
    auto* connection = x11::Connection::Get();
    event_source_ = std::make_unique<X11EventSource>(connection);

    PlatformWindowInitProperties init_params(gfx::Rect(100, 100));
    init_params.type = PlatformWindowType::kWindow;
    window_ = std::make_unique<X11Window>(&delegate_);
    window_->Initialize(std::move(init_params));
    window_->Show(false);

    client_ = std::make_unique<TestDragDropClient>(window_.get());
  }

  void TearDown() override {
    client_.reset();
    window_.reset();
  }

  TestDragDropClient* client() { return client_.get(); }

 private:
  std::unique_ptr<base::test::TaskEnvironment> task_env_;
  std::unique_ptr<X11EventSource> event_source_;

  std::unique_ptr<TestDragDropClient> client_;

  TestOSExchangeDataProvideFactory data_exchange_provider_factory_;
  TestPlatformWindowDelegate delegate_;

  // The window used to initiate drags.
  std::unique_ptr<X11Window> window_;
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
  x11::Connection::Get()->GetArrayProperty(
      client->source_xwindow(), x11::GetAtom("XdndTypeList"), &targets);
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

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&BasicStep2, client(), toplevel));
  DragOperation result = StartDragAndDrop();
  EXPECT_EQ(DragOperation::kCopy, result);

  // Do another drag and drop to test that the data is properly cleaned up as a
  // result of the XdndFinished message.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&BasicStep3, client(), toplevel));
  result = StartDragAndDrop();
  EXPECT_EQ(DragOperation::kCopy, result);
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TargetDoesNotRespondStep2, client()));
  DragOperation result = StartDragAndDrop();
  EXPECT_EQ(DragOperation::kNone, result);
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&QueuePositionStep2, client()));
  DragOperation result = StartDragAndDrop();
  EXPECT_EQ(DragOperation::kCopy, result);
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TargetChangesStep2, client()));
  DragOperation result = StartDragAndDrop();
  EXPECT_EQ(DragOperation::kCopy, result);
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&RejectAfterMouseReleaseStep2, client()));
  DragOperation result = StartDragAndDrop();
  EXPECT_EQ(DragOperation::kNone, result);

  // Repeat the test but reject the drop in the XdndFinished message instead.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&RejectAfterMouseReleaseStep3, client()));
  result = StartDragAndDrop();
  EXPECT_EQ(DragOperation::kNone, result);
}

}  // namespace ui
