/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow_lite_support/cc/port/default/tflite_wrapper.h"

#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "tensorflow/lite/acceleration/configuration/flatbuffer_to_proto.h"
#include "tensorflow/lite/acceleration/configuration/proto_to_flatbuffer.h"
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/delegates/interpreter_utils.h"
#include "tensorflow/lite/minimal_logging.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"

namespace tflite {
namespace support {

namespace {
using tflite::delegates::DelegatePluginRegistry;
using tflite::delegates::InterpreterUtils;
using tflite::proto::ComputeSettings;
using tflite::proto::Delegate;
}  // namespace

/* static */
absl::Status TfLiteInterpreterWrapper::SanityCheckComputeSettings(
    const ComputeSettings& compute_settings) {
  Delegate delegate = compute_settings.tflite_settings().delegate();
  if (delegate != Delegate::NONE && delegate != Delegate::GPU &&
      delegate != Delegate::HEXAGON && delegate != Delegate::NNAPI &&
      delegate != Delegate::XNNPACK && delegate != Delegate::EDGETPU_CORAL &&
      delegate != Delegate::CORE_ML) {
    return absl::UnimplementedError(absl::StrFormat(
        "Using delegate '%s' is not supported.", Delegate_Name(delegate)));
  }
  return absl::OkStatus();
}

TfLiteInterpreterWrapper::TfLiteInterpreterWrapper(
    const std::string& default_model_namespace,
    const std::string& default_model_id)
    : delegate_(nullptr, nullptr),
      got_error_do_not_delegate_anymore_(false),
      default_model_namespace_(default_model_namespace),
      default_model_id_(default_model_id),
      mini_benchmark_(nullptr) {}

std::string TfLiteInterpreterWrapper::ModelNamespace() {
  const auto& ns_from_acceleration =
      compute_settings_.model_namespace_for_statistics();
  return ns_from_acceleration.empty() ? default_model_namespace_
                                      : ns_from_acceleration;
}

std::string TfLiteInterpreterWrapper::ModelID() {
  const auto& id_from_acceleration =
      compute_settings_.model_identifier_for_statistics();
  return id_from_acceleration.empty() ? default_model_id_
                                      : id_from_acceleration;
}

// This is the deprecated overload that doesn't take an
// InterpreterCreationResources parameter.
absl::Status TfLiteInterpreterWrapper::InitializeWithFallback(
    std::function<absl::Status(std::unique_ptr<tflite::Interpreter>*)>
        interpreter_initializer,
    const ComputeSettings& compute_settings) {
  return InitializeWithFallback(
      [interpreter_initializer](
          const InterpreterCreationResources& resources,
          std::unique_ptr<tflite::Interpreter>* interpreter_out)
          -> absl::Status {
        TFLITE_RETURN_IF_ERROR(interpreter_initializer(interpreter_out));
        if (*interpreter_out != nullptr &&
            resources.optional_delegate != nullptr) {
          TfLiteStatus status =
              (*interpreter_out)
                  ->ModifyGraphWithDelegate(resources.optional_delegate);
          if (status != kTfLiteOk) {
            *interpreter_out = nullptr;
            TFLITE_RETURN_IF_ERROR(
                absl::InvalidArgumentError("Applying delegate failed"));
          }
        }
        return absl::OkStatus();
      },
      compute_settings);
}

absl::Status TfLiteInterpreterWrapper::InitializeWithFallback(
    std::function<absl::Status(const InterpreterCreationResources&,
                               std::unique_ptr<tflite::Interpreter>*)>
        interpreter_initializer,
    const ComputeSettings& compute_settings) {
  // Store interpreter initializer if not already here.
  if (interpreter_initializer_) {
    return absl::FailedPreconditionError(
        "InitializeWithFallback already called.");
  }
  interpreter_initializer_ = std::move(interpreter_initializer);

  // Sanity check and copy ComputeSettings.
  TFLITE_RETURN_IF_ERROR(SanityCheckComputeSettings(compute_settings));
  compute_settings_ = compute_settings;
  if (compute_settings_.has_settings_to_test_locally()) {
    flatbuffers::FlatBufferBuilder mini_benchmark_settings_fbb;
    const auto* mini_benchmark_settings =
        tflite::ConvertFromProto(compute_settings_.settings_to_test_locally(),
                                 &mini_benchmark_settings_fbb);
    mini_benchmark_ = tflite::acceleration::CreateMiniBenchmark(
        *mini_benchmark_settings, ModelNamespace(), ModelID());
    const tflite::ComputeSettingsT from_minibenchmark =
        mini_benchmark_->GetBestAcceleration();
    if (from_minibenchmark.tflite_settings != nullptr) {
      TFLITE_LOG_PROD_ONCE(TFLITE_LOG_INFO, "Using mini benchmark results\n");
      compute_settings_ = tflite::ConvertFromFlatbuffer(
          from_minibenchmark, /*skip_mini_benchmark_settings=*/true);
    }
    // Trigger mini benchmark if it hasn't already run. Vast majority of cases
    // should not actually do anything, since first runs are rare.
    mini_benchmark_->TriggerMiniBenchmark();
    mini_benchmark_->MarkAndGetEventsToLog();
  }

  // Initialize fallback behavior.
  fallback_on_compilation_error_ =
      compute_settings_.tflite_settings()
          .fallback_settings()
          .allow_automatic_fallback_on_compilation_error() ||
      // Deprecated, keep supporting for backward compatibility.
      compute_settings_.tflite_settings()
          .nnapi_settings()
          .fallback_settings()
          .allow_automatic_fallback_on_compilation_error();
  fallback_on_execution_error_ =
      compute_settings_.tflite_settings()
          .fallback_settings()
          .allow_automatic_fallback_on_execution_error() ||
      // Deprecated, keep supporting for backward compatibility.
      compute_settings_.tflite_settings()
          .nnapi_settings()
          .fallback_settings()
          .allow_automatic_fallback_on_execution_error();

  return InitializeWithFallbackAndResize();
}

absl::Status TfLiteInterpreterWrapper::AllocateTensors() {
  if (interpreter_->AllocateTensors() != kTfLiteOk) {
    return absl::InternalError("AllocateTensors() failed.");
  }
  return absl::OkStatus();
}

// TODO(b/173406463): the `resize` parameter is going to be used by
// ResizeAndAllocateTensors functions, coming soon.
absl::Status TfLiteInterpreterWrapper::InitializeWithFallbackAndResize(
    std::function<absl::Status(Interpreter*)> resize) {
  InterpreterCreationResources resources{};
  if (got_error_do_not_delegate_anymore_ ||
      compute_settings_.tflite_settings().delegate() == Delegate::NONE) {
    delegate_.reset(nullptr);
  } else {
    // Initialize delegate and add it to 'resources'.
    TFLITE_RETURN_IF_ERROR(InitializeDelegate());
    resources.optional_delegate = delegate_.get();
  }

  absl::Status status = interpreter_initializer_(resources, &interpreter_);
  if (resources.optional_delegate == nullptr) {
    TFLITE_RETURN_IF_ERROR(status);
  }
  if (resources.optional_delegate != nullptr && !status.ok()) {
    // Any error when constructing the interpreter is assumed to be a delegate
    // compilation error.  If a delegate compilation error occurs, stop
    // delegation from happening in the future.
    got_error_do_not_delegate_anymore_ = true;
    delegate_.reset(nullptr);
    if (fallback_on_compilation_error_) {
      InterpreterCreationResources fallback_resources{};
      fallback_resources.optional_delegate = nullptr;
      TFLITE_RETURN_IF_ERROR(
          interpreter_initializer_(fallback_resources, &interpreter_));
    } else {
      // If instructed not to fallback, return error.
      return absl::InternalError(absl::StrFormat(
          "ModifyGraphWithDelegate() failed for delegate '%s'.",
          Delegate_Name(compute_settings_.tflite_settings().delegate())));
    }
  }

  TFLITE_RETURN_IF_ERROR(resize(interpreter_.get()));
  if (compute_settings_.tflite_settings().cpu_settings().num_threads() != -1) {
    if (interpreter_->SetNumThreads(
            compute_settings_.tflite_settings().cpu_settings().num_threads()) !=
        kTfLiteOk) {
      return absl::InternalError("Failed setting number of CPU threads");
    }
  }
  SetTfLiteCancellation();

  if (!delegate_) {
    // Just allocate tensors and return.
    return AllocateTensors();
  }

  // The call to ModifyGraphWithDelegate() leaves the interpreter in a usable
  // state in case of failure: calling AllocateTensors() will silently fallback
  // on CPU in such a situation.
  return AllocateTensors();
}

absl::Status TfLiteInterpreterWrapper::InitializeDelegate() {
  if (delegate_ == nullptr) {
    Delegate which_delegate = compute_settings_.tflite_settings().delegate();
    const tflite::ComputeSettings* compute_settings =
        tflite::ConvertFromProto(compute_settings_, &flatbuffers_builder_);

    if (which_delegate == Delegate::NNAPI) {
      TFLITE_RETURN_IF_ERROR(
          LoadDelegatePlugin("Nnapi", *compute_settings->tflite_settings()));
    } else if (which_delegate == Delegate::HEXAGON) {
      TFLITE_RETURN_IF_ERROR(
          LoadDelegatePlugin("Hexagon", *compute_settings->tflite_settings()));
    } else if (which_delegate == Delegate::GPU) {
      TFLITE_RETURN_IF_ERROR(
          LoadDelegatePlugin("Gpu", *compute_settings->tflite_settings()));
    } else if (which_delegate == Delegate::EDGETPU) {
      TFLITE_RETURN_IF_ERROR(
          LoadDelegatePlugin("EdgeTpu", *compute_settings->tflite_settings()));
    } else if (which_delegate == Delegate::EDGETPU_CORAL) {
      TFLITE_RETURN_IF_ERROR(LoadDelegatePlugin("EdgeTpuCoral",
                                         *compute_settings->tflite_settings()));
    } else if (which_delegate == Delegate::XNNPACK) {
      TFLITE_RETURN_IF_ERROR(
          LoadDelegatePlugin("XNNPack", *compute_settings->tflite_settings()));
    } else if (which_delegate == Delegate::CORE_ML) {
      TFLITE_RETURN_IF_ERROR(
          LoadDelegatePlugin("CoreML", *compute_settings->tflite_settings()));
    }
  }
  return absl::OkStatus();
}

absl::Status TfLiteInterpreterWrapper::InvokeWithFallback(
    const std::function<absl::Status(tflite::Interpreter* interpreter)>&
        set_inputs) {
  TFLITE_RETURN_IF_ERROR(set_inputs(interpreter_.get()));
  if (cancel_flag_.Get()) {
    cancel_flag_.Set(false);
    return absl::CancelledError("cancelled before Invoke() was called");
  }
  TfLiteStatus status = kTfLiteError;
  if (fallback_on_execution_error_) {
    status = InterpreterUtils::InvokeWithCPUFallback(interpreter_.get());
  } else {
    status = interpreter_->Invoke();
  }
  if (status == kTfLiteOk) {
    return absl::OkStatus();
  }
  // Assume InvokeWithoutFallback() is guarded under caller's synchronization.
  // Assume the inference is cancelled successfully if Invoke() returns
  // kTfLiteError and the cancel flag is `true`.
  if (status == kTfLiteError && cancel_flag_.Get()) {
    cancel_flag_.Set(false);
    return absl::CancelledError("Invoke() cancelled.");
  }
  if (delegate_) {
    // Mark that an error occurred so that later invocations immediately
    // fallback to CPU.
    got_error_do_not_delegate_anymore_ = true;
    // InvokeWithCPUFallback returns `kTfLiteDelegateError` in case of
    // *successful* fallback: convert it to an OK status.
    if (status == kTfLiteDelegateError) {
      return absl::OkStatus();
    }
  }
  return absl::InternalError("Invoke() failed.");
}

absl::Status TfLiteInterpreterWrapper::InvokeWithoutFallback() {
  if (cancel_flag_.Get()) {
    cancel_flag_.Set(false);
    return absl::CancelledError("cancelled before Invoke() was called");
  }
  TfLiteStatus status = interpreter_->Invoke();
  if (status != kTfLiteOk) {
    // Assume InvokeWithoutFallback() is guarded under caller's synchronization.
    // Assume the inference is cancelled successfully if Invoke() returns
    // kTfLiteError and the cancel flag is `true`.
    if (status == kTfLiteError && cancel_flag_.Get()) {
      cancel_flag_.Set(false);
      return absl::CancelledError("Invoke() cancelled.");
    }
    return absl::InternalError("Invoke() failed.");
  }
  return absl::OkStatus();
}

void TfLiteInterpreterWrapper::Cancel() { cancel_flag_.Set(true); }

void TfLiteInterpreterWrapper::SetTfLiteCancellation() {
  // Create a cancellation check function and set to the TFLite interpreter.
  auto check_cancel_flag = [](void* data) {
    auto* cancel_flag = reinterpret_cast<CancelFlag*>(data);
    return cancel_flag->Get();
  };
  interpreter_->SetCancellationFunction(reinterpret_cast<void*>(&cancel_flag_),
                                        check_cancel_flag);
}

absl::Status TfLiteInterpreterWrapper::LoadDelegatePlugin(
    const std::string& name, const tflite::TFLiteSettings& tflite_settings) {
  delegate_plugin_ = DelegatePluginRegistry::CreateByName(
      absl::StrFormat("%sPlugin", name), tflite_settings);

  if (delegate_plugin_ == nullptr) {
    return absl::InternalError(absl::StrFormat(
        "Could not create %s plugin. Have you linked in the %s_plugin target?",
        name, name));
  }

  delegate_ = delegate_plugin_->Create();
  if (delegate_ == nullptr) {
    return absl::InternalError(
        absl::StrFormat("Plugin did not create %s delegate.", name));
  }

  return absl::OkStatus();
}

bool TfLiteInterpreterWrapper::HasMiniBenchmarkCompleted() {
  if (mini_benchmark_ != nullptr &&
      mini_benchmark_->NumRemainingAccelerationTests() == 0) {
    return true;
  }
  return false;
}

}  // namespace support
}  // namespace tflite
