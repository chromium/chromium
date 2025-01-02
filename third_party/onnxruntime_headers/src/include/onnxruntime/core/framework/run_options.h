// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <atomic>

#include "core/common/inlined_containers_fwd.h"
#include "core/session/onnxruntime_c_api.h"
#include "core/framework/config_options.h"

namespace onnxruntime {
namespace lora {
class LoraAdapter;
}
}  // namespace onnxruntime

/**
 * Configuration information for a Run call.
 */
struct OrtRunOptions {
  /// Log severity.  See https://github.com/microsoft/onnxruntime/blob/main/include/onnxruntime/core/common/logging/severity.h
  /// Default = -1 (use the log severity from the InferenceSession that the Run is for).
  int run_log_severity_level = -1;
  int run_log_verbosity_level = 0;  ///< VLOG level if debug build and run_log_severity_level is 0 (VERBOSE).
  std::string run_tag;              ///< A tag for the Run() calls using this.

  // Set to 'true' to ensure the termination of all the outstanding Run() calls
  // that use this OrtRunOptions instance. Some of the outstanding Run() calls may
  // be forced to terminate with an error status.
  bool terminate = false;

  // Set to 'true' to run only the nodes from feeds to required fetches.
  // So it is possible that only some of the nodes are executed.
  bool only_execute_path_to_fetches = false;

#ifdef ENABLE_TRAINING
  // Used by onnxruntime::training::TrainingSession. This class is now deprecated.
  // Delete training_mode when TrainingSession is deleted.
  // Set to 'true' to run in training mode.
  bool training_mode = true;
#endif

  // Stores the configurations for this run
  // To add an configuration to this specific run, call OrtApis::AddRunConfigEntry
  // The configuration keys and value formats are defined in
  // /include/onnxruntime/core/session/onnxruntime_run_options_config_keys.h
  onnxruntime::ConfigOptions config_options;

  onnxruntime::InlinedVector<const onnxruntime::lora::LoraAdapter*> active_adapters;

  OrtRunOptions() = default;
  ~OrtRunOptions() = default;
};

namespace onnxruntime {
using RunOptions = ::OrtRunOptions;
}  // namespace onnxruntime
