// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener_linux.h"

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/nix/xdg_util.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/run_until.h"
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

          ASSERT_EQ(shortcuts->size(), 1u);
          auto& [_, props] = (*shortcuts)[0];
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

  auto listener =
      std::make_unique<GlobalAcceleratorListenerLinux>(mock_bus, kSessionToken);

  auto callback_target = std::make_unique<WeakCommandCallback>();
  ui::CommandMap commands;
  commands[kCommandName] = ui::Command(kCommandName, kShortcutDescription,
                                       /*global=*/false);

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
// registered with the portal, no BindShortcuts call is made. A subsequent
// OnCommandsChanged() that adds a new global command must still bind it.
TEST(GlobalAcceleratorListenerLinuxTest, BindsCommandAddedAfterUpToDateList) {
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

}  // namespace ui
