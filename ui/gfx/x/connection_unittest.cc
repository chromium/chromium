// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/connection.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/supports_user_data.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/event_observer.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/shm.h"
#include "ui/gfx/x/xkb.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {

namespace {

Window CreateWindow(Connection* connection) {
  Window window = connection->GenerateId<Window>();
  auto create_window_future = connection->CreateWindow({
      .depth = connection->default_root_depth().depth,
      .wid = window,
      .parent = connection->default_screen().root,
      .width = 1,
      .height = 1,
      .override_redirect = Bool32(true),
  });
  auto create_window_response = create_window_future.Sync();
  EXPECT_FALSE(create_window_response.error);
  return window;
}

std::vector<uint32_t> GetKeysymsForRange(const Connection& connection,
                                         KeyCode min_keycode,
                                         KeyCode max_keycode) {
  std::vector<uint32_t> keysyms;
  keysyms.reserve(
      (static_cast<int>(max_keycode) - static_cast<int>(min_keycode) + 1) * 2);
  for (int keycode = static_cast<int>(min_keycode);
       keycode <= static_cast<int>(max_keycode); ++keycode) {
    keysyms.push_back(
        connection.KeycodeToKeysym(static_cast<KeyCode>(keycode), 0));
    keysyms.push_back(
        connection.KeycodeToKeysym(static_cast<KeyCode>(keycode),
                                   static_cast<uint32_t>(KeyButMask::Shift)));
  }
  return keysyms;
}

}  // namespace

// Connection setup and teardown.
TEST(X11ConnectionTest, Basic) {
  Connection connection;
  ASSERT_TRUE(connection.Ready());
}

TEST(X11ConnectionTest, PartialKeyboardMappingPreservesKeycodeRange) {
  Connection connection;
  ASSERT_TRUE(connection.Ready());

  const KeyCode min_keycode = connection.setup().min_keycode;
  const KeyCode max_keycode = connection.setup().max_keycode;
  const auto keysyms = GetKeysymsForRange(connection, min_keycode, max_keycode);

  connection.DispatchEvent(Event(false, MappingNotifyEvent{
                                            .request = Mapping::Keyboard,
                                            .first_keycode = max_keycode,
                                            .count = 1,
                                        }));

  ASSERT_EQ(connection.setup().min_keycode, min_keycode);
  ASSERT_EQ(connection.setup().max_keycode, max_keycode);
  EXPECT_EQ(GetKeysymsForRange(connection, min_keycode, max_keycode), keysyms);
}

TEST(X11ConnectionTest, ModifierMappingPreservesKeycodeRange) {
  Connection connection;
  ASSERT_TRUE(connection.Ready());

  const KeyCode min_keycode = connection.setup().min_keycode;
  const KeyCode max_keycode = connection.setup().max_keycode;
  const auto keysyms = GetKeysymsForRange(connection, min_keycode, max_keycode);

  // The keycode fields are not defined for modifier mapping notifications.
  connection.DispatchEvent(
      Event(false, MappingNotifyEvent{.request = Mapping::Modifier}));

  ASSERT_EQ(connection.setup().min_keycode, min_keycode);
  ASSERT_EQ(connection.setup().max_keycode, max_keycode);
  EXPECT_EQ(GetKeysymsForRange(connection, min_keycode, max_keycode), keysyms);
}

TEST(X11ConnectionTest, NewKeyboardNotifyUpdatesKeycodeRange) {
  Connection connection;
  ASSERT_TRUE(connection.Ready());

  const KeyCode min_keycode = connection.setup().min_keycode;
  const KeyCode max_keycode = connection.setup().max_keycode;
  const auto keysyms = GetKeysymsForRange(connection, min_keycode, max_keycode);
  const auto new_min_keycode =
      static_cast<KeyCode>(static_cast<int>(min_keycode) + 1);
  ASSERT_LT(new_min_keycode, max_keycode);

  connection.DispatchEvent(Event(false, Xkb::NewKeyboardNotifyEvent{
                                            .minKeyCode = new_min_keycode,
                                            .maxKeyCode = max_keycode,
                                            .oldMinKeyCode = min_keycode,
                                            .oldMaxKeyCode = max_keycode,
                                            .changed = Xkb::NKNDetail::Keycodes,
                                        }));

  EXPECT_EQ(connection.setup().min_keycode, new_min_keycode);
  EXPECT_EQ(connection.setup().max_keycode, max_keycode);

  connection.DispatchEvent(Event(false, Xkb::NewKeyboardNotifyEvent{
                                            .minKeyCode = min_keycode,
                                            .maxKeyCode = max_keycode,
                                            .oldMinKeyCode = new_min_keycode,
                                            .oldMaxKeyCode = max_keycode,
                                            .changed = Xkb::NKNDetail::Keycodes,
                                        }));
  ASSERT_EQ(connection.setup().min_keycode, min_keycode);
  ASSERT_EQ(connection.setup().max_keycode, max_keycode);
  EXPECT_EQ(GetKeysymsForRange(connection, min_keycode, max_keycode), keysyms);
}

TEST(X11ConnectionTest, Request) {
  Connection connection;
  ASSERT_TRUE(connection.Ready());

  Window window = CreateWindow(&connection);

  auto attributes = connection.GetWindowAttributes({window}).Sync();
  ASSERT_TRUE(attributes);
  EXPECT_EQ(attributes->map_state, MapState::Unmapped);
  EXPECT_TRUE(attributes->override_redirect);

  auto geometry = connection.GetGeometry(window).Sync();
  ASSERT_TRUE(geometry);
  EXPECT_EQ(geometry->x, 0);
  EXPECT_EQ(geometry->y, 0);
  EXPECT_EQ(geometry->width, 1u);
  EXPECT_EQ(geometry->height, 1u);
}

TEST(X11ConnectionTest, Event) {
  Connection connection;
  ASSERT_TRUE(connection.Ready());

  Window window = CreateWindow(&connection);

  auto cwa_future = connection.ChangeWindowAttributes({
      .window = window,
      .event_mask = EventMask::PropertyChange,
  });
  EXPECT_FALSE(cwa_future.Sync().error);

  std::vector<uint8_t> data{0};
  auto prop_future = connection.ChangeProperty({
      .window = static_cast<Window>(window),
      .property = Atom::WM_NAME,
      .type = Atom::STRING,
      .format = CHAR_BIT,
      .data_len = base::checked_cast<uint32_t>(data.size()),
      .data = base::MakeRefCounted<base::RefCountedBytes>(std::move(data)),
  });
  EXPECT_FALSE(prop_future.Sync().error);

  connection.ReadResponses();
  ASSERT_EQ(connection.events().size(), 1u);
  auto* prop = connection.events().front().As<PropertyNotifyEvent>();
  ASSERT_TRUE(prop);
  EXPECT_EQ(prop->atom, Atom::WM_NAME);
  EXPECT_EQ(prop->state, Property::NewValue);
}

TEST(X11ConnectionTest, Error) {
  Connection connection;
  ASSERT_TRUE(connection.Ready());

  Window invalid_window = connection.GenerateId<Window>();

  auto geometry = connection.GetGeometry(invalid_window).Sync();
  ASSERT_FALSE(geometry);
  Error* error = geometry.error.get();
  ASSERT_TRUE(error);
  EXPECT_FALSE(error->As<RequestError>());
  EXPECT_FALSE(error->As<ValueError>());
  EXPECT_FALSE(error->As<WindowError>());
  auto* drawable_error = error->As<DrawableError>();
  ASSERT_TRUE(drawable_error);
  EXPECT_EQ(drawable_error->bad_value, static_cast<uint32_t>(invalid_window));

  const Error* const_error = error;
  EXPECT_EQ(const_error->As<DrawableError>(), drawable_error);
  EXPECT_FALSE(const_error->As<WindowError>());

  if (connection.shm().present()) {
    Shm::Seg invalid_seg = connection.GenerateId<Shm::Seg>();
    auto detach = connection.shm().Detach({invalid_seg}).Sync();
    Error* shm_error = detach.error.get();
    ASSERT_TRUE(shm_error);
    EXPECT_FALSE(shm_error->As<ValueError>());
    EXPECT_FALSE(shm_error->As<DrawableError>());
    auto* bad_seg_error = shm_error->As<Shm::BadSegError>();
    ASSERT_TRUE(bad_seg_error);
    EXPECT_EQ(bad_seg_error->bad_value, static_cast<uint32_t>(invalid_seg));
  }
}

TEST(X11ConnectionTest, UserDataDestroyedBeforeConnection) {
  class ObserverUserData : public base::SupportsUserData::Data,
                           public EventObserver {
   public:
    ObserverUserData(Connection* connection,
                     scoped_refptr<base::RefCountedData<bool>> destroyed_ready)
        : connection_(connection),
          window_(CreateWindow(connection)),
          destroyed_ready_(std::move(destroyed_ready)) {
      connection_->AddEventObserver(this);
    }

    ~ObserverUserData() override {
      destroyed_ready_->data = connection_->Ready();
      connection_->DestroyWindow({window_});
      connection_->RemoveEventObserver(this);
    }

    void OnEvent(const Event&) override {}

   private:
    raw_ptr<Connection> connection_;
    Window window_;
    scoped_refptr<base::RefCountedData<bool>> destroyed_ready_;
  };

  static const int kKey = 0;
  auto destroyed_while_ready =
      base::MakeRefCounted<base::RefCountedData<bool>>(false);
  {
    Connection connection;
    ASSERT_TRUE(connection.Ready());
    connection.SetUserData(&kKey, std::make_unique<ObserverUserData>(
                                      &connection, destroyed_while_ready));
  }
  EXPECT_TRUE(destroyed_while_ready->data);

  // Verify DetachFromSequence() also detaches SupportsUserData so a cloned
  // connection can attach and destroy user data on another thread.
  Connection main_connection;
  ASSERT_TRUE(main_connection.Ready());
  auto cloned = main_connection.Clone();
  cloned->DetachFromSequence();

  auto thread_destroyed_while_ready =
      base::MakeRefCounted<base::RefCountedData<bool>>(false);
  base::Thread worker("X11ConnectionUserDataTest");
  ASSERT_TRUE(worker.Start());
  worker.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](std::unique_ptr<Connection> conn,
                        scoped_refptr<base::RefCountedData<bool>> out_ready) {
                       ASSERT_TRUE(conn->Ready());
                       conn->SetUserData(&kKey,
                                         std::make_unique<ObserverUserData>(
                                             conn.get(), std::move(out_ready)));
                     },
                     std::move(cloned), thread_destroyed_while_ready));
  worker.Stop();
  EXPECT_TRUE(thread_destroyed_while_ready->data);
}

TEST(X11ConnectionTest, LargeQueryTree) {
  Connection connection;
  ASSERT_TRUE(connection.Ready());

  Window root = CreateWindow(&connection);
  for (size_t i = 0; i < 0x10000; i++) {
    connection.CreateWindow({
        .depth = connection.default_root_depth().depth,
        .wid = connection.GenerateId<Window>(),
        .parent = root,
        .width = 1,
        .height = 1,
        .override_redirect = Bool32(true),
    });
  }

  // Ensure large QueryTree requests don't cause a crash.
  connection.QueryTree(root).Sync();
}

TEST(X11ConnectionTest, GetPropertyReplyValidation) {
  // Simulate a malicious response with format 39.
  // The reply length is in 4-byte units, starting from after the first 32
  // bytes. A GetProperty reply has a fixed size of 32 bytes followed by the
  // value.
  std::vector<uint8_t> data(32, 0);
  data[0] = 1;   // response_type: Reply
  data[1] = 39;  // format: 39 (Invalid, should be 8, 16, or 32)
  data[10] = 0;  // length: 0
  data[11] = 0;

  ReadBuffer buf(x11::ThrowAwaySizeRefCountedMemory::From(std::move(data)));
  auto reply = detail::ReadReply<GetPropertyReply>(&buf);
  EXPECT_FALSE(reply);
}

TEST(X11ConnectionTest, GetPropertyReplyValid) {
  // Simulate a valid response with format 32.
  std::vector<uint8_t> data(32, 0);
  data[0] = 1;   // response_type: Reply
  data[1] = 32;  // format: 32
  data[10] = 0;  // length: 0
  data[11] = 0;
  data[16] = 0;  // type: None
  data[24] = 0;  // value_len: 0

  ReadBuffer buf(x11::ThrowAwaySizeRefCountedMemory::From(std::move(data)));
  auto reply = detail::ReadReply<GetPropertyReply>(&buf);
  ASSERT_TRUE(reply);
  EXPECT_EQ(reply->format, 32);
}

TEST(X11ConnectionTest, WmSupportsEwmhSync) {
  Connection connection;
  ASSERT_TRUE(connection.Ready());
  // Calling GetWmName() and WmSupportsHint() immediately before event dispatch
  // should work synchronously.
  EXPECT_FALSE(connection.GetWmName().empty());
  EXPECT_TRUE(
      connection.WmSupportsHint(connection.GetAtom("_NET_ACTIVE_WINDOW")));
}

}  // namespace x11
