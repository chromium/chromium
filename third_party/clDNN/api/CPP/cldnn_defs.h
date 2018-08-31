/*
// Copyright (c) 2016 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

/*! @mainpage clDNN Documentation
* @section intro Introduction
* Compute Library for Deep Neural Networks (clDNN) is a middle-ware software
*  for accelerating DNN inference on Intel&reg; HD and Iris&trade; Pro Graphics.
*  This project includes CNN primitives implementations on Intel GPUs with C and C++ interfaces.
*
* clDNN Library implements set of primitives:
*  - Convolution
*  - Fully connected (inner product)
*  - Pooling
*   * average
*   * maximum
*  - Normalization
*   * across channel
*   * within channel
*   * batch
*  - Activation
*   * logistic
*   * tanh
*   * rectified linear unit (ReLU)
*   * softplus (softReLU)
*   * abs
*   * square
*   * sqrt
*   * linear
*  - Softmax
*  - Crop
*  - Deconvolution
*  - Depth concatenation
*  - Eltwise
*  - ROI pooling
*  - Simpler NMS
*  - Prior box
*  - Detection output
*
*  With this primitive set, user can build and execute most common image recognition, semantic segmentation and object detection networks topologies like:
*   - Alexnet 
*   - Googlenet(v1-v3)
*   - ResNet
*   - VGG
*   - faster-rCNN 
* and other.
*  
*
* @section model Programming Model
*  Intel&reg; clDNN is graph oriented library. To execute CNN you have to build, compile graph/topology and run to get results. 
*  
*  <B> Terminology: </B>
*  - Primitive - dnn base functionality i.e. convolution, pooling, softmax. 
*  - Data - special primitive type representing primitive parameters (weights and biases), inputs and outputs
*  - Engine - type of accelerator that is executing network. Currently ocl engine is the only available. 
*  - Topology - container of primitives, data, and relations between them. Topology represents graph. 
*  - Program - optional step between Topology and Network. It is compiled Topology without memory allocation.
*  - Network - compiled Topology with memory allocation. Ready to be executed. During compilation, buidling parameters trigger special optimizations like fusing, data reordering.
*
*  <B> Execution Steps: </B>
*
* \image html workflow.jpg
* -# Create Engine
* -# Declare or define primitives parameters (weights and biases) if needed.
* -# Create primitives. It is required to provide name for each primitive. This is a name of primitive which output will be input to current one. Name can be used before primitive definition.
* -# Create topology
* -# Add primitives to topology
* -# Build Network from topology
* -# Set Inputs data 
* -# Execute Network
*
*
* @section graph_compilation Graph compilation
*
* If user choose build option optimize_data when program is being created - explicit or implicit over network creation, clDNN perform some graph optimizations as follows:
* * <B> Stage 0: Graph initiation:</B>
*  * build nodes from primitives
*  * node replacement: 
*   * replace each split node with series of crop nodes. Name of crop primitive will be concatenation of split + port names. 
*   * replace upsampling node with deconvolution node if upsampling mode is bilinear.
*  * set outputs - mark nodes that are defined by user as output (blocks fusing etc) or have no users (leafs).
*  * calculate processing order - using dfs on graph to establish processing order
* * <B> Stage 1: Priorboxes:</B>
*  * priorbox is primitive that is executed during network compilation. Node is removed from a network execution.
* * <B> Stage 2: Graph analysis:</B>
*  * mark constatns
*  * mark data flow 
*  * mark dominators
* * <B> Stage 3: Trimming:</B>
*  * apply backward bfs on each output to find unnecessary nodes/branches, then remove those. 
* * <B> Stage 4: Inputs and biases:</B>
*  * reorder input - format of convolution's input/output is being selected. 
*  * reorder biases for conv,fc and deconv nodes
* * <B> Stage 5: Redundant reorders:</B>
*  * previous stages can provide additional reorders due to format changes per primitive. This stage removes redundant and fuses series of reorders into one.  
* * <B> Stage 6: Constant propagation:</B>
*  * prepare padding - goes thrugh all primitves and checks if its user requires padding, if so, set output padding.
*  * prepare depthwise separable opt - if split param is greater than 16 and number of IFM <= 8*split in conv or deconv, this stage changes execution from multi kernels into one.
*  * constant propagation - replace constant nodes, that are not outputs with data type nodes. Constant primitive is the primitive that doesn't depend on any non-constant primitive and doesn't have to be executed: priorbox, data.
* * <B> Stage 7: Fusing:</B>
*  * buffer fusing
*   * concat - if concatenation is the only user of its dependencies then remove concat node and setting proper output paddings in every dependencies. 
*   * crop - if crop has only one dependecy, and its users doesn't require padding, remove crop and set proper output padding in its dependecy.
*   * reorder - if primitive before reorder supports different input vs output type reorder can be fused with previous node.
*  * primitive fusing - right now this stage fuses activation node with previous node only, only if previous node supports activation fusing. 
* * <B> Stage 8: Compile graph:</B>
*  * at this stage using kernel selector, graph chooses the best kernel implementation for each node.
* * <B> Stage 9: reorder weights:</B>
*  * at this stage weights are converted into format suitable for selected kernel implementation.
* * <B> Stage 10 & 11: Redundant reorders and constant propagation:</B>
*  * check again if whole graph compilation didn't provide any redundant reorders and constants.
* * <B> Stage 12: Compile program:</B>
*  * at this stage engine compiles cl_kernels. 
*
* @section example C++ API Example MNIST network
* @include example_cldnn.cpp
*/

///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <stdexcept>

#include "../C/cldnn.h"

namespace cldnn {
    // There is no portable half precision floating point support.
    // Using wrapped integral type with the same size and alignment restrictions.
    class half_impl
    {
    public:
        half_impl() = default;
        template <typename T, typename = typename std::enable_if<!std::is_floating_point<T>::value>::type>
        explicit half_impl(T data) : _data(data) {}

        operator uint16_t() const { return _data; }
        operator float() const
        {
           cldnn_status status = CLDNN_SUCCESS;
           auto value = cldnn_half_to_float(_data, &status);
           if (status != CLDNN_SUCCESS)
               throw std::runtime_error("Conversion from half failed");
           return value;
        }
        explicit half_impl(float value)
        {
            cldnn_status status = CLDNN_SUCCESS;
            _data = cldnn_float_to_half(value, &status);
            if (status != CLDNN_SUCCESS)
                throw std::runtime_error("Conversion to half failed");
        }

    private:
        uint16_t _data;
    };
}
// Use complete implementation if necessary.
#if defined HALF_HALF_HPP
typedef half half_t;
#else
typedef cldnn::half_impl half_t;
#endif

namespace cldnn {
/// @addtogroup cpp_api C++ API
/// @{

/// @defgroup cpp_error Error Handling
/// @{

using status_t = ::cldnn_status;

/// @brief clDNN specific exception type.
class error : public std::runtime_error
{
public:
    explicit error(const std::string& _Message, status_t status = CLDNN_ERROR)
        : runtime_error(_Message)
        , _status(status)
    {
    }

    explicit error(const char* _Message, status_t status = CLDNN_ERROR)
        : runtime_error(_Message)
        , _status(status)
    {
    }

    /// @brief Returns clDNN status code.
    const status_t& status() const { return _status; }
private:
    status_t _status;
};

#define CLDNN_THROW(msg, status) throw cldnn::error(msg, status);

template<class T>
T check_status(std::string err_msg, std::function<T(status_t*)> func)
{
    status_t status = CLDNN_SUCCESS;
    auto result = func(&status);
    if (status != CLDNN_SUCCESS)
        CLDNN_THROW(err_msg.append(": ").append(cldnn_get_last_error_message()), status);
    return result;
}

template<>
inline void check_status<void>(std::string err_msg, std::function<void(status_t*)> func)
{
    status_t status = CLDNN_SUCCESS;
    func(&status);
    if (status != CLDNN_SUCCESS)
        CLDNN_THROW(err_msg.append(": ").append(cldnn_get_last_error_message()), status);
}

/// @}

/// @defgroup cpp_version Version Information
/// @{

using version_t = ::cldnn_version;

/// @brief Get information about version of clDNN.
inline version_t get_version()
{
    return check_status<version_t>("get_version: fetching version information failed",
                                   [](status_t* status)
                                   {
                                       return ::cldnn_get_version(status);
                                   });
}

/// @}

/// @cond CPP_HELPERS

/// @defgroup cpp_helpers Helpers
/// @{

#define CLDNN_API_CLASS(the_class) static_assert(std::is_standard_layout<the_class>::value, #the_class " has to be 'standart layout' class");


template<typename T>
typename std::enable_if<std::is_integral<T>::value, T>::type align_to(T size, size_t align) {
    return static_cast<T>((size % align == 0) ? size : size - size % align + align);
}

template<typename T>
typename std::enable_if<std::is_integral<T>::value, T>::type pad_to(T size, size_t align) {
    return static_cast<T>((size % align == 0) ? 0 : align - size % align);
}

template<typename T>
typename std::enable_if<std::is_integral<T>::value, bool>::type is_aligned_to(T size, size_t align)
{
    return !(size % align);
}

/// Computes ceil(@p val / @p divider) on unsigned integral numbers.
///
/// Computes division of unsigned integral numbers and rounds result up to full number (ceiling).
/// The function works for unsigned integrals only. Signed integrals are converted to corresponding
/// unsigned ones.
///
/// @tparam T1   Type of @p val. Type must be integral (SFINAE).
/// @tparam T2   Type of @p divider. Type must be integral (SFINAE).
///
/// @param val       Divided value. If value is signed, it will be converted to corresponding unsigned type.
/// @param divider   Divider value. If value is signed, it will be converted to corresponding unsigned type.
///
/// @return   Result of ceil(@p val / @p divider). The type of result is determined as if in normal integral
///           division, except each operand is converted to unsigned type if necessary.
template <typename T1, typename T2>
constexpr auto ceil_div(T1 val, T2 divider)
    -> typename std::enable_if<std::is_integral<T1>::value && std::is_integral<T2>::value,
                               decltype(std::declval<typename std::make_unsigned<T1>::type>() / std::declval<typename std::make_unsigned<T2>::type>())>::type
{
    typedef typename std::make_unsigned<T1>::type UT1;
    typedef typename std::make_unsigned<T2>::type UT2;
    typedef decltype(std::declval<UT1>() / std::declval<UT2>()) RetT;

    return static_cast<RetT>((static_cast<UT1>(val) + static_cast<UT2>(divider) - 1U) / static_cast<UT2>(divider));
}

/// Rounds @p val to nearest multiply of @p rounding that is greater or equal to @p val.
///
/// The function works for unsigned integrals only. Signed integrals are converted to corresponding
/// unsigned ones.
///
/// @tparam T1       Type of @p val. Type must be integral (SFINAE).
/// @tparam T2       Type of @p rounding. Type must be integral (SFINAE).
///
/// @param val        Value to round up. If value is signed, it will be converted to corresponding unsigned type.
/// @param rounding   Rounding value. If value is signed, it will be converted to corresponding unsigned type.
///
/// @return   @p val rounded up to nearest multiply of @p rounding. The type of result is determined as if in normal integral
///           division, except each operand is converted to unsigned type if necessary.
template <typename T1, typename T2>
constexpr auto round_up_to(T1 val, T2 rounding)
    -> typename std::enable_if<std::is_integral<T1>::value && std::is_integral<T2>::value,
                               decltype(std::declval<typename std::make_unsigned<T1>::type>() / std::declval<typename std::make_unsigned<T2>::type>())>::type
{
    typedef typename std::make_unsigned<T1>::type UT1;
    typedef typename std::make_unsigned<T2>::type UT2;
    typedef decltype(std::declval<UT1>() / std::declval<UT2>()) RetT;

    return static_cast<RetT>(ceil_div(val, rounding) * static_cast<UT2>(rounding));
}

///
/// \brief Converts C API float array to std::vector<float>
///
inline std::vector<float> float_arr_to_vector(const cldnn_float_arr& arr)
{
    std::vector<float> result(arr.size);
    for (size_t i = 0; i < arr.size; i++)
    {
        result[i] = arr.data[i];
    }
    return result;
}

///
/// \brief Converts C API float array to std::vector<uint16_t>
///
inline std::vector<uint16_t> uint16_t_arr_to_vector(const cldnn_uint16_t_arr& arr)
{
    std::vector<uint16_t> result(arr.size);
    for (size_t i = 0; i < arr.size; i++)
    {
        result[i] = arr.data[i];
    }
    return result;
}


///
/// \brief Converts std::vector<float> to C API float_array
///
inline cldnn_float_arr float_vector_to_arr(const std::vector<float>& stor)
{
    return { stor.data(), stor.size() };
}

///
/// \brief Converts std::vector<uint16_t> to C API float_array
///
inline cldnn_uint16_t_arr uint16_t_vector_to_arr(const std::vector<uint16_t>& stor)
{
    return{ stor.data(), stor.size() };
}

///
/// \brief Converts std::vector<tensor> to C API tensor_array
///
inline cldnn_tensor_arr tensor_vector_to_arr(const std::vector<cldnn_tensor>& stor)
{
    return cldnn_tensor_arr{ stor.data(), stor.size() };
}

/// @}

/// @endcond

/// @}
}
