// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/devtools_server_android.h"

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/android/devtools_auth.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "content/public/common/content_switches.h"
#include "net/base/net_errors.h"
#include "net/socket/unix_domain_server_socket_posix.h"

namespace weblayer {
namespace {
bool g_remote_debugging_enabled = false;

const int kBackLog = 10;

const char kSocketNameFormat[] = "weblayer_devtools_remote_%d";
const char kTetheringSocketName[] = "weblayer_devtools_tethering_%d_%d";

class UnixDomainServerSocketFactory : public content::DevToolsSocketFactory {
 public:
  explicit UnixDomainServerSocketFactory(const std::string& socket_name)
      : socket_name_(socket_name) {}

 private:
  // content::DevToolsAgentHost::ServerSocketFactory.
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    auto socket = std::make_unique<net::UnixDomainServerSocket>(
        base::BindRepeating(&content::CanUserConnectToDevTools),
        true /* use_abstract_namespace */);
    if (socket->BindAndListen(socket_name_, kBackLog) != net::OK)
      return nullptr;

    return std::move(socket);
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* name) override {
    *name = base::StringPrintf(kTetheringSocketName, getpid(),
                               ++last_tethering_socket_);
    auto socket = std::make_unique<net::UnixDomainServerSocket>(
        base::BindRepeating(&content::CanUserConnectToDevTools),
        true /* use_abstract_namespace */);
    if (socket->BindAndListen(*name, kBackLog) != net::OK)
      return nullptr;

    return std::move(socket);
  }

  std::string socket_name_;
  int last_tethering_socket_ = 0;

  DISALLOW_COPY_AND_ASSIGN(UnixDomainServerSocketFactory);
};

}  // namespace

// static
void DevToolsServerAndroid::SetRemoteDebuggingEnabled(bool enabled) {
  if (g_remote_debugging_enabled == enabled)
    return;

  g_remote_debugging_enabled = enabled;
  if (enabled) {
    auto factory = std::make_unique<UnixDomainServerSocketFactory>(
        base::StringPrintf(kSocketNameFormat, getpid()));
    content::DevToolsAgentHost::StartRemoteDebuggingServer(
        std::move(factory), base::FilePath(), base::FilePath());
  } else {
    content::DevToolsAgentHost::StopRemoteDebuggingServer();
  }
}

// static
bool DevToolsServerAndroid::GetRemoteDebuggingEnabled() {
  return g_remote_debugging_enabled;
}

}  // namespace weblayer
