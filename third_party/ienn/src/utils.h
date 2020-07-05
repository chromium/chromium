// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IE_UTILS_H
#define IE_UTILS_H

#include <map>
#include <memory>
#include <vector>

#include "ie_nn_c_api.h"

namespace InferenceEngine {

// Put this in the declarations for a class to be uncopyable.
#define DISALLOW_COPY(TypeName) TypeName(const TypeName&) = delete

// Put this in the declarations for a class to be unassignable.
#define DISALLOW_ASSIGN(TypeName) TypeName& operator=(const TypeName&) = delete

// Put this in the declarations for a class to be uncopyable and unassignable.
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  DISALLOW_COPY(TypeName);                 \
  DISALLOW_ASSIGN(TypeName)

uint32_t product(const std::vector<uint32_t>& dims);

struct Operation {
  Operation();
  ~Operation();
  int32_t type;
  std::vector<uint32_t> inputs;
  std::vector<uint32_t> outputs;
};

struct Operand {
  int32_t type;
  std::vector<uint32_t> dimensions;
  float scale;
  int32_t zeroPoint;
};

// the value of operand has passed with operand index, so the offset is 0,
// and the memory of buffer is hold in application that isn't copied.
typedef struct OperandValue {
  OperandValue();
  ~OperandValue();
  OperandValue(const void* buffer, uint32_t length);
  const void* buffer;
  uint32_t length;
  size_t offset;
};

typedef struct OutputData {
  OutputData();
  ~OutputData();
  OutputData(void* buffer, uint32_t length);
  void* buffer;
  uint32_t length;
  size_t offset;
};

/**
 * @struct ie_model
 * @brief Represents model information that reflects the set of supported
 * operations
 */
struct ModelInfo {
  ModelInfo();
  ~ModelInfo();
  std::vector<Operand> operands;
  std::vector<Operation> operations;
  std::vector<uint32_t> inputs;
  std::vector<uint32_t> outputs;
  std::map<uint32_t, OperandValue> values;

  DISALLOW_COPY_AND_ASSIGN(ModelInfo);
};
using ModelInfoPtr = std::shared_ptr<ModelInfo>;
// struct Compilation {
//     ModelInfoPtr model;
//     int32_t preference;
// };

struct ElementWiseParams {
  int32_t fuse_code;
};

struct ConvParams {
  bool depthwise;
  bool atrous;
  uint32_t input_batch;
  uint32_t input_height;
  uint32_t input_width;
  uint32_t input_channel;
  uint32_t output_batch;
  uint32_t output_height;
  uint32_t output_width;
  uint32_t output_channel;
  uint32_t filter_height;
  uint32_t filter_width;
  uint32_t bias_length;
  uint32_t depth_in;
  uint32_t depth_out;
  uint32_t padding_left;
  uint32_t padding_right;
  uint32_t padding_top;
  uint32_t padding_bottom;
  uint32_t stride_width;
  uint32_t stride_height;
  uint32_t dilation_width;
  uint32_t dilation_height;
  uint32_t depthwise_multiplier;
  int32_t fuse_code;
};

struct PoolingParams {
  uint32_t input_batch;
  uint32_t input_height;
  uint32_t input_width;
  uint32_t input_channel;
  uint32_t output_batch;
  uint32_t output_height;
  uint32_t output_width;
  uint32_t output_channel;
  uint32_t filter_height;
  uint32_t filter_width;
  uint32_t padding_left;
  uint32_t padding_right;
  uint32_t padding_top;
  uint32_t padding_bottom;
  uint32_t stride_width;
  uint32_t stride_height;
  int32_t fuse_code;
};

struct SoftmaxParams {
  float beta;
};

struct ConcatParams {
  int32_t axis;
};

struct FullyConnectedParams {
  int32_t input_batch_size;
  uint32_t num_units;
  int32_t input_size;
  uint32_t bias_num_units;
  uint32_t output_batch_size;
  uint32_t output_num_units;
  int32_t fuse_code;
};

struct ResizeBilinearParams {
  uint32_t height;
  uint32_t width;
  uint32_t new_height;
  uint32_t new_width;
  float y_scale;
  float x_scale;
  bool align_corners;
};

struct ArgmaxParams {
  int32_t axis;
};

int32_t GetScalarInt32(ModelInfoPtr model, uint32_t index);

float GetScalarFloat(ModelInfoPtr model, uint32_t index);

int32_t GetElementWiseParams(ModelInfoPtr model,
                             const Operation&,
                             ElementWiseParams&);

int32_t GetConvParams(ModelInfoPtr model, const Operation&, ConvParams&);

int32_t GetPoolingParams(ModelInfoPtr model, const Operation&, PoolingParams&);

int32_t GetSoftmaxParams(ModelInfoPtr model, const Operation&, SoftmaxParams&);

int32_t GetConcatParams(ModelInfoPtr model, const Operation&, ConcatParams&);

int32_t GetFullyConnectedParams(ModelInfoPtr model,
                                const Operation&,
                                FullyConnectedParams&);

int32_t GetResizeBilinearParams(ModelInfoPtr model,
                                const Operation&,
                                ResizeBilinearParams&);

int32_t GetArgmaxParams(ModelInfoPtr model, const Operation&, ArgmaxParams&);

}  // namespace InferenceEngine

#endif  // IE_UTILS_H