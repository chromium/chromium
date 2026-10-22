// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener_linux.h"

#include <algorithm>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/nix/xdg_util.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
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
#include "ui/base/ui_base_features.h"
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

class WeakCommandCallback {
 public:
  base::WeakPtr<WeakCommandCallback> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void Run(const std::string&, const std::string&) {}

 private:
  base::WeakPtrFactory<WeakCommandCallback> weak_ptr_factory_{this};
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
  const dbus::ObjectPath session_path(
      base::nix::XdgDesktopPortalSessionPath(kBusName, kSessionToken));

  EXPECT_CALL(*mock_bus,
              GetObjectProxy(GlobalAcceleratorListenerLinux::kPortalServiceName,
                             session_path))
      .WillRepeatedly([&](std::string_view service_name,
                          const dbus::ObjectPath& object_path) {
        if (!session_proxy) {
          session_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
              mock_bus.get(),
              GlobalAcceleratorListenerLinux::kPortalServiceName, object_path);
        }
        return session_proxy.get();
      });

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
                                 base::nix::XdgDesktopPortalSessionPath(
                                     kBusName, kSessionToken)));
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
        GetObjectProxy(GlobalAcceleratorListenerLinux::kPortalServiceName,
                       testing::Ne(session_path)))
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
              EXPECT_EQ(*token, kSessionToken);

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
          ASSERT_TRUE(shortcuts);

          ASSERT_EQ(shortcuts->size(), commands.size());
          const std::string first_id =
              kSessionId + std::string("-") + kCommandName;
          auto shortcut_it =
              std::ranges::find_if(*shortcuts, [&](const auto& shortcut) {
                return std::get<0>(shortcut) == first_id;
              });
          ASSERT_NE(shortcut_it, shortcuts->end());
          auto& props = std::get<1>(*shortcut_it);
          auto trigger_it = props.find("preferred_trigger");
          if (global_shortcut_listener->set_preferred_trigger_) {
            ASSERT_NE(trigger_it, props.end());
            auto trigger_value =
                std::move(trigger_it->second).Take<std::string>();
            ASSERT_TRUE(trigger_value);
            EXPECT_EQ(*trigger_value, "CTRL+a");
          } else {
            EXPECT_EQ(trigger_it, props.end());
          }

          std::string parent_window;
          EXPECT_TRUE(reader.PopString(&parent_window));
          EXPECT_EQ(parent_window, "test_handle");

          auto response = dbus::Response::CreateEmpty();
          dbus::MessageWriter writer(response.get());
          writer.AppendObjectPath(bind_shortcuts_request_proxy->object_path());
          std::move(callback).Run(response.get(), nullptr);
        });

    global_shortcut_listener->OnCommandsChanged(
        kExtensionId, kProfileId, commands, widget,
        base::BindRepeating(&MockObserver::ExecuteCommand,
                            base::Unretained(observer.get())));
    task_environment.RunUntilIdle();
  };

  commands[kCommandName] = ui::Command(kCommandName, kShortcutDescription,
                                       /*global=*/true);
  commands[kCommandName].set_accelerator(
      ui::Accelerator(ui::VKEY_A, ui::EF_CONTROL_DOWN));

  update_commands();

  EXPECT_CALL(
      *session_proxy,
      CallMethodWithErrorResponse(
          MatchMethod(GlobalAcceleratorListenerLinux::kSessionInterface,
                      GlobalAcceleratorListenerLinux::kMethodCloseSession),
          _, _));

  // Adding a global command re-creates the session and binds both commands.
  static constexpr char kSecondCommandName[] = "second_command";
  commands[kSecondCommandName] =
      ui::Command(kSecondCommandName, kShortcutDescription, /*global=*/true);
  commands[kSecondCommandName].set_accelerator(
      ui::Accelerator(ui::VKEY_B, ui::EF_CONTROL_DOWN));

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
  dbus_utils::WriteValue(writer, DbusDictionary());  // options
  activated_callback.Run(&signal);

  // Cleanup
  EXPECT_CALL(
      *session_proxy,
      CallMethodWithErrorResponse(
          MatchMethod(GlobalAcceleratorListenerLinux::kSessionInterface,
                      GlobalAcceleratorListenerLinux::kMethodCloseSession),
          _, _));
  global_shortcut_listener.reset();
  dbus_xdg::SetPortalStateForTesting(dbus_xdg::PortalRegistrarState::kIdle);
}

// Tests that PruneStaleCommands() removes entries whose callbacks are
// cancelled.
TEST(GlobalAcceleratorListenerLinuxTest, PruneStaleCommands) {
  dbus_xdg::SetPortalStateForTesting(dbus_xdg::PortalRegistrarState::kSuccess);
  base::ScopedClosureRunner restore_portal_state(base::BindOnce([] {
    dbus_xdg::SetPortalStateForTesting(dbus_xdg::PortalRegistrarState::kIdle);
  }));

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

  EXPECT_CALL(
      *mock_global_shortcuts_proxy,
      ConnectToSignal(GlobalAcceleratorListenerLinux::kGlobalShortcutsInterface,
                      GlobalAcceleratorListenerLinux::kSignalActivated, _, _))
      .WillOnce(
          [](const std::string& interface_name, const std::string& signal_name,
             dbus::ObjectProxy::SignalCallback,
             dbus::ObjectProxy::OnConnectedCallback on_connected_callback) {
            std::move(on_connected_callback)
                .Run(interface_name, signal_name, true);
          });

  // OnCommandsChanged() creates a portal session for global commands. The
  // session isn't relevant for this test, so never respond.
  std::vector<scoped_refptr<dbus::MockObjectProxy>> request_proxies;
  EXPECT_CALL(
      *mock_bus,
      GetObjectProxy(GlobalAcceleratorListenerLinux::kPortalServiceName,
                     testing::Ne(dbus::ObjectPath(
                         GlobalAcceleratorListenerLinux::kPortalObjectPath))))
      .WillRepeatedly([&](std::string_view service_name,
                          const dbus::ObjectPath& object_path) {
        scoped_refptr<dbus::MockObjectProxy> proxy =
            base::MakeRefCounted<testing::NiceMock<dbus::MockObjectProxy>>(
                mock_bus.get(), std::string(service_name), object_path);
        request_proxies.push_back(proxy);
        return proxy.get();
      });
  EXPECT_CALL(*mock_global_shortcuts_proxy,
              CallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly([] {});

  auto listener =
      std::make_unique<GlobalAcceleratorListenerLinux>(mock_bus, kSessionToken);

  auto callback_target = std::make_unique<WeakCommandCallback>();
  ui::CommandMap commands;
  commands[kCommandName] = ui::Command(kCommandName, kShortcutDescription,
                                       /*global=*/true);

  const auto expected_command_id =
      base::StrCat({kSessionId, "-", kCommandName});

  listener->OnCommandsChanged(
      kExtensionId, kProfileId, commands, gfx::kNullAcceleratedWidget,
      base::BindRepeating(&WeakCommandCallback::Run,
                          callback_target->AsWeakPtr()));

  EXPECT_TRUE(listener->bound_commands_.contains(expected_command_id));

  listener->PruneStaleCommands();
  EXPECT_TRUE(listener->bound_commands_.contains(expected_command_id));

  callback_target.reset();
  listener->PruneStaleCommands();

  EXPECT_FALSE(listener->bound_commands_.contains(expected_command_id));
}

// Regression test: when ListShortcuts reports that every command is already
// registered with a version 1 portal, no BindShortcuts call is made. A
// subsequent OnCommandsChanged() that adds a new global command must still bind
// it.
TEST(GlobalAcceleratorListenerLinuxTest, BindsCommandAddedAfterUpToDateList) {
  dbus_xdg::SetPortalStateForTesting(dbus_xdg::PortalRegistrarState::kSuccess);
  // Binding is only skipped for listed shortcuts on version 1 of the
  // interface.
  dbus_xdg::SetPortalInterfaceVersionForTesting(
      GlobalAcceleratorListenerLinux::kGlobalShortcutsInterface, 1);
  base::ScopedClosureRunner restore_portal_state(base::BindOnce([] {
    dbus_xdg::SetPortalStateForTesting(dbus_xdg::PortalRegistrarState::kIdle);
  }));

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
  EXPECT_CALL(
      *mock_global_shortcuts_proxy,
      ConnectToSignal(GlobalAcceleratorListenerLinux::kGlobalShortcutsInterface,
                      GlobalAcceleratorListenerLinux::kSignalActivated, _, _))
      .WillOnce(
          [](const std::string& interface_name, const std::string& signal_name,
             dbus::ObjectProxy::SignalCallback,
             dbus::ObjectProxy::OnConnectedCallback on_connected_callback) {
            std::move(on_connected_callback)
                .Run(interface_name, signal_name, true);
          });

  auto listener =
      std::make_unique<GlobalAcceleratorListenerLinux>(mock_bus, kSessionToken);

  scoped_refptr<dbus::MockObjectProxy> session_proxy;
  const dbus::ObjectPath session_path(
      base::nix::XdgDesktopPortalSessionPath(kBusName, kSessionToken));
  EXPECT_CALL(*mock_bus,
              GetObjectProxy(GlobalAcceleratorListenerLinux::kPortalServiceName,
                             session_path))
      .WillRepeatedly([&](std::string_view,
                          const dbus::ObjectPath& object_path) {
        if (!session_proxy) {
          session_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
              mock_bus.get(),
              GlobalAcceleratorListenerLinux::kPortalServiceName, object_path);
        }
        return session_proxy.get();
      });

  const std::string first_command_id =
      base::StrCat({kSessionId, "-", kCommandName});
  static constexpr char kSecondCommandName[] = "second_command";
  const std::string second_command_id =
      base::StrCat({kSessionId, "-", kSecondCommandName});

  // Portal requests live at uniquely generated object paths. This returns a
  // GetObjectProxy() action which creates a mock proxy for one in
  // `*request_proxy` and immediately delivers a successful Response signal
  // whose results dictionary is written by `write_results`.
  auto respond_to_request =
      [&](scoped_refptr<dbus::MockObjectProxy>* request_proxy,
          auto write_results) {
        return [&, request_proxy, write_results](
                   std::string_view,
                   const dbus::ObjectPath& object_path) -> dbus::ObjectProxy* {
          *request_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
              mock_bus.get(),
              GlobalAcceleratorListenerLinux::kPortalServiceName, object_path);
          EXPECT_CALL(**request_proxy,
                      ConnectToSignal("org.freedesktop.portal.Request",
                                      "Response", _, _))
              .WillOnce([write_results](
                            const std::string& interface_name,
                            const std::string& signal_name,
                            dbus::ObjectProxy::SignalCallback signal_callback,
                            dbus::ObjectProxy::OnConnectedCallback
                                on_connected_callback) {
                std::move(on_connected_callback)
                    .Run(interface_name, signal_name, true);
                dbus::Signal signal(interface_name, signal_name);
                dbus::MessageWriter writer(&signal);
                writer.AppendUint32(kResponseSuccess);
                write_results(writer);
                signal_callback.Run(&signal);
              });
          return request_proxy->get();
        };
      };

  // A CallMethodWithErrorResponse() action replying with the request path.
  auto reply_with_request_path =
      [](scoped_refptr<dbus::MockObjectProxy>* request_proxy) {
        return [request_proxy](dbus::MethodCall*, int,
                               dbus::ObjectProxy::ResponseOrErrorCallback cb) {
          auto response = dbus::Response::CreateEmpty();
          dbus::MessageWriter writer(response.get());
          writer.AppendObjectPath((*request_proxy)->object_path());
          std::move(cb).Run(response.get(), nullptr);
        };
      };

  auto write_session_handle = [](dbus::MessageWriter& writer) {
    DbusDictionary dict;
    dict.emplace("session_handle", dbus_utils::Variant::Wrap<"s">(
                                       base::nix::XdgDesktopPortalSessionPath(
                                           kBusName, kSessionToken)));
    dbus_utils::WriteValue(writer, dict);
  };

  // The portal reports the first command as already registered, e.g. because
  // the user approved it in a previous browser session.
  auto write_first_command_registered = [&](dbus::MessageWriter& writer) {
    DbusShortcuts shortcuts;
    shortcuts.emplace_back(first_command_id, DbusDictionary());
    DbusDictionary dict;
    dict.emplace("shortcuts",
                 dbus_utils::Variant::Wrap<"a(sa{sv})">(std::move(shortcuts)));
    dbus_utils::WriteValue(writer, dict);
  };

  scoped_refptr<dbus::MockObjectProxy> create_session_request;
  scoped_refptr<dbus::MockObjectProxy> list_shortcuts_request;
  scoped_refptr<dbus::MockObjectProxy> bind_shortcuts_request;

  auto expect_create_session_and_list_shortcuts = [&]() {
    EXPECT_CALL(
        *mock_global_shortcuts_proxy,
        CallMethodWithErrorResponse(
            MatchMethod(
                GlobalAcceleratorListenerLinux::kGlobalShortcutsInterface,
                GlobalAcceleratorListenerLinux::kMethodCreateSession),
            _, _))
        .WillOnce(reply_with_request_path(&create_session_request));
    EXPECT_CALL(
        *mock_global_shortcuts_proxy,
        CallMethodWithErrorResponse(
            MatchMethod(
                GlobalAcceleratorListenerLinux::kGlobalShortcutsInterface,
                GlobalAcceleratorListenerLinux::kMethodListShortcuts),
            _, _))
        .WillOnce(reply_with_request_path(&list_shortcuts_request));
  };

  // Register the first command. Since ListShortcuts reports it as already
  // registered, BindShortcuts must not be called.
  EXPECT_CALL(*mock_bus,
              GetObjectProxy(GlobalAcceleratorListenerLinux::kPortalServiceName,
                             testing::Ne(session_path)))
      .WillOnce(
          respond_to_request(&create_session_request, write_session_handle))
      .WillOnce(respond_to_request(&list_shortcuts_request,
                                   write_first_command_registered));
  expect_create_session_and_list_shortcuts();
  EXPECT_CALL(
      *mock_global_shortcuts_proxy,
      CallMethodWithErrorResponse(
          MatchMethod(GlobalAcceleratorListenerLinux::kGlobalShortcutsInterface,
                      GlobalAcceleratorListenerLinux::kMethodBindShortcuts),
          _, _))
      .Times(0);

  ui::CommandMap commands;
  commands[kCommandName] = ui::Command(kCommandName, kShortcutDescription,
                                       /*global=*/true);
  commands[kCommandName].set_accelerator(
      ui::Accelerator(ui::VKEY_A, ui::EF_CONTROL_DOWN));
  listener->OnCommandsChanged(kExtensionId, kProfileId, commands,
                              gfx::kNullAcceleratedWidget, base::DoNothing());
  EXPECT_TRUE(base::test::RunUntil([&] {
    return listener->bind_state_ ==
           GlobalAcceleratorListenerLinux::BindState::kBound;
  }));

  // Add a second command. The session must be re-created and, since the
  // second command is not registered with the portal, BindShortcuts must be
  // called with it.
  EXPECT_CALL(
      *session_proxy,
      CallMethodWithErrorResponse(
          MatchMethod(GlobalAcceleratorListenerLinux::kSessionInterface,
                      GlobalAcceleratorListenerLinux::kMethodCloseSession),
          _, _));
  EXPECT_CALL(*mock_bus,
              GetObjectProxy(GlobalAcceleratorListenerLinux::kPortalServiceName,
                             testing::Ne(session_path)))
      .WillOnce(
          respond_to_request(&create_session_request, write_session_handle))
      .WillOnce(respond_to_request(&list_shortcuts_request,
                                   write_first_command_registered))
      .WillOnce(respond_to_request(
          &bind_shortcuts_request, [](dbus::MessageWriter& writer) {
            dbus_utils::WriteValue(writer, DbusDictionary());
          }));
  expect_create_session_and_list_shortcuts();
  EXPECT_CALL(
      *mock_global_shortcuts_proxy,
      CallMethodWithErrorResponse(
          MatchMethod(GlobalAcceleratorListenerLinux::kGlobalShortcutsInterface,
                      GlobalAcceleratorListenerLinux::kMethodBindShortcuts),
          _, _))
      .WillOnce([&](dbus::MethodCall* method_call, int,
                    dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        dbus::MessageReader reader(method_call);
        dbus::ObjectPath path;
        EXPECT_TRUE(reader.PopObjectPath(&path));
        auto shortcuts = dbus_utils::ReadValue<DbusShortcuts>(reader);
        ASSERT_TRUE(shortcuts);
        std::vector<std::string> ids;
        for (const auto& [id, _] : *shortcuts) {
          ids.push_back(id);
        }
        EXPECT_THAT(ids, testing::Contains(second_command_id));

        auto reply = reply_with_request_path(&bind_shortcuts_request);
        reply(method_call, 0, std::move(callback));
      });

  commands[kSecondCommandName] =
      ui::Command(kSecondCommandName, kShortcutDescription, /*global=*/true);
  commands[kSecondCommandName].set_accelerator(
      ui::Accelerator(ui::VKEY_B, ui::EF_CONTROL_DOWN));
  listener->OnCommandsChanged(kExtensionId, kProfileId, commands,
                              gfx::kNullAcceleratedWidget, base::DoNothing());
  EXPECT_TRUE(base::test::RunUntil([&] {
    return listener->bind_state_ ==
           GlobalAcceleratorListenerLinux::BindState::kBound;
  }));

  // Cleanup
  EXPECT_CALL(
      *session_proxy,
      CallMethodWithErrorResponse(
          MatchMethod(GlobalAcceleratorListenerLinux::kSessionInterface,
                      GlobalAcceleratorListenerLinux::kMethodCloseSession),
          _, _));
  listener.reset();
}

namespace {

class FakeStateStore : public GlobalAcceleratorListener::StateStore {
 public:
  using Data = base::RefCountedData<base::DictValue>;

  explicit FakeStateStore(scoped_refptr<Data> data) : data_(std::move(data)) {}
  ~FakeStateStore() override = default;

  // GlobalAcceleratorListener::StateStore:
  base::DictValue Load() override { return data_->data.Clone(); }
  void Save(base::DictValue state) override { data_->data = std::move(state); }

 private:
  scoped_refptr<Data> data_;
};

struct BindCall {
  // In the order they were sent, so duplicates can be detected.
  std::vector<std::string> ids;
  std::map<std::string, std::string> descriptions;
  std::map<std::string, std::string> triggers;
};

constexpr uint32_t kResponseCancelled = 1;
constexpr uint32_t kResponseOther = 2;
constexpr char kOtherProfileId[] = "other_profile_id";
// This is computed based on `kExtensionId` and `kOtherProfileId`.
constexpr char kOtherProfilePrefix[] = "21E88D02747614A1D6F5D8535A0B9CB0";
constexpr char kRequestInterface[] = "org.freedesktop.portal.Request";
constexpr char kResponseSignal[] = "Response";

// Returns the portal shortcut ID of `command_name` with `prefix`, which
// defaults to the prefix for `kExtensionId` in `kProfileId`. Hard-coded
// prefixes are used so that tests fail if the ID ever changes.
std::string Id(std::string_view command_name,
               std::string_view prefix = kSessionId) {
  return base::StrCat({prefix, "-", command_name});
}

}  // namespace

// Tests the bind flow against a fake portal that records BindShortcuts calls.
class GlobalAcceleratorListenerLinuxBindTest : public testing::Test {
 public:
  GlobalAcceleratorListenerLinuxBindTest() = default;
  ~GlobalAcceleratorListenerLinuxBindTest() override = default;

  void SetUp() override {
    dbus_xdg::SetPortalStateForTesting(
        dbus_xdg::PortalRegistrarState::kSuccess);

    mock_bus_ = base::MakeRefCounted<testing::NiceMock<dbus::MockBus>>(
        dbus::Bus::Options());
    global_shortcuts_proxy_ =
        base::MakeRefCounted<testing::NiceMock<dbus::MockObjectProxy>>(
            mock_bus_.get(), GlobalAcceleratorListenerLinux::kPortalServiceName,
            dbus::ObjectPath(
                GlobalAcceleratorListenerLinux::kPortalObjectPath));

    ON_CALL(*mock_bus_, GetConnectionName()).WillByDefault(Return(kBusName));
    ON_CALL(*mock_bus_, GetObjectProxy(_, _))
        .WillByDefault([this](std::string_view service_name,
                              const dbus::ObjectPath& object_path) {
          return GetProxy(service_name, object_path);
        });
    ON_CALL(*global_shortcuts_proxy_, CallMethodWithErrorResponse(_, _, _))
        .WillByDefault(
            [this](dbus::MethodCall* method_call, int,
                   dbus::ObjectProxy::ResponseOrErrorCallback callback) {
              HandleMethodCall(method_call, std::move(callback));
            });
  }

  void TearDown() override {
    listener_.reset();
    dbus_xdg::SetPortalStateForTesting(dbus_xdg::PortalRegistrarState::kIdle);
  }

 protected:
  using BindState = GlobalAcceleratorListenerLinux::BindState;

  static ui::Command MakeCommand(const std::string& name,
                                 bool global,
                                 ui::KeyboardCode key = ui::VKEY_7,
                                 std::u16string description = u"Description") {
    ui::Command command(name, description, global);
    command.set_accelerator(
        ui::Accelerator(key, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN));
    return command;
  }

  // Simulates a browser start.
  void CreateListener() {
    listener_.reset();
    listener_ = std::make_unique<GlobalAcceleratorListenerLinux>(mock_bus_,
                                                                 kSessionToken);
  }

  // Loads `commands` the same way ExtensionCommandsGlobalRegistry does.
  void LoadCommands(const std::string& profile_id,
                    std::vector<ui::Command> commands) {
    ui::CommandMap command_map;
    for (auto& command : commands) {
      std::string name = command.command_name();
      command_map[name] = std::move(command);
    }
    GlobalAcceleratorListener* listener = listener_.get();
    listener->SetStateStore(std::make_unique<FakeStateStore>(state_));
    listener->OnCommandsChanged(kExtensionId, profile_id, command_map,
                                gfx::kNullAcceleratedWidget, base::DoNothing());
    WaitForPortalIdle();
  }

  // Waits until every non-deferred portal request has been responded to. The
  // fake posts each response to the main thread without delay, and the
  // listener's follow-up steps (including Session posting its creation
  // callback) are also posted without delay or run synchronously. So once the
  // main thread is idle, the listener has no further requests to make until a
  // deferred bind is responded to or the test calls into it again.
  void WaitForPortalIdle() {
    EXPECT_TRUE(base::test::RunUntil([&] { return pending_responses_ == 0; }));
  }

  // Responds to the BindShortcuts call that was deferred because
  // `defer_bind_responses_` was set. Call WaitForPortalIdle() afterwards.
  void RespondToDeferredBind() {
    ASSERT_TRUE(deferred_bind_response_);
    PostResponse(std::move(deferred_bind_response_));
  }

  size_t CountCreateSessionCalls() const {
    return std::ranges::count(
        method_calls_, GlobalAcceleratorListenerLinux::kMethodCreateSession);
  }

  // Must be called before CreateListener().
  static void SetPortalVersion(uint32_t version) {
    dbus_xdg::SetPortalInterfaceVersionForTesting(
        GlobalAcceleratorListenerLinux::kGlobalShortcutsInterface, version);
  }

  BindState bind_state() const { return listener_->bind_state_; }

  bool IsBound() const { return bind_state() == BindState::kBound; }

  bool HasSession() const { return !!listener_->session_; }

  std::optional<int> StoredState(const std::string& id) const {
    const base::DictValue* entry = state_->data.FindDict(id);
    if (!entry) {
      return std::nullopt;
    }
    return entry->FindInt("state");
  }

  void ClearCalls() {
    method_calls_.clear();
    bind_calls_.clear();
  }

  base::test::ScopedFeatureList feature_list_{
      features::kGlobalShortcutsPortalPreferredTrigger};
  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> global_shortcuts_proxy_;
  std::unique_ptr<GlobalAcceleratorListenerLinux> listener_;
  scoped_refptr<FakeStateStore::Data> state_ =
      base::MakeRefCounted<FakeStateStore::Data>();

  // IDs returned by ListShortcuts. Successfully bound IDs are appended, like a
  // real portal persists them.
  std::vector<std::string> listed_ids_;
  // Response codes for successive BindShortcuts calls. Success if empty.
  std::vector<uint32_t> bind_response_codes_;
  // If set, BindShortcuts responses are held until RespondToDeferredBind().
  bool defer_bind_responses_ = false;

  std::vector<std::string> method_calls_;
  std::vector<BindCall> bind_calls_;

 private:
  dbus::ObjectProxy* GetProxy(std::string_view service_name,
                              const dbus::ObjectPath& object_path) {
    if (object_path ==
        dbus::ObjectPath(GlobalAcceleratorListenerLinux::kPortalObjectPath)) {
      return global_shortcuts_proxy_.get();
    }
    auto& proxy = proxies_[object_path.value()];
    if (!proxy) {
      proxy = base::MakeRefCounted<testing::NiceMock<dbus::MockObjectProxy>>(
          mock_bus_.get(), std::string(service_name), object_path);
      ON_CALL(*proxy, ConnectToSignal(kRequestInterface, kResponseSignal, _, _))
          .WillByDefault(
              [this, path = object_path.value()](
                  const std::string& interface_name,
                  const std::string& signal_name,
                  dbus::ObjectProxy::SignalCallback signal_callback,
                  dbus::ObjectProxy::OnConnectedCallback on_connected) {
                response_callbacks_[path] = signal_callback;
                std::move(on_connected).Run(interface_name, signal_name, true);
              });
    }
    return proxy.get();
  }

  void HandleMethodCall(dbus::MethodCall* method_call,
                        dbus::ObjectProxy::ResponseOrErrorCallback callback) {
    const std::string method = method_call->GetMember();
    method_calls_.push_back(method);

    dbus::MessageReader reader(method_call);
    uint32_t response_code = kResponseSuccess;
    DbusDictionary results;
    std::vector<std::string> bound_ids;
    const bool is_bind =
        method == GlobalAcceleratorListenerLinux::kMethodBindShortcuts;
    if (method == GlobalAcceleratorListenerLinux::kMethodCreateSession) {
      results.emplace(
          "session_handle",
          dbus_utils::Variant::Wrap<"s">(
              base::nix::XdgDesktopPortalSessionPath(kBusName, kSessionToken)));
    } else if (method == GlobalAcceleratorListenerLinux::kMethodListShortcuts) {
      dbus::ObjectPath session_path;
      ASSERT_TRUE(reader.PopObjectPath(&session_path));
      DbusShortcuts shortcuts;
      for (const std::string& id : listed_ids_) {
        DbusDictionary props;
        props.emplace("description",
                      dbus_utils::Variant::Wrap<"s">(std::string("Listed")));
        shortcuts.emplace_back(id, std::move(props));
      }
      results.emplace("shortcuts", dbus_utils::Variant::Wrap<"a(sa{sv})">(
                                       std::move(shortcuts)));
    } else if (is_bind) {
      dbus::ObjectPath session_path;
      ASSERT_TRUE(reader.PopObjectPath(&session_path));
      auto shortcuts = dbus_utils::ReadValue<DbusShortcuts>(reader);
      ASSERT_TRUE(shortcuts);
      std::string parent_window;
      ASSERT_TRUE(reader.PopString(&parent_window));
      BindCall bind_call;
      for (auto& [id, props] : *shortcuts) {
        bind_call.ids.push_back(id);
        if (auto it = props.find("description"); it != props.end()) {
          bind_call.descriptions[id] =
              std::move(it->second).Take<std::string>().value_or("");
        }
        if (auto it = props.find("preferred_trigger"); it != props.end()) {
          bind_call.triggers[id] =
              std::move(it->second).Take<std::string>().value_or("");
        }
      }
      bound_ids = bind_call.ids;
      bind_calls_.push_back(std::move(bind_call));
      if (!bind_response_codes_.empty()) {
        response_code = bind_response_codes_.front();
        bind_response_codes_.erase(bind_response_codes_.begin());
      }
    } else {
      ADD_FAILURE() << "Unexpected method " << method;
      return;
    }

    auto options = dbus_utils::ReadValue<DbusDictionary>(reader);
    ASSERT_TRUE(options);
    auto token_it = options->find("handle_token");
    ASSERT_NE(token_it, options->end());
    auto token = std::move(token_it->second).Take<std::string>();
    ASSERT_TRUE(token);
    const std::string request_path =
        base::nix::XdgDesktopPortalRequestPath(kBusName, *token);

    base::OnceClosure respond = base::BindOnce(
        &GlobalAcceleratorListenerLinuxBindTest::Respond,
        weak_ptr_factory_.GetWeakPtr(), std::move(callback), request_path,
        response_code, std::move(results), std::move(bound_ids));
    if (is_bind && defer_bind_responses_) {
      ASSERT_FALSE(deferred_bind_response_);
      deferred_bind_response_ = std::move(respond);
      return;
    }
    PostResponse(std::move(respond));
  }

  // Responds asynchronously like a real portal.
  void PostResponse(base::OnceClosure respond) {
    ++pending_responses_;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&GlobalAcceleratorListenerLinuxBindTest::RunResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(respond)));
  }

  void RunResponse(base::OnceClosure respond) {
    std::move(respond).Run();
    // Decrement after the listener has handled the response, since it may
    // make another request synchronously.
    --pending_responses_;
  }

  void Respond(dbus::ObjectProxy::ResponseOrErrorCallback callback,
               const std::string& request_path,
               uint32_t response_code,
               DbusDictionary results,
               std::vector<std::string> bound_ids) {
    if (response_code == kResponseSuccess) {
      for (std::string& id : bound_ids) {
        if (!std::ranges::contains(listed_ids_, id)) {
          listed_ids_.push_back(std::move(id));
        }
      }
    }

    auto response = dbus::Response::CreateEmpty();
    dbus::MessageWriter response_writer(response.get());
    response_writer.AppendObjectPath(dbus::ObjectPath(request_path));
    std::move(callback).Run(response.get(), nullptr);

    auto it = response_callbacks_.find(request_path);
    if (it != response_callbacks_.end()) {
      dbus::Signal signal(kRequestInterface, kResponseSignal);
      dbus::MessageWriter writer(&signal);
      writer.AppendUint32(response_code);
      dbus_utils::WriteValue(writer, results);
      it->second.Run(&signal);
    }
  }

  std::map<std::string, scoped_refptr<dbus::MockObjectProxy>> proxies_;
  int pending_responses_ = 0;
  base::OnceClosure deferred_bind_response_;
  std::map<std::string, dbus::ObjectProxy::SignalCallback> response_callbacks_;

  base::WeakPtrFactory<GlobalAcceleratorListenerLinuxBindTest>
      weak_ptr_factory_{this};
};

// Tests that a global command is bound with its description and trigger.
TEST_F(GlobalAcceleratorListenerLinuxBindTest, BindsGlobalCommand) {
  CreateListener();
  LoadCommands(kProfileId, {MakeCommand("global", /*global=*/true)});

  ASSERT_EQ(bind_calls_.size(), 1u);
  const std::string id = Id("global");
  EXPECT_THAT(bind_calls_[0].ids, testing::ElementsAre(id));
  EXPECT_THAT(bind_calls_[0].descriptions,
              testing::Contains(testing::Pair(id, "Description")));
  EXPECT_THAT(bind_calls_[0].triggers,
              testing::Contains(testing::Pair(id, "CTRL+SHIFT+7")));
  EXPECT_TRUE(IsBound());
}

// Tests that no preferred trigger is sent when the feature is disabled.
TEST_F(GlobalAcceleratorListenerLinuxBindTest, NoPreferredTriggerIfDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kGlobalShortcutsPortalPreferredTrigger);
  CreateListener();
  LoadCommands(kProfileId, {MakeCommand("global", /*global=*/true)});

  ASSERT_EQ(bind_calls_.size(), 1u);
  EXPECT_THAT(bind_calls_[0].ids, testing::ElementsAre(Id("global")));
  EXPECT_TRUE(bind_calls_[0].triggers.empty());
}

// With version 1 of the interface, commands that the portal already lists are
// not bound again.
TEST_F(GlobalAcceleratorListenerLinuxBindTest, DoesNotBindListedCommands) {
  SetPortalVersion(1);
  listed_ids_ = {Id("global")};
  CreateListener();
  LoadCommands(kProfileId, {MakeCommand("global", /*global=*/true)});

  EXPECT_TRUE(bind_calls_.empty());
  EXPECT_TRUE(IsBound());
}

// With version 1 of the interface, shortcuts bound before a browser restart are
// not bound again.
TEST_F(GlobalAcceleratorListenerLinuxBindTest, DoesNotRebindAfterRestart) {
  SetPortalVersion(1);
  CreateListener();
  LoadCommands(kProfileId, {MakeCommand("global", /*global=*/true)});
  ASSERT_EQ(bind_calls_.size(), 1u);
  ASSERT_TRUE(IsBound());

  ClearCalls();
  CreateListener();
  LoadCommands(kProfileId, {MakeCommand("global", /*global=*/true)});

  EXPECT_EQ(CountCreateSessionCalls(), 1u);
  EXPECT_TRUE(bind_calls_.empty());
  EXPECT_TRUE(IsBound());
}

// A failed bind, e.g. because the user cancelled the dialog, must not prevent
// binding commands that are added later.
TEST_F(GlobalAcceleratorListenerLinuxBindTest, RebindsAfterFailedBind) {
  bind_response_codes_ = {kResponseCancelled};
  CreateListener();
  LoadCommands(kProfileId, {MakeCommand("first", /*global=*/true)});
  ASSERT_EQ(bind_calls_.size(), 1u);
  EXPECT_TRUE(listed_ids_.empty());

  LoadCommands(kProfileId,
               {MakeCommand("first", /*global=*/true),
                MakeCommand("second", /*global=*/true, ui::VKEY_8)});
  ASSERT_EQ(bind_calls_.size(), 2u);
  // The declined shortcut is not offered again.
  EXPECT_THAT(bind_calls_[1].ids, testing::ElementsAre(Id("second")));
}

// Adding a command while a bind is in flight binds the complete set once the
// first bind finishes.
TEST_F(GlobalAcceleratorListenerLinuxBindTest, CommandAddedDuringBind) {
  defer_bind_responses_ = true;
  CreateListener();
  LoadCommands(kProfileId, {MakeCommand("first", /*global=*/true)});
  ASSERT_EQ(bind_calls_.size(), 1u);
  ASSERT_EQ(bind_state(), BindState::kBindCalled);

  LoadCommands(kProfileId,
               {MakeCommand("first", /*global=*/true),
                MakeCommand("second", /*global=*/true, ui::VKEY_8)});
  ASSERT_EQ(bind_state(), BindState::kNeedsRebind);
  EXPECT_EQ(bind_calls_.size(), 1u);

  defer_bind_responses_ = false;
  RespondToDeferredBind();
  WaitForPortalIdle();

  EXPECT_EQ(CountCreateSessionCalls(), 2u);
  ASSERT_EQ(bind_calls_.size(), 2u);
  EXPECT_THAT(bind_calls_[1].ids,
              testing::UnorderedElementsAre(Id("first"), Id("second")));
  EXPECT_TRUE(IsBound());
}

// Adding a command while a bind is in flight and removing it again before the
// bind finishes doesn't cause a rebind.
TEST_F(GlobalAcceleratorListenerLinuxBindTest,
       CommandAddedAndRemovedDuringBind) {
  defer_bind_responses_ = true;
  CreateListener();
  LoadCommands(kProfileId, {MakeCommand("first", /*global=*/true)});
  ASSERT_EQ(bind_calls_.size(), 1u);
  ASSERT_EQ(bind_state(), BindState::kBindCalled);

  LoadCommands(kProfileId,
               {MakeCommand("first", /*global=*/true),
                MakeCommand("second", /*global=*/true, ui::VKEY_8)});
  ASSERT_EQ(bind_state(), BindState::kNeedsRebind);

  LoadCommands(kProfileId, {MakeCommand("first", /*global=*/true)});
  EXPECT_EQ(bind_state(), BindState::kBindCalled);

  defer_bind_responses_ = false;
  RespondToDeferredBind();
  WaitForPortalIdle();

  EXPECT_EQ(CountCreateSessionCalls(), 1u);
  EXPECT_EQ(bind_calls_.size(), 1u);
  EXPECT_TRUE(IsBound());
}

// Non-global commands must not be bound. Otherwise they are shown in the
// portal dialog, and changing them causes the dialog to be shown again.
TEST_F(GlobalAcceleratorListenerLinuxBindTest, BindsOnlyGlobalCommands) {
  CreateListener();
  LoadCommands(kProfileId,
               {MakeCommand("global", /*global=*/true),
                MakeCommand("local", /*global=*/false, ui::VKEY_K)});

  ASSERT_EQ(bind_calls_.size(), 1u);
  EXPECT_THAT(bind_calls_[0].ids, testing::ElementsAre(Id("global")));
  EXPECT_EQ(StoredState(Id("global")), 1);
  EXPECT_FALSE(StoredState(Id("local")));
}

// Shortcuts reported by ListShortcuts must not be sent twice.
TEST_F(GlobalAcceleratorListenerLinuxBindTest, NoDuplicateIds) {
  listed_ids_ = {Id("global")};
  CreateListener();
  LoadCommands(kProfileId, {MakeCommand("global", /*global=*/true)});

  ASSERT_EQ(bind_calls_.size(), 1u);
  EXPECT_THAT(bind_calls_[0].ids, testing::ElementsAre(Id("global")));
}

// Some backends ignore shortcuts without a description.
TEST_F(GlobalAcceleratorListenerLinuxBindTest, EmptyDescription) {
  CreateListener();
  LoadCommands(kProfileId, {MakeCommand("global", /*global=*/true, ui::VKEY_7,
                                        /*description=*/u"")});
  ASSERT_EQ(bind_calls_.size(), 1u);
  EXPECT_EQ(bind_calls_[0].descriptions[Id("global")], "global");
}

// Listed shortcuts that aren't tracked, e.g. because they were bound by an
// older version, are passed through unless they belong to a loaded extension.
TEST_F(GlobalAcceleratorListenerLinuxBindTest, UntrackedListedShortcuts) {
  listed_ids_ = {"untracked-id", Id("local")};
  CreateListener();
  LoadCommands(kProfileId,
               {MakeCommand("global", /*global=*/true),
                MakeCommand("local", /*global=*/false, ui::VKEY_K)});
  ASSERT_EQ(bind_calls_.size(), 1u);
  EXPECT_THAT(bind_calls_[0].ids,
              testing::UnorderedElementsAre("untracked-id", Id("global")));
  EXPECT_EQ(bind_calls_[0].descriptions["untracked-id"], "Listed");
}

// Every bind replaces the shortcuts that the portal has stored for the app, so
// shortcuts of profiles that aren't loaded must still be bound. Otherwise
// switching between profiles shows the dialog on every start on GNOME.
TEST_F(GlobalAcceleratorListenerLinuxBindTest, KeepsShortcutsOfOtherProfiles) {
  CreateListener();
  LoadCommands(kProfileId, {MakeCommand("global", /*global=*/true)});
  ASSERT_EQ(bind_calls_.size(), 1u);
  EXPECT_THAT(bind_calls_[0].ids, testing::ElementsAre(Id("global")));

  // Like GNOME, don't list any shortcuts, so that the shortcut of the unloaded
  // profile can only come from the state store.
  ClearCalls();
  listed_ids_.clear();
  CreateListener();
  LoadCommands(kOtherProfileId,
               {MakeCommand("global", /*global=*/true, ui::VKEY_8)});
  ASSERT_EQ(bind_calls_.size(), 1u);
  EXPECT_THAT(bind_calls_[0].ids,
              testing::UnorderedElementsAre(Id("global"),
                                            Id("global", kOtherProfilePrefix)));
  // The shortcut of the unloaded profile keeps its description, but no
  // preferred trigger is sent for it.
  EXPECT_EQ(bind_calls_[0].descriptions[Id("global")], "Description");
  EXPECT_FALSE(bind_calls_[0].triggers.contains(Id("global")));
  EXPECT_THAT(bind_calls_[0].triggers,
              testing::Contains(testing::Pair(Id("global", kOtherProfilePrefix),
                                              "CTRL+SHIFT+8")));

  ClearCalls();
  listed_ids_.clear();
  CreateListener();
  LoadCommands(kProfileId, {MakeCommand("global", /*global=*/true)});
  ASSERT_EQ(bind_calls_.size(), 1u);
  EXPECT_THAT(bind_calls_[0].ids,
              testing::UnorderedElementsAre(Id("global"),
                                            Id("global", kOtherProfilePrefix)));
  EXPECT_EQ(StoredState(Id("global")), 1);
  EXPECT_EQ(StoredState(Id("global", kOtherProfilePrefix)), 1);
}

// If the user cancels the dialog, the shortcut is not offered again on the
// next start, but it is offered again when its accelerator changes.
TEST_F(GlobalAcceleratorListenerLinuxBindTest, DeclinedShortcutNotOffered) {
  bind_response_codes_ = {kResponseCancelled};
  CreateListener();
  LoadCommands(kProfileId, {MakeCommand("global", /*global=*/true)});
  EXPECT_EQ(bind_calls_.size(), 1u);
  EXPECT_EQ(StoredState(Id("global")), 2);

  ClearCalls();
  CreateListener();
  LoadCommands(kProfileId, {MakeCommand("global", /*global=*/true)});
  EXPECT_EQ(CountCreateSessionCalls(), 0u);
  EXPECT_TRUE(bind_calls_.empty());

  // Changing the accelerator offers the shortcut again.
  LoadCommands(kProfileId,
               {MakeCommand("global", /*global=*/true, ui::VKEY_9)});
  ASSERT_EQ(bind_calls_.size(), 1u);
  EXPECT_THAT(bind_calls_[0].ids, testing::ElementsAre(Id("global")));
  EXPECT_EQ(StoredState(Id("global")), 1);
}

// Making a declined command non-global and then global again offers it again.
TEST_F(GlobalAcceleratorListenerLinuxBindTest, ToggleGlobalReoffersDeclined) {
  bind_response_codes_ = {kResponseCancelled};
  CreateListener();
  LoadCommands(kProfileId, {MakeCommand("global", /*global=*/true)});
  EXPECT_EQ(StoredState(Id("global")), 2);

  LoadCommands(kProfileId, {MakeCommand("global", /*global=*/false)});
  EXPECT_FALSE(StoredState(Id("global")));

  ClearCalls();
  LoadCommands(kProfileId, {MakeCommand("global", /*global=*/true)});
  ASSERT_EQ(bind_calls_.size(), 1u);
  EXPECT_EQ(StoredState(Id("global")), 1);
}

// Declining a new shortcut must not deactivate previously accepted shortcuts.
TEST_F(GlobalAcceleratorListenerLinuxBindTest, DeclineKeepsKnownShortcuts) {
  CreateListener();
  LoadCommands(kProfileId, {MakeCommand("global", /*global=*/true)});

  // Like GNOME, don't list any shortcuts, so that the known state can only come
  // from the state store.
  ClearCalls();
  listed_ids_.clear();
  bind_response_codes_ = {kResponseCancelled};
  CreateListener();
  LoadCommands(kOtherProfileId,
               {MakeCommand("global", /*global=*/true, ui::VKEY_8)});
  ASSERT_EQ(bind_calls_.size(), 2u);
  EXPECT_THAT(bind_calls_[0].ids,
              testing::UnorderedElementsAre(Id("global"),
                                            Id("global", kOtherProfilePrefix)));
  EXPECT_THAT(bind_calls_[1].ids, testing::ElementsAre(Id("global")));
  EXPECT_EQ(StoredState(Id("global")), 1);
  EXPECT_EQ(StoredState(Id("global", kOtherProfilePrefix)), 2);
}

// Some backends, e.g. xdg-desktop-portal-gnome without GNOME Shell (as used by
// niri), show the dialog but always respond with code 2
// (kRequestCancelledOther). The shortcut must not be offered again on the next
// start.
TEST_F(GlobalAcceleratorListenerLinuxBindTest, OtherResponseDeclinesShortcut) {
  bind_response_codes_ = {kResponseOther};
  CreateListener();
  LoadCommands(kProfileId, {MakeCommand("global", /*global=*/true)});
  ASSERT_EQ(bind_calls_.size(), 1u);
  EXPECT_EQ(StoredState(Id("global")), 2);
  // Nothing is left to bind, so the session is closed.
  EXPECT_FALSE(HasSession());

  ClearCalls();
  CreateListener();
  LoadCommands(kProfileId, {MakeCommand("global", /*global=*/true)});
  EXPECT_EQ(CountCreateSessionCalls(), 0u);
  EXPECT_TRUE(bind_calls_.empty());
}

// If the portal shows the dialog for known shortcuts only, e.g. because its
// stored shortcuts were reset, and it's cancelled, the known shortcuts are
// declined so that the dialog isn't shown on every start.
TEST_F(GlobalAcceleratorListenerLinuxBindTest, CancelDeclinesKnownShortcuts) {
  CreateListener();
  LoadCommands(kProfileId, {MakeCommand("global", /*global=*/true)});
  EXPECT_EQ(StoredState(Id("global")), 1);

  ClearCalls();
  listed_ids_.clear();
  bind_response_codes_ = {kResponseCancelled};
  CreateListener();
  LoadCommands(kProfileId, {MakeCommand("global", /*global=*/true)});
  ASSERT_EQ(bind_calls_.size(), 1u);
  EXPECT_EQ(StoredState(Id("global")), 2);
  EXPECT_FALSE(HasSession());

  ClearCalls();
  CreateListener();
  LoadCommands(kProfileId, {MakeCommand("global", /*global=*/true)});
  EXPECT_EQ(CountCreateSessionCalls(), 0u);
  EXPECT_TRUE(bind_calls_.empty());
}

// Response code 2 may be returned without showing a dialog, e.g. if the backend
// isn't ready yet, so it must not decline known shortcuts. They're bound again
// on the next start.
TEST_F(GlobalAcceleratorListenerLinuxBindTest,
       OtherResponseKeepsKnownShortcuts) {
  CreateListener();
  LoadCommands(kProfileId, {MakeCommand("global", /*global=*/true)});
  EXPECT_EQ(StoredState(Id("global")), 1);

  ClearCalls();
  listed_ids_.clear();
  bind_response_codes_ = {kResponseOther};
  CreateListener();
  LoadCommands(kProfileId, {MakeCommand("global", /*global=*/true)});
  // There's no retry, since nothing was declined.
  ASSERT_EQ(bind_calls_.size(), 1u);
  EXPECT_EQ(StoredState(Id("global")), 1);

  ClearCalls();
  CreateListener();
  LoadCommands(kProfileId, {MakeCommand("global", /*global=*/true)});
  ASSERT_EQ(bind_calls_.size(), 1u);
  EXPECT_THAT(bind_calls_[0].ids, testing::ElementsAre(Id("global")));
  EXPECT_EQ(StoredState(Id("global")), 1);
}

// If the retry without the declined new shortcuts shows the dialog again and
// it's cancelled too, the known shortcuts are declined as well, instead of
// showing the dialog on every start.
TEST_F(GlobalAcceleratorListenerLinuxBindTest,
       CancelledRetryDeclinesKnownShortcuts) {
  CreateListener();
  LoadCommands(kProfileId, {MakeCommand("first", /*global=*/true)});

  ClearCalls();
  listed_ids_.clear();
  bind_response_codes_ = {kResponseCancelled, kResponseCancelled};
  CreateListener();
  LoadCommands(kProfileId,
               {MakeCommand("first", /*global=*/true),
                MakeCommand("second", /*global=*/true, ui::VKEY_8)});
  ASSERT_EQ(bind_calls_.size(), 2u);
  EXPECT_THAT(bind_calls_[0].ids,
              testing::UnorderedElementsAre(Id("first"), Id("second")));
  EXPECT_THAT(bind_calls_[1].ids, testing::ElementsAre(Id("first")));
  EXPECT_EQ(StoredState(Id("first")), 2);
  EXPECT_EQ(StoredState(Id("second")), 2);
  EXPECT_FALSE(HasSession());
}

}  // namespace ui
