// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IE_COMPILATION_H
#define IE_COMPILATION_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <ie_builders.hpp>
#include <inference_engine.hpp>
#include "constants.h"
#include "ie_nn_c_api.h"
#include "utils.h"

namespace InferenceEngine {

static float asfloat(uint32_t v) {
  union {
    float f;
    std::uint32_t u;
  } converter = {0};
  converter.u = v;
  return converter.f;
}

static short f32tof16(float x) {
  static float min16 = asfloat((127 - 14) << 23);

  static float max16 = asfloat(((127 + 15) << 23) | 0x007FE000);
  static uint32_t max16f16 = ((15 + 15) << 10) | 0x3FF;

  static constexpr std::uint32_t EXP_MASK_F32 = 0x7F800000U;

  union {
    float f;
    uint32_t u;
  } v = {0};
  v.f = x;

  uint32_t s = (v.u >> 16) & 0x8000;

  v.u &= 0x7FFFFFFF;

  if ((v.u & EXP_MASK_F32) == EXP_MASK_F32) {
    if (v.u & 0x007FFFFF) {
      return static_cast<short>(s | (v.u >> (23 - 10)) | 0x0200);
    } else {
      return static_cast<short>(s | (v.u >> (23 - 10)));
    }
  }

  float halfULP = asfloat(v.u & EXP_MASK_F32) * asfloat((127 - 11) << 23);
  v.f += halfULP;

  if (v.f < min16 * 0.5f) {
    return static_cast<short>(s);
  }

  if (v.f < min16) {
    return static_cast<short>(s | (1 << 10));
  }

  if (v.f >= max16) {
    return static_cast<short>(max16f16 | s);
  }

  v.u -= ((127 - 15) << 23);

  v.u >>= (23 - 10);

  return static_cast<short>(v.u | s);
}

template <typename T>
int32_t Reorder(T* dst,
                const float* src,
                const std::vector<uint32_t>& dims,
                bool nhwc_to_nchw = true) {
  if (!(std::is_same<T, float>::value || std::is_same<T, int16_t>::value ||
        std::is_same<T, int32_t>::value)) {
    std::cout << "Data type is not supported";
    return error_t::BAD_DATA;
  }
  if (dims.size() == 1 || dims.size() == 2) {
    size_t size = product(dims);
    if (std::is_same<T, float>::value || std::is_same<T, int32_t>::value) {
      const size_t buffer_length = size * sizeof(T);
      memcpy(static_cast<void*>(dst), static_cast<const void*>(src),
             buffer_length);
    } else if (std::is_same<T, int16_t>::value) {
      for (size_t i = 0; i < size; ++i) {
        dst[i] = f32tof16(src[i]);
      }
    }
  } else if (dims.size() == 3 || dims.size() == 4) {
    // dims is in NHWC
    const bool rank3 = dims.size() == 3;
    const uint32_t batches = rank3 ? 1 : dims[0];
    const uint32_t channels = rank3 ? dims[2] : dims[3];
    const uint32_t height = rank3 ? dims[0] : dims[1];
    const uint32_t width = rank3 ? dims[1] : dims[2];

    for (uint32_t b = 0; b < batches; ++b) {
      for (uint32_t c = 0; c < channels; ++c) {
        for (uint32_t y = 0; y < height; ++y) {
          for (uint32_t x = 0; x < width; ++x) {
            uint32_t dst_index, src_index;
            if (nhwc_to_nchw) {
              dst_index = b * channels * height * width + c * height * width +
                          y * width + x;
              src_index = b * height * width * channels + y * width * channels +
                          x * channels + c;
            } else {
              dst_index = b * height * width * channels + y * width * channels +
                          x * channels + c;
              src_index = b * channels * height * width + c * height * width +
                          y * width + x;
            }
            if (std::is_same<T, float>::value ||
                std::is_same<T, int32_t>::value) {
              dst[dst_index] = src[src_index];
            } else if (std::is_same<T, int16_t>::value) {
              dst[dst_index] = f32tof16(src[src_index]);
            }
          }
        }
      }
    }
  } else {
    std::cout << "Tensor rank " << dims.size() << " is not supproted";
    return error_t::BAD_DATA;
  }
  return error_t::NOT_ERROR;
}

bool GNADevice();

class Compilation {
 public:
  explicit Compilation(ModelInfoPtr model);
  ~Compilation() = default;

  int32_t Compile();
  void SetPreference(int32_t prefer);
  int32_t GetPreference();

 private:
  int32_t CreateBlob(uint32_t index,
                     std::shared_ptr<InferenceEngine::Blob>& blob);
  int32_t AddInput(uint32_t index);
  int32_t AddOutput(uint32_t index);
  int32_t AddConstant(uint32_t index);
  int32_t AddActivationByFusedCode(int32_t fuse_code,
                                   size_t input_layer,
                                   const std::string& name,
                                   size_t& activiation_layer_id);
  int32_t AddElementwise(const Operation& operation);
  int32_t AddConvolution(const Operation& operation);
  int32_t AddPooling(const Operation& operation);
  int32_t AddSoftmax(const Operation& operation);
  int32_t AddReshape(const Operation& operation);
  int32_t AddConcatenation(const Operation& operation);
  int32_t AddFullyConnected(const Operation& operation);
  int32_t AddResizeBilinear(const Operation& operation);
  int32_t AddSigmoid(const Operation& operation);
  int32_t AddArgmax(const Operation& operation);

 private:
  friend class Execution;
  ModelInfoPtr model_;
  int32_t preference_;

  std::unique_ptr<Builder::Network> builder_;
  std::unique_ptr<CNNNetwork> network_;

  std::map<uint32_t, size_t> layer_id_map_;

  DISALLOW_COPY_AND_ASSIGN(Compilation);
};

}  // namespace InferenceEngine

#endif  // IE_COMPILATION_H
