// Copyright(C) 2019 Intel Corporation
// Licensed under the MIT License

#include "onnxruntime_c_api.h"

#ifdef __cplusplus
struct ProviderInfo_OpenVINO {
  virtual std::vector<std::string> GetAvailableDevices() const = 0;
};

extern "C" {
#endif

/**
 * \param device_type openvino device type and precision. Could be any of
 * CPU_FP32, CPU_FP16, GPU_FP32, GPU_FP16
 */
ORT_API_STATUS(OrtSessionOptionsAppendExecutionProvider_OpenVINO,
               _In_ OrtSessionOptions* options, _In_ const char* device_type);

#ifdef __cplusplus
}
#endif
