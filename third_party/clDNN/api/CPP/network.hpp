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
#include "memory.hpp"
#include "program.hpp"
#include "event.hpp"

#include <cstdint>
#include <algorithm>
#include <map>

namespace cldnn
{

/// @addtogroup cpp_api C++ API
/// @{

/// @defgroup cpp_network Network Execution
/// @{

/// @brief Represents network output returned by @ref network::get_output().
struct network_output
{
    /// @brief Returns @ref event associated with the output.
    event get_event() const { return _event; }

    /// @brief Returns @ref memory object of the output. Blocked until associated @ref event is not complete.
    memory get_memory() const
    {
        _event.wait();
        return _result;
    }
private:
    event _event;
    memory _result;
    network_output(event evt, memory mem): _event(evt), _result(mem){}
    network_output(cldnn_event evt, cldnn_memory mem): _event(evt), _result(mem){}
    friend struct network;
};

/// @brief Executable network allocated from @ref program.
struct network
{
    /// @brief Allocate network
    /// @param program The program object which contains compiled primitives this network should allocate memory for.
    network(program const& program)
        :_impl(check_status<cldnn_network>("network allocation failed", [&](status_t* status)
                {
                    return cldnn_allocate_network(program.get(), status);
                }))
    {}

    /// @brief Constructs network object from implicitly created program object. This is a shorthand for network(program(engine, topology, options))
    /// @param engine
    /// @param topology
    /// @param options 
    network(const engine& engine, const topology& topology, const build_options& options = build_options())
        :network(program(engine, topology, options))
    {}

    /// @brief Constructs network object from C API @ref cldnn_network.
    network(cldnn_network impl) :_impl(impl)
    {
        if (_impl == nullptr) throw std::invalid_argument("implementation pointer should not be null");
    }

    /// @brief Copy construction.
    network(const network& other) :_impl(other._impl)
    {
        retain();
    }

    /// @brief Copy assignment.
    network& operator=(const network& other)
    {
        if (_impl == other._impl) return *this;
        release();
        _impl = other._impl;
        retain();
        return *this;
    }

    /// @brief Releases wrapped C API @ref cldnn_network.
    ~network()
    {
        release();
    }

    friend bool operator==(const network& lhs, const network& rhs) { return lhs._impl == rhs._impl; }
    friend bool operator!=(const network& lhs, const network& rhs) { return !(lhs == rhs); }

    /// @brief Returns @ref engine by which network was built.
    engine get_engine() const
    {
        return check_status<cldnn_engine>("get network engine failed", [&](status_t* status) { return cldnn_get_network_engine(_impl, status); });
    }

    /// @brief Returns network internal @ref program.
    program get_program() const
    {
        return check_status<cldnn_program>("get network program failed", [&](status_t* status) { return cldnn_get_network_program(_impl, status); });
    }

    /// @brief Provides @ref memory for @ref input_layout primitives defined by user in source @ref topology.
    void set_input_data(const primitive_id& id, const memory& mem) const
    {
        check_status<void>("set network input failed", [&](status_t* status) { cldnn_set_network_input(_impl, id.c_str(), mem.get(), status); });
    }

    /// @brief Sets learning rate for training primitives.
    void set_learning_rate(const float lr)
    {
        check_status<void>("set learning rate failed", [&](status_t* status) { cldnn_set_learning_rate(_impl, lr, status); });
    }

    /// @brief Return learning rate.
    float get_learning_rate()
    {
        return check_status<float>("get learning rate failed", [&](status_t* status) { return cldnn_get_learning_rate(_impl, status); });
    }

   
    std::string get_primitive_info(const primitive_id& id) const
    {
        size_t size_ret = 0;
        status_t err_invalid_arg = CLDNN_SUCCESS;
        
        cldnn_get_primitive_info(_impl, id.c_str(), nullptr, 0, &size_ret, &err_invalid_arg);
        assert(err_invalid_arg == CLDNN_INVALID_ARG);
        assert(size_ret > 0);
        std::vector<char> names_buf(size_ret);

        check_status<void>("get primitive info failed", [&](status_t* status)
        {
            cldnn_get_primitive_info(_impl, id.c_str(), names_buf.data(), names_buf.size(), &size_ret, status);
        });
        assert(names_buf.size() == size_ret);

        std::string result(names_buf.begin(), names_buf.end());
        return result;
    }

    /// @brief Returns the list of executed primitives.
    std::vector<primitive_id> get_executed_primitive_ids() const
    {
        return get_prim_ids(cldnn_get_network_executed_primitive_names);
    }

    /// @brief Returns the list of all primitives ids in network.
    std::vector<primitive_id> get_all_primitive_ids() const
    {
        return get_prim_ids(cldnn_get_network_all_primitive_names);
    }

    /// @brief Returns the list of all primitives ids in network before graph optimization.
    std::vector<primitive_id> get_all_primitive_org_ids() const
    {
        return get_prim_ids(cldnn_get_network_all_primitive_org_names);
    }

    /// @brief Returns the list of available network outputs.
    std::vector<primitive_id> get_output_ids() const
    {
        return get_prim_ids(cldnn_get_network_output_names);
    }

    /// @brief Returns @ref network_output object for particular @p output. Can't be called before network execution
    network_output get_output(const primitive_id& output_id) const
    {
        cldnn_network_output output =
        check_status<cldnn_network_output>("get network output failed", [&](status_t* status)
        {
            return cldnn_get_network_output(_impl, output_id.c_str(), status);
        });
        return network_output( output.event, output.memory );
    }

    /// @brief Returns @ref memory object for particular @p output. Can be called before network execution
    memory get_output_memory(const primitive_id& output_id) const
    {
        cldnn_memory output =
            check_status<cldnn_memory>("get output memory failed", [&](status_t* status)
        {
            return cldnn_get_network_output_memory(_impl, output_id.c_str(), status);
        });
        return output;
    }

    /// @brief Returns @ref event object for particular @p primitive. Can't be called before network execution
    event get_primitive_event(const primitive_id& output_id) const
    {
        cldnn_event output =
            check_status<cldnn_event>("get output event failed", [&](status_t* status)
        {
            return cldnn_get_network_output_event(_impl, output_id.c_str(), status);
        });
        return output;
    }

    /// @brief Returns the list of @ref event for the primitives that were executed in network.
    std::map<primitive_id, event> get_executed_primitives() const
    {
        auto primitive_ids = get_executed_primitive_ids();
        auto all_primitive_ids = get_all_primitive_ids();
        auto all_primitive_org_ids = get_all_primitive_org_ids();
        //Get list of optimized prmitives
        std::vector<primitive_id> optimized_primitives;
        for (decltype(all_primitive_org_ids.size()) i = 0; i < all_primitive_org_ids.size(); i++)
        {
            if (all_primitive_ids[i] == "_optimized_")
                optimized_primitives.push_back(all_primitive_org_ids[i]);
        }
        std::map<primitive_id, event> result;
        for (auto& id : primitive_ids)
        {
            if(std::find(optimized_primitives.begin(), optimized_primitives.end(), id) == optimized_primitives.end())
                result.emplace(id, get_primitive_event(id));
        }
        return result;
    }

    /// @brief Returns the list of primitive ids before and after graph optimization.
    /// @details If primitive was not optimized, the old and actual id will be the same.
    /// @n If primitive was optimized during graph optimization, the actual id will be "_optimized_".
    std::map<primitive_id, primitive_id> get_all_primitives() const
    {
        auto primitive_ids = get_all_primitive_ids();
        auto primitive_org_ids = get_all_primitive_org_ids();
        std::map<primitive_id, primitive_id> result;
        for (decltype(primitive_org_ids.size()) i = 0; i < primitive_org_ids.size(); i++)
        {
            result.emplace(primitive_org_ids[i], primitive_ids[i]);
        }
        return result;
    }

    /// @brief Executes network and returns the list of @ref network_output.
    /// @param dependencies List of @ref event objects to be waited before network execution.
    /// @note User should call set_input_data() for every @ref input_layout defined in source @ref topology
    /// before network execution.
    std::map<primitive_id, network_output> execute(const std::vector<event>& dependencies = {}) const
    {
        std::vector<cldnn_event> dep_refs(dependencies.size());
        for(decltype(dependencies.size()) i = 0; i < dependencies.size(); i++)
        {
            dep_refs[i] = dependencies[i].get();
        }

        check_status<void>("network execute failed", [&](status_t* status)
        {
            return cldnn_execute_network(_impl, dep_refs.data(), dep_refs.size(), status);
        });

        auto output_ids = get_output_ids();
        std::map<primitive_id, network_output> result;
        for(auto& id : output_ids)
        {
            result.emplace(id, get_output(id));
        }
        return result;
    }

    /// @brief Returns wrapped C API @ref cldnn_network handler.
    cldnn_network get() const { return _impl; }

private:
    cldnn_network _impl;

    typedef void(*get_prim_ids_func_t)(cldnn_network network, char* names, size_t size, size_t* size_ret, cldnn_status* status);

    void retain()
    {
        check_status<void>("retain topology failed", [=](status_t* status) { cldnn_retain_network(_impl, status); });
    }
    void release()
    {
        check_status<void>("retain topology failed", [=](status_t* status) { cldnn_release_network(_impl, status); });
    }

    std::vector<primitive_id> get_prim_ids(get_prim_ids_func_t func) const
    {
        size_t size_ret = 0;
        status_t err_invalid_arg = CLDNN_SUCCESS;
        func(_impl, nullptr, 0, &size_ret, &err_invalid_arg);
        assert(err_invalid_arg == CLDNN_INVALID_ARG);
        assert(size_ret > 0);
        std::vector<char> names_buf(size_ret);

        check_status<void>("get network output ids failed", [&](status_t* status)
        {
            func(_impl, names_buf.data(), names_buf.size(), &size_ret, status);
        });
        assert(names_buf.size() == size_ret);

        std::vector<primitive_id> result;
        for (auto buf_ptr = names_buf.data(); *buf_ptr != 0; buf_ptr += result.back().size() + 1)
        {
            result.emplace_back(buf_ptr);
        }
        return result;
    }

};
CLDNN_API_CLASS(network)
/// @}
/// @}
}
