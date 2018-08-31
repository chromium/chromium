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
#include "engine.hpp"
#include "profiling.hpp"
#include <algorithm>
#include <cassert>

namespace cldnn
{

/// @addtogroup cpp_api C++ API
/// @{

/// @addtogroup cpp_event Events Support
/// @{

/// @brief Represents an clDNN Event object
struct event
{
    /// @brief Create an event which can be set to 'completed' by user.
    static event create_user_event(const engine& engine)
    {
        return check_status<cldnn_event>("create user event failed", [&](status_t* status) { return cldnn_create_user_event(engine.get(), status); });
    }
    
    /// @brief Construct from C API handler @ref ::cldnn_event.
    event(cldnn_event impl) : _impl(impl)
    {
        if (_impl == nullptr) throw std::invalid_argument("implementation pointer should not be null");
    }

    event(const event& other) : _impl(other._impl)
    {
        retain();
    }
    
    event& operator=(const event& other)
    {
        if (_impl == other._impl) return *this;
        release();
        _impl = other._impl;
        retain();
        return *this;
    }

    ~event()
    {
        release();
    }

    friend bool operator==(const event& lhs, const event& rhs) { return lhs._impl == rhs._impl; }
    friend bool operator!=(const event& lhs, const event& rhs) { return !(lhs == rhs); }

    /// @brief Wait for event completion.
    void wait() const { check_status<void>("wait event failed", [=](status_t* status) { cldnn_wait_for_event(_impl, status); }); }

    /// @brief Set event status to 'completed'.
    void set() const { check_status<void>("set event failed", [=](status_t* status) { cldnn_set_event(_impl, status); }); }

    /// @brief Register call back to be called on event completion.
    void set_event_handler(cldnn_event_handler handler, void* param) const
    {
        check_status<void>("set event handler failed", [=](status_t* status) { cldnn_add_event_handler(_impl, handler, param, status); });
    }

    /// @brief Get profiling info for the event associated with network output.
    std::vector<instrumentation::profiling_interval> get_profiling_info() const
    {
        using namespace instrumentation;
        wait();
        size_t size_ret = 0;
        status_t err_invalid_arg = CLDNN_SUCCESS;
        cldnn_get_event_profiling_info(_impl, nullptr, 0, &size_ret, &err_invalid_arg);
        
        if (size_ret == 0)
        {
            return{};
        }

        std::vector<cldnn_profiling_interval> profiling_info_ref(size_ret);

        check_status<void>("get event profiling info failed", [&](status_t* status)
        {
            cldnn_get_event_profiling_info(_impl, profiling_info_ref.data(), profiling_info_ref.size(), &size_ret, status);
        });
        assert(profiling_info_ref.size() == size_ret);

        std::vector<profiling_interval> result(profiling_info_ref.size());
        std::transform(
            std::begin(profiling_info_ref),
            std::end(profiling_info_ref),
            std::begin(result),
            [](const cldnn_profiling_interval& ref) -> profiling_interval
            {
                return{
                    ref.name,
                    std::make_shared<profiling_period_basic>(std::chrono::nanoseconds(ref.nanoseconds))
                };
            }
        );
        return result;
    }

    /// @brief Returns C API event handler.
    cldnn_event get() const { return _impl; }
private:
    cldnn_event _impl;
    void retain()
    {
        check_status<void>("retain event failed", [=](status_t* status) { cldnn_retain_event(_impl, status); });
    }
    void release()
    {
        check_status<void>("retain event failed", [=](status_t* status) { cldnn_release_event(_impl, status); });
    }
};
CLDNN_API_CLASS(event)

/// @}
/// @}
}
