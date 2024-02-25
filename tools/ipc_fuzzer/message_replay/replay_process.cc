// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/ipc_fuzzer/message_replay/replay_process.h"

#include <limits.h>

#include <string>
#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "content/common/child_process.mojom-test-utils.h"
#include "content/common/content_constants_internal.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc.mojom.h"
#include "ipc/ipc_channel_mojo.h"
#include "mojo/core/embedder/configuration.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/invitation.h"

#if BUILDFLAG(IS_POSIX)
#include "base/posix/global_descriptors.h"
#include "content/public/common/content_descriptors.h"
#endif

namespace ipc_fuzzer {

namespace {

// Used to simulate a basic child process IPC endpoint and bootstrap the legacy
// IPC channel driven by this process.
class FakeChildProcessImpl
    : public content::mojom::ChildProcessInterceptorForTesting {
 public:
  FakeChildProcessImpl() {
    std::ignore = disconnected_process_.BindNewPipeAndPassReceiver();
  }

  // content::mojom::ChildProcessInterceptorForTesting overrides:
  content::mojom::ChildProcess* GetForwardingInterface() override {
    return disconnected_process_.get();
  }

 private:
  mojo::Remote<content::mojom::ChildProcess> disconnected_process_;
};

}  // namespace

void InitializeMojo() {
  mojo::core::Configuration config;
  config.max_message_num_bytes = 64 * 1024 * 1024;
  mojo::core::Init(config);
}

mojo::IncomingInvitation InitializeMojoIPCChannel() {
  mojo::PlatformChannelEndpoint endpoint;
#if BUILDFLAG(IS_WIN)
  endpoint = mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
      *base::CommandLine::ForCurrentProcess());
#elif BUILDFLAG(IS_POSIX)
  endpoint = mojo::PlatformChannelEndpoint(mojo::PlatformHandle(base::ScopedFD(
      base::GlobalDescriptors::GetInstance()->Get(kMojoIPCChannel))));
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
  logging::SetMinLogLevel(logging::LOGGING_ERROR);
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_ALL;
  settings.log_file_path = FILE_PATH_LITERAL("ipc_replay.log");
  logging::InitLogging(settings);

  // Make sure to initialize Mojo before starting the IO thread.
  InitializeMojo();

  io_thread_.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0));

#if BUILDFLAG(IS_POSIX)
  base::GlobalDescriptors* g_fds = base::GlobalDescriptors::GetInstance();
  g_fds->Set(kMojoIPCChannel,
             kMojoIPCChannel + base::GlobalDescriptors::kBaseDescriptor);
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
  mojo::ScopedMessagePipeHandle child_process_pipe_for_receiver =
      mojo_invitation_->ExtractMessagePipe(
          content::kChildProcessReceiverAttachmentName);
  mojo::ScopedMessagePipeHandle legacy_ipc_bootstrap_pipe =
      mojo_invitation_->ExtractMessagePipe(
          content::kLegacyIpcBootstrapAttachmentName);
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<FakeChildProcessImpl>(),
      mojo::PendingReceiver<content::mojom::ChildProcess>(
          std::move(child_process_pipe_for_receiver)));
  channel_ = IPC::ChannelProxy::Create(
      IPC::ChannelMojo::CreateClientFactory(
          std::move(legacy_ipc_bootstrap_pipe), io_thread_.task_runner(),
          base::SingleThreadTaskRunner::GetCurrentDefault()),
      this, io_thread_.task_runner(),
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

bool ReplayProcess::OpenTestcase() {
  base::FilePath path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kIpcFuzzerTestcase);
  return MessageFile::Read(path, &messages_);
}

void ReplayProcess::SendNextMessage() {
  if (message_index_ >= messages_.size()) {
    loop_.QuitWhenIdle();
    return;
  }

  std::unique_ptr<IPC::Message> message =
      std::move(messages_[message_index_++]);

  if (!channel_->Send(message.release())) {
    LOG(ERROR) << "ChannelProxy::Send() failed after "
               << message_index_ << " messages";
    loop_.QuitWhenIdle();
  }
}

void ReplayProcess::Run() {
  base::RepeatingTimer timer;
  timer.Start(FROM_HERE, base::Milliseconds(1),
              base::BindRepeating(&ReplayProcess::SendNextMessage,
                                  base::Unretained(this)));
  loop_.Run();
}

bool ReplayProcess::OnMessageReceived(const IPC::Message& msg) {
  return true;
}

void ReplayProcess::OnChannelError() {
  LOG(ERROR) << "Channel error, quitting after "
             << message_index_ << " messages";
  loop_.QuitWhenIdle();
}

}  // namespace ipc_fuzzer
