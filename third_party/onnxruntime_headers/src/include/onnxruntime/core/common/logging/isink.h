// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>

#include "core/common/logging/logging.h"
#include "core/common/logging/sink_types.h"

namespace onnxruntime {
namespace logging {
class ISink {
 public:
  explicit ISink(SinkType type = SinkType::BaseSink) : type_(type) {}

  SinkType GetType() const { return type_; }

  /**
     Sends the message to the sink.
     @param timestamp The timestamp.
     @param logger_id The logger identifier.
     @param message The captured message.
  */
  void Send(const Timestamp& timestamp, const std::string& logger_id, const Capture& message) {
    SendImpl(timestamp, logger_id, message);
  }

  /**
    Sends a Profiling Event Record to the sink.
    @param Profiling Event Record
  */
  virtual void SendProfileEvent(profiling::EventRecord&) const {};

  virtual ~ISink() = default;

 private:
  SinkType type_;

  // Make Code Analysis happy by disabling all for now. Enable as needed.
  ORT_DISALLOW_COPY_ASSIGNMENT_AND_MOVE(ISink);

  virtual void SendImpl(const Timestamp& timestamp, const std::string& logger_id, const Capture& message) = 0;
};
}  // namespace logging
}  // namespace onnxruntime
