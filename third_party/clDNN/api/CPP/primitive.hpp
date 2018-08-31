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
#include "layout.hpp"

#include <algorithm>
#include <string>
#include <vector>
#include <iostream>

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{

/// @addtogroup cpp_topology Network Topology
/// @{

/// @brief Globally unique primitive type id.
using primitive_type_id = cldnn_primitive_type_id;
/// @brief C API compatible unique @p id of a primitive within a topology.
using primitive_id_ref = cldnn_primitive_id;
/// @brief Unique @p id of a primitive within a topology.
using primitive_id = std::string;

/// @brief Dynamic cast to specified primitive description type.
template<class PType>
typename PType::dto* as_dto(CLDNN_PRIMITIVE_DESC(primitive)* dto)
{
    if (dto->type != PType::type_id()) throw std::invalid_argument("type");
    return reinterpret_cast<typename PType::dto*>(dto);
}

/// @brief Dynamic cast to specified primitive description type.
template<class PType>
const typename PType::dto* as_dto(const CLDNN_PRIMITIVE_DESC(primitive)* dto)
{
    if (dto->type != PType::type_id()) throw std::invalid_argument("type");
    return reinterpret_cast<const typename PType::dto*>(dto);
}

/// @brief Base class of network primitive description.
struct primitive
{
    /// @brief Initialize fields common for all primitives.
    struct fixed_size_vector_ref
    {
    private:
        std::vector<primitive_id>& vref;

    public:
        fixed_size_vector_ref(std::vector<primitive_id>& ref) : vref(ref)
        {}

        auto size() const -> decltype(vref.size()) { return vref.size(); }
        auto begin() const -> decltype(vref.begin()) { return vref.begin(); }
        auto end() const -> decltype(vref.end()) { return vref.end(); }
        auto cbegin() const -> decltype(vref.cbegin()) { return vref.cbegin(); }
        auto cned() const -> decltype(vref.cend()) { return vref.cend(); }

        primitive_id& operator[](size_t idx) { return vref[idx]; }
        primitive_id const& operator[](size_t idx) const { return vref[idx]; }

        primitive_id& at(size_t idx) { return vref.at(idx); }
        primitive_id const& at(size_t idx) const { return vref.at(idx); }

        primitive_id* data() { return vref.data(); }
        const primitive_id* data() const { return vref.data(); }

        const std::vector<primitive_id>& ref() const { return vref; }
    };
public:
    primitive(
        const primitive_type_id& type,
        const primitive_id& id,
        const std::vector<primitive_id>& input,
        const padding& output_padding = padding()
    )
        :type(type), id(id), input(_input.cpp_ids), output_padding(output_padding), _input(input)
    {}

    /// @brief Constructs a copy from basic C API @CLDNN_PRIMITIVE_DESC{primitive}
    primitive(const CLDNN_PRIMITIVE_DESC(primitive)* dto)
        :type(dto->type), id(dto->id), input(_input.cpp_ids), output_padding(dto->output_padding), _input(dto->input)
    {}

    virtual ~primitive() = default;

    /// @brief Requested output padding.
    /// @brief Requested output padding.
    /// @brief Returns pointer to a C API primitive descriptor casted to @CLDNN_PRIMITIVE_DESC{primitive}.
    virtual const CLDNN_PRIMITIVE_DESC(primitive)* get_dto() const = 0;

    /// @brief Returns references to all primitive ids on which this primitive depends - inputs, weights, biases, etc.
    std::vector<std::reference_wrapper<primitive_id>> dependencies()
    {
        std::vector<std::reference_wrapper<primitive_id>> result;
        auto&& deps = get_dependencies();
        
        result.reserve(_input.size() + deps.size());
        for (auto& pid : _input.cpp_ids)
            result.push_back(std::ref(pid));
        for (auto& pid : deps)
            result.push_back(std::ref(const_cast<primitive_id&>(pid.get())));

        return result;
    }

    /// @brief Returns copy of all primitive ids on which this primitive depends - inputs, weights, biases, etc.
    std::vector<primitive_id> dependencies() const
    {
        auto result = input.ref();
        auto deps = get_dependencies();
        result.insert(result.end(), deps.begin(), deps.end());
        return result;
    }

    /// @brief Implicit conversion to primiitive id.
    operator primitive_id() const { return id; }

    /// @brief Primitive's type id.
    const primitive_type_id type;

    /// @brief Primitive's id.
    const primitive_id id;

    /// @brief List of ids of input primitives.
    fixed_size_vector_ref input;

    /// @brief Requested output padding.
    padding output_padding;

protected:
    struct primitive_id_arr
    {
        primitive_id_arr(std::vector<primitive_id> const& vec) : cpp_ids(vec)
        {}

        primitive_id_arr(std::vector<primitive_id>&& vec) : cpp_ids(std::move(vec))
        {}

        //create from C API id array
        primitive_id_arr(cldnn_primitive_id_arr c_id_arr)
        {
            cpp_ids.resize(c_id_arr.size);
            for (size_t i = 0; i < c_id_arr.size; ++i)
                cpp_ids[i] = c_id_arr.data[i];
        }

        std::vector<primitive_id> cpp_ids;
        mutable std::vector<cldnn_primitive_id> c_ids;
        //get C API id array
        auto ref() const -> decltype(cldnn_primitive_id_arr{c_ids.data(), c_ids.size()})
        {
            c_ids.resize(cpp_ids.size());
            for (size_t i = 0; i < cpp_ids.size(); ++i)
                c_ids[i] = cpp_ids[i].c_str();

            return cldnn_primitive_id_arr{ c_ids.data(), c_ids.size() };
        }

        size_t size() const { return cpp_ids.size(); }
    };

    primitive_id_arr _input;

    virtual std::vector<std::reference_wrapper<const primitive_id>> get_dependencies() const { return{}; }
};

/// @brief base class for all primitives implementations.
template<class PType, class DTO>
class primitive_base : public primitive
{
public:
    /// @brief Returns pointer to a C API primitive descriptor casted to @CLDNN_PRIMITIVE_DESC{primitive}.
    const CLDNN_PRIMITIVE_DESC(primitive)* get_dto() const override
    {
        //update common dto fields
        _dto.id = id.c_str();
        _dto.type = type;
        _dto.input = _input.ref();
        _dto.output_padding = output_padding;

        //call abstract method to update primitive-specific fields
        update_dto(_dto);
        return reinterpret_cast<const CLDNN_PRIMITIVE_DESC(primitive)*>(&_dto);
    }

protected:
    explicit primitive_base(
        const primitive_id& id,
        const std::vector<primitive_id>& input,
        const padding& output_padding = padding())
        : primitive(PType::type_id(), id, input, output_padding)
    {}

    primitive_base(const DTO* dto)
        : primitive(reinterpret_cast<const CLDNN_PRIMITIVE_DESC(primitive)*>(dto))
    {
        if (dto->type != PType::type_id()) 
            throw std::invalid_argument("DTO type mismatch");
    }

private:
    mutable DTO _dto;

    virtual void update_dto(DTO& dto) const = 0;
};

#define CLDNN_DEFINE_TYPE_ID(PType) static primitive_type_id type_id()\
    {\
        return check_status<primitive_type_id>( #PType " type id failed", [](status_t* status)\
        {\
            return cldnn_##PType##_type_id(status);\
        });\
    }

#define CLDNN_DECLARE_PRIMITIVE(PType) typedef CLDNN_PRIMITIVE_DESC(PType) dto;\
    CLDNN_DEFINE_TYPE_ID(PType)
/// @}
/// @}
}