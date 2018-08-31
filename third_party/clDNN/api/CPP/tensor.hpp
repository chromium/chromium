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

///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "cldnn_defs.h"
#include "compounds.h"
#include "meta_utils.hpp"

#include <map>
#include <list>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <algorithm>
#include <sstream>

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{

/// @addtogroup cpp_memory Memory description and management
/// @{

/// @brief Format information helper class.
struct format_traits
{
    /// @brief Number of batch dimensions in a format.
    size_t batch_num;
    /// @brief Number of feature map/channel dimensions in a format.
    size_t feature_num;
    /// @brief Number of spatial (x,y) dimensions in a format.
    size_t spatial_num;
    /// @brief Dimensions changing order from rare to often.
    std::string order;
    /// @brief Dimensions order for internal storage.
    std::string internal_order;
    /// @brief Characters representing batch dimensions in an order.
    static const char* batch_chars() { return "bn"; }
    /// @brief Characters representing feature map/channel dimensions in an order.
    static const char* feature_chars() { return "fioc"; }
    /// @brief Characters representing spatial dimensions in an order.
    static const char* spatial_chars() { return "xyzhsw"; }
    /// @brief Checks if @p c represents batch dimension.
    static bool is_batch_char(char c) { return std::string(batch_chars()).find_first_of(c) != std::string::npos; }
    /// @brief Checks if @p c represents feature map/channel dimension.
    static bool is_feature_char(char c) { return std::string(feature_chars()).find_first_of(c) != std::string::npos; }
    /// @brief Checks if @p c represents spatial dimension.
    static bool is_spatial_char(char c) { return std::string(spatial_chars()).find_first_of(c) != std::string::npos; }
};

/// @brief Represents memory formats (orders).
/// @n In CNN most of data is described as 4 dimensional blocks. In Intel(R) clDNN library we describe memory with 4 letters
/// - b - number of blocks in batch. For weights formats: output features - conv, neurons - inner product
/// - f - number of feature maps, features or channels. For weights formats: input features - conv, inputs, inner product
/// - x - spatial, width
/// - y - spatial, height
/// /n
/// For explanation how each format type is implemented in memory we will use naming shown bellow (b=2,f=3,y=3,x=3):
/// \image html layout_memory_representation.jpg
struct format
{
    enum type : int32_t
    {
        yxfb = cldnn_format_yxfb, ///< batch first, feature and than spatials \n \image html yxfb.jpg
        byxf = cldnn_format_byxf, ///< used in bitmaps, input from user i.e b images of RGB format \n \image html byxf.jpg
        bfyx = cldnn_format_bfyx, ///< the most common format for activations in clDNN. \n \image html bfyx.jpg
        fyxb = cldnn_format_fyxb, ///< format not used inside clDNN, but supported in reorder as extension for user provided formats.
        os_iyx_osv16 = cldnn_format_os_iyx_osv16, ///< format used only for convolution weights: os - output feature maps slice, i - input feature maps, yx - spatials, sv16 - 16 values of single slice.
                                                  ///< \n \image html os_iyx_osv16.jpg
        bs_xs_xsv8_bsv8 = cldnn_format_bs_xs_xsv8_bsv8,  ///< format used only for fully connected weights: bs - batch slice, xs - x slice, bsv8 - 8 values of single slice.
                                                         ///< \n \image html bs_xs_xsv8_bsv8.jpg
        bs_xs_xsv8_bsv16 = cldnn_format_bs_xs_xsv8_bsv16,///< format used only for fully connected weights: bs - batch slice, xs - x slice, bsv16 - 16 values of single slice.
                                                         ///< \n \image html bs_xs_xsv8_bsv16.jpg
        bs_x_bsv16 = cldnn_format_bs_x_bsv16, ///< format used only for fully connected weights fp16 batch=1 : bs - batch slice (responses slice), bsv16 - 16 values of single batch slice, x - flattened plane of (fyx).
                                              ///< \n \image html bs_x_bsv16.jpg
        bf8_xy16 = cldnn_format_bf8_xy16, ///< format used only for convolution 1x1 input, xy aligned to 16, f aligned to 8
                                          ///< \n \image html bf8_xy16.jpg
        image_2d_weights_c4_fyx_b = cldnn_format_image_2d_weights_c4_fyx_b, ///< image format for weights, width size is f*y*x/4 (4-channels filled with fyx data), height is b
                                                                      ///< \n \image html image_2d_weights_c4_fyx_b.jpg
        image_2d_weights_c1_b_fyx = cldnn_format_image_2d_weights_c1_b_fyx, ///< image format for weights, width size is b, height is f*y*x, single channel
                                                                            ///< \n \image html image_2d_weights_c1_b_fyx.jpg
        winograd_2x3_s1_data,       ///< format used for input for winograd convolution, F(2,3) -- filter 3x3 with stride 1
        winograd_2x3_s1_weights,    ///< format used for weights for winograd non-fused convolution, F(2,3) -- filter 3x3 with stride 1
        winograd_2x3_s1_fused_weights,    ///< format used for weights for winograd fused convolution, F(2,3) -- filter 3x3 with stride 1
        winograd_6x3_s1_fused_weights,    ///< format used for weights for winograd fused convolution, F(6,3) -- filter 3x3 with stride 1
        image_2d_weights_winograd_6x3_s1_fbxyb,      ///< image format used for weights for winograd fused convolution, F(6,3) -- filter 3x3 with stride 1
        image_2d_weights_winograd_6x3_s1_xfbyb,      ///< image format used for weights for winograd fused convolution, F(6,3) -- filter 3x3 with stride 1
        os_is_yx_isa8_osv8_isv4,                        /// format for weights for MMAD convolution
        byxf_af32,           /// < \n format for input for primitives using MMAD
        format_num = cldnn_format_format_num, ///< number of format types
        any = cldnn_format_any
    };

    /// @brief Get format traits for particular @p format::type
    static const format_traits& traits(type fmt)
    {
        static const std::map<type, format_traits> traits
        {
            { yxfb,{ 1, 1, 2, "yxfb", "bfxy" } },
            { byxf,{ 1, 1, 2, "byxf", "bfxy" } },
            { bfyx,{ 1, 1, 2, "bfyx", "bfxy" } },
            { fyxb,{ 1, 1, 2, "fyxb", "bfxy" } },
            { os_iyx_osv16, { 1, 1, 2, "bfyx", "bfxy" } },
            { bs_xs_xsv8_bsv8, { 1, 1, 1, "bx", "b?x?" } },
            { bs_xs_xsv8_bsv16,{ 1, 1, 1, "bx", "b?x?" } },
            { bs_x_bsv16, { 1, 1, 1, "bx", "b?x?" } },
            { bf8_xy16, { 1, 1, 2, "bfyx", "bfxy" }},
            { image_2d_weights_c4_fyx_b, { 1, 1, 2, "bfyx", "bfxy" } },
            { image_2d_weights_c1_b_fyx, { 1, 1, 2, "bfyx", "bfxy" } },
            { winograd_2x3_s1_data, { 1, 1, 2, "bxyf", "bfxy" } },
            { winograd_2x3_s1_weights, { 1, 1, 2, "bfyx", "bfxy" } },
            { winograd_2x3_s1_fused_weights, { 1, 1, 2, "xyfb", "bfxy" } },
            { winograd_6x3_s1_fused_weights,{ 1, 1, 2, "xyfb", "bfxy" } },
            { image_2d_weights_winograd_6x3_s1_fbxyb,{ 1, 1, 2, "xyfb", "bfxy" } },
            { image_2d_weights_winograd_6x3_s1_xfbyb,{ 1, 1, 2, "xyfb", "bfxy" } },
            { os_is_yx_isa8_osv8_isv4, { 1, 1, 2, "bfyx", "bfxy" } },
            { byxf_af32, { 1, 1, 2, "byxf", "bfxy" } }
        };
        return traits.at(fmt);
    }

    /// @brief Returns number of batch dimensions for a @p format.
    static size_t batch_num(type fmt) { return traits(fmt).batch_num; }
    /// @brief Returns number of feature dimensions for a @p format.
    static size_t feature_num(type fmt) { return traits(fmt).feature_num; }
    /// @brief Returns number of spatial dimensions for a @p format.
    static size_t spatial_num(type fmt) { return traits(fmt).spatial_num; }
    /// @brief Returns an order of dimensions for a @ format.
    static const std::string& order(type fmt) { return traits(fmt).order; }
    /// @brief Returns an internal orders of dimensions for a @p format.
    static const std::string& internal_order(type fmt) { return traits(fmt).internal_order; }
    /// @brief Returns number of dimensions contained within a @p format
    static size_t dimension(type fmt) { return order(fmt).size(); }
    /// @brief Checks if @p format is a winograd format
    static bool is_winograd(type fmt) { return (fmt == winograd_2x3_s1_data || fmt == winograd_2x3_s1_weights || fmt == winograd_2x3_s1_fused_weights || fmt == winograd_6x3_s1_fused_weights || fmt == image_2d_weights_winograd_6x3_s1_fbxyb || fmt == image_2d_weights_winograd_6x3_s1_xfbyb); }
    /// @brief Checks if @p format is of image2d type
    static bool is_image_2d(type fmt) { return (fmt == image_2d_weights_c4_fyx_b || fmt == image_2d_weights_c1_b_fyx || fmt == image_2d_weights_winograd_6x3_s1_fbxyb || fmt == image_2d_weights_winograd_6x3_s1_xfbyb); }
    /// @brief Checks if @p format is of image type
    static bool is_image(type fmt) { return (is_image_2d(fmt)); }

    /// @brief Returns number of batch dimensions.
    size_t batch_num() const { return traits(value).batch_num; }
    /// @brief Returns number of feature dimensions.
    size_t feature_num() const { return traits(value).feature_num; }
    /// @brief Returns number of spatial dimensions.
    size_t spatial_num() const { return traits(value).spatial_num; }
    /// @brief Returns an order of dimensions in form of string.
    const std::string& order() const { return traits(value).order; }
    /// @brief Returns an internal orders of dimensions form of string.
    const std::string& internal_order() const { return traits(value).internal_order; }
    /// @brief Returns number of dimensions contained within this format
    size_t dimension() const { return order(value).size(); }
    /// @brief Checks if @p format is a winograd format
    bool is_winograd() const { return is_winograd(value); }
    /// @brief Checks if @p format is of image 2d type
    bool is_image_2d() const { return is_image_2d(value); }
    /// @brief Checks if @p format is of image type
    bool is_image() const { return is_image(value); }

    type value;
    /// @brief Implicit conversion from format::type.
    constexpr format(type t) :value(t) {}
    /// @brief Implicit conversion to format::type.
    constexpr operator type() const { return value; }
    /// @brief Conversion from C API @ref ::cldnn_format_type.
    constexpr explicit format(cldnn_format_type t) : value(static_cast<type>(t)) {}
    /// @brief Conversion to C API @ref ::cldnn_format_type.
    constexpr explicit operator cldnn_format_type() const { return static_cast<cldnn_format_type>(value); }
};

struct tensor;

/// @brief Helper structs used in tensor constructor with dim_vec_kinds
namespace details
{
/// @brief enum class that represent dimension kinds
enum class dim_vec_kind
{
    batch,
    feature,
    spatial
};

/// @brief template class with max_dimensionalities and dimension offset for dimension kinds
template <dim_vec_kind Kind>
struct dim_vec_limits
{
    static_assert(meta::always_false_ty_val<dim_vec_kind, Kind>::value, "Limits are undefined for selected value of dim_vec_kind.");
};

template <>
struct dim_vec_limits<dim_vec_kind::batch>
{
    static constexpr int32_t max_dimentionality = CLDNN_TENSOR_BATCH_DIM_MAX;
    static constexpr int32_t dim_offset = 0;
};

template <>
struct dim_vec_limits<dim_vec_kind::feature>
{
    static constexpr int32_t max_dimentionality = CLDNN_TENSOR_FEATURE_DIM_MAX;
    static constexpr int32_t dim_offset = CLDNN_TENSOR_BATCH_DIM_MAX;
};

template <>
struct dim_vec_limits<dim_vec_kind::spatial>
{
    static constexpr int32_t max_dimentionality = CLDNN_TENSOR_SPATIAL_DIM_MAX;
    static constexpr int32_t dim_offset = CLDNN_TENSOR_BATCH_DIM_MAX + CLDNN_TENSOR_FEATURE_DIM_MAX;
};

/// @brief Template class used in tensor constructor using dim_vec_kinds
template <dim_vec_kind Kind>
class dim_vec_kind_init
{
public:
    static constexpr auto _max_dimensionality = dim_vec_limits<Kind>::max_dimentionality;
    static constexpr auto _dimOffset = dim_vec_limits<Kind>::dim_offset;

    template <typename ... DimTys>
    explicit dim_vec_kind_init(DimTys&& ... values)
        : _sizes{ int32_t(std::forward<DimTys>(values)) ... }, _dimSize(sizeof...(DimTys))
    {
    }

    void init_tensor_values(cldnn::tensor& t);

    int32_t _sizes[_max_dimensionality];
    int32_t _dimSize;
};
}

template <typename ... InitTys>
details::dim_vec_kind_init<details::dim_vec_kind::batch> batch(InitTys&& ... inits)
{
    return details::dim_vec_kind_init<details::dim_vec_kind::batch>(std::forward<InitTys>(inits) ...);
}

template <typename ... InitTys>
details::dim_vec_kind_init<details::dim_vec_kind::feature> feature(InitTys&& ... inits)
{
    return details::dim_vec_kind_init<details::dim_vec_kind::feature>(std::forward<InitTys>(inits) ...);
}

template <typename ... InitTys>
details::dim_vec_kind_init<details::dim_vec_kind::spatial> spatial(InitTys&& ... inits)
{
    return details::dim_vec_kind_init<details::dim_vec_kind::spatial>(std::forward<InitTys>(inits) ...);
}

/// @brief N-dimensional vector. Mostly used to represent memory size.
struct tensor
{
    friend class details::dim_vec_kind_init<details::dim_vec_kind::batch>;
    friend class details::dim_vec_kind_init<details::dim_vec_kind::feature>;
    friend class details::dim_vec_kind_init<details::dim_vec_kind::spatial>;

    typedef int32_t value_type;     ///< Values type stored in tensor.
    //TODO find the way to prevent direct change of following fields.
    mutable_array_ref<value_type> raw;      ///< Raw representation of all dimensions.
    mutable_array_ref<value_type> batch;    ///< Batch dimensions.
    mutable_array_ref<value_type> feature;  ///< Feature maps.
    mutable_array_ref<value_type> spatial;  ///< Spatial dimensions.

private:
    value_type _sizes[CLDNN_TENSOR_DIM_MAX];
    value_type _dimOffset;
    value_type _dimSize;

public:
    tensor(value_type default_size = 0)
        : raw(_sizes, CLDNN_TENSOR_DIM_MAX)
        , batch(_sizes, CLDNN_TENSOR_BATCH_DIM_MAX)
        , feature(_sizes + CLDNN_TENSOR_BATCH_DIM_MAX, CLDNN_TENSOR_FEATURE_DIM_MAX)
        , spatial(_sizes + CLDNN_TENSOR_BATCH_DIM_MAX + CLDNN_TENSOR_FEATURE_DIM_MAX, CLDNN_TENSOR_SPATIAL_DIM_MAX)
    {
        std::fill_n(_sizes, CLDNN_TENSOR_DIM_MAX, default_size);
    }

    /// @brief Constructs tensor.
    /// @param[in] kind_inits Dimensions defined using dim_vec_kind. If dimension is not provided it is set to 1.
    /// @details Example:
    /*! @code
    *
    tensor my_tensor(batch(2), spatial(5, 6));   // y=6, x=5, b=2, f - not set
    cout << my_tensor.batch[0] << endl;           // 2
    cout << my_tensor.feature[0] << endl;         // 1 - default_size
    cout << "x=" << my_tensor.spatial[0] << endl; // x=5
    cout << "y=" << my_tensor.spatial[1] << endl; // y=6
    *
    * @endcode
    */
    template <typename ... KindInitTys,
        typename = typename std::enable_if<
            meta::all<
                meta::is_any_of<KindInitTys,
                    cldnn::details::dim_vec_kind_init<cldnn::details::dim_vec_kind::batch>,
                    cldnn::details::dim_vec_kind_init<cldnn::details::dim_vec_kind::feature>,
                    cldnn::details::dim_vec_kind_init<details::dim_vec_kind::spatial>
                >::value...
            >::value, void>::type>
    tensor(KindInitTys&& ... kind_inits)
        : tensor(1)
    {
        assign_inits(std::forward<KindInitTys>(kind_inits) ...);
    }

    /// @brief Constructs @p tensor.
    /// @details Example:
    /*! @code
     * 
       tensor my_tensor( 2, 3, 4, 5 );   // b=2, f=3, x=4, y=5
       cout << my_tensor.batch[0] << endl;           // 2
       cout << my_tensor.feature[0] << endl;         // 3
       cout << "x=" << my_tensor.spatial[0] << endl; // x=4
       cout << "y=" << my_tensor.spatial[1] << endl; // y=5
     *
     * @endcode
     */ 
    tensor(value_type batch_num, value_type feature_num, value_type width, value_type height)
        : tensor(1)
    {
        _sizes[0] = batch_num;
        _sizes[CLDNN_TENSOR_BATCH_DIM_MAX] = feature_num;
        _sizes[CLDNN_TENSOR_BATCH_DIM_MAX + CLDNN_TENSOR_FEATURE_DIM_MAX] = width;
        _sizes[CLDNN_TENSOR_BATCH_DIM_MAX + CLDNN_TENSOR_FEATURE_DIM_MAX + 1] = height;
    }

    /// @brief Constructs @p tensor using vector of sizes.
    /// @param[in] sizes dimensions need to be provided in the following order {batch, feature, spatial_x, spatial_y}.
    /// @param[in] default_size default_size for tensor dimensions.
    /// @details Example:
    /*! @code
     * 
       tensor my_tensor = { 2, 3, 4, 5 };   // b=2, f=3, x=4, y=5
       cout << my_tensor.batch[0] << endl;           // 2
       cout << my_tensor.feature[0] << endl;         // 3
       cout << "x=" << my_tensor.spatial[0] << endl; // x=4
       cout << "y=" << my_tensor.spatial[1] << endl; // y=5
     *
     * @endcode
     */ 
    tensor(const std::vector<value_type>& sizes, value_type default_size = 1)
        : tensor(default_size)
    {
        _sizes[0] = sizes[0];
        _sizes[CLDNN_TENSOR_BATCH_DIM_MAX] = sizes[CLDNN_TENSOR_BATCH_DIM_MAX];
        _sizes[CLDNN_TENSOR_BATCH_DIM_MAX + CLDNN_TENSOR_FEATURE_DIM_MAX] = sizes[CLDNN_TENSOR_BATCH_DIM_MAX + CLDNN_TENSOR_FEATURE_DIM_MAX];
        _sizes[CLDNN_TENSOR_BATCH_DIM_MAX + CLDNN_TENSOR_FEATURE_DIM_MAX + 1] = sizes[CLDNN_TENSOR_BATCH_DIM_MAX + CLDNN_TENSOR_FEATURE_DIM_MAX + 1];
    }

    tensor(format fmt, const std::vector<value_type>& sizes, value_type default_size = 1)
        : tensor(default_size)
    {
        auto in_order = fmt.order();
        auto out_order = fmt.internal_order();
        if (in_order.size() != sizes.size())
            throw std::invalid_argument("The count of values passed to initialize tensor does not match passed format.");

        for (size_t out_idx = 0; out_idx < out_order.size(); ++out_idx)
        {
            auto channel = out_order[out_idx];
            if (channel == '?')
                continue;
            
            auto in_idx = in_order.find(channel);
            if (in_idx == in_order.npos)
                throw std::runtime_error("Internal order of a format contains channel which does not appear in external order.");

            _sizes[out_idx] = sizes[in_idx];
        }
    }

    /// @brief Implicit conversion form C API :: cldnn_tensor.
    tensor(const cldnn_tensor& other)
        : tensor()
    {
        std::copy_n(other.sizes, CLDNN_TENSOR_DIM_MAX, _sizes);
    }

    /// @brief Implicit conversion to C API ::cldnn_tensor.
    operator cldnn_tensor() const
    {
        cldnn_tensor result;
        result.batch_num = batch.size();
        result.feature_num = feature.size();
        result.spatial_num = spatial.size();
        std::copy_n(_sizes, CLDNN_TENSOR_DIM_MAX, result.sizes);
        return result;
    }

    /// @brief Copy construction.
    tensor(const tensor& other)
        : tensor()
    {
        std::copy_n(other._sizes, CLDNN_TENSOR_DIM_MAX, _sizes);
    }

    /// @brief Copy assignment.
    tensor& operator=(const tensor& other)
    {
        if (this == &other)
            return *this;
        std::copy_n(other._sizes, CLDNN_TENSOR_DIM_MAX, _sizes);
        return *this;
    }

    friend bool operator==(const tensor& lhs, const tensor& rhs)
    {
        return lhs.raw.size() == rhs.raw.size()
            && std::equal(lhs.raw.begin(), lhs.raw.end(), rhs.raw.begin());
    }

    friend bool operator!=(const tensor& lhs, const tensor& rhs)
    {
        return !(lhs == rhs);
    }

    friend bool operator<(const tensor& lhs, const tensor& rhs)
    {
        if (lhs.raw.size() != rhs.raw.size())
            return lhs.raw.size() < rhs.raw.size();
        for (size_t i = 0; i < lhs.raw.size(); ++i)
        {
            if (lhs.raw[i] < rhs.raw[i])
                return true;
            if (rhs.raw[i] < lhs.raw[i])
                return false;
        }

        return false;
    }

    friend std::ostream& operator<<(std::ostream& os, const tensor& tensor)
    {
        os << tensor.to_string();
        return os;
    }

    std::string to_string() const
    {
        std::stringstream out;
		const char* delim = "";

        out << "[b:";
        for (size_t i = 0; i < batch.size(); ++i)
        {
            out << delim << batch[i];
			delim = ",";
        }
		delim = "";

        out << ", f:";
		for (size_t i = 0; i < feature.size(); ++i)
		{
			out << delim << feature[i];
			delim = ",";
		}

        std::vector<std::string> spatial_dim_names = { ", x", ", y", ", z", ", w" };
        for (size_t i = 0; i < spatial.size(); ++i)
        {
            out << spatial_dim_names[i] << ":" << spatial[i];
        }
        out << "]";

        return out.str();
    }

    /// @brief Returns a tensor with all negated elements.
    tensor negate() const
    {
        auto result = *this;
        for (size_t i = 0; i < CLDNN_TENSOR_DIM_MAX; i++)
        {
            result._sizes[i] = -_sizes[i];
        }
        return result;
    }

    /// @brief Returns a tensor with all elements multilied to @p multiplier.
    tensor mul(value_type multiplier) const
    {
        auto result = *this;
        for(size_t i = 0; i < CLDNN_TENSOR_DIM_MAX; i++ )
        {
            result._sizes[i] *= multiplier;
        }
        return result;
    }

    /// @brief Returns a tensor with all elements divided by @p divider.
    tensor div(value_type divider) const
    {
        auto result = *this;
        for (size_t i = 0; i < CLDNN_TENSOR_DIM_MAX; i++)
        {
            result._sizes[i] /= divider;
        }
        return result;
    }

    /// @brief Returns a tensor with all elements added by appropriate elements of @p rhs
    tensor add(const tensor& rhs) const
    {
        auto result = *this;
        for (size_t i = 0; i < CLDNN_TENSOR_DIM_MAX; i++)
        {
            result._sizes[i] += rhs._sizes[i];
        }
        return result;
    }

    /// @brief Returns a tensor with all elements subtracted by appropriate elements of @p rhs
    tensor sub(const tensor& rhs) const
    {
        return add(rhs.negate());
    }

    /// @brief Assign and add
    tensor& operator+=(const tensor& rhs)
    {
        for (size_t i = 0; i < CLDNN_TENSOR_DIM_MAX; i++)
            _sizes[i] += rhs._sizes[i];
        return *this;
    }

    /// @brief Assign and subtract
    tensor& operator-=(const tensor& rhs)
    {
        for (size_t i = 0; i < CLDNN_TENSOR_DIM_MAX; i++)
            _sizes[i] -= rhs._sizes[i];
        return *this;
    }

    /// @brief Returns a vector of tensors values, ordered regarding to @p format.
    std::vector<value_type> sizes(cldnn::format fmt) const {
        auto output_order = fmt.order();
        auto internal_order = fmt.internal_order();
        std::vector<value_type> sizes(output_order.size(), 0);

        for (size_t i = 0; i < sizes.size(); ++i)
        {
            auto c = output_order[i];
            auto pos = internal_order.find(c);
            if (pos == internal_order.npos)
                throw std::domain_error(std::string("Unknown coord type: ") + c);
            
            sizes[i] = _sizes[pos];
        }

        return sizes;
    }

    /// @brief Returns a vector of tensors values, ordered batch, feature, spatial_x, spatial_y.
    std::vector<value_type> sizes() const {
        std::vector<value_type> sizes(sizeof(_sizes) / sizeof(_sizes[0]), 0);
        for (size_t i = 0; i < sizes.size(); ++i)
            sizes[i] = _sizes[i];
        return sizes;
    }

    /// @brief Returns tensor elements count calculated as multiplication of all elements.
    size_t count() const { 
        return std::accumulate(
            raw.begin(),
            raw.end(), 
            static_cast<size_t>(1),
            std::multiplies<size_t>()
        );
    }

    /// @brief Returns new tensor based on current but transformed to new @p format.
    /// @param[in] new_fmt Format of new tensor.
    /// @param[in] default_size Default element values for positions not defined by current format.
    /// @details Example:
    /*!
     * @code
       tensor my_tensor({ 2, 3, 4, 5 });
       auto my_sizes = my_tensor.sizes();
       cout << "dims_num=" << my_sizes.size() << endl; // dims_num=2
       cout << "b=" << my_sizes[0] << endl;            // b=2
       cout << "f=" << my_sizes[1] << endl;            // f=3
       cout << "x=" << my_sizes[2] << endl;            // x=5
       cout << "y=" << my_sizes[3] << endl;            // y=4
       auto new_tensor = my_tensor.transform(format::yxfb, 10);
       auto new_sizes = new_tensor.sizes();
       cout << "new_num=" << new_sizes.size() << endl;   // new_num=4
       for(auto dim : new_sizes) cout << " " << dim;     //  5 4 3 2
       cout << endl;
       * @endcode
     */
    tensor transform(cldnn::format new_fmt, value_type default_size) const
    {
        cldnn::format format = cldnn::format::bfyx;
        auto val_order = format.internal_order();
        auto new_order = new_fmt.internal_order();
        std::vector<value_type> old_sizes = sizes();
        std::vector<value_type> new_sizes(old_sizes.size(), default_size);
        auto tmp = 1;
        for(size_t i = 0; i < format.order().size(); i++)
        {
            auto c = val_order[i];
            //skip f or b and y for the formats that do not have it
            if (((new_fmt == format::bs_xs_xsv8_bsv8) || (new_fmt == format::bs_xs_xsv8_bsv16) || (new_fmt == format::bs_x_bsv16)) && ((c == 'f') || (c == 'y')))
            {
                if (new_order[i] == '?')
                    new_sizes[i] = default_size;

                tmp *= old_sizes[i];
                continue;
            }

            auto new_pos = new_order.find(c);
            if (new_pos == std::string::npos)
                throw std::invalid_argument("cannot convert to new format");
            new_sizes[new_pos] = old_sizes[i];
        }
        
        //in case of formats with smaller number of dimensions than input, flatten is performed below
        if (tmp != 1)
        {
            for (size_t i = 0; i < format.order().size(); i++)
            {
                auto c = val_order[i];
                if (c == 'x')
                {
                    auto new_pos = new_order.find(c);
                    new_sizes[new_pos] *= tmp;
                }
            }
        }

        return { new_sizes };
    }

    /// @brief Calculates linear offset for given @p coord within current tensor.
    /// @param coord The coordinate within current tensor.
    size_t get_linear_offset(const tensor& coord, cldnn::format fmt) const
    {
        auto my_sizes = this->sizes();
        auto adjusted_coords = coord.sizes();
        if (fmt == cldnn::format::os_iyx_osv16 && !is_aligned_to(my_sizes[0], 16))
        {
            my_sizes[0] = align_to(my_sizes[0], 16);
            adjusted_coords[0] = align_to(adjusted_coords[0], 16);
        }
        else if (fmt == cldnn::format::bs_xs_xsv8_bsv8 && !(is_aligned_to(my_sizes[0], 8) && is_aligned_to(my_sizes[1], 8)))
        {
            my_sizes[0] = align_to(my_sizes[0], 8);
            my_sizes[1] = align_to(my_sizes[1], 8);
            adjusted_coords[0] = align_to(adjusted_coords[0], 8);
            adjusted_coords[1] = align_to(adjusted_coords[1], 8);
        }
        else if (fmt == cldnn::format::bs_xs_xsv8_bsv16 && !(is_aligned_to(my_sizes[0], 16) && is_aligned_to(my_sizes[1], 8)))
        {
            my_sizes[0] = align_to(my_sizes[0], 16);
            my_sizes[1] = align_to(my_sizes[1], 8);
            adjusted_coords[0] = align_to(adjusted_coords[0], 16);
            adjusted_coords[1] = align_to(adjusted_coords[1], 8);
        }
        else if (fmt == cldnn::format::bs_x_bsv16 && !is_aligned_to(my_sizes[0], 16))
        {
            my_sizes[0] = align_to(my_sizes[0], 16);
            adjusted_coords[0] = align_to(adjusted_coords[0], 16);
        }
        else if (fmt == cldnn::format::bf8_xy16 && !(is_aligned_to(my_sizes[1], 8) && is_aligned_to(my_sizes[2] * my_sizes[3], 16)))
        {
            my_sizes[1] = align_to(my_sizes[1], 8);
            my_sizes[3] = align_to(my_sizes[2] * my_sizes[3], 16);
            my_sizes[2] = 1;
            adjusted_coords[1] = align_to(adjusted_coords[1], 8);
            adjusted_coords[3] = align_to(adjusted_coords[3], 16);
            adjusted_coords[2] = 1;
        }
        else if (fmt == cldnn::format::os_is_yx_isa8_osv8_isv4 && !(is_aligned_to(my_sizes[0], 8)) && !(is_aligned_to(my_sizes[1], 32)))
        {
            my_sizes[0] = align_to(my_sizes[0], 8);
            my_sizes[1] = align_to(my_sizes[1], 32);
            adjusted_coords[0] = align_to(adjusted_coords[0], 8);
            adjusted_coords[1] = align_to(adjusted_coords[1], 32);
        }
        else if (fmt == cldnn::format::byxf_af32 && !(is_aligned_to(my_sizes[1], 32)))
        {
            my_sizes[1] = align_to(my_sizes[1], 32);
            adjusted_coords[1] = align_to(adjusted_coords[1], 32);
        }

        assert(my_sizes.size() == adjusted_coords.size());

        assert(adjusted_coords.size() > 0);
        size_t offset = adjusted_coords[0];
        for(size_t i = 1; i < adjusted_coords.size(); i++ )
        {
            offset = offset * my_sizes[i - 1] + adjusted_coords[i];
        }
        return offset;
    }

    /// @brief Returns a tensor containing values maximum from @p lhs and @p rhs.
    static tensor max(tensor const& lhs, tensor const& rhs)
    {
        auto ret = lhs;
        for (size_t i = 0; i < CLDNN_TENSOR_DIM_MAX; ++i)
            ret._sizes[i] = std::max(ret._sizes[i], rhs._sizes[i]);

        return ret;
    }

    /// @brief Returns a tensor containing values minimum from @p lhs and @p rhs.
    static tensor min(tensor const& lhs, tensor const& rhs)
    {
        auto ret = lhs;
        for (size_t i = 0; i < CLDNN_TENSOR_DIM_MAX; ++i)
            ret._sizes[i] = std::min(ret._sizes[i], rhs._sizes[i]);

        return ret;
    }

private:

    /// @brief Helper functions for tensor constructor using dim_vec_kinds
    template <typename KindInitT>
    void assign_inits(KindInitT&& init)
    {
        init.init_tensor_values(*this);
    }

    template <typename KindInitT, typename ... KindInitTys>
    void assign_inits(KindInitT&& init, KindInitTys&& ... kind_inits)
    {
        init.init_tensor_values(*this);
        assign_inits(std::forward<KindInitTys>(kind_inits) ...);
    }
};


template<details::dim_vec_kind Kind>
inline void details::dim_vec_kind_init<Kind>::init_tensor_values(cldnn::tensor & t)
{
    for (size_t i = _dimOffset; i < (size_t)(_dimOffset + _dimSize); i++)
        t._sizes[i] = _sizes[i - _dimOffset];
}

/// @brief Adds two @p tensors
inline tensor operator+(const tensor& lhs, const tensor& rhs) { return lhs.add(rhs); }
/// @brief Subtracts two @p tensors
inline tensor operator-(const tensor& lhs, const tensor& rhs) { return lhs.sub(rhs); }
/// @brief Multiplies a @p tensor to a @p scalar
inline tensor operator*(const tensor& lhs, tensor::value_type rhs) { return lhs.mul(rhs); }
/// @brief Divides a @p tensor by a @p scalar
inline tensor operator/(const tensor& lhs, tensor::value_type rhs) { return lhs.div(rhs); }

/// @}
/// @}
}
