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
#include "tensor.hpp"
#include <cmath>
#include <cstdlib>

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{

/// @addtogroup cpp_memory Memory description and management
/// @{

/// @brief Possible data types could be stored in memory.
enum class data_types : size_t
{
    i8 = cldnn_i8,/// Not supported in current HW
    u8 = cldnn_u8,/// 
    f16 = cldnn_f16,
    f32 = cldnn_f32,
};

/// Converts C++ type to @ref data_types .
template <typename T> struct type_to_data_type;
#ifndef DOXYGEN_SHOULD_SKIP_THIS
template<> struct type_to_data_type  <int8_t> { static const data_types value = data_types::i8; };
template<> struct type_to_data_type <uint8_t> { static const data_types value = data_types::u8; };
template<> struct type_to_data_type  <half_t> { static const data_types value = data_types::f16; };
template<> struct type_to_data_type   <float> { static const data_types value = data_types::f32; };
#endif

/// Converts @ref data_types to C++ type.
template<data_types Data_Type> struct data_type_to_type;
#ifndef DOXYGEN_SHOULD_SKIP_THIS
template<> struct data_type_to_type <data_types::i8> { typedef int8_t type; };
template<> struct data_type_to_type<data_types::f16> { typedef half_t type; };
template<> struct data_type_to_type<data_types::f32> { typedef float type; };
#endif


/// Helper class to identify key properties for data_types.
struct data_type_traits
{
    static size_t size_of(data_types data_type)
    {
        return (static_cast<uint32_t>(data_type) & ~(CLDNN_FLOAT_TYPE_MASK | CLDNN_UINT_TYPE_MASK));
    }

    static bool is_floating_point(data_types data_type)
    {
        return (static_cast<uint32_t>(data_type) & CLDNN_FLOAT_TYPE_MASK) != 0;
    }

    static size_t align_of(data_types data_type)
    {
        switch (data_type)
        {
        case data_types::i8:
            return alignof(data_type_to_type<data_types::i8>::type);
        case data_types::f16:
            return alignof(data_type_to_type<data_types::f16>::type);
        case data_types::f32:
            return alignof(data_type_to_type<data_types::f32>::type);
        default: return size_t(1);
        }
    }

    static std::string name(data_types data_type)
    {
        switch (data_type)
        {
        case data_types::i8:
            return "i8";
        case data_types::f16:
            return "f16";
        case data_types::f32:
            return "f32";
        default:
            assert(0);
            return std::string("invalid data type: " + std::to_string((int)data_type));
        }
    }
};

/// Helper function to check if C++ type matches @p data_type.
template <typename T>
bool data_type_match(data_types data_type)
{
    return data_type == type_to_data_type<T>::value;
}

/// Helper function to get both data_types and format::type in a single, unique value. Useable in 'case' statement.
constexpr auto fuse(data_types dt, cldnn::format::type fmt) -> decltype(static_cast<std::underlying_type<data_types>::type>(dt) | static_cast<std::underlying_type<format::type>::type>(fmt))
{
    using dt_type = std::underlying_type<data_types>::type;
    using fmt_type = std::underlying_type<cldnn::format::type>::type;
    using fmt_narrow_type = int16_t;

    return static_cast<fmt_type>(fmt) <= std::numeric_limits<fmt_narrow_type>::max() &&
        static_cast<dt_type>(dt) <= (std::numeric_limits<dt_type>::max() >> (sizeof(fmt_narrow_type) * 8))
        ? (static_cast<dt_type>(dt) << (sizeof(fmt_narrow_type) * 8)) |
        (static_cast<fmt_type>(fmt) >= 0 ? static_cast<fmt_narrow_type>(fmt) : static_cast<fmt_narrow_type>(-1))
        : throw std::invalid_argument("data_type and/or format values are too big to be fused into single value");
}


/// @brief Represents data padding information.
struct padding
{
    /// @brief Filling value for padding area.
    float filling_value() const { return _filling_value; }

    /// @brief Gets lower padding sizes. For spatials, it means size of left (X) and top (Y) padding.
    /// @return Tensor with padding for top/left/lower bounds of data.
    tensor lower_size() const { return _lower_size; }

    /// @brief Gets upper padding sizes. For spatials, it means size of right (X) and bottom (Y) padding.
    /// @return Tensor with padding for bottom/right/upper bounds of data.
    tensor upper_size() const { return _upper_size; }

    /// @brief 
    /// @param lower_sizes Top-left padding sizes. See @ref tensor::tensor(const std::vector<value_type>&, value_type) for details.
    /// @param upper_sizes Bottom-right padding sizes. See @ref tensor::tensor(const std::vector<value_type>&, value_type) for details.
    /// @param filling_value Filling value for padding area.
    padding(const std::vector<tensor::value_type>& lower_sizes, const std::vector<tensor::value_type>& upper_sizes, float filling_value = 0.0f)
        : _lower_size(to_abs(lower_sizes), 0), _upper_size(to_abs(upper_sizes), 0), _filling_value(filling_value)
    {}

    /// @brief Constrcuts symmetric padding.
    /// @param sizes Top-left and bottom-right padding sizes. See @ref tensor::tensor(const std::vector<value_type>&, value_type) for details.
    /// @param filling_value Filling value for padding area.
    padding(const std::vector<tensor::value_type>& sizes, float filling_value = 0.0f)
        : padding(sizes, sizes, filling_value)
    {}

    /// @brief Constructs "zero-sized" padding.
    padding() : padding({ 0,0,0,0 }, 0) {}

    /// @brief Copy construction.
    padding(const cldnn_padding& other)
        : _lower_size(other.lower_size), _upper_size(other.upper_size), _filling_value(other.filling_value)
    {}

    /// @brief Implicit conversion to C API @ref cldnn_padding.
    operator cldnn_padding() const
    {
        return{ static_cast<cldnn_tensor>(_lower_size),
                 static_cast<cldnn_tensor>(_upper_size),
                 _filling_value };
    }

    /// @brief Returns true if padding size is not zero.
    explicit operator bool() const
    {
        return std::any_of(_lower_size.raw.begin(), _lower_size.raw.end(), [](const tensor::value_type& el) { return el != 0; }) ||
            std::any_of(_upper_size.raw.begin(), _upper_size.raw.end(), [](const tensor::value_type& el) { return el != 0; });
    }

    friend bool operator ==(const padding& lhs, const padding& rhs)
    {
        return lhs._lower_size == rhs._lower_size
            && lhs._upper_size == rhs._upper_size
            && lhs._filling_value == rhs._filling_value;
    }

    friend bool operator !=(const padding& lhs, const padding& rhs)
    {
        return !(lhs == rhs);
    }

    friend bool operator <(const padding& lhs, const padding& rhs)
    {
        if (lhs._filling_value != rhs._filling_value)
            return (lhs._filling_value < rhs._filling_value);
        if (lhs._lower_size != rhs._lower_size)
            return (lhs._lower_size < rhs._lower_size);
        return (lhs._upper_size < rhs._upper_size);
    }

    static padding max(padding const& lhs, padding const& rhs, float filling_value = 0.0f)
    {
        auto lower = tensor::max(lhs.lower_size(), rhs.lower_size());
        auto upper = tensor::max(lhs.upper_size(), rhs.upper_size());
        return padding{ lower.sizes(), upper.sizes(), filling_value };
    }

private:
    tensor _lower_size;    ///< Lower padding sizes. For spatials, it means size of left (X) and top (Y) padding.
    tensor _upper_size;    ///< Upper padding sizes. For spatials, it means size of right (X) and bottom (Y) padding.
    // TODO: Add support for non-zero filling value (if necessary) or remove variable (if not necessary).
    float  _filling_value; ///< Filling value for an element of padding. If data type of elements is different than float it is converted
                           ///< to it using round-towards-nearest-even (for floating-point data types) or round-towards-zero (for integral
                           ///< data types).

    static std::vector<tensor::value_type> to_abs(const std::vector<tensor::value_type>& sizes)
    {
        std::vector<tensor::value_type> result;
        result.reserve(sizes.size());
        std::transform(sizes.cbegin(), sizes.cend(), std::back_inserter(result), [](const tensor::value_type& el) { return abs(el); });
        return result; // NRVO
    }
};

/// @brief Describes memory layout.
/// @details Contains information about data stored in @ref memory.
struct layout
{
    /// Constructs layout based on @p data_type and @p size information described by @ref tensor
    layout(data_types data_type, cldnn::format fmt, tensor size, padding apadding = padding())
        : data_type(data_type)
        , format(fmt)
        , size(size)
        , data_padding(apadding)
    {}

    /// Construct C++ layout based on C API @p cldnn_layout
    layout(const cldnn_layout& other)
        : data_type(static_cast<data_types>(other.data_type))
        , format(static_cast<cldnn::format::type>(other.format))
        , size(other.size)
        , data_padding(other.padding)
    {}

    /// Convert to C API @p cldnn_layout
    operator cldnn_layout() const
    {
        return{ static_cast<decltype(cldnn_layout::data_type)>(data_type), static_cast<decltype(cldnn_layout::format)>(format), size, data_padding };
    }

    layout(const layout& other) = default;

    layout& operator=(const layout& other)
    {
        if (this == &other)
            return *this;
        data_type = other.data_type;
        format = other.format;
        size = other.size;
        data_padding = other.data_padding;
        return *this;
    }

    friend bool operator==(const layout& lhs, const layout& rhs)
    {
        return lhs.data_type == rhs.data_type
            && lhs.format == rhs.format
            && lhs.size == rhs.size
            && lhs.data_padding == rhs.data_padding;
    }

    friend bool operator!=(const layout& lhs, const layout& rhs)
    {
        return !(lhs == rhs);
    }

    friend bool operator<(const layout& lhs, const layout& rhs)
    {
        if (lhs.data_type != rhs.data_type)
            return (lhs.data_type < rhs.data_type);
        if (lhs.format != rhs.format)
            return (lhs.format < rhs.format);
        if (lhs.size < rhs.size)
            return (lhs.size < rhs.size);
        return (lhs.data_padding < rhs.data_padding);
    }

    /// Number of elements to be stored in this memory layout
    size_t count() const { return size.count(); }

    /// Layout size with padding included
    tensor get_buffer_size() const
    {
        return size.add(data_padding.lower_size()).add(data_padding.upper_size());
    }

    tensor get_pitches() const
    {
        auto sizes = get_buffer_size().sizes(format);
        if (format == format::byxf_af32)
        {
            sizes[3] = align_to(sizes[3], 32);
        }
        std::vector<tensor::value_type> pitches(sizes.size(), tensor::value_type(1));
        std::partial_sum(sizes.rbegin(), sizes.rend() - 1, pitches.rbegin() + 1, std::multiplies<tensor::value_type>());
        return{ format, pitches };
    }

    // @brief Calculates position within buffer of the data element pointed by the provided tensor.
    // element == { 0,0,0,0 } means first no-padding (i.e. data) element
    size_t get_linear_offset(tensor element = { 0,0,0,0 }) const
    {
        auto pitches = get_pitches();
        auto l_padd = data_padding.lower_size();
        auto u_padd = data_padding.upper_size();

        if ((element.batch[0] < 0 && -element.batch[0] > l_padd.batch[0]) ||
            (element.feature[0] < 0 && -element.feature[0] > l_padd.feature[0]) ||
            (element.spatial[0] < 0 && -element.spatial[0] > l_padd.spatial[0]) ||
            (element.spatial[1] < 0 && -element.spatial[1] > l_padd.spatial[1]) ||
            (element.batch[0] >= size.batch[0] + u_padd.batch[0]) ||
            (element.feature[0] >= size.feature[0] + u_padd.feature[0]) ||
            (element.spatial[0] >= size.spatial[0] + u_padd.spatial[0]) ||
            (element.spatial[1] >= size.spatial[1] + u_padd.spatial[1]))
            throw std::invalid_argument("Requested to calculate linear offset for an element which lies outside of the buffer range.");

        size_t linear_offset =
            static_cast<size_t>(element.batch[0] + l_padd.batch[0]) * static_cast<size_t>(pitches.batch[0]) +
            static_cast<size_t>(element.feature[0] + l_padd.feature[0]) * static_cast<size_t>(pitches.feature[0]) +
            static_cast<size_t>(element.spatial[0] + l_padd.spatial[0]) * static_cast<size_t>(pitches.spatial[0]) +
            static_cast<size_t>(element.spatial[1] + l_padd.spatial[1]) * static_cast<size_t>(pitches.spatial[1]);

        return linear_offset;
    }

    /// @brief Get aligned linear size calculated as multiplication of all elements. 
    size_t get_linear_size() const
    {
        auto sizes = get_buffer_size().sizes();
        if (this->format == cldnn::format::os_iyx_osv16 && !is_aligned_to(sizes[0], 16))
        {
            sizes[0] = align_to(sizes[0], 16);
        }
        else if (this->format == cldnn::format::bs_xs_xsv8_bsv8 && !(is_aligned_to(sizes[0], 8) && is_aligned_to(sizes[2], 8)))
        {
            sizes[0] = align_to(sizes[0], 8);
            sizes[2] = align_to(sizes[2], 8);
        }
        else if (this->format == cldnn::format::bs_xs_xsv8_bsv16 && !(is_aligned_to(sizes[0], 16) && is_aligned_to(sizes[2], 8)))
        {
            sizes[0] = align_to(sizes[0], 16);
            sizes[2] = align_to(sizes[2], 8);
        }
        else if (this->format == cldnn::format::bs_x_bsv16 && !is_aligned_to(sizes[0], 16))
        {
            sizes[0] = align_to(sizes[0], 16);
        }
        else if (this->format == cldnn::format::bf8_xy16 && !(is_aligned_to(sizes[1], 8) && is_aligned_to(sizes[2] * sizes[3], 16)))
        {
            sizes[1] = align_to(sizes[1], 8);
            sizes[3] = align_to(sizes[2]*sizes[3], 16);
            sizes[2] = 1;
        }
        else if (this->format == cldnn::format::byxf_af32 && !(is_aligned_to(sizes[1], 32)))
        {
            sizes[1] = align_to(sizes[1], 32);
        }
        else if (this->format == cldnn::format::os_is_yx_isa8_osv8_isv4 && !(is_aligned_to(sizes[0], 8)) && !(is_aligned_to(sizes[1], 32)))
        {
            sizes[0] = align_to(sizes[0], 8);
            sizes[1] = align_to(sizes[1], 32);
        }
        return std::accumulate(
            sizes.begin(),
            sizes.end(),
            static_cast<size_t>(1),
            std::multiplies<size_t>()
        );
    }

    /// Modify padding in layout
    layout with_padding(padding const& padd) const
    {
        layout ret = *this;
        ret.data_padding = padd;
        return ret;
    }

    /// Data type stored in @ref memory (see. @ref data_types)
    data_types data_type;

    /// Format stored in @ref memory (see. @ref format)
    cldnn::format format;

    /// The size of the @ref memory (excluding padding)
    tensor size;

    /// Explicit padding of the @ref memory
    padding data_padding;

    /// Number of bytes needed to store this layout
    size_t bytes_count() const { return data_type_traits::size_of(data_type) * get_linear_size(); }

    bool has_fused_format(data_types const& dt, cldnn::format const& fmt) const
    {
        return (data_type == dt && format == fmt);
    }

    auto fused_format() const -> decltype(fuse(data_type, format))
    {
        return fuse(data_type, format);
    }
};


/// @}
/// @}
}
