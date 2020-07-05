// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "utils.h"

#include <iostream>

#include "constants.h"

namespace InferenceEngine {

namespace {

inline void CalculateExplicitPadding(bool padding_same,
                                     uint32_t in_size,
                                     uint32_t stride,
                                     uint32_t filter_size,
                                     uint32_t& padding_head,
                                     uint32_t& padding_tail,
                                     uint32_t dilation = 1) {
  padding_head = 0;
  padding_tail = 0;

  if (padding_same) {
    uint32_t out_size = (in_size + stride - 1) / stride;
    uint32_t effective_filter_size = (filter_size - 1) * dilation + 1;
    uint32_t tmp = (out_size - 1) * stride + effective_filter_size;
    if (tmp > in_size) {
      padding_head = (tmp - in_size) / 2;
      padding_tail = (tmp - in_size) - padding_head;
    }
  }
}
}  // namespace

Operation::Operation() = default;
Operation::~Operation() = default;

ModelInfo::ModelInfo() = default;
ModelInfo::~ModelInfo() = default;

OperandValue::OperandValue() = default;
OperandValue::~OperandValue() = default;
OperandValue::OperandValue(const void* buffer, uint32_t length)
    : buffer(buffer), length(length){};

OutputData::OutputData() = default;
OutputData::~OutputData() = default;
OutputData::OutputData(void* buffer, uint32_t length)
    : buffer(buffer), length(length){};

uint32_t product(const std::vector<uint32_t>& dims) {
  uint32_t prod = 1;
  for (size_t i = 0; i < dims.size(); ++i)
    prod *= dims[i];
  return prod;
}

int32_t GetScalarInt32(ModelInfoPtr model, uint32_t index) {
  const OperandValue& info = model->values[index];
  const int32_t* data = reinterpret_cast<const int32_t*>(info.buffer);
  std::cout << "==== " << data[0];
  return reinterpret_cast<const int32_t*>(info.buffer)[0];
}

float GetScalarFloat(ModelInfoPtr model, uint32_t index) {
  OperandValue& info = model->values[index];
  return reinterpret_cast<const float*>(info.buffer)[0];
}

int32_t GetElementWiseParams(ModelInfoPtr model,
                             const Operation& operation,
                             ElementWiseParams& params) {
  const int32_t type = operation.type;
  if (!(type == ADD || type == MUL)) {
    std::cout << "Operation type " << type << " is not element-wise";
    return BAD_DATA;
  }
  params.fuse_code = GetScalarInt32(model, operation.inputs[2]);
  return NOT_ERROR;
}

int32_t GetConvParams(ModelInfoPtr model,
                      const Operation& operation,
                      ConvParams& params) {
  const int32_t type = operation.type;
  if (!(type == CONV_2D || type == DEPTHWISE_CONV_2D ||
        type == ATROUS_CONV_2D || type == ATROUS_DEPTHWISE_CONV_2D)) {
    std::cout << "Operation type " << type << " is not convolution";
    return BAD_DATA;
  }
  const std::vector<uint32_t>& inputs = operation.inputs;
  const std::vector<uint32_t>& outputs = operation.outputs;
  params.depthwise =
      (type == DEPTHWISE_CONV_2D || type == ATROUS_DEPTHWISE_CONV_2D) ? true
                                                                      : false;
  params.atrous = (type == ATROUS_CONV_2D || type == ATROUS_DEPTHWISE_CONV_2D)
                      ? true
                      : false;
  const uint32_t output_index = outputs[0];
  const Operand& output = model->operands[output_index];
  params.output_batch = output.dimensions[0];
  params.output_height = output.dimensions[1];
  params.output_width = output.dimensions[2];
  params.output_channel = output.dimensions[3];
  uint32_t index = 0;
  const uint32_t input_index = inputs[index++];
  const Operand& input = model->operands[input_index];
  params.input_batch = input.dimensions[0];
  params.input_height = input.dimensions[1];
  params.input_width = input.dimensions[2];
  params.input_channel = input.dimensions[3];

  const uint32_t filter_idx = inputs[index++];
  Operand& filter = model->operands[filter_idx];
  if (params.depthwise) {
    params.depth_out = filter.dimensions[3];
  } else {
    params.depth_out = filter.dimensions[0];
    params.depth_in = filter.dimensions[3];
  }
  params.filter_height = filter.dimensions[1];
  params.filter_width = filter.dimensions[2];

  const uint32_t bias_idx = inputs[index++];
  Operand& bias = model->operands[bias_idx];
  params.bias_length = bias.dimensions[0];

  bool implicit_padding;
  int32_t padding_code;
  if ((!params.depthwise && inputs.size() == 10) ||
      (params.depthwise && inputs.size() == 11)) {
    implicit_padding = false;
    params.padding_left = GetScalarInt32(model, inputs[index++]);
    params.padding_right = GetScalarInt32(model, inputs[index++]);
    params.padding_top = GetScalarInt32(model, inputs[index++]);
    params.padding_bottom = GetScalarInt32(model, inputs[index++]);
  } else if ((!params.depthwise && inputs.size() == 7) ||
             (params.depthwise && inputs.size() == 8)) {
    implicit_padding = true;
    padding_code = GetScalarInt32(model, inputs[index++]);
  } else {
    std::cout << "Inputs size is incorrect";
    return BAD_DATA;
  }
  if (!params.atrous) {
    params.stride_width = GetScalarInt32(model, inputs[index++]);
    params.stride_height = GetScalarInt32(model, inputs[index++]);
    params.dilation_width = 1;
    params.dilation_height = 1;
  } else {
    params.dilation_width = GetScalarInt32(model, inputs[index++]);
    params.dilation_height = GetScalarInt32(model, inputs[index++]);
    params.stride_width = 1;
    params.stride_height = 1;
  }
  if (params.depthwise) {
    params.depthwise_multiplier = GetScalarInt32(model, inputs[index++]);
    params.depth_in = params.depth_out / params.depthwise_multiplier;
  }
  params.fuse_code = GetScalarInt32(model, inputs[index++]);

  if (implicit_padding) {
    CalculateExplicitPadding(padding_code == PADDING_SAME, params.input_width,
                             params.stride_width, params.filter_width,
                             params.padding_left, params.padding_right,
                             params.dilation_width);
    CalculateExplicitPadding(padding_code == PADDING_SAME, params.input_height,
                             params.stride_height, params.filter_height,
                             params.padding_top, params.padding_bottom,
                             params.dilation_height);
  }

  return NOT_ERROR;
}

int32_t GetPoolingParams(ModelInfoPtr model,
                         const Operation& operation,
                         PoolingParams& params) {
  const int32_t type = operation.type;
  if (!(type == AVERAGE_POOL_2D || type == MAX_POOL_2D)) {
    std::cout << "Operation type " << type << " is not pooling";
    return BAD_DATA;
  }
  const std::vector<uint32_t>& inputs = operation.inputs;
  const std::vector<uint32_t>& outputs = operation.outputs;
  const uint32_t output_index = outputs[0];
  const Operand& output = model->operands[output_index];
  params.output_batch = output.dimensions[0];
  params.output_height = output.dimensions[1];
  params.output_width = output.dimensions[2];
  params.output_channel = output.dimensions[3];
  int32_t i = 0;
  const int32_t input_index = inputs[i++];
  const Operand& input = model->operands[input_index];
  params.input_batch = input.dimensions[0];
  params.input_height = input.dimensions[1];
  params.input_width = input.dimensions[2];
  params.input_channel = input.dimensions[3];

  bool implicit_padding;
  int32_t padding_code;
  if (inputs.size() == 10) {
    implicit_padding = false;
    params.padding_left = GetScalarInt32(model, inputs[i++]);
    params.padding_right = GetScalarInt32(model, inputs[i++]);
    params.padding_top = GetScalarInt32(model, inputs[i++]);
    params.padding_bottom = GetScalarInt32(model, inputs[i++]);
  } else if (inputs.size() == 7) {
    implicit_padding = true;
    padding_code = GetScalarInt32(model, inputs[i++]);
  } else {
    std::cout << "  inputs size is incorrect";
    return BAD_DATA;
  }
  params.stride_width = GetScalarInt32(model, inputs[i++]);
  params.stride_height = GetScalarInt32(model, inputs[i++]);
  params.filter_width = GetScalarInt32(model, inputs[i++]);
  params.filter_height = GetScalarInt32(model, inputs[i++]);
  params.fuse_code = GetScalarInt32(model, inputs[i++]);

  // Setup paddings.
  if (implicit_padding) {
    CalculateExplicitPadding(padding_code == PADDING_SAME, params.input_width,
                             params.stride_width, params.filter_width,
                             params.padding_left, params.padding_right);
    CalculateExplicitPadding(padding_code == PADDING_SAME, params.input_height,
                             params.stride_height, params.filter_height,
                             params.padding_top, params.padding_bottom);
  }
  return NOT_ERROR;
}

int32_t GetSoftmaxParams(ModelInfoPtr model,
                         const Operation& operation,
                         SoftmaxParams& params) {
  const int32_t type = operation.type;
  if (type != SOFTMAX) {
    std::cout << "Operation type " << type << " is not softmax";
    return BAD_DATA;
  }
  params.beta = GetScalarFloat(model, operation.inputs[1]);
  return NOT_ERROR;
}

int32_t GetConcatParams(ModelInfoPtr model,
                        const Operation& operation,
                        ConcatParams& params) {
  const int32_t type = operation.type;
  if (type != CONCATENATION) {
    std::cout << "Operation type " << type << " is not concatenation";
    return BAD_DATA;
  }
  const std::vector<uint32_t>& inputs = operation.inputs;
  params.axis = GetScalarInt32(model, inputs[inputs.size() - 1]);
  return NOT_ERROR;
}

int32_t GetFullyConnectedParams(ModelInfoPtr model,
                                const Operation& operation,
                                FullyConnectedParams& params) {
  const int32_t type = operation.type;
  if (type != FULLY_CONNECTED) {
    std::cout << "Operation type " << type << " is not fully connected";
    return BAD_DATA;
  }
  const std::vector<uint32_t>& inputs = operation.inputs;
  const std::vector<uint32_t>& outputs = operation.outputs;
  // The output tensor, of shape [batch_size, num_units]
  const uint32_t output_index = outputs[0];
  const Operand& output = model->operands[output_index];
  params.output_batch_size = output.dimensions[0];
  params.output_num_units = output.dimensions[1];

  uint32_t index = 0;
  const uint32_t input_index = inputs[index++];
  const Operand& input = model->operands[input_index];
  // A tensor of at least rank 2, specifying the input.
  if (input.dimensions.size() < 2) {
    std::cout << "Input tenosr must have least rank 2.";
    return BAD_DATA;
  }

  const uint32_t weights_idx = inputs[index++];
  const Operand& weights = model->operands[weights_idx];
  params.num_units = weights.dimensions[0];
  params.input_size = weights.dimensions[1];

  // According to Android NN API doc:
  // If rank is greater than 2, then it gets flattened to a 2-D Tensor.
  // The (flattened) 2-D Tensor is reshaped (if necessary) to
  // [batch_size, input_size], where "input_size" corresponds to the number of
  // inputs to the layer, matching the second dimension of weights, and
  // "batch_size" is calculated by dividing the number of elements by
  // "input_size".
  if (input.dimensions.size() > 2) {
    params.input_batch_size = product(input.dimensions) / params.input_size;
  } else {
    if (input.dimensions[1] != params.input_size) {
      std::cout << "input.dimensions[1] (" << input.dimensions[1] << ") "
                << "!= input_size (" << params.input_size << ")";
      return BAD_DATA;
    }
    params.input_batch_size = input.dimensions[0];
  }

  // A 1-D tensor, of shape [num_units]
  const uint32_t bias_idx = inputs[index++];
  const Operand& bias = model->operands[bias_idx];
  params.bias_num_units = bias.dimensions[0];
  if (params.bias_num_units != params.num_units) {
    std::cout << "bias_num_units (" << params.bias_num_units << ") != "
              << "num_units (" << params.num_units << ")";
    return BAD_DATA;
  }

  params.fuse_code = GetScalarInt32(model, inputs[index++]);

  return NOT_ERROR;
}

int32_t GetResizeBilinearParams(ModelInfoPtr model,
                                const Operation& operation,
                                ResizeBilinearParams& params) {
  const int32_t type = operation.type;
  if (operation.type != operation_t::RESIZE_BILINEAR_NN) {
    std::cout << "Operation type " << type << " is not resize bilinear";
    return BAD_DATA;
  }
  const std::vector<uint32_t>& inputs = operation.inputs;
  if (inputs.size() != 3 && inputs.size() != 4) {
    std::cout << "Inputs size is wrong " << inputs.size();
    return BAD_DATA;
  }
  const Operand& input = model->operands[inputs[0]];
  if (input.dimensions.size() != 4) {
    std::cout << "Input must be a 4-D tensor";
    return BAD_DATA;
  }
  params.height = input.dimensions[1];
  params.width = input.dimensions[2];
  params.new_height = GetScalarInt32(model, inputs[1]);
  params.new_width = GetScalarInt32(model, inputs[2]);
  params.y_scale = params.new_height / params.height;
  params.x_scale = params.new_width / params.width;

  params.align_corners = false;
  if (inputs.size() == 4) {
    params.align_corners = GetScalarInt32(model, inputs[3]) == 0 ? false : true;
  }

  return NOT_ERROR;
}

int32_t GetArgmaxParams(ModelInfoPtr model,
                        const Operation& operation,
                        ArgmaxParams& params) {
  const int32_t type = operation.type;
  if (type != ARGMAX) {
    std::cout << "Operation type " << type << " is not argmax";
    return BAD_DATA;
  }
  const std::vector<uint32_t>& inputs = operation.inputs;
  params.axis = GetScalarInt32(model, inputs[1]);
  return NOT_ERROR;
}

}  // namespace InferenceEngine
