
#include "ie_execution.h"

#include <utility>
#include <gna/gna_config.hpp>
#include "constants.h"

namespace InferenceEngine {

// TODO(Junwei): GNA device only be opened for one instance of
// ExecutableNetwork, there will be memory leak for these static objects.
static std::unique_ptr<Core> s_ie_core = nullptr;
static std::unique_ptr<ExecutableNetwork> s_gna_execution =
    nullptr;
static std::unique_ptr<InferRequest> s_gna_infer_request =
    nullptr;

Execution::Execution(std::unique_ptr<Compilation> compilation)
    : compilation_(std::move(compilation)),
      infer_request_(nullptr),
      plugin_(nullptr),
      execution_(nullptr),
      ie_core_(nullptr) {}

int32_t Execution::Init() {
    int32_t preference = compilation_->GetPreference();
  try {
    std::string device_name;
    if (preference == prefer_t::PREFER_FAST_SINGLE_ANSWER) {
      device_name = "CPU";
    } else if (preference == prefer_t::PREFER_SUSTAINED_SPEED) {
      device_name = "GPU";
    } else if (preference == prefer_t::PREFER_LOW_POWER) {
      device_name = "MYRIAD";
    } else if (preference == prefer_t::PREFER_ULTRA_LOW_POWER) {
      device_name = "GNA";
      // Release in squence to avoid crash. Close GNA device befere re-open,
      s_gna_infer_request.reset(nullptr);
      s_gna_execution.reset(nullptr);
      s_ie_core.reset(nullptr);
    }
    std::unique_ptr<InferRequest> infer_request;
    std::unique_ptr<Core> ie_core;
    std::unique_ptr<ExecutableNetwork> execution;
    ie_core.reset(new Core());
    std::map<std::string, std::string> plugin_Config = {};
    if (preference == prefer_t::PREFER_ULTRA_LOW_POWER) {
        // Get the input_scale from model.
        ModelInfoPtr model = compilation_->model_;
        if (model->inputs.size() != 1) {
            std::cout << "GNA plugin only support one input.";
            return error_t::OP_FAILED;
        }
        int index = model->inputs[0];
        int input_scale = model->operands[index].scale;
        if (input_scale > 0) {
            std::string scaleFactorConfigKey = GNA_CONFIG_KEY(SCALE_FACTOR);
            plugin_Config[scaleFactorConfigKey] = std::to_string(input_scale);
        }
        plugin_Config[GNAConfigParams::KEY_GNA_DEVICE_MODE] = "GNA_AUTO";
        // Note that it is not always possible to use 8-bit weights due to GNA
        // hardware limitations. For example, convolutional layers always use
        // 16-bit weights (GNA harware verison 1 and 2). This limitation will be
        // removed in GNA hardware version 3 and higher.
        // gnaPluginConfig[GNAConfigParams::KEY_GNA_PRECISION] = "I8";
    }
    execution.reset(
        new ExecutableNetwork(static_cast<IExecutableNetwork::Ptr&>(
            ie_core->LoadNetwork(*(compilation_->network_), device_name, plugin_Config))));
            // plugin->LoadNetwork(*(compilation_->network_), plugin_Config))));
    infer_request.reset(new InferRequest(
        static_cast<IInferRequest::Ptr>(execution->CreateInferRequest())));
    initialized_ = true;

    if (preference == prefer_t::PREFER_ULTRA_LOW_POWER) {
      s_gna_infer_request = std::move(infer_request);
      s_gna_execution = std::move(execution);
      s_ie_core = std::move(ie_core);
    } else {
      infer_request_ = std::move(infer_request);
      execution_ = std::move(execution);
      ie_core_ = std::move(ie_core);
    }
  } catch (const std::exception& ex) {
    std::cout << "[IE] exception " << ex.what();
    initialized_ = false;
    return error_t::OP_FAILED;
  }
  return error_t::NOT_ERROR;
}

Execution::~Execution() {
  if (compilation_->GetPreference() != prefer_t::PREFER_ULTRA_LOW_POWER) {
    // Release in squence to avoid crash.
    infer_request_.reset(nullptr);
    execution_.reset(nullptr);
    plugin_.reset(nullptr);
  }
}

int32_t Execution::SetInputOperandValue(void* buffer, uint32_t length) {
    input_data_.push_back(OperandValue(buffer, length));
    return error_t::NOT_ERROR;
}

int32_t Execution::SetOutputOperandValue(void* buffer, uint32_t length) {
    output_data_.push_back(OutputData(buffer, length));
    return error_t::NOT_ERROR;
}

int32_t Execution::StartCompute() {
  if (!initialized_) {
    std::cout << "Not initialized";
    return error_t::BAD_STATE;
  }
  try {
    int32_t result;
    uint32_t total_length = 0;
    InferRequest* infer_request =
        compilation_->GetPreference() == prefer_t::PREFER_ULTRA_LOW_POWER ? s_gna_infer_request.get()
                                                     : infer_request_.get();
    ModelInfoPtr model = compilation_->model_;
    for (size_t i = 0; i < model->inputs.size(); ++i) {
        size_t index = model->inputs[i];
      const Operand& operand = model->operands[index];
      const uint32_t offset = total_length;
      const uint32_t length = input_data_[i].length;
      total_length += length;
      if (operand.type != data_t::TENSOR_FLOAT32) {
        std::cout << "Only TENSOR_FLOAT32 operand type is supported";
        return error_t::BAD_DATA;
      }

      std::string input_id = std::to_string(index);
      Blob::Ptr input_blob = infer_request->GetBlob(input_id);

      float* dst =
          input_blob->buffer()
              .as<PrecisionTrait<Precision::FP32>::value_type*>();
      const float* src = reinterpret_cast<const float*>(input_data_[i].buffer);
      if (operand.dimensions.size() == 3) {
        // Only reorder HWC to CHW
        result = Reorder<float>(dst, src, operand.dimensions);
        if (result != error_t::NOT_ERROR) return error_t::BAD_DATA;
      } else {
        const size_t length = product(operand.dimensions) * sizeof(float);
        memcpy(static_cast<void*>(dst), static_cast<const void*>(src), length);
      }
    }

    infer_request->Infer();

    for (size_t i = 0; i < model->outputs.size(); ++i) {
        uint32_t index = model->outputs[i];
      const Operand& operand = model->operands[index];
      const uint32_t offset = total_length;
      const uint32_t length = output_data_[i].length;;
      total_length += length;
      void* mapping = output_data_[i].buffer;
      std::string output_id = std::to_string(index);
      const Blob::Ptr output_blob = infer_request->GetBlob(output_id);
      if (operand.dimensions.size() == 3) {
        const float* src =
            output_blob->buffer()
                .as<PrecisionTrait<Precision::FP32>::value_type*>();
        if (operand.type == data_t::TENSOR_FLOAT32) {
          float* dst = reinterpret_cast<float*>(mapping);
          result = Reorder<float>(dst, src, operand.dimensions, false);
        } else if (operand.type == data_t::TENSOR_INT32) {
          int32_t* dst = reinterpret_cast<int32_t*>(mapping);
          result = Reorder<int32_t>(dst, src, operand.dimensions, false);
        }
        if (result != error_t::NOT_ERROR) {
          return error_t::BAD_DATA;
        }
      } else {
        memcpy(mapping, output_blob->buffer(), length);
      }
    }
  } catch (const std::exception& ex) {
    std::cout << "[IE] exception " << ex.what();
    return error_t::OP_FAILED;
  }
  return error_t::NOT_ERROR;
}

}  // namespace InferenceEngine
