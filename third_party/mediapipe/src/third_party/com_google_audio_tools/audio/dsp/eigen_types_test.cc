/*
 * Copyright 2020-2021 Google LLC
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

#include "audio/dsp/eigen_types.h"

#include <vector>

#include "audio/dsp/testing_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "audio/dsp/porting.h"  // auto-added.

namespace audio_dsp {
namespace {

using ::Eigen::ArrayXf;
using ::Eigen::ArrayXXf;
using ::Eigen::Dynamic;
using ::Eigen::InnerStride;
using ::Eigen::MatrixXf;
using ::Eigen::Map;
using ::Eigen::RowMajor;
using ::Eigen::VectorXf;

using RowMajorMatrixXf = Eigen::Matrix<float, Dynamic, Dynamic, RowMajor>;

TEST(EigenTypesTest, TransposeToRowVector) {
  // The following should be transposed.
  {
    Eigen::ArrayXf x = Eigen::ArrayXf::Ones(20);
    Eigen::Transpose<Eigen::ArrayXf> y = TransposeToRowVector(x);
    EXPECT_EQ(y.rows(), 1);
    EXPECT_EQ(y.cols(), 20);
    y[0] *= 2;  // Test that result is mutable.
    EXPECT_EQ(x[0], 2.0f);
  }
  {
    Eigen::VectorXf x = Eigen::VectorXf::Ones(20);
    auto y = TransposeToRowVector(x);
    EXPECT_EQ(y.rows(), 1);
    EXPECT_EQ(y.cols(), 20);
    auto y_segment = TransposeToRowVector(x.segment(5, 10));
    EXPECT_EQ(y_segment.rows(), 1);
    EXPECT_EQ(y_segment.cols(), 10);
    y_segment[0] *= 2;
    EXPECT_EQ(x[5], 2.0f);
  }
  {
    auto y = TransposeToRowVector(Eigen::ArrayXf::Zero(20));
    EXPECT_EQ(y.rows(), 1);
    EXPECT_EQ(y.cols(), 20);
  }

  // The following should be unchanged.
  {
    Eigen::ArrayXXf x = Eigen::ArrayXXf::Ones(5, 20);
    Eigen::ArrayXXf& y = TransposeToRowVector(x);
    EXPECT_EQ(y.rows(), 5);
    EXPECT_EQ(y.cols(), 20);
    y(0, 0) *= 2;
    EXPECT_EQ(x(0, 0), 2.0f);
  }
  {
    Eigen::RowVectorXf x(20);
    Eigen::RowVectorXf& y = TransposeToRowVector(x);
    EXPECT_EQ(y.rows(), 1);
    EXPECT_EQ(y.cols(), 20);
  }
}

TEST(EigenTypesTest, IsContiguous1DEigenType) {
  // Return true on Array, Vector, and Mapped Array and Vector types.
  EXPECT_TRUE(IsContiguous1DEigenType<ArrayXf>::Value);
  EXPECT_TRUE(IsContiguous1DEigenType<VectorXf>::Value);
  EXPECT_TRUE(IsContiguous1DEigenType<Map<ArrayXf>>::Value);
  EXPECT_TRUE(IsContiguous1DEigenType<Map<const VectorXf>>::Value);
  // Return true on segments of 1D contiguous arrays.
  EXPECT_TRUE(IsContiguous1DEigenType<
              decltype(ArrayXf().segment(0, 9))>::Value);
  // Return true on 1D matrix slices along the major direction.
  EXPECT_TRUE(IsContiguous1DEigenType<
              decltype(MatrixXf().col(0))>::Value);
  EXPECT_TRUE(IsContiguous1DEigenType<
              decltype(RowMajorMatrixXf().row(0))>::Value);

  // Return false on non-Eigen types.
  EXPECT_FALSE(IsContiguous1DEigenType<std::vector<float>>::Value);
  // Return false on 2D Eigen types.
  EXPECT_FALSE(IsContiguous1DEigenType<ArrayXXf>::Value);
  EXPECT_FALSE(IsContiguous1DEigenType<MatrixXf>::Value);
  // Return false on expressions whose elements aren't stored.
  EXPECT_FALSE(IsContiguous1DEigenType<
               decltype(ArrayXf::Random(9))>::Value);
  // Return false if inner stride isn't 1.
  using MappedWithStride2 = Map<VectorXf, 0, InnerStride<2>>;
  EXPECT_FALSE(IsContiguous1DEigenType<MappedWithStride2>::Value);
  EXPECT_FALSE(IsContiguous1DEigenType<
               decltype(ArrayXf().reverse())>::Value);
  EXPECT_FALSE(IsContiguous1DEigenType<
               decltype(MatrixXf().row(0))>::Value);
  EXPECT_FALSE(IsContiguous1DEigenType<
               decltype(RowMajorMatrixXf().col(0))>::Value);
}

// Test WrapContainer(absl::Span).
TEST(EigenTypesTest, ContainerWrapperSpan) {
  int buffer[8];
  auto buffer_const_span = absl::MakeConstSpan(buffer);
  auto x = WrapContainer(buffer_const_span);
  using XType = decltype(x);
  EXPECT_EQ(XType::Dims, 1);
  EXPECT_TRUE(XType::IsVectorAtCompileTime);
  EXPECT_FALSE(XType::IsResizable);
  EXPECT_EQ(x.size(), 8);
  EXPECT_TRUE(x.resize(8));
  EXPECT_FALSE(x.resize(7));
  EXPECT_EQ(x.AsMatrix(1).data(), buffer);

  auto buffer_span = absl::MakeSpan(buffer);
  auto y = WrapContainer(buffer_span);
  EXPECT_EQ(y.AsMatrix(1).data(), buffer);
}

// Test WrapContainer(std::vector).
TEST(EigenTypesTest, ContainerWrapperVector) {
  std::vector<int> buffer;
  auto x = WrapContainer(buffer);
  using XType = decltype(x);
  EXPECT_EQ(XType::Dims, 1);
  EXPECT_TRUE(XType::IsVectorAtCompileTime);
  EXPECT_TRUE(XType::IsResizable);
  EXPECT_EQ(x.size(), 0);

  EXPECT_TRUE(x.resize(7));
  EXPECT_EQ(x.size(), 7);
  EXPECT_EQ(buffer.size(), 7);
  EXPECT_EQ(x.AsMatrix(1).data(), buffer.data());
}

// Test WrapContainer(Eigen::ArrayXXf).
TEST(EigenTypesTest, ContainerWrapperEigenArray) {
  ArrayXXf buffer = ArrayXXf::Random(3, 4);
  auto x = WrapContainer(buffer);
  using XType = decltype(x);
  EXPECT_EQ(XType::Dims, 2);
  EXPECT_EQ(XType::RowsAtCompileTime, Dynamic);
  EXPECT_EQ(XType::ColsAtCompileTime, Dynamic);
  EXPECT_FALSE(XType::IsVectorAtCompileTime);
  EXPECT_TRUE(XType::IsResizable);
  EXPECT_EQ(x.rows(), 3);
  EXPECT_EQ(x.cols(), 4);
  EXPECT_FALSE(x.resize(7));

  EXPECT_TRUE(x.resize(2, 7));
  EXPECT_EQ(x.rows(), 2);
  EXPECT_EQ(x.cols(), 7);
  EXPECT_EQ(buffer.rows(), 2);
  EXPECT_EQ(buffer.cols(), 7);

  buffer = ArrayXXf::Random(2, 7);
  EXPECT_THAT(x.AsMatrix(3), EigenArrayEq(buffer));
}

// Test WrapContainer(Eigen::MatrixXf).
TEST(EigenTypesTest, ContainerWrapperEigenMatrix) {
  MatrixXf buffer = MatrixXf::Random(3, 4);
  auto x = WrapContainer(buffer);
  using XType = decltype(x);
  EXPECT_EQ(XType::Dims, 2);
  EXPECT_EQ(XType::RowsAtCompileTime, Dynamic);
  EXPECT_EQ(XType::ColsAtCompileTime, Dynamic);
  EXPECT_FALSE(XType::IsVectorAtCompileTime);
  EXPECT_TRUE(XType::IsResizable);
  EXPECT_EQ(x.rows(), 3);
  EXPECT_EQ(x.cols(), 4);
  EXPECT_FALSE(x.resize(7));

  EXPECT_TRUE(x.resize(2, 7));
  EXPECT_EQ(x.rows(), 2);
  EXPECT_EQ(x.cols(), 7);
  EXPECT_EQ(buffer.rows(), 2);
  EXPECT_EQ(buffer.cols(), 7);

  buffer = MatrixXf::Random(2, 7);
  EXPECT_THAT(x.AsMatrix(3), EigenArrayEq(buffer));
}

// Test WrapContainer(Eigen::Block).
TEST(EigenTypesTest, ContainerWrapperEigenBlock) {
  MatrixXf buffer = MatrixXf::Random(3, 4);
  auto row = buffer.row(1);
  auto x = WrapContainer(row);
  using XType = decltype(x);
  EXPECT_EQ(XType::Dims, 2);
  EXPECT_EQ(XType::RowsAtCompileTime, 1);
  EXPECT_EQ(XType::ColsAtCompileTime, Dynamic);
  EXPECT_TRUE(XType::IsVectorAtCompileTime);
  EXPECT_FALSE(XType::IsResizable);
  EXPECT_EQ(x.size(), 4);

  EXPECT_THAT(x.AsMatrix(1), EigenArrayEq(buffer.row(1)));
}

// Test WrapContainer(Eigen::VectorBlock).
TEST(EigenTypesTest, ContainerWrapperEigenVectorBlock) {
  ArrayXf buffer = ArrayXf::Random(5);
  auto head = buffer.head(3);
  auto x = WrapContainer(head);
  using XType = decltype(x);
  EXPECT_EQ(XType::Dims, 2);
  EXPECT_EQ(XType::RowsAtCompileTime, Dynamic);
  EXPECT_EQ(XType::ColsAtCompileTime, 1);
  EXPECT_TRUE(XType::IsVectorAtCompileTime);
  EXPECT_FALSE(XType::IsResizable);
  EXPECT_EQ(x.size(), 3);

  EXPECT_THAT(x.AsMatrix(3), EigenArrayEq(buffer.head(3)));
}

// Test WrapContainer(Eigen::CWiseNullaryOp).
TEST(EigenTypesTest, ContainerWrapperEigenCWiseNullaryOp) {
  auto constant_xpr = Eigen::ArrayXXf::Constant(3, 10, 4.2f);
  auto x = WrapContainer(constant_xpr);
  using XType = decltype(x);
  EXPECT_EQ(XType::Dims, 2);
  EXPECT_EQ(XType::RowsAtCompileTime, Dynamic);
  EXPECT_EQ(XType::ColsAtCompileTime, Dynamic);
  EXPECT_FALSE(XType::IsVectorAtCompileTime);
  EXPECT_FALSE(XType::IsResizable);
  EXPECT_EQ(x.rows(), 3);
  EXPECT_EQ(x.cols(), 10);

  ArrayXXf y = x.AsMatrix(3).eval();
  EXPECT_EQ(y.rows(), 3);
  EXPECT_EQ(y.cols(), 10);
  EXPECT_EQ(y(1, 1), 4.2f);
}

}  // namespace
}  // namespace audio_dsp
