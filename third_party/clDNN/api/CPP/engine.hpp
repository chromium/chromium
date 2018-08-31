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

namespace cldnn
{

/// @addtogroup cpp_api C++ API
/// @{

/// @defgroup cpp_engine Execution Engine
/// @{

/// @brief Defines available engine types
enum class engine_types : int32_t
{
    ocl = cldnn_engine_ocl
};

/// @brief Defines available priority mode types
enum class priority_mode_types : int16_t
{
    disabled = cldnn_priority_disabled,
    low = cldnn_priority_low,
    med = cldnn_priority_med,
    high = cldnn_priority_high
};

/// @brief Defines available priority mode types
enum class throttle_mode_types : int16_t
{
    disabled = cldnn_throttle_disabled,
    low = cldnn_throttle_low,
    med = cldnn_throttle_med,
    high = cldnn_throttle_high
};

/// @brief Configuration parameters for created engine.
struct engine_configuration
{
    const bool enable_profiling;                ///< Enable per-primitive profiling.
    const bool meaningful_kernels_names;        ///< Generate meaniful names fo OpenCL kernels.
    const bool dump_custom_program;             ///< Dump the user OpenCL programs to files
    const std::string compiler_options;         ///< OpenCL compiler options string.
    const std::string single_kernel_name;       ///< If provided, runs specific layer.
    const bool enable_parallelisation;          ///< Enables parallel execution of primitives which don't depend on each other. Disabled by default.
    const std::string engine_log;               ///< Specifies a file to which engine log should be dumped. Empty by default (means no logging).
    const std::string sources_dumps_dir;        ///< Specifies a directory where sources of cldnn::program objects should be dumped. Empty by default (means no dumping).
    const priority_mode_types priority_mode;    ///< Priority mode (support of priority hints in command queue). If cl_khr_priority_hints extension is not supported by current OpenCL implementation, the value must be set to cldnn_priority_disabled.
    const throttle_mode_types throttle_mode;    ///< Placeholder for throttle mode (support of throttle hints in command queue). It has no effect for now and should be set to cldnn_throttle_disabled.
    bool enable_memory_pool;              ///< Enables memory usage optimization. memory objects will be reused when possible (switched off for older drivers then NEO).

    /// @brief Constructs engine configuration with specified options.
    /// @param profiling Enable per-primitive profiling.
    /// @param decorate_kernel_names Generate meaniful names fo OpenCL kernels.
    /// @param dump_custom_program Dump the custom OpenCL programs to files
    /// @param options OpenCL compiler options string.
    /// @param single_kernel If provided, runs specific layer.
    engine_configuration(
            bool profiling = false,
            bool decorate_kernel_names = false,
            bool dump_custom_program = false,
            const std::string& options = std::string(),
            const std::string& single_kernel = std::string(),
            bool primitives_parallelisation = true,
            const std::string& engine_log = std::string(),
            const std::string& sources_dumps_dir = std::string(),
            priority_mode_types priority_mode = priority_mode_types::disabled,
            throttle_mode_types throttle_mode = throttle_mode_types::disabled,
            bool memory_pool = true)
        : enable_profiling(profiling)
        , meaningful_kernels_names(decorate_kernel_names)
        , dump_custom_program(dump_custom_program)
        , compiler_options(options)
        , single_kernel_name(single_kernel)
        , enable_parallelisation(primitives_parallelisation)
        , engine_log(engine_log)
        , sources_dumps_dir(sources_dumps_dir)
        , priority_mode(priority_mode)
        , throttle_mode(throttle_mode)
        , enable_memory_pool(memory_pool)
    {}

    engine_configuration(const cldnn_engine_configuration& c_conf)
        : enable_profiling(c_conf.enable_profiling != 0)
        , meaningful_kernels_names(c_conf.meaningful_kernels_names != 0)
        , dump_custom_program(c_conf.dump_custom_program != 0)
        , compiler_options(c_conf.compiler_options)
        , single_kernel_name(c_conf.single_kernel_name)
        , enable_parallelisation(c_conf.enable_parallelisation != 0)
        , engine_log(c_conf.engine_log)
        , sources_dumps_dir(c_conf.sources_dumps_dir)
        , priority_mode(static_cast<priority_mode_types>(c_conf.priority_mode))
        , throttle_mode(static_cast<throttle_mode_types>(c_conf.throttle_mode))
        , enable_memory_pool(c_conf.enable_memory_pool != 0)
    {}

    /// @brief Implicit conversion to C API @ref ::cldnn_engine_configuration
    operator ::cldnn_engine_configuration() const
    {
        return{
            enable_profiling,
            meaningful_kernels_names,
            dump_custom_program,
            compiler_options.c_str(),
            single_kernel_name.c_str(),
            enable_parallelisation,
            engine_log.c_str(),
            sources_dumps_dir.c_str(),
            static_cast<int16_t>(priority_mode),
            static_cast<int16_t>(throttle_mode),
            enable_memory_pool
        };
    }
};

/// @brief Information about the engine properties and capabilities.
/// @details Look into @ref ::cldnn_engine_info for details.
using engine_info = ::cldnn_engine_info;

/// @brief Represents clDNN engine object.
struct engine
{
    /// @brief Constructs @p OpenCL engine
    engine(const engine_configuration& configuration = engine_configuration())
        :engine(engine_types::ocl, 0, configuration)
    {}

    /// @brief Construct engine of the specified @p type, @p engine_num, and @p configuration options.
    /// @param[in] type Engine type @ref cldnn_engine_type. Only OCL engine is supported.
    /// @param[in] engine_num Engine index. Should be 0.
    /// @param[in] configuration Pointer to engine configuration options.
    engine(engine_types type, uint32_t engine_num, const engine_configuration& configuration = engine_configuration())
        :_impl(check_status<::cldnn_engine>("failed to create engine", [&](status_t* status)
              {
                  cldnn_engine_configuration conf = configuration;
                  return cldnn_create_engine(static_cast<int32_t>(type), engine_num, &conf, status);
              }))
    {}

    // TODO add move construction/assignment
    engine(const engine& other) :_impl(other._impl)
    {
        retain();
    }

    engine& operator=(const engine& other)
    {
        if (_impl == other._impl) return *this;
        release();
        _impl = other._impl;
        retain();
        return *this;
    }

    ~engine()
    {
        release();
    }

    friend bool operator==(const engine& lhs, const engine& rhs) { return lhs._impl == rhs._impl; }
    friend bool operator!=(const engine& lhs, const engine& rhs) { return !(lhs == rhs); }

    /// @brief Returns number of available engines of the particular @p type.
    static uint32_t engine_count(engine_types type)
    {
        return check_status<uint32_t>("engine_count failed", [=](status_t* status)
        {
            return cldnn_get_engine_count(static_cast<int32_t>(type), status);
        });
    }


    /// @brief Release pending memory allocated in OpenCL context.
    void release_pending_memory() const
    {
        check_status<void>("flush_memory failed", [=](status_t* status)
        {
            return cldnn_release_pending_memory(_impl, status);
        });
    }


    /// @brief Returns information about properties and capabilities for the engine.
    engine_info get_info() const
    {
        return check_status<engine_info>("engine_count failed", [=](status_t* status)
        {
            return cldnn_get_engine_info(_impl, status);
        });
    }

    /// @brief Returns total size of all resources allocated using given engine
    uint64_t get_max_used_device_memory_size() const
    {
        return check_status<uint64_t>("get total device memory failed", [=](status_t* status)
        {
            return cldnn_get_max_used_device_memory_size(_impl, status);
        });
    }

    /// @brief Returns total size of currently resources allocated using given engine
    uint64_t get_temp_used_device_memory_size() const
    {
        return check_status<uint64_t>("get device memory failed", [=](status_t* status)
        {
            return cldnn_get_temp_used_device_memory_size(_impl, status);
        });
    }

    /// @brief Returns type of the engine.
    engine_types get_type() const
    {
        return check_status<engine_types>("engine_count failed", [=](status_t* status)
        {
            return static_cast<engine_types>(cldnn_get_engine_type(_impl, status));
        });
    }

    /// @brief get C API engine handler.
    ::cldnn_engine get() const { return _impl; }

private:
    friend struct network;
    friend struct memory;
    friend struct event;
    engine(::cldnn_engine impl) : _impl(impl)
    {
        if (_impl == nullptr) throw std::invalid_argument("implementation pointer should not be null");
    }
    ::cldnn_engine _impl;

    void retain()
    {
        check_status<void>("retain engine failed", [=](status_t* status) { cldnn_retain_engine(_impl, status); });
    }
    void release()
    {
        check_status<void>("release engine failed", [=](status_t* status) { cldnn_release_engine(_impl, status); });
    }
};
CLDNN_API_CLASS(engine)

/// @}

/// @}

}
