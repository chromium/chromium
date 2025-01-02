//-----------------------------------------------------------------------------
//
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
//-----------------------------------------------------------------------------
#pragma once
#include "onnxruntime_config.h"
// build/external/eigen/unsupported/Eigen/CXX11/src/Tensor/TensorEvaluator.h:162:71:
// error: ignoring attributes on template argument "Eigen::PacketType<const float, Eigen::DefaultDevice>::type {aka __vector(4) float}" [-Werror=ignored-attributes]
#if defined(__GNUC__)
#pragma GCC diagnostic push
#if __GNUC__ >= 6
#pragma GCC diagnostic ignored "-Wignored-attributes"
#endif
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-result"
#ifdef HAS_DEPRECATED_COPY
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
#endif
// cmake/external/eigen/unsupported/Eigen/CXX11/../../../Eigen/src/Core/arch/NEON/PacketMath.h:1633:9:
// error: ‘void* memcpy(void*, const void*, size_t)’ copying an object of non-trivial type ‘Eigen::internal::Packet4c’
// {aka ‘struct Eigen::internal::eigen_packet_wrapper<int, 2>’} from an array of ‘const int8_t’
// {aka ‘const signed char’} [-Werror=class-memaccess]
#ifdef HAS_CLASS_MEMACCESS
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif

// cmake/external/eigen\Eigen/src/Core/util/Meta.h:454:25:
// error: 'result_of<Eigen::internal::scalar_product_op<unsigned long long> (const unsigned long long &, const unsigned long long &)>'
// is deprecated [-Werror,-Wdeprecated-declarations]
//   typedef typename std::result_of<T>::type type1;
#ifdef HAS_DEPRECATED_DECLARATIONS
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

// cmake/external/eigen\Eigen/CXX11/src/Tensor/TensorTrace.h:130:9:
// error: variable 'num_distinct_reduce_dims' set but not used [-Werror,-Wunused-but-set-variable]
//   int num_distinct_reduce_dims = 0;
#ifdef HAS_UNUSED_BUT_SET_VARIABLE
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

// eigen-src/unsupported/Eigen/CXX11/src/ThreadPool/EventCount.h:231:56: error: implicit conversion loses integer
//   precision: 'uint64_t' (aka 'unsigned long long') to 'size_t' (aka 'unsigned long') [-Werror,-Wshorten-64-to-32]
// next = wnext == kStackMask ? nullptr : &waiters_[wnext];
//                                         ~~~~~~~~ ^~~~~
#ifdef HAS_SHORTEN_64_TO_32
#pragma GCC diagnostic ignored "-Wshorten-64-to-32"
#endif

// eigen-src/unsupported/Eigen/CXX11/src/Tensor/TensorDeviceThreadPool.h:215:9:
// error: implicit capture of 'this' with a capture default of '=' is deprecated [-Werror,-Wdeprecated-this-capture]
#ifdef HAS_DEPRECATED_THIS_CAPTURE
#pragma GCC diagnostic ignored "-Wdeprecated-this-capture"
#endif

#elif defined(_MSC_VER)
// build\windows\debug\external\eigen3\unsupported\eigen\cxx11\src/Tensor/Tensor.h(76):
// warning C4554: '&': check operator precedence for possible error; use parentheses to clarify precedence

// unsupported\eigen\cxx11\src\Tensor\TensorUInt128.h(150,0): Warning C4245: 'initializing': conversion from '__int64'
// to 'uint64_t', signed/unsigned mismatch
#pragma warning(push)
#pragma warning(disable : 4554)
#pragma warning(disable : 4245)
#pragma warning(disable : 4127)
#endif

#include "unsupported/Eigen/CXX11/Tensor"

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif
