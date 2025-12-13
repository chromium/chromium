// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener_linux.h"

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/nix/xdg_util.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/dbus/utils/read_value.h"
#include "components/dbus/utils/variant.h"
#include "components/dbus/utils/write_value.h"
#include "components/dbus/xdg/portal.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/sha2.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/linux/linux_ui_delegate.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Return;

namespace ui {

namespace {

constexpr uint32_t kResponseSuccess = 0;
constexpr char kBusName[] = ":1.456";
constexpr char kExtensionId[] = "test_extension_id";
constexpr char kProfileId[] = "test_profile_id";
// This is computed based on `kExtensionId` and `kProfileId`. The value should
// not change, otherwise user registered shortcuts will be lost.
constexpr char kSessionId[] = "40E0F983AEACE624C2FE6A78C8E19771";
constexpr char kSessionToken[] = "test_session_token";
constexpr char kCommandName[] = "test_command";
constexpr char16_t kShortcutDescription[] = u"Test Shortcut Description";

MATCHER_P2(MatchMethod, interface, member, "") {
  return arg->GetInterface() == interface && arg->GetMember() == member;
}

class MockLinuxUiDelegate : public LinuxUiDelegate {
 public:
  MockLinuxUiDelegate() = default;
  ~MockLinuxUiDelegate() override = default;

  LinuxUiBackend GetBackend() const override { return LinuxUiBackend::kStub; }

  void SetTransientWindowForParent(gfx::AcceleratedWidget parent,
                                   gfx::AcceleratedWidget transient) override {}

  MOCK_METHOD(void,
              ExportWindowHandle,
              (gfx::AcceleratedWidget window_id,
               base::OnceCallback<void(std::string)> callback),
              (override));
};

}  // namespace

using DbusDictionary = std::map<std::string, dbus_utils::Variant>;
using DbusShortcuts = std::vector<std::tuple<std::string, DbusDictionary>>;

class MockObserver final : public GlobalAcceleratorListener::Observer {
 public:
  void OnKeyPressed(const ui::Accelerator& accelerator) override {
    // GlobalAcceleratorListenerLinux uses ExecuteCommand() instead.
    NOTREACHED();
  }

  MOCK_METHOD2(ExecuteCommand,
               void(const std::string& extension_id,
                    const std::string& command_name));
};

TEST(GlobalAcceleratorListenerLinuxTest, OnCommandsChanged) {
  dbus_xdg::SetPortalStateForTesting(dbus_xdg::PortalRegistrarState::kSuccess);

  // A UI environment is required since GlobalShortcutListener (base class of
  // GlobalAcceleratorListenerLinux) CHECKs that it's running on a UI thread.
  content::BrowserTaskEnvironment task_environment;

  auto mock_bus = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());

  auto mock_dbus_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      mock_bus.get(), DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));

  auto mock_global_shortcuts_proxy =
      base::MakeRefCounted<dbus::MockObjectProxy>(
          mock_bus.get(), GlobalAcceleratorListenerLinux::kPortalServiceName,
          dbus::ObjectPath(GlobalAcceleratorListenerLinux::kPortalObjectPath));

  EXPECT_CALL(*mock_bus, AssertOnOriginThread()).WillRepeatedly([] {});

  EXPECT_CALL(*mock_bus, GetObjectProxy(DBUS_SERVICE_DBUS,
                                        dbus::ObjectPath(DBUS_PATH_DBUS)))
      .WillRepeatedly(Return(mock_dbus_proxy.get()));

  EXPECT_CALL(
      *mock_bus,
      GetObjectProxy(
          GlobalAcceleratorListenerLinux::kPortalServiceName,
          dbus::ObjectPath(GlobalAcceleratorListenerLinux::kPortalObjectPath)))
      .WillRepeatedly(Return(mock_global_shortcuts_proxy.get()));

  EXPECT_CALL(*mock_bus, GetConnectionName()).WillRepeatedly(Return(kBusName));

  // Activated signal
  dbus::ObjectProxy::SignalCallback activated_callback;
  EXPECT_CALL(
      *mock_global_shortcuts_proxy,
      ConnectToSignal(GlobalAcceleratorListenerLinux::kGlobalShortcutsInterface,
                      GlobalAcceleratorListenerLinux::kSignalActivated, _, _))
      .WillOnce(
          [&](const std::string& interface_name, const std::string& signal_name,
              dbus::ObjectProxy::SignalCallback signal_callback,
              dbus::ObjectProxy::OnConnectedCallback on_connected_callback) {
            // Simulate successful connection
            std::move(on_connected_callback)
                .Run(interface_name, signal_name, true);

            // Save the signal callback for later use
            activated_callback = signal_callback;
          });

  auto global_shortcut_listener =
      std::make_unique<GlobalAcceleratorListenerLinux>(mock_bus, kSessionToken);
  auto observer = std::make_unique<MockObserver>();
  scoped_refptr<dbus::MockObjectProxy> session_proxy;
  ui::CommandMap commands;

  // MockLinuxUiDelegate subclasses from LinuxUiDelegate which installs itself
  // as the singleton instance on construction.
  MockLinuxUiDelegate linux_ui_delegate;

  auto update_commands = [&]() {
    // These object proxies have unique generated names, so are initialized when
    // GetObjectProxy() is called.
    scoped_refptr<dbus::MockObjectProxy> create_session_request_proxy;
    scoped_refptr<dbus::MockObjectProxy> list_shortcuts_request_proxy;
    scoped_refptr<dbus::MockObjectProxy> bind_shortcuts_request_proxy;

    auto get_object_proxy_session =
        [&](std::string_view service_name,
            const dbus::ObjectPath& object_path) -> dbus::ObjectProxy* {
      // The first call in the sequence is for the session proxy.
      session_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
          mock_bus.get(), GlobalAcceleratorListenerLinux::kPortalServiceName,
          object_path);
      return session_proxy.get();
    };

    auto get_object_proxy_create_session =
        [&](std::string_view service_name,
            const dbus::ObjectPath& object_path) -> dbus::ObjectProxy* {
      // CreateSession
      create_session_request_proxy =
          base::MakeRefCounted<dbus::MockObjectProxy>(
              mock_bus.get(),
              GlobalAcceleratorListenerLinux::kPortalServiceName, object_path);
      EXPECT_CALL(*create_session_request_proxy, ConnectToSignal(_, _, _, _))
          .WillOnce(
              [&](const std::string& interface_name,
                  const std::string& signal_name,
                  dbus::ObjectProxy::SignalCallback signal_callback,
                  dbus::ObjectProxy::OnConnectedCallback
                      on_connected_callback) {
                EXPECT_EQ(interface_name, "org.freedesktop.portal.Request");
                EXPECT_EQ(signal_name, "Response");

                std::move(on_connected_callback)
                    .Run(interface_name, signal_name, true);

                dbus::Signal signal(interface_name, signal_name);
                dbus::MessageWriter writer(&signal);
                writer.AppendUint32(kResponseSuccess);
                DbusDictionary dict;
                dict.emplace("session_handle",
                             dbus_utils::Variant::Wrap<"s">(
                                 session_proxy->object_path().value()));
                dbus_utils::WriteValue(writer, dict);
                signal_callback.Run(&signal);
              });
      return create_session_request_proxy.get();
    };

    auto get_object_proxy_list_shortcuts =
        [&](std::string_view service_name,
            const dbus::ObjectPath& object_path) -> dbus::ObjectProxy* {
      // ListShortcuts
      list_shortcuts_request_proxy =
          base::MakeRefCounted<dbus::MockObjectProxy>(
              mock_bus.get(),
              GlobalAcceleratorListenerLinux::kPortalServiceName, object_path);
      EXPECT_CALL(*list_shortcuts_request_proxy, ConnectToSignal(_, _, _, _))
          .WillOnce(
              [&](const std::string& interface_name,
                  const std::string& signal_name,
                  dbus::ObjectProxy::SignalCallback signal_callback,
                  dbus::ObjectProxy::OnConnectedCallback
                      on_connected_callback) {
                EXPECT_EQ(interface_name, "org.freedesktop.portal.Request");
                EXPECT_EQ(signal_name, "Response");

                std::move(on_connected_callback)
                    .Run(interface_name, signal_name, true);

                dbus::Signal signal(interface_name, signal_name);
                dbus::MessageWriter writer(&signal);
                writer.AppendUint32(kResponseSuccess);
                // Simulate empty list of shortcuts
                DbusDictionary dict;
                dict.emplace(
                    "shortcuts",
                    dbus_utils::Variant::Wrap<"a(sa{sv})">(DbusShortcuts()));
                dbus_utils::WriteValue(writer, dict);
                signal_callback.Run(&signal);
              });
      return list_shortcuts_request_proxy.get();
    };

    auto get_object_proxy_bind_shortcuts =
        [&](std::string_view service_name,
            const dbus::ObjectPath& object_path) -> dbus::ObjectProxy* {
      // BindShortcuts
      bind_shortcuts_request_proxy =
          base::MakeRefCounted<dbus::MockObjectProxy>(
              mock_bus.get(),
              GlobalAcceleratorListenerLinux::kPortalServiceName, object_path);
      EXPECT_CALL(*bind_shortcuts_request_proxy, ConnectToSignal(_, _, _, _))
          .WillOnce([&](const std::string& interface_name,
                        const std::string& signal_name,
                        dbus::ObjectProxy::SignalCallback signal_callback,
                        dbus::ObjectProxy::OnConnectedCallback
                            on_connected_callback) {
            EXPECT_EQ(interface_name, "org.freedesktop.portal.Request");
            EXPECT_EQ(signal_name, "Response");

            std::move(on_connected_callback)
                .Run(interface_name, signal_name, true);

            dbus::Signal signal(interface_name, signal_name);
            dbus::MessageWriter writer(&signal);
            writer.AppendUint32(kResponseSuccess);
            dbus_utils::WriteValue(writer, DbusDictionary());
            signal_callback.Run(&signal);
          });
      return bind_shortcuts_request_proxy.get();
    };

    EXPECT_CALL(
        *mock_bus,
        GetObjectProxy(GlobalAcceleratorListenerLinux::kPortalServiceName, _))
        .WillOnce(get_object_proxy_session)
        .WillOnce(get_object_proxy_create_session)
        .WillOnce(get_object_proxy_list_shortcuts)
        .WillOnce(get_object_proxy_bind_shortcuts);

    // CreateSession request
    EXPECT_CALL(
        *mock_global_shortcuts_proxy,
        CallMethodWithErrorResponse(
            MatchMethod(
                GlobalAcceleratorListenerLinux::kGlobalShortcutsInterface,
                GlobalAcceleratorListenerLinux::kMethodCreateSession),
            _, _))
        .WillOnce(
            [&](dbus::MethodCall* method_call, int timeout_ms,
                dbus::ObjectProxy::ResponseOrErrorCallback callback) {
              dbus::MessageReader reader(method_call);
              auto options = dbus_utils::ReadValue<DbusDictionary>(reader);
              ASSERT_TRUE(options);
              auto it = options->find("session_handle_token");
              ASSERT_NE(it, options->end());
              auto token = std::move(it->second).Take<std::string>();
              ASSERT_TRUE(token);
              std::string session_path_str =
                  base::nix::XdgDesktopPortalSessionPath(kBusName, *token);
              EXPECT_EQ(dbus::ObjectPath(session_path_str),
                        session_proxy->object_path());

              auto response = dbus::Response::CreateEmpty();
              dbus::MessageWriter writer(response.get());
              writer.AppendObjectPath(
                  create_session_request_proxy->object_path());
              std::move(callback).Run(response.get(), nullptr);
            });

    // ListShortcuts request
    EXPECT_CALL(
        *mock_global_shortcuts_proxy,
        CallMethodWithErrorResponse(
            MatchMethod(
                GlobalAcceleratorListenerLinux::kGlobalShortcutsInterface,
                GlobalAcceleratorListenerLinux::kMethodListShortcuts),
            _, _))
        .WillOnce([&](dbus::MethodCall* method_call, int timeout_ms,
                      dbus::ObjectProxy::ResponseOrErrorCallback callback) {
          dbus::MessageReader reader(method_call);
          dbus::ObjectPath session_path;
          EXPECT_TRUE(reader.PopObjectPath(&session_path));
          auto options = dbus_utils::ReadValue<DbusDictionary>(reader);
          EXPECT_TRUE(options);

          auto response = dbus::Response::CreateEmpty();
          dbus::MessageWriter writer(response.get());
          writer.AppendObjectPath(list_shortcuts_request_proxy->object_path());
          std::move(callback).Run(response.get(), nullptr);
        });

    gfx::AcceleratedWidget widget = static_cast<gfx::AcceleratedWidget>(12345);
    EXPECT_CALL(linux_ui_delegate, ExportWindowHandle(widget, _))
        .WillOnce([](gfx::AcceleratedWidget window_id,
                     base::OnceCallback<void(std::string)> callback) {
          std::move(callback).Run("test_handle");
        });

    // BindShortcuts request
    EXPECT_CALL(
        *mock_global_shortcuts_proxy,
        CallMethodWithErrorResponse(
            MatchMethod(
                GlobalAcceleratorListenerLinux::kGlobalShortcutsInterface,
                GlobalAcceleratorListenerLinux::kMethodBindShortcuts),
            _, _))
        .WillOnce([&](dbus::MethodCall* method_call, int timeout_ms,
                      dbus::ObjectProxy::ResponseOrErrorCallback callback) {
          dbus::MessageReader reader(method_call);
          dbus::ObjectPath session_path;
          EXPECT_TRUE(reader.PopObjectPath(&session_path));
          auto shortcuts = dbus_utils::ReadValue<DbusShortcuts>(reader);
          EXPECT_TRUE(shortcuts);
          std::string parent_window;
          EXPECT_TRUE(reader.PopString(&parent_window));
          EXPECT_EQ(parent_window, "test_handle");

          auto response = dbus::Response::CreateEmpty();
          dbus::MessageWriter writer(response.get());
          writer.AppendObjectPath(bind_shortcuts_request_proxy->object_path());
          std::move(callback).Run(response.get(), nullptr);
        });

    global_shortcut_listener->OnCommandsChanged(
        kExtensionId, kProfileId, commands, widget, observer.get());
  };

  commands[kCommandName] = ui::Command(kCommandName, kShortcutDescription,
                                       /*global=*/true);
  commands[kCommandName].set_accelerator(
      ui::Accelerator(ui::VKEY_A, ui::EF_CONTROL_DOWN));

  update_commands();

  EXPECT_CALL(
      *session_proxy,
      CallMethod(
          MatchMethod(GlobalAcceleratorListenerLinux::kSessionInterface,
                      GlobalAcceleratorListenerLinux::kMethodCloseSession),
          _, _));

  update_commands();

  const std::string expected_command_id =
      kSessionId + std::string("-") + kCommandName;

  // Expect that when the activated signal is received, the observer is invoked
  // with the accelerator group id and the modified command id.
  EXPECT_CALL(*observer, ExecuteCommand(kExtensionId, kCommandName));

  // Simulate the Activated signal using the modified command id.
  dbus::Signal signal(GlobalAcceleratorListenerLinux::kGlobalShortcutsInterface,
                      GlobalAcceleratorListenerLinux::kSignalActivated);
  dbus::MessageWriter writer(&signal);
  writer.AppendObjectPath(session_proxy->object_path());
  writer.AppendString(expected_command_id);
  writer.AppendUint64(0);  // timestamp
  activated_callback.Run(&signal);

  // Cleanup
  EXPECT_CALL(
      *session_proxy,
      CallMethod(
          MatchMethod(GlobalAcceleratorListenerLinux::kSessionInterface,
                      GlobalAcceleratorListenerLinux::kMethodCloseSession),
          _, _));
  global_shortcut_listener.reset();
  dbus_xdg::SetPortalStateForTesting(dbus_xdg::PortalRegistrarState::kIdle);
}

}  // namespace ui
