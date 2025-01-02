// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

namespace onnxruntime {
namespace logging {
// mild violation of naming convention. the 'k' lets us use token concatenation in the macro
// ::onnxruntime::Logging::Severity::k##severity. It's not legal to have ::onnxruntime::Logging::Severity::##severity
// the uppercase makes the LOG macro usage look as expected for passing an enum value as it will be LOGS(logger, ERROR)
enum class Severity {
  kVERBOSE = 0,
  kINFO = 1,
  kWARNING = 2,
  kERROR = 3,
  kFATAL = 4
};

constexpr const char* SEVERITY_PREFIX = "VIWEF";

}  // namespace logging
}  // namespace onnxruntime
