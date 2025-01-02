// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <algorithm>
#include <cstring>
#include <iosfwd>
#include <string>
#include <vector>

#include <gsl/gsl>
#include "core/common/inlined_containers_fwd.h"
#include "core/common/span_utils.h"
#include "onnxruntime_config.h"

namespace onnxruntime {
#ifdef __GNUC__
#pragma GCC diagnostic push
#ifdef HAS_NULL_DEREFERENCE
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
#endif

constexpr size_t kTensorShapeSmallBufferElementsSize = 5;

// Use this type to build a shape and then create TensorShape.
// We opt to re-use a common instantiation instead of a typedef with kTensorShapeSmallBufferElementsSize
// To reduce on binary size.
using TensorShapeVector = InlinedVector<int64_t>;

inline TensorShapeVector ToShapeVector(const gsl::span<const int64_t>& span) {
  TensorShapeVector out;
  out.reserve(span.size());
  out.assign(span.begin(), span.end());
  return out;
}

inline gsl::span<const int64_t> ToConstSpan(const TensorShapeVector& vec) {
  return gsl::make_span(vec);
}

class TensorShape {
  // We use negative numbers for unknown symbolic dimension. Each negative
  // number represents a unique symbolic dimension.
 public:
  TensorShape() = default;

  TensorShape(const TensorShape& other) : TensorShape(other.GetDims()) {}
  TensorShape& operator=(const TensorShape& other);
  TensorShape& operator=(const gsl::span<const int64_t>& dims) {
    *this = TensorShape(dims);
    return *this;
  }

  TensorShape(TensorShape&& other) noexcept { operator=(std::move(other)); }
  TensorShape& operator=(TensorShape&& other) noexcept;

  TensorShape(gsl::span<const int64_t> dims);
  TensorShape(const TensorShapeVector& dims) : TensorShape(gsl::make_span(dims)) {}
  TensorShape(std::initializer_list<int64_t> dims) : TensorShape(gsl::make_span(dims.begin(), dims.end())) {}
  TensorShape(const int64_t* dimension_sizes, size_t dimension_count) : TensorShape(gsl::span<const int64_t>(dimension_sizes, dimension_count)) {}
  TensorShape(const std::vector<int64_t>& dims, size_t start, size_t end) : TensorShape(gsl::span<const int64_t>(&dims[start], end - start)) {}

  // Create a TensorShape that points to an existing buffer internally. As no copy is made, 'data' must remain valid for the life of the TensorShape
  static const TensorShape FromExistingBuffer(const std::vector<int64_t>& data) {
    return TensorShape(External{}, gsl::span<int64_t>(const_cast<int64_t*>(data.data()), data.size()));
  }

  /**
     Return the dimension specified by <idx>.
  */
  int64_t operator[](size_t idx) const { return values_[idx]; }
  int64_t& operator[](size_t idx) { return values_[idx]; }

  bool operator==(const TensorShape& other) const noexcept { return SpanEq(GetDims(), other.GetDims()); }
  bool operator!=(const TensorShape& other) const noexcept { return !(*this == other); }

  size_t NumDimensions() const noexcept {
    return values_.size();
  }

  /**
     Copy dims into an array with given size
  */
  void CopyDims(int64_t* dims, size_t num_dims) const {
    memcpy(dims, values_.data(), sizeof(int64_t) * std::min(num_dims, NumDimensions()));
  }

  /**
     Copy dims from a specific start dim into an array with given size
     `start_dim` is expected to be in the inclusive range [0, NumDimensions() - 1]
     and this function does no checks to ensure that
  */
  void CopyDims(int64_t* dims, size_t start_dim, size_t num_dims) const {
    memcpy(dims, values_.data() + start_dim, sizeof(int64_t) * std::min(num_dims, NumDimensions() - start_dim));
  }

  /**
     Return underlying vector representation.
  */
  gsl::span<const int64_t> GetDims() const { return values_; }

  TensorShapeVector AsShapeVector() const {
    return ToShapeVector(values_);
  }

  /**
   * Return the total number of elements. Returns 1 for an empty (rank 0) TensorShape.
   *
   * May return -1
   */
  int64_t Size() const;

  /**
     Return the total number of elements up to the specified dimension.
     If the dimension interval is empty (dimension == 0), return 1.
     @param dimension Return size up to this dimension. Value must be between 0 and this->NumDimensions(), inclusive.
  */
  int64_t SizeToDimension(size_t dimension) const;

  /**
     Return the total number of elements from the specified dimension to the end of the tensor shape.
     If the dimension interval is empty (dimension == this->NumDimensions()), return 1.
     @param dimension Return size from this dimension to the end. Value must be between 0 and this->NumDimensions(),
                      inclusive.
  */
  int64_t SizeFromDimension(size_t dimension) const;

  /**
     Return a new TensorShape of the dimensions from dimstart to dimend.
  */
  TensorShape Slice(size_t dimstart, size_t dimend) const;

  /**
     Return a new TensorShape of the dimensions from dimstart to end.
  */
  TensorShape Slice(size_t dimstart) const { return Slice(dimstart, values_.size()); }

  /**
     output dimensions nicely formatted
  */
  std::string ToString() const;

  /**
     Calculate size between start and end.
     Assumes start and end are between 0 and this->NumDimensions(), inclusive, and that
     start < end.
  */
  int64_t SizeHelper(size_t start, size_t end) const;

  /**
     empty shape or 1D shape (1) is regarded as scalar tensor
  */
  bool IsScalar() const {
    size_t len = values_.size();
    return len == 0 || (len == 1 && values_[0] == 1);
  }

 private:
  struct External {};
  TensorShape(External, gsl::span<int64_t> buffer) : values_{buffer} {}

  void Allocate(size_t size);

  gsl::span<int64_t> values_;
  int64_t small_buffer_[kTensorShapeSmallBufferElementsSize]{0};
  std::unique_ptr<int64_t[]> allocated_buffer_;

  friend struct ProviderHostImpl;  // So that the shared provider interface can access Allocate
};

// operator<< to nicely output to a stream
std::ostream& operator<<(std::ostream& out, const TensorShape& shape);

}  // namespace onnxruntime
