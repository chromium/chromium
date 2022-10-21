// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/examples/browser/devtools/devtools_server.h"

#include "base/atomicops.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "content/public/common/content_switches.h"
#include "net/base/net_errors.h"
#include "net/socket/tcp_server_socket.h"

namespace webui_examples::devtools {

namespace {

base::subtle::Atomic32 g_last_used_port;

class TCPServerSocketFactory : public content::DevToolsSocketFactory {
 public:
  static std::unique_ptr<content::DevToolsSocketFactory> Create() {
    return std::make_unique<TCPServerSocketFactory>("127.0.0.1", 0);
  }

  TCPServerSocketFactory(const std::string& address, uint16_t port)
      : address_(address), port_(port) {}
  TCPServerSocketFactory(const TCPServerSocketFactory&) = delete;
  TCPServerSocketFactory& operator=(const TCPServerSocketFactory&) = delete;

 private:
  // content::DevToolsSocketFactory.
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    constexpr int kBackLog = 10;
    std::unique_ptr<net::ServerSocket> socket =
        std::make_unique<net::TCPServerSocket>(nullptr, net::NetLogSource());
    if (socket->ListenWithAddressAndPort(address_, port_, kBackLog) != net::OK)
      return nullptr;

    net::IPEndPoint endpoint;
    if (socket->GetLocalAddress(&endpoint) == net::OK)
      base::subtle::NoBarrier_Store(&g_last_used_port, endpoint.port());

    return socket;
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* out_name) override {
    return nullptr;
  }

  const std::string address_;
  const uint16_t port_;
};

}  // namespace

void StartHttpHandler(content::BrowserContext* browser_context) {
  content::DevToolsAgentHost::StartRemoteDebuggingServer(
      TCPServerSocketFactory::Create(), browser_context->GetPath(),
      base::FilePath());

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kRemoteDebuggingPipe)) {
    content::DevToolsAgentHost::StartRemoteDebuggingPipeHandler(
        base::OnceClosure());
  }
}

void StopHttpHandler() {
  content::DevToolsAgentHost::StopRemoteDebuggingServer();
}

int GetHttpHandlerPort() {
  return base::subtle::NoBarrier_Load(&g_last_used_port);
}

}  // namespace webui_examples::devtools
