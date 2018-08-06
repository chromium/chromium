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

#pragma once
#include "cldnn_defs.h"
#include <chrono>
#include <memory>
#include <vector>

namespace cldnn {
namespace instrumentation
{
/// @addtogroup cpp_api C++ API
/// @{

/// @addtogroup cpp_event Events Support
/// @{

/// @brief Helper class to calculate time periods.
template<class ClockTy = std::chrono::steady_clock>
class timer {
    typename ClockTy::time_point start_point;
public:
    /// @brief Timer value type.
    typedef typename ClockTy::duration val_type;

    /// @brief Starts timer.
    timer() :start_point(ClockTy::now()) {}

    /// @brief Returns time eapsed since construction.
    val_type uptime() const { return ClockTy::now() - start_point; }
};

/// @brief Abstract class to represent profiling period.
struct profiling_period
{
    /// @brief Returns profiling period value.
    virtual std::chrono::nanoseconds value() const = 0;
    /// @brief Destructor.
    virtual ~profiling_period() = default;
};

/// @brief Basic @ref profiling_period implementation which stores data as an simple period value.
struct profiling_period_basic : profiling_period
{
    /// @brief Constructs from @p std::chrono::duration.
    template<class _Rep, class _Period>
    profiling_period_basic(const std::chrono::duration<_Rep, _Period>& val) :
        _value(std::chrono::duration_cast<std::chrono::nanoseconds>(val)) {}

    /// @brief Returns profiling period value passed in constructor.
    std::chrono::nanoseconds value() const override { return _value; }
private:
    std::chrono::nanoseconds _value;
};

/// @brief Represents prifiling interval as its name and value.
/// @sa @ref ::cldnn_profiling_interval
struct profiling_interval {
    std::string name;                           ///< @brief Display name.
    std::shared_ptr<profiling_period> value;    ///< @brief Interval value.
};

/// @brief Represents list of @ref profiling_interval
struct profiling_info {
    std::string name;                           ///< @brief Display name.
    std::vector<profiling_interval> intervals;  ///< @brief List of intervals.
};
/// @}
/// @}
}}
