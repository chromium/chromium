#pragma once

namespace onnxruntime {
namespace logging {
enum class SinkType {
  BaseSink,
  CompositeSink,
  EtwSink
};
}  // namespace logging
}  // namespace onnxruntime
