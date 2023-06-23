// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/p2p/base/bridge_ice_transport_factory.h"

#include <memory>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"

#include "third_party/webrtc/api/make_ref_counted.h"
#include "third_party/webrtc/p2p/base/active_ice_controller_factory_interface.h"
#include "third_party/webrtc/p2p/base/basic_ice_controller.h"
#include "third_party/webrtc/p2p/base/default_ice_transport_factory.h"
#include "third_party/webrtc/p2p/base/p2p_transport_channel.h"
#include "third_party/webrtc_overrides/p2p/base/bridge_ice_controller.h"

namespace blink {

namespace {

// A factory that constructs a BridgeIceController wrapping over a native
// BasicIceController.
class BasicActiveIceControllerFactory
    : public cricket::ActiveIceControllerFactoryInterface {
 public:
  explicit BasicActiveIceControllerFactory(
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : task_runner_(std::move(task_runner)) {}
  ~BasicActiveIceControllerFactory() override = default;

  std::unique_ptr<cricket::ActiveIceControllerInterface> Create(
      const cricket::ActiveIceControllerFactoryArgs& args) override {
    return std::make_unique<blink::BridgeIceController>(
        task_runner_, args.ice_agent,
        std::make_unique<cricket::BasicIceController>(args.legacy_args));
  }

 private:
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace

BridgeIceTransportFactory::BridgeIceTransportFactory(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : network_task_runner_(std::move(task_runner)) {}

rtc::scoped_refptr<webrtc::IceTransportInterface>
BridgeIceTransportFactory::CreateIceTransport(const std::string& transport_name,
                                              int component,
                                              webrtc::IceTransportInit init) {
  // Check that we're running on the WebRTC network thread before handing it
  // over to the ICE controller factory, because P2PTransportChannel expects to
  // be called only on the current thread.
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  BasicActiveIceControllerFactory factory(network_task_runner_);
  init.set_active_ice_controller_factory(&factory);
  return rtc::make_ref_counted<webrtc::DefaultIceTransport>(
      cricket::P2PTransportChannel::Create(transport_name, component,
                                           std::move(init)));
}

}  // namespace blink
