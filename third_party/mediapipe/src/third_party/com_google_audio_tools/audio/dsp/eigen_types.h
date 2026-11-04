/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Utilities for working generically with Eigen types.

#ifndef AUDIO_DSP_EIGEN_TYPES_H_
#define AUDIO_DSP_EIGEN_TYPES_H_

#include <type_traits>

#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/types/span.h"
#include "third_party/eigen3/Eigen/Core"



namespace audio_dsp {

// If EigenType `x` is a column vector at compile time, TransposeToRowVector(x)
// returns x.transpose(). Otherwise, `x` is returned unchanged.
template <typename EigenType>
auto TransposeToRowVector(
    EigenType&& x,
    typename std::enable_if_t<  // If `x` is a column vector at compile time.
        std::decay_t<EigenType>::ColsAtCompileTime == 1>* absl_nullable =
        nullptr) {
  return x.transpose();  // Return Eigen::Transpose expression object by value.
}
template <typename EigenType>
EigenType&& TransposeToRowVector(
    EigenType&& x,
    typename std::enable_if_t<  // If `x` is not a column vector.
        std::decay_t<EigenType>::ColsAtCompileTime != 1>* absl_nullable =
        nullptr) {
  return std::forward<EigenType>(x);  // Forward the object without change.
}

// Traits class to test at compile time whether a type is a contiguous 1D Eigen
// type, such that the ith element is accessible through .data()[i].
//
// Examples:
//   IsContiguous1DEigenType<ArrayXf>::Value  // = true
//   IsContiguous1DEigenType<Map<VectorXf>>::Value  // = true
//   IsContiguous1DEigenType<std::vector<float>>::Value  // = false
template <typename Type, typename = void>
struct IsContiguous1DEigenType { enum { Value = false }; };

namespace internal {
// For use with SFINAE to enable only when T is an Eigen::DenseBase type.
template <typename T> void WellFormedIfDenseEigenType(Eigen::DenseBase<T>&&);
}  // namespace internal

template <typename EigenType>
struct IsContiguous1DEigenType<EigenType,
    decltype(internal::WellFormedIfDenseEigenType(std::declval<EigenType>()))> {
  enum {
    Value = EigenType::IsVectorAtCompileTime &&
        EigenType::InnerStrideAtCompileTime == 1
  };
};

namespace internal {
template <typename Container> class ContainerWrapper;
template <typename Container>
struct ContainerWrapperTraits { enum { Valid = false }; };
}  // namespace internal

// Wrap std::vector, absl::Span, and Eigen types with a uniform interface.
// WrapContainer() returns a `ContainerWrapper` (defined below) to make a
// uniform interface for accessing the size and resizing the wrapped object and
// also to return an Eigen Matrix representation (e.g. an Eigen::Map for vectors
// and Spans).
//
// The following container types are supported:
//
//  * Resizable containers
//    * std::vector
//    * Eigen::Array
//    * Eigen::Matrix, Eigen::Vector, Eigen::RowVector
//
//  * Non-resizable containers
//    * absl::Span
//    * Eigen::VectorBlock: the expression objects created by
//      .head(), .segment(), .tail()
//    * Eigen::Block: the expression objects created by
//      .row(), .leftCols(), .block(), etc.
//    * Eigen::CwiseNullaryOp: expression objects like Zero, Ones, and Random.
//    * Eigen::Map
//
// This wrapper is useful for writing generic code in a form like:
//
//   template <typename Output>
//   void Foo(Output&& output) {  // Public interface.
//     FooWrapped(WrapContainer(std::forward<Output>(output)));
//   }
//
//   template <typename WrappedOutput>
//   void FooWrapped(WrappedOutput&& output) {
//     if constexpr (Output::Dims == 1) {  // Output is a 1D type.
//       ABSL_CHECK(output.resize(rows * cols));
//     } else {  // Output is a 2D type.
//       ABSL_CHECK(output.resize(rows, cols));
//     }
//     FooImpl(output.AsMatrix(rows));
//   }
//
//   template <typename EigenMatrixType>
//   void FooImpl(EigenMatrixType&& output) { ... }  // Implementation.
//
//   // Calling code.
//   std::vector<float> x;
//   Foo(x);
//
//   // `input` lives at least throughout the scope of Fun().
//   Fun(WrapContainer(input));  // OK.
//
// For comparison, do not do this:
//
//   // Span returned by MakeSpan is destroyed after this line.
//   auto wrapper = WrapContainer(absl::MakeSpan(buffer));  // WRONG.
//   // (Code using wrapper.)
//
// Code using the wrapper attempts to use a dead reference, which is undefined
// behavior. Also be careful when wrapping Eigen expressions. Do not do this:
//
//   Eigen::MatrixXf m;
//   // Block expression returned by m.row(0) is destroyed after this line.
//   auto wrapper = WrapContainer(m.row(0));  // WRONG.
//   // (Code using wrapper.)
//
// SIMPLIFIED API
// Alternatively, at a small performance penalty, a simplified API may be used
// through the "Hold" methods. Here, the returned Eigen objects are consistently
// Eigen::Map<Array<Scalar, Dynamic, Dynamic>> (or with const, or with Matrix),
// instead of working with Matrix-like objects whose exact type depends on the
// wrapped container. If the Scalar type can be fixed, this makes it possible to
// put the generic implementation in the .cc. This interface works even for
// blocks and expression objects, by creating a temporary backing copy when
// needed.
//
// There are three "Hold" methods. The need for this is so that the holder knows
// when to copy to and/or from the backing array:
//
//   `HoldReadOnly`    Container will be read but not written (an input arg)
//   `HoldWriteOnly`   Container will be written but not read (an output arg)
//   `HoldReadWrite`   Container will be both read and written (an in-out arg)
//
// The downsides are that an allocation and additional copying are sometimes
// needed and that the generic implementation does not benefit from fixed array
// dimensions or memory alignment. For many purposes, this may be a reasonable
// price for simpler code. Intended use:
//
//   template <typename Output>
//   void Foo(Output&& output) {  // Public interface.
//     FooWrapped(WrapContainer(std::forward<Output>(output)));
//   }
//
//   template <typename WrappedOutput>
//   void FooWrapped(WrappedOutput&& output) {
//     // Check or reshape dimensions...
//     FooImpl(output.HoldWriteOnly().AsArray(rows));
//   }
//
//   void FooImpl(Eigen::Map<Eigen::ArrayXXf> output);  // Implementation.
//
// Particularly, `FooImpl` is a regular non-template function and could be
// defined in the .cc file.
template <typename Container>
auto WrapContainer(Container&& container) {
  return internal::ContainerWrapper<
      typename std::remove_reference_t<Container>>(
      std::forward<Container>(container));
}

namespace internal {

template <typename Container, bool ReadAccess, bool WriteAccess,
          bool IsColMajorContiguous>
class ContainerHolder;

template <typename Container_>
class ContainerWrapper {
 public:
  using Container = typename std::remove_reference_t<Container_>;
  using Traits = ContainerWrapperTraits<typename std::decay_t<Container>>;
  static_assert(Traits::Valid, "Invalid type for ContainerWrapper.");
  enum {
    // The number of container dimensions, either 1 or 2. Returns 2 for all
    // Eigen types, even those that are vectors at compile time.
    //
    //   std::vector<T>  => 1
    //   Eigen::MatrixXf => 2
    //   Eigen::VectorXf => 2
    Dims = Traits::Dims,
    // Number of rows at compile time. Returns Eigen::Dynamic if not fixed.
    RowsAtCompileTime = Traits::RowsAtCompileTime,
    // Number of columns at compile time. Returns Eigen::Dynamic if not fixed.
    ColsAtCompileTime = Traits::ColsAtCompileTime,
    // True if the container is 1D or an Eigen vector type.
    IsVectorAtCompileTime = Traits::IsVectorAtCompileTime,
    // Whether the container data is const.
    IsConst = Traits::IsConst || std::is_const_v<Container>,
    // Whether the container is resizable. True for non-const std::vector,
    // Eigen::Array, and Eigen::Matrix.
    IsResizable = Traits::IsResizable && !std::is_const_v<Container>,
    // Whether the container data is column-major and contiguous, and therefore
    // representable with Eigen::Map with default strides.
    IsColMajorContiguous = Traits::IsColMajorContiguous,
  };

  explicit ContainerWrapper(Container& c): c_(c) {}
  explicit ContainerWrapper(Container&& c): c_(c) {}

  // size accessor, works for all containers.
  Eigen::Index size() const { return static_cast<Eigen::Index>(c_.size()); }
  // rows and cols accessors, should only be used if Dims == 2.
  Eigen::Index rows() const { return c_.rows(); }
  Eigen::Index cols() const { return c_.cols(); }

  // Resizes a 1D container or checks the size. Returns true on success.
  bool resize(Eigen::Index new_size) {
    if constexpr (!IsVectorAtCompileTime) {
      return false;  // Fail if container is not 1D.
    } else if constexpr (IsResizable) {
      if (ABSL_PREDICT_FALSE(size() != new_size)) {
        c_.resize(new_size);
      }
      return true;
    } else {
      return size() == new_size;
    }
  }

  // Resizes a 2D container or checks the shape. Returns true on success.
  bool resize(Eigen::Index new_rows, Eigen::Index new_cols) {
    if constexpr (Dims != 2) {
      return false;  // Fail if container is not 2D.
    } else if constexpr (IsResizable) {
      if (ABSL_PREDICT_FALSE(rows() != new_rows || cols() != new_cols)) {
        c_.resize(new_rows, new_cols);
      }
      return true;
    } else {
      return rows() == new_rows && cols() == new_cols;
    }
  }

  template <int MapRows>
  using Matrix = decltype(Traits::template AsMatrix<MapRows>(
     std::declval<Container&>(), 1));

  // Represent the container as an Eigen Matrix type. For Eigen types, returns
  // `container.matrix()`. Otherwise returns an Eigen::Map mapping the container
  // with `map_rows` rows.
  Matrix<Eigen::Dynamic> AsMatrix(int map_rows) const {
    return Traits::template AsMatrix<Eigen::Dynamic>(c_, map_rows);
  }
  // Same as above, but specifying `map_rows` at compile time.
  template <int MapRows>
  Matrix<MapRows> AsMatrix() const {
    return Traits::template AsMatrix<MapRows>(c_, MapRows);
  }

  // For the "simplified API" (see above). Returns a `ContainerHolder` object
  // with a backing array when needed such that the container can be mapped as
  // an Eigen::Map<Matrix<Scalar, Dynamic, Dynamic>>.
  //
  // Choose the "Hold*" variant depending on how the container will be accessed:
  //
  //   `HoldReadOnly`    Container will be read but not written (an input arg)
  //   `HoldWriteOnly`   Container will be written but not read (an output arg)
  //   `HoldReadWrite`   Container will be both read and written (an in-out arg)
  auto HoldReadOnly() const {
    return ContainerHolder<Container, true, false, IsColMajorContiguous>(c_);
  }
  auto HoldWriteOnly() const {
    return ContainerHolder<Container, false, true, IsColMajorContiguous>(c_);
  }
  auto HoldReadWrite() const {
    return ContainerHolder<Container, true, true, IsColMajorContiguous>(c_);
  }

  using BackingArray = typename Traits::BackingArray;
  // Allocate a backing array having the same shape as the container.
  BackingArray AllocateSameShape() {
    if constexpr (Dims == 1) {
      return {1, c_.size()};
    } else {  // Dims == 2.
      return {c_.rows(), c_.cols()};
    }
  }
  // Copy the container content to `dest`, resizing `dest` if necessary.
  void CopyTo(BackingArray& dest) {
    Traits::Copy(c_, dest);
  }
  // Copy from `src` to container. Should only be used if container is mutable.
  // The function attempts to resize the container to match `src`. Returns true
  // on success.
  bool CopyFrom(const BackingArray& src) {
    if constexpr (Dims == 1) {
      if (!resize(src.size())) { return false; }
    } else {  // Dims == 2.
      if (!resize(src.rows(), src.cols())) { return false; }
    }
    Traits::Copy(src, c_);
    return true;
  }

 private:
  Container& c_;
};

template <bool Condition, typename T>
using AddConstIf = std::conditional<Condition, const T, T>;

// ContainerHolder definition when IsColMajorContiguous == false, in which case
// a temporary backing array is used.
template <typename Container, bool ReadAccess, bool WriteAccess,
          bool IsColMajorContiguous>
class ContainerHolder {
 public:
  using Traits = ContainerWrapperTraits<typename std::decay_t<Container>>;
  using MapArray = Eigen::Map<typename AddConstIf<
      Traits::IsConst || !WriteAccess,
      typename Eigen::Array<typename Traits::Scalar, Eigen::Dynamic,
                            Eigen::Dynamic>>::type>;
  using MapMatrix = Eigen::Map<typename AddConstIf<
      Traits::IsConst || !WriteAccess,
      typename Eigen::Matrix<typename Traits::Scalar, Eigen::Dynamic,
                             Eigen::Dynamic>>::type>;

  explicit ContainerHolder(Container& c)
      : wrapper_(c), backing_(wrapper_.AllocateSameShape()) {
    // If data will be read from this holder, copy the container to the backing.
    if constexpr (ReadAccess) {
      wrapper_.CopyTo(backing_);
    }
  }

  ~ContainerHolder() {
    // If data was written to this holder, copy the backing to the container.
    if constexpr (WriteAccess) {
      wrapper_.CopyFrom(backing_);
    }
  }

  MapArray AsArray(int map_rows) {
    return {backing_.data(), backing_.rows(), backing_.cols()};
  }

  MapMatrix AsMatrix(int map_rows) {
    return {backing_.data(), backing_.rows(), backing_.cols()};
  }

 private:
  ContainerWrapper<Container> wrapper_;
  typename Traits::BackingArray backing_;
};

// ContainerHolder for when IsColMajorContiguous == true, and Maps are made
// directly on the container memory.
template <typename Container, bool ReadAccess, bool WriteAccess>
class ContainerHolder<Container, ReadAccess, WriteAccess, true> {
 public:
  using Traits = ContainerWrapperTraits<typename std::decay_t<Container>>;
  using MapArray = Eigen::Map<typename AddConstIf<
      Traits::IsConst || !WriteAccess,
      typename Eigen::Array<typename Traits::Scalar, Eigen::Dynamic,
                            Eigen::Dynamic>>::type>;
  using MapMatrix = Eigen::Map<typename AddConstIf<
      Traits::IsConst || !WriteAccess,
      typename Eigen::Matrix<typename Traits::Scalar, Eigen::Dynamic,
                             Eigen::Dynamic>>::type>;

  explicit ContainerHolder(Container& c) : wrapper_(c) {}

  MapArray AsArray(int map_rows) const {
    auto matrix_object = wrapper_.AsMatrix(map_rows);
    return {matrix_object.data(), matrix_object.rows(), matrix_object.cols()};
  }

  MapMatrix AsMatrix(int map_rows) const {
    auto matrix_object = wrapper_.AsMatrix(map_rows);
    return {matrix_object.data(), matrix_object.rows(), matrix_object.cols()};
  }

 private:
  ContainerWrapper<Container> wrapper_;
};

// Traits ContainerWrapperTraits<C> for different container types C are defined
// as partial template specializations, which ContainerWrapper then looks up as
// `ContainerWrapperTraits<typename std::decay_t<C>>`. Beware that for this look
// up to work, a specialization for containers of type D below must match C
// directly. It is not enough if C is merely convertible to D, not even if C
// inherits from D. Particularly:
//
// * Even though many containers are convertible to absl::Span, they will not
//   find the absl::Span traits.
// * Even though Eigen::PlainObjectBase is the base class of Array and Matrix, a
//   partial specialization for PlainObjectBase wouldn't match Array or Matrix.

// absl::Span<T>.
template <typename ValueType>
struct ContainerWrapperTraits<absl::Span<ValueType>> {
  using Container = absl::Span<ValueType>;
  using Scalar = typename std::remove_const_t<ValueType>;
  enum {
    Valid = true,
    Dims = 1,
    RowsAtCompileTime = Eigen::Dynamic,
    ColsAtCompileTime = Eigen::Dynamic,
    IsVectorAtCompileTime = true,
    IsConst = std::is_const_v<ValueType>,
    IsResizable = false,
    IsColMajorContiguous = true,
  };
  using BackingArray = typename Eigen::Array<Scalar, 1, Eigen::Dynamic>;

  template <int MapRows>
  static auto AsMatrix(Container& c, int map_rows) {
    using Matrix = typename Eigen::Matrix<Scalar, MapRows, Eigen::Dynamic>;
    return Eigen::Map<typename AddConstIf<IsConst, Matrix>::type>(
        c.data(), map_rows, c.size() / map_rows);
  }
  template <int MapRows>
  static auto AsMatrix(const Container& c, int map_rows) {
    using Matrix = typename Eigen::Matrix<Scalar, MapRows, Eigen::Dynamic>;
    return Eigen::Map<const Matrix>(c.data(), map_rows, c.size() / map_rows);
  }

  static void Copy(const Container& src, BackingArray& dest) {
    dest = AsMatrix(src, 1);
  }
  static void Copy(const BackingArray& src, Container& dest) {
    if constexpr (!IsConst) {
      AsMatrix(dest, 1) = src;
    }
  }
};

// std::vector<T>.
template <typename ValueType, typename Allocator>
struct ContainerWrapperTraits<std::vector<ValueType, Allocator>> {
  using Container = std::vector<ValueType, Allocator>;
  using Scalar = typename Container::value_type;
  enum {
    Valid = true,
    Dims = 1,
    RowsAtCompileTime = Eigen::Dynamic,
    ColsAtCompileTime = Eigen::Dynamic,
    IsVectorAtCompileTime = true,
    IsConst = false,
    IsResizable = true,
    IsColMajorContiguous = true,
  };
  using BackingArray = typename Eigen::Array<Scalar, 1, Eigen::Dynamic>;

  template <int MapRows>
  static auto AsMatrix(Container& c, int map_rows) {
    return Eigen::Map<Eigen::Matrix<Scalar, MapRows, Eigen::Dynamic>>(
        c.data(), map_rows, c.size() / map_rows);
  }
  template <int MapRows>
  static auto AsMatrix(const Container& c, int map_rows) {
    return Eigen::Map<const Eigen::Matrix<Scalar, MapRows, Eigen::Dynamic>>(
        c.data(), map_rows, c.size() / map_rows);
  }

  static void Copy(const Container& src, BackingArray& dest) {
    dest = AsMatrix(src, 1);
  }
  static void Copy(const BackingArray& src, Container& dest) {
    AsMatrix(dest, 1) = src;
  }
};

// Base traits for Eigen types, reused by the definitions below.
template <typename EigenType, bool IsResizable_, bool IsColMajorContiguous_>
struct EigenContainerWrapperTraits {
  using Container = EigenType;
  using Scalar = typename EigenType::Scalar;
  using CoeffReturnType = decltype(std::declval<Container&>()(0, 0));
  enum {
    Valid = true,
    Dims = 2,
    RowsAtCompileTime = Container::RowsAtCompileTime,
    ColsAtCompileTime = Container::ColsAtCompileTime,
    IsVectorAtCompileTime = Container::IsVectorAtCompileTime,
    IsConst = std::is_const_v<std::remove_reference_t<CoeffReturnType>> ||
              !std::is_lvalue_reference_v<CoeffReturnType>,
    IsResizable = IsResizable_,
    IsColMajorContiguous = IsColMajorContiguous_,
  };
  using BackingArray =
      typename Eigen::Array<Scalar, RowsAtCompileTime, ColsAtCompileTime>;

  template <int UnusedMapRows>
  static decltype(std::declval<Container&>().matrix()) AsMatrix(
      Container& c, int) {
    return c.matrix();
  }
  template <int UnusedMapRows>
  static decltype(std::declval<const Container&>().matrix()) AsMatrix(
      const Container& c, int) {
    return c.matrix();
  }

  template <typename Src, typename Dest>
  static void Copy(const Src& src, Dest& dest) { dest = src; }
};

// Traits specializations for specific Eigen types.

// Eigen::Array.
template <typename Scalar, int Rows, int Cols, int Options>
struct ContainerWrapperTraits<Eigen::Array<Scalar, Rows, Cols, Options>>
    : public EigenContainerWrapperTraits<
          Eigen::Array<Scalar, Rows, Cols, Options>, /*IsResizable=*/true,
          /*IsColMajorContiguous=*/!(Options & Eigen::RowMajor)> {};
// Eigen::Matrix.
template <typename Scalar, int Rows, int Cols, int Options>
struct ContainerWrapperTraits<Eigen::Matrix<Scalar, Rows, Cols, Options>>
    : public EigenContainerWrapperTraits<
          Eigen::Matrix<Scalar, Rows, Cols, Options>, /*IsResizable=*/true,
          /*IsColMajorContiguous=*/!(Options & Eigen::RowMajor)> {};
// Eigen::VectorBlock.
template <typename VectorType, int BlockSize>
struct ContainerWrapperTraits<Eigen::VectorBlock<VectorType, BlockSize>>
    : public EigenContainerWrapperTraits<
          Eigen::VectorBlock<VectorType, BlockSize>,
          /*IsResizable=*/false,
          // Vector block is col-major contiguous if inner stride == 1.
          /*IsColMajorContiguous=*/
          Eigen::VectorBlock<VectorType, BlockSize>::InnerStrideAtCompileTime ==
              1> {};
// Eigen::Block.
template <typename XprType, int BlockRows, int BlockCols, bool InnerPanel>
struct ContainerWrapperTraits<
    Eigen::Block<XprType, BlockRows, BlockCols, InnerPanel>>
    : public EigenContainerWrapperTraits<
          Eigen::Block<XprType, BlockRows, BlockCols, InnerPanel>,
          /*IsResizable=*/false, /*IsColMajorContiguous=*/false> {};
// Eigen::CwiseNullaryOp.
template <typename NullaryOp, typename PlainObjectType>
struct ContainerWrapperTraits<Eigen::CwiseNullaryOp<NullaryOp, PlainObjectType>>
    : public EigenContainerWrapperTraits<
          Eigen::CwiseNullaryOp<NullaryOp, PlainObjectType>,
          /*IsResizable=*/false,
          /*IsColMajorContiguous=*/false> {};
// Eigen::Map.
template <typename PlainObjectType, int MapOptions, typename StrideType>
struct ContainerWrapperTraits<
    Eigen::Map<PlainObjectType, MapOptions, StrideType>>
    : EigenContainerWrapperTraits<
          Eigen::Map<PlainObjectType, MapOptions, StrideType>,
          /*IsResizable=*/false,
          // Map with default strides is col-major and contiguous provided the
          // `PlainObjectType` is either col-major or a vector.
          /*IsColMajorContiguous=*/
          !PlainObjectType::IsRowMajor ||
              PlainObjectType::IsVectorAtCompileTime> {};

}  // namespace internal

}  // namespace audio_dsp

#endif  // AUDIO_DSP_EIGEN_TYPES_H_
