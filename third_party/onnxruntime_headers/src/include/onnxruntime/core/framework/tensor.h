// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <cstddef>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <gsl/gsl>
#include "core/common/common.h"
#include "core/framework/allocator.h"
#include "core/framework/tensor_shape.h"
#include "core/framework/buffer_deleter.h"
#include "onnxruntime_config.h"
#include "core/framework/data_types.h"
#include "core/framework/data_types_internal.h"

struct OrtValue;

namespace onnxruntime {

// TODO:ensure dtype_!=nullptr
#ifdef __GNUC__
#pragma GCC diagnostic push
#ifdef HAS_NULL_DEREFERENCE
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
#endif
/*
  We want to keep tensor as simple as possible, it is just a placeholder
  for a piece of memory, with additional shape information.
  Memory is owned and managed by Executor / Workspace, so Tensor just uses
  it, and won't do any allocation / release.
*/

class Tensor final {
 public:
  // NB! Removing Create() methods returning unique_ptr<Tensor>.
  // Still available in other EPs that are dynamically linked.
  // Strive not to allocate Tensor with new/delete as it is a shallow class and using it by value is just fine.
  // Use InitOrtValue() methods to allocate for OrtValue.

  Tensor() = default;  // to allow creating vector<Tensor> to support seq(tensor)

  /**
   * Create tensor with given type, shape, pre-allocated memory and allocator info.
   * This function does not check if the preallocated buffer(p_data) has enough room for the shape.
   * \param elt_type Data type of the tensor elements.
   * \param shape Shape of the tensor
   * \param p_data A preallocated buffer. Can be NULL if the shape is empty.
   *               Tensor does not own the data and will not delete it
   * \param location Memory info for location of p_data.
   * \param offset Offset in bytes to start of Tensor within p_data.
   * \param strides Strides span. Can be empty if the tensor is contiguous.
   */
  Tensor(MLDataType elt_type, const TensorShape& shape, void* p_data, const OrtMemoryInfo& location,
         ptrdiff_t offset = 0, gsl::span<const int64_t> strides = {});

  /**
   * Create tensor with given type, shape, pre-allocated memory and allocator which will be used to free the
   * pre-allocated memory. The Tensor will take over ownership of p_data.
   * This function does not check if the preallocated buffer(p_data) has enough room for the shape.
   * \param elt_type Data type of the tensor elements.
   * \param shape Shape of the tensor
   * \param p_data A preallocated buffer. Can be NULL if the shape is empty.
   *               Tensor will own the memory and will delete it when the tensor instance is destructed.
   * \param deleter Allocator used to free the pre-allocated memory
   * \param offset Offset in bytes to start of Tensor within p_data.
   * \param strides Strides span. Can be empty if the tensor is contiguous.
   */
  Tensor(MLDataType elt_type, const TensorShape& shape, void* p_data, std::shared_ptr<IAllocator> deleter,
         ptrdiff_t offset = 0, gsl::span<const int64_t> strides = {});

  /// <summary>
  /// Create a Tensor that allocates and owns the buffer required for the specified shape.
  /// </summary>
  /// <param name="elt_type">Data type of the tensor elements.</param>
  /// <param name="shape">Tensor shape.</param>
  /// <param name="allocator">Allocator to use to create and free buffer.</param>
  Tensor(MLDataType elt_type, const TensorShape& shape, std::shared_ptr<IAllocator> allocator);

  ~Tensor();

  // Move is allowed
  ORT_DISALLOW_COPY_AND_ASSIGNMENT(Tensor);

  Tensor(Tensor&& other) noexcept;
  Tensor& operator=(Tensor&& other) noexcept;

  /// <summary>
  /// Creates an instance of Tensor on the heap and initializes OrtValue with it.
  /// </summary>
  /// <param name="elt_type">Data type of the tensor elements.</param>
  /// <param name="shape">Tensor shape.</param>
  /// <param name="p_data">Tensor data.</param>
  /// <param name="location">Memory info for location of p_data.</param>
  /// <param name="ort_value">OrtValue to populate with Tensor.</param>
  /// <param name="offset">Optional offset if Tensor refers to a subset of p_data.</param>
  /// <param name="strides">Optional strides if Tensor refers to a subset of p_data.</param>
  static void InitOrtValue(MLDataType elt_type, const TensorShape& shape, void* p_data, const OrtMemoryInfo& location,
                           OrtValue& ort_value,
                           ptrdiff_t offset = 0, gsl::span<const int64_t> strides = {});

  /// <summary>
  /// Creates an instance of Tensor on the heap which will take over ownership of the pre-allocated buffer.
  /// </summary>
  /// <param name="elt_type">Data type of the tensor elements.</param>
  /// <param name="shape"Tensor shape.</param>
  /// <param name="p_data">Tensor data.</param>
  /// <param name="allocator">Allocator that was used to create p_data and will be used to free it.</param>
  /// <param name="ort_value">OrtValue to populate with Tensor.</param>
  /// <param name="offset">Optional offset if Tensor refers to a subset of p_data.</param>
  /// <param name="strides">Optional strides if Tensor refers to a subset of p_data.</param>
  static void InitOrtValue(MLDataType elt_type, const TensorShape& shape, void* p_data,
                           std::shared_ptr<IAllocator> allocator,
                           OrtValue& ort_value,
                           ptrdiff_t offset = 0, gsl::span<const int64_t> strides = {});

  /// <summary>
  /// Creates an instance of Tensor on the heap and initializes OrtValue with it.
  /// The Tensor instance will allocate and own the data required for `shape`.
  /// </summary>
  /// <param name="elt_type">Data type of the tensor elements.</param>
  /// <param name="shape">Tensor shape.</param>
  /// <param name="allocator">Allocator that was used to create p_data and will be used to free it.</param>
  /// <param name="ort_value">OrtValue to populate with Tensor.</param>
  static void InitOrtValue(MLDataType elt_type, const TensorShape& shape, std::shared_ptr<IAllocator> allocator,
                           OrtValue& ort_value);

  /// <summary>
  /// Initializes OrtValue with an existing Tensor.
  /// </summary>
  /// <param name="tensor">Tensor.</param>
  /// <param name="ort_value">OrtValue to populate with Tensor.</param>
  static void InitOrtValue(Tensor&& tensor, OrtValue& ort_value);

  /// <summary>
  /// Calculate the required storage for the tensor.
  /// </summary>
  /// <param name="elt_type">Data type of the tensor elements.</param>
  /// <param name="shape">Tensor shape.</param>
  /// <returns>Bytes required.</returns>
  static size_t CalculateTensorStorageSize(MLDataType elt_type, const TensorShape& shape);

  /// <summary>
  /// Calculate the required storage for the tensor.
  /// </summary>
  /// <param name="elt_type">Data type of the tensor elements.</param>
  /// <param name="shape">Tensor shape.</param>
  /// <param name="alignment">Power of 2 alignment to include in calculation.
  /// Bumps up result to the nearest multiple of alignment. Set to 0 to ignore.</param>
  /// <param name="storage_size">The resulting storage size.</param>
  /// <returns>Status indicating success or failure.</returns>
  static Status CalculateTensorStorageSize(MLDataType elt_type, const TensorShape& shape, size_t alignment,
                                           size_t& storage_size);
  /**
     Returns the data type.
  */
  MLDataType DataType() const { return dtype_; }

  /**
     Returns the data type enum constant
     @remarks Use utils::ToTensorProtoElementType<T> for comparison.
  */
  int32_t GetElementType() const {
    return dtype_->GetDataType();
  }

  // Check if contains string data. This is a separate
  // interface bc it is frequently used.
  bool IsDataTypeString() const {
    return utils::IsPrimitiveDataType<std::string>(dtype_);
  }

  // Checks if the Tensor contains data type T
  template <class T>
  bool IsDataType() const {
    return utils::IsPrimitiveDataType<T>(dtype_);
  }

  /**
     Returns the shape of the tensor.
  */
  const TensorShape& Shape() const noexcept { return shape_; }

  /**
     Returns the location of the tensor's memory
  */
  const OrtMemoryInfo& Location() const { return alloc_info_; }

  /**
     May return nullptr if tensor size is zero
  */
  template <typename T>
  T* MutableData() {
    // Type check
    ORT_ENFORCE(utils::IsPrimitiveDataType<T>(dtype_), "Tensor type mismatch. ",
                "T ", "!=", dtype_);
    return reinterpret_cast<T*>(static_cast<char*>(p_data_) + byte_offset_);
  }

  /**
     May return nullptr if tensor size is zero
  */
  template <typename T>
  gsl::span<T> MutableDataAsSpan() {
    // Type check
    ORT_ENFORCE(utils::IsPrimitiveDataType<T>(dtype_), "Tensor type mismatch. ",
                "T ", "!=", dtype_);
    T* data = reinterpret_cast<T*>(static_cast<char*>(p_data_) + byte_offset_);
    return gsl::make_span(data, static_cast<size_t>(NumStorageElements()));
  }

  template <typename T>
  const T* Data() const {
    // Type check
    ORT_ENFORCE(utils::IsPrimitiveDataType<T>(dtype_), "Tensor type mismatch. ",
                "T ", "!=", dtype_);
    return reinterpret_cast<const T*>(static_cast<char*>(p_data_) + byte_offset_);
  }

  template <typename T>
  gsl::span<const T> DataAsSpan() const {
    // Type check
    ORT_ENFORCE(utils::IsPrimitiveDataType<T>(dtype_), "Tensor type mismatch. ",
                "T ", "!=", dtype_);
    const T* data = reinterpret_cast<const T*>(static_cast<char*>(p_data_) + byte_offset_);
    return gsl::make_span(data, static_cast<typename gsl::span<T>::size_type>(NumStorageElements()));
  }

  void* MutableDataRaw(MLDataType type) {
    ORT_ENFORCE(type == dtype_, "Tensor type mismatch.", type, "!=", dtype_);
    return static_cast<char*>(p_data_) + byte_offset_;
  }

  const void* DataRaw(MLDataType type) const {
    ORT_ENFORCE(type == dtype_, "Tensor type mismatch.", type, "!=", dtype_);
    return static_cast<char*>(p_data_) + byte_offset_;
  }

  void* MutableDataRaw() noexcept {
    return static_cast<char*>(p_data_) + byte_offset_;
  }

  const void* DataRaw() const noexcept {
    return static_cast<char*>(p_data_) + byte_offset_;
  }

  bool OwnsBuffer() const noexcept {
    return buffer_deleter_ != nullptr;
  }

  /**
   * Resizes the tensor without touching underlying storage.
   * This requires the total size of the tensor to remains constant.
   * @warning this function is NOT thread-safe.
   */
  inline void Reshape(const TensorShape& new_shape) {
    ORT_ENFORCE(shape_.Size() == new_shape.Size(),
                "Tensor size (" + std::to_string(shape_.Size()) +
                    ") != new size (" + std::to_string(new_shape.Size()) + ")");
    shape_ = new_shape;
  }

  /**
   * Get the byte offset with respect to the p_data
   * @warning this is a temporary solution for reusing the buffer bigger than needed.
   * @warning use with caution - make sure you do boundary check before calling this method (see view.cc)
   */
  inline ptrdiff_t ByteOffset() const {
    return byte_offset_;
  }

  /**
   * Set the byte offset with respect to the p_data
   * @warning this is a temporary solution for reusing the buffer bigger than needed.
   */
  inline void SetByteOffset(ptrdiff_t byte_offset) {
    byte_offset_ = byte_offset;
  }

  /// <summary>
  /// The number of Tensor "storage" elements. A single storage element may contain multiple sub-elements for
  /// sub-byte data types (e.g., int4).
  ///
  /// For element types smaller than 1 byte (e.g., int4), a single storage element stores multiple sub-byte elements.
  /// Example: Tensor<int4> of shape (4,) has 2 storage elements.
  ///
  /// For element types >= 1 byte, this function returns the product of the shape.
  /// Example: Tensor<int8> of shape (4,) has 4 storage elements.
  /// </summary>
  /// <returns>Number of tensor storage elements</returns>
  int64_t NumStorageElements() const;

  /**
  The number of bytes of data.
  */
  size_t SizeInBytes() const;

#ifdef ENABLE_STRIDED_TENSORS
  /**
   * Get the strides of the tensor.
   */
  gsl::span<const int64_t> Strides() const;

  /**
   * Return if the tensor is contiguous.
   */
  bool IsContiguous() const noexcept { return is_contiguous_; }

  /**
   * Set strides.
   */
  void SetShapeAndStrides(const TensorShape& new_shape, gsl::span<const int64_t> new_strides);
#endif

  // More API methods.
 private:
  void Init(MLDataType elt_type,
            const TensorShape& shape,
            void* p_raw_data,
            AllocatorPtr deleter,
            ptrdiff_t offset = 0,
            gsl::span<const int64_t> strides = {});

  void ReleaseBuffer();

#ifdef ENABLE_STRIDED_TENSORS
  bool CheckIsContiguous() const;
#endif

  void* p_data_;
  /**
     if buffer_deleter_ is null, it means tensor does not own the buffer.
     otherwise tensor will use the deleter to release the buffer when
     tensor is released.
  */
  AllocatorPtr buffer_deleter_;

  TensorShape shape_;
#ifdef ENABLE_STRIDED_TENSORS
  mutable TensorShapeVector strides_;
  bool is_contiguous_ = true;
#endif

  const PrimitiveDataTypeBase* dtype_;
  OrtMemoryInfo alloc_info_;
  ptrdiff_t byte_offset_;
};
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
}  // namespace onnxruntime
