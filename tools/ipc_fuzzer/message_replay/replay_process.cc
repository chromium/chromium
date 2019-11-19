// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/ipc_fuzzer/message_replay/replay_process.h"

#include <limits.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/common/connection_filter.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "ipc/ipc.mojom.h"
#include "ipc/ipc_channel_mojo.h"
#include "mojo/core/embedder/configuration.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/invitation.h"
#include "services/service_manager/embedder/switches.h"

#if defined(OS_POSIX)
#include "base/posix/global_descriptors.h"
#include "services/service_manager/embedder/descriptors.h"
#endif

namespace ipc_fuzzer {
namespace {

class IPCChannelBootstrapper : public content::ConnectionFilter {
 public:
  explicit IPCChannelBootstrapper(
      mojo::ScopedMessagePipeHandle bootstrap_handle)
      : bootstrap_handle_(std::move(bootstrap_handle)) {}

 private:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle* interface_pipe,
                       service_manager::Connector* connector) override {
    if (interface_name != IPC::mojom::ChannelBootstrap::Name_)
      return;

    DCHECK(bootstrap_handle_.is_valid());
    mojo::FuseMessagePipes(std::move(*interface_pipe),
                           std::move(bootstrap_handle_));
  }

  mojo::ScopedMessagePipeHandle bootstrap_handle_;

  DISALLOW_COPY_AND_ASSIGN(IPCChannelBootstrapper);
};

}  // namespace

void InitializeMojo() {
  mojo::core::Configuration config;
  config.max_message_num_bytes = 64 * 1024 * 1024;
  mojo::core::Init(config);
}

mojo::IncomingInvitation InitializeMojoIPCChannel() {
  mojo::PlatformChannelEndpoint endpoint;
#if defined(OS_WIN)
  endpoint = mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
      *base::CommandLine::ForCurrentProcess());
#elif defined(OS_POSIX)
  endpoint = mojo::PlatformChannelEndpoint(mojo::PlatformHandle(
      base::ScopedFD(base::GlobalDescriptors::GetInstance()->Get(
          service_manager::kMojoIPCChannel))));
#endif
  CHECK(endpoint.is_valid());
  return mojo::IncomingInvitation::Accept(std::move(endpoint));
}

ReplayProcess::ReplayProcess()
    : io_thread_("Chrome_ChildIOThread"),
      shutdown_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                      base::WaitableEvent::InitialState::NOT_SIGNALED),
      message_index_(0) {}

ReplayProcess::~ReplayProcess() {
  channel_.reset();

  // Signal this event before shutting down the service process. That way all
  // background threads can cleanup.
  shutdown_event_.Signal();
  io_thread_.Stop();
}

bool ReplayProcess::Initialize(int argc, const char** argv) {
  base::CommandLine::Init(argc, argv);

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kIpcFuzzerTestcase)) {
    LOG(ERROR) << "This binary shouldn't be executed directly, "
               << "please use tools/ipc_fuzzer/scripts/play_testcase.py";
    return false;
  }

  // Log to both stderr and file destinations.
  logging::SetMinLogLevel(logging::LOG_ERROR);
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_ALL;
  settings.log_file_path = FILE_PATH_LITERAL("ipc_replay.log");
  logging::InitLogging(settings);

  // Make sure to initialize Mojo before starting the IO thread.
  InitializeMojo();

  io_thread_.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0));

#if defined(OS_POSIX)
  base::GlobalDescriptors* g_fds = base::GlobalDescriptors::GetInstance();
  g_fds->Set(service_manager::kMojoIPCChannel,
             service_manager::kMojoIPCChannel +
                 base::GlobalDescriptors::kBaseDescriptor);
#endif

  mojo_ipc_support_.reset(new mojo::core::ScopedIPCSupport(
      io_thread_.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST));
  mojo_invitation_ =
      std::make_unique<mojo::IncomingInvitation>(InitializeMojoIPCChannel());

  return true;
}

void ReplayProcess::OpenChannel() {
  DCHECK(mojo_invitation_);
  service_manager_connection_ = content::ServiceManagerConnection::Create(
      service_manager::mojom::ServiceRequest(
          mojo_invitation_->ExtractMessagePipe(
              base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                  service_manager::switches::kServiceRequestChannelToken))),
      io_thread_.task_runner());
  mojo::MessagePipe ipc_pipe;
  service_manager_connection_->AddConnectionFilter(
      std::make_unique<IPCChannelBootstrapper>(std::move(ipc_pipe.handle0)));
  service_manager_connection_->Start();
  channel_ = IPC::ChannelProxy::Create(
      IPC::ChannelMojo::CreateClientFactory(
          std::move(ipc_pipe.handle1), io_thread_.task_runner(),
          base::ThreadTaskRunnerHandle::Get()),
      this, io_thread_.task_runner(), base::ThreadTaskRunnerHandle::Get());
}

bool ReplayProcess::OpenTestcase() {
  base::FilePath path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kIpcFuzzerTestcase);
  return MessageFile::Read(path, &messages_);
}

void ReplayProcess::SendNextMessage() {
  if (message_index_ >= messages_.size()) {
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
    return;
  }

  std::unique_ptr<IPC::Message> message =
      std::move(messages_[message_index_++]);

  if (!channel_->Send(message.release())) {
    LOG(ERROR) << "ChannelProxy::Send() failed after "
               << message_index_ << " messages";
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }
}

void ReplayProcess::Run() {
  base::RepeatingTimer timer;
  timer.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(1),
              base::BindRepeating(&ReplayProcess::SendNextMessage,
                                  base::Unretained(this)));
  base::RunLoop().Run();
}

bool ReplayProcess::OnMessageReceived(const IPC::Message& msg) {
  return true;
}

void ReplayProcess::OnChannelError() {
  LOG(ERROR) << "Channel error, quitting after "
             << message_index_ << " messages";
  base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

}  // namespace ipc_fuzzer
