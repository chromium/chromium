// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// Do not include this file directly. Please include "experimental_onnxruntime_cxx_api.h" instead.
//
// These are the inline implementations of the C++ header APIs. They are in this separate file as to not clutter
// the main C++ file with implementation details.
namespace Ort {
namespace Experimental {

inline std::vector<Ort::Value> Session::Run(const std::vector<std::string>& input_names, const std::vector<Ort::Value>& input_values,
                                            const std::vector<std::string>& output_names, const RunOptions& run_options) {
  size_t output_names_count = GetOutputNames().size();
  std::vector<Ort::Value> output_values;
  for (size_t i = 0; i < output_names_count; i++) output_values.emplace_back(nullptr);
  Run(input_names, input_values, output_names, output_values, run_options);
  return output_values;
}

inline void Session::Run(const std::vector<std::string>& input_names, const std::vector<Ort::Value>& input_values,
                         const std::vector<std::string>& output_names, std::vector<Ort::Value>& output_values, const RunOptions& run_options) {
  size_t input_names_count = input_names.size();
  size_t output_names_count = output_names.size();
  std::vector<const char*> input_names_(input_names_count, nullptr);
  size_t i = 0;
  for (auto it = input_names.begin(); it != input_names.end(); it++) input_names_[i++] = (*it).c_str();
  std::vector<const char*> output_names_(output_names_count, nullptr);
  i = 0;
  for (auto it = output_names.begin(); it != output_names.end(); it++) output_names_[i++] = (*it).c_str();
  Ort::Session::Run(run_options, input_names_.data(), input_values.data(), input_names_count, output_names_.data(), output_values.data(), output_names_count);
}

inline std::vector<std::string> Session::GetInputNames() const {
  Ort::AllocatorWithDefaultOptions allocator;
  size_t node_count = GetInputCount();
  std::vector<std::string> out(node_count);
  for (size_t i = 0; i < node_count; i++) {
    auto tmp = GetInputNameAllocated(i, allocator);
    out[i] = tmp.get();
  }
  return out;
}

inline std::vector<std::string> Session::GetOutputNames() const {
  Ort::AllocatorWithDefaultOptions allocator;
  size_t node_count = GetOutputCount();
  std::vector<std::string> out(node_count);
  for (size_t i = 0; i < node_count; i++) {
    auto tmp = GetOutputNameAllocated(i, allocator);
    out[i] = tmp.get();
  }
  return out;
}

inline std::vector<std::string> Session::GetOverridableInitializerNames() const {
  Ort::AllocatorWithDefaultOptions allocator;
  size_t init_count = GetOverridableInitializerCount();
  std::vector<std::string> out(init_count);
  for (size_t i = 0; i < init_count; i++) {
    auto tmp = GetOverridableInitializerNameAllocated(i, allocator);
    out[i] = tmp.get();
  }
  return out;
}

inline std::vector<std::vector<int64_t>> Session::GetInputShapes() const {
  size_t node_count = GetInputCount();
  std::vector<std::vector<int64_t>> out(node_count);
  for (size_t i = 0; i < node_count; i++) out[i] = GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape();
  return out;
}

inline std::vector<std::vector<int64_t>> Session::GetOutputShapes() const {
  size_t node_count = GetOutputCount();
  std::vector<std::vector<int64_t>> out(node_count);
  for (size_t i = 0; i < node_count; i++) out[i] = GetOutputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape();
  return out;
}

inline std::vector<std::vector<int64_t>> Session::GetOverridableInitializerShapes() const {
  size_t init_count = GetOverridableInitializerCount();
  std::vector<std::vector<int64_t>> out(init_count);
  for (size_t i = 0; i < init_count; i++) out[i] = GetOverridableInitializerTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape();
  return out;
}

template <typename T>
inline Ort::Value Value::CreateTensor(T* p_data, size_t p_data_element_count, const std::vector<int64_t>& shape) {
  return CreateTensor(p_data, p_data_element_count * sizeof(T), shape, TypeToTensorType<T>::type);
}

inline Ort::Value Value::CreateTensor(void* p_data, size_t p_data_byte_count, const std::vector<int64_t>& shape, ONNXTensorElementDataType type) {
  Ort::MemoryInfo info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
  return Ort::Value::CreateTensor(info, p_data, p_data_byte_count, shape.data(), shape.size(), type);
}

template <typename T>
inline Ort::Value Value::CreateTensor(const std::vector<int64_t>& shape) {
  return CreateTensor(shape, TypeToTensorType<T>::type);
}

inline Ort::Value Value::CreateTensor(const std::vector<int64_t>& shape, ONNXTensorElementDataType type) {
  Ort::AllocatorWithDefaultOptions allocator;
  return Ort::Value::CreateTensor(allocator, shape.data(), shape.size(), type);
}

}  // namespace Experimental
}  // namespace Ort
