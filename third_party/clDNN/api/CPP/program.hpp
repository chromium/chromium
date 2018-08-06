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
#include "topology.hpp"
#include "engine.hpp"
#include <iostream>

#include <memory>

namespace cldnn
{

/// @addtogroup cpp_api C++ API
/// @{

/// @defgroup cpp_program Program compilation
/// @{

/// @brief Represents user-provided program build option type.
enum class build_option_type
{
    /// @brief Allow primitives fusing during program build (default: false).
    fusing = cldnn_build_option_fusing,

    /// @brief Enable implicit reordering for user inputs (default: false).
    optimize_data = cldnn_build_option_optimize_data,

    /// @brief Enable debug mode (default: false).
    /// @details This option enforce all program primitives to be accessible as outputs.
    debug = cldnn_build_option_debug,

    /// @brief User selected list of program outputs.
    outputs = cldnn_build_option_outputs,

    /// @brief Tuning config (default: Tuning is disabled).
    /// @details The tuner will automatically find the optimal kernel/config for each node in the graph,
    /// by running multiple implementations and configurations per node and storing the optimal one in cache.
    /// Expect long execution time in the first run.
    /// After the first run a cache with the tuning results will be created in the path provided.
    /// This cache will be used in the next runs.
    tuning_config = cldnn_build_option_tuning_config,

    /// @brief Specifies a directory to which stages of network compilation should be dumped. (default: empty, i.e. no dumping)
    graph_dumps_dir = cldnn_build_option_graph_dumps_dir
};

/// @brief Tuning mode.
enum class tuning_mode
{
    /// @brief Tuning is disabled.
    tuning_disabled = cldnn_tuning_disabled,

    /// @brief Tuning using the cached data (no on-line tuning for non-existing data).
    tuning_use_cache = cldnn_tuning_use_cache,

    /// @brief Tuning using the cached data if exist, tune and update cache otherwise.
    tuning_tune_and_cache = cldnn_tuning_tune_and_cache
};

/// @brief Tuning configuration.
struct tuning_config_options
{
    tuning_mode mode;
    std::string cache_file_path;

    tuning_config_options() :
        mode(tuning_mode::tuning_disabled),
        cache_file_path("")
    {}
};

/// @brief Represents user-provided program build option.
struct build_option
{
    /// @brief Allow primitives fusing during program build (default: false).
    static std::shared_ptr<const build_option> fusing(bool enable = false);

    /// @brief Enable implicit reordering for user inputs (default: false).
    static std::shared_ptr<const build_option> optimize_data(bool enable = false);

    /// @brief Enable debug mode (default: false).
    /// @details This option enforce all program primitives to be accessible as outputs.
    static std::shared_ptr<const build_option> debug(bool enable = false);

    /// @brief User selected list of program outputs.
    static std::shared_ptr<const build_option> outputs(const std::vector<primitive_id>& outs);

    /// @brief Tuning configuration (default: false).
    /// @details This option will automatically find the optimal kernel/config for each node in the graph,
    /// by running multiple implementations and configurations per node and storing the optimal one in cache.
    /// Expect long execution time in the first run (unless the cache only mode is enabled).
    /// After the first run a cache with the tuning results will be created in the path provided.
    /// This cache will be used in the next runs.
    static std::shared_ptr<const build_option> tuning_config(const tuning_config_options& config = tuning_config_options());

    /// @brief Specifies a directory to which stages of network compilation should be dumped (default: empty, i.e. no dumping)
    static std::shared_ptr<const build_option> graph_dumps_dir(const std::string& dir_path);

    virtual ~build_option() = default;

private:
    /// @brief Returns option type represented by this object.
    virtual build_option_type get_type() const = 0;

    /// @brief Returns option @ref ::cldnn_build_option::data represented by this object.
    virtual const void* get_data() const = 0;

    friend class build_options;
};

/// @brief @ref build_option specialization for boolean options.
template<build_option_type OptType>
struct build_option_bool : build_option
{
    /// @brief Constructs option.
    /// @param value Is option enabled.
    explicit build_option_bool(bool value) : _value(value ? 1 : 0) {}

    /// @brief Constructs from C API @ref ::cldnn_build_option.
    explicit build_option_bool(const cldnn_build_option& value)
        : _value(reinterpret_cast<uintptr_t>(value.data))
    {
        assert(value.type == static_cast<int32_t>(OptType));
    }

    /// @brief Is option enabled.
    bool enabled() const { return _value != 0; }
private:
    build_option_type get_type() const override { return OptType; }
    const void* get_data() const override { return reinterpret_cast<const void*>(_value); }
    uintptr_t _value;
};

/// @brief @ref build_option specialization for program outputs list.
struct build_option_outputs : build_option
{
    /// @brief The list of output ids (names)
    const std::vector<primitive_id> outputs;

    /// @brief Constructs option.
    /// @param outs List of ouput ids (names)
    explicit build_option_outputs(const std::vector<primitive_id>& outs)
        : outputs(outs)
        , _ref_store(to_refs(outputs))
        , _outputs_ref({ _ref_store.data(), _ref_store.size() })
    {}

    /// @brief Constructs from C API @ref ::cldnn_build_option.
    explicit build_option_outputs(const cldnn_build_option& value)
        : build_option_outputs(make_outputs_from_ref(value))
    {
        assert(value.type == static_cast<int32_t>(cldnn_build_option_outputs));
    }

private:
    /// @brief Returns build_option_type::outputs.
    build_option_type get_type() const override { return build_option_type::outputs; }
    /// @brief Returns pointer to @ref cldnn_primitive_is_arr
    const void* get_data() const override { return &_outputs_ref; }

    build_option_outputs(const build_option_outputs& other) = delete;
    build_option_outputs& operator=(const build_option_outputs& other) = delete;

    const std::vector<cldnn_primitive_id> _ref_store;
    const cldnn_primitive_id_arr _outputs_ref;

    static std::vector<cldnn_primitive_id> to_refs(const std::vector<primitive_id>& stor)
    {
        std::vector<cldnn_primitive_id> result(stor.size());
        for (size_t i = 0; i < stor.size(); i++)
        {
            result[i] = stor[i].c_str();
        }
        return result;
    }

    static std::vector<primitive_id> make_outputs_from_ref(const cldnn_build_option& value)
    {
        if (value.type != cldnn_build_option_outputs) throw std::invalid_argument("option type does not match: should be 'output'");
        if (value.data == nullptr) throw std::invalid_argument("output data is empty");
        auto refs = reinterpret_cast<const cldnn_primitive_id_arr*>(value.data);
        std::vector<primitive_id> result;
        result.reserve(refs->size);
        for (decltype(refs->size) i = 0; i < refs->size; i++)
        {
            result.push_back(refs->data[i]);
        }
        return result;
    }
};

/// @brief @ref build_option specialization for tuning config.
struct build_option_tuning_config : build_option
{
    /// @brief Tuning configuration
    const tuning_config_options config;

    /// @brief Constructs tuning config build option.
    /// @param tuning_config Configuration for the tuning.
    explicit build_option_tuning_config(const tuning_config_options& tuning_config) :
        config(tuning_config),
        config_ref({ static_cast<int32_t>(config.mode), config.cache_file_path.c_str() })
    {}

    /// @brief Constructs tuning config build option from C API @ref ::cldnn_build_option.
    explicit build_option_tuning_config(const cldnn_build_option& value)
        : build_option_tuning_config(make_config_from_ref(value))
    {
        assert(value.type == static_cast<int32_t>(cldnn_build_option_tuning_config));
    }

private:
    /// @brief Returns build_option_type::tuning_config.
    build_option_type get_type() const override { return build_option_type::tuning_config; }
    /// @brief Returns pointer to @ref cldnn_tuning_config
    const void* get_data() const override { return &config_ref; }

    build_option_tuning_config(const build_option_tuning_config& other) = delete;
    build_option_tuning_config& operator=(const build_option_tuning_config& other) = delete;

    const cldnn_tuning_config config_ref;

    static tuning_config_options make_config_from_ref(const cldnn_build_option& value)
    {
        if (value.type != cldnn_build_option_tuning_config) throw std::invalid_argument("option type does not match: should be 'tuning_config'");
        if (value.data == nullptr) throw std::invalid_argument("Tuning config data is empty");
        auto refs = reinterpret_cast<const cldnn_tuning_config*>(value.data);
        tuning_config_options result;
        result.mode = tuning_mode(refs->mode);
        result.cache_file_path = std::string(refs->cache_file_path);
        return result;
    }
};

/// @brief @ref build_option specialization for selecting a directory.
template<build_option_type OptType>
struct build_option_directory : build_option
{
    const std::string directory_path;

    /// @brief Constructs option.
    /// @param outs List of ouput ids (names)
    explicit build_option_directory(const std::string& dir_path)
        : directory_path(dir_path)
    {}

    /// @brief Constructs from C API @ref ::cldnn_build_option.
    explicit build_option_directory(const cldnn_build_option& value)
        : directory_path(from_c_value(value))
    {}

private:
    /// @brief Returns build_option_type::graph_dumps_dir.
    build_option_type get_type() const override { return build_option_type::graph_dumps_dir; }
    /// @brief Returns null terminated C string.
    const void* get_data() const override { return (directory_path.empty() ? nullptr : directory_path.c_str()); }

    build_option_directory(const build_option_directory& other) = delete;
    build_option_directory& operator=(const build_option_directory& other) = delete;

    static std::string from_c_value(const cldnn_build_option& value)
    {
        if (value.type != static_cast<int32_t>(OptType))
            throw std::invalid_argument("option type does not match");
        if (value.data == nullptr)
            return{};

        return{ static_cast<const char*>(value.data) };
    }
};

namespace detail
{
    /// @brief Helper template to convert @ref build_option_type value to particular @ref build_option class.
    template<build_option_type OptType>
    struct build_option_traits
    {
        /// @brief @ref build_option object type which represents the particular @p OptType.
        typedef build_option object_type;
        /// @brief Make default @ref build_option corresponding @p OptType
        static std::shared_ptr<const build_option> make_default();
        /// @brief Make @ref build_option from C API @ref ::cldnn_build_option
        static std::shared_ptr<const build_option> make_option(const cldnn_build_option& option);
    };

#ifndef DOXYGEN_SHOULD_SKIP_THIS
    template<> struct build_option_traits<build_option_type::fusing>
    {
        typedef build_option_bool<build_option_type::fusing> object_type;
        static std::shared_ptr<const build_option> make_default() { return build_option::fusing(); }
        static std::shared_ptr<const build_option> make_option(const cldnn_build_option& option)
        {
            assert(option.type == cldnn_build_option_fusing);
            return std::make_shared<object_type>(option);
        }
    };
    template<> struct build_option_traits<build_option_type::optimize_data>
    {
        typedef build_option_bool<build_option_type::optimize_data> object_type;
        static std::shared_ptr<const build_option> make_default() { return build_option::optimize_data(); }
        static std::shared_ptr<const build_option> make_option(const cldnn_build_option& option)
        {
            assert(option.type == cldnn_build_option_optimize_data);
            return std::make_shared<object_type>(option);
        }
    };
    template<> struct build_option_traits<build_option_type::debug>
    {
        typedef build_option_bool<build_option_type::debug> object_type;
        static std::shared_ptr<const build_option> make_default() { return build_option::debug(); }
        static std::shared_ptr<const build_option> make_option(const cldnn_build_option& option)
        {
            assert(option.type == cldnn_build_option_debug);
            return std::make_shared<object_type>(option);
        }
    };
    template<> struct build_option_traits<build_option_type::outputs>
    {
        typedef build_option_outputs object_type;
        static std::shared_ptr<const build_option> make_default() { return build_option::outputs({}); }
        static std::shared_ptr<const build_option> make_option(const cldnn_build_option& option)
        {
            assert(option.type == cldnn_build_option_outputs);
            return std::make_shared<object_type>(option);
        }
    };
    template<> struct build_option_traits<build_option_type::tuning_config>
    {
        typedef build_option_tuning_config object_type;
        static std::shared_ptr<const build_option> make_default() { return build_option::tuning_config(); }
        static std::shared_ptr<const build_option> make_option(const cldnn_build_option& option)
        {
            assert(option.type == cldnn_build_option_tuning_config);
            return std::make_shared<object_type>(option);
        }
    };
    template<> struct build_option_traits<build_option_type::graph_dumps_dir>
    {
        typedef build_option_directory<build_option_type::graph_dumps_dir> object_type;
        static std::shared_ptr<const build_option> make_default() { return build_option::graph_dumps_dir({}); }
        static std::shared_ptr<const build_option> make_option(const cldnn_build_option& option)
        {
            assert(option.type == cldnn_build_option_graph_dumps_dir);
            return std::make_shared<object_type>(option);
        }
    };
#endif
} // namespace detail

#ifndef DOXYGEN_SHOULD_SKIP_THIS
inline std::shared_ptr<const build_option> build_option::fusing(bool enable)
{
    return std::make_shared<build_option_bool<build_option_type::fusing>>(enable);
}

inline std::shared_ptr<const build_option> build_option::optimize_data(bool enable)
{
    return std::make_shared<build_option_bool<build_option_type::optimize_data>>(enable);
}

inline std::shared_ptr<const build_option> build_option::debug(bool enable)
{
    return std::make_shared<build_option_bool<build_option_type::debug>>(enable);
}

inline std::shared_ptr<const build_option> build_option::outputs(const std::vector<primitive_id>& outs)
{
    return std::make_shared<build_option_outputs>(outs);
}

inline std::shared_ptr<const build_option> build_option::tuning_config(const tuning_config_options& config)
{
    return std::make_shared<build_option_tuning_config>(config);
}

inline std::shared_ptr<const build_option> build_option::graph_dumps_dir(const std::string& dir_path)
{
    return std::make_shared<build_option_directory<build_option_type::graph_dumps_dir>>(dir_path);
}
#endif

/// @brief Represents program build options list.
class build_options
{
public:
    /// @brief Adds or replace option to the options list
    void set_option(std::shared_ptr<const build_option> opt)
    {
        add_or_replace_option(opt);
    }

    /// @brief Adds or replace options to the options list
    template<typename ...Args>
    void set_option(std::shared_ptr<const build_option> opt, Args... args)
    {
        add_or_replace_option(opt);
        set_option(args...);
    }

    /// @brief Constructs build options list from its arguments.
    template<typename ...Args>
    build_options(Args... args)
    {
        set_option(args...);
    }

    /// @brief Constructs build options list from C API ::cldnn_build_options.
    build_options(array_ref<cldnn_build_option> options)
    {
        for (auto& o : options)
        {
            _options.emplace_back(make_option(o));
        }
    }

    /// @brief Returns program build option for @p OptType
    template<build_option_type OptType>
    std::shared_ptr<const typename detail::build_option_traits<OptType>::object_type>
        get() const
    {
        using T = typename detail::build_option_traits<OptType>::object_type;
        for (auto& option : _options)
        {
            if (option->get_type() == OptType)
                return std::static_pointer_cast<const T>(option);
        }
        return std::static_pointer_cast<const T>(detail::build_option_traits<OptType>::make_default());
    }

private:
    friend struct program;
    std::vector<std::shared_ptr<const build_option>> _options;
    void set_option(void) {}

    /// @brief Returns C API compatible list of ::cldnn_build_option
    std::vector<cldnn_build_option> get_refs() const
    {
        std::vector<cldnn_build_option> result;
        for (auto& o : _options)
        {
            result.push_back({ static_cast<int32_t>(o->get_type()), o->get_data() });
        }
        return result;
    }

    void add_or_replace_option(std::shared_ptr<const build_option> opt)
    {
        for (auto& p : _options)
        {
            if (p->get_type() == opt->get_type())
            {
                p = opt;
                return;
            }
        }
        _options.push_back(opt);
    }

    static std::shared_ptr<const build_option> make_option(const cldnn_build_option& option)
    {
        switch (option.type)
        {
        case cldnn_build_option_fusing:
            return  detail::build_option_traits<build_option_type::fusing>::make_option(option);
        case cldnn_build_option_optimize_data:
            return detail::build_option_traits<build_option_type::optimize_data>::make_option(option);
        case cldnn_build_option_debug:
            return detail::build_option_traits<build_option_type::debug>::make_option(option);
        case cldnn_build_option_outputs:
            return detail::build_option_traits<build_option_type::outputs>::make_option(option);
        case cldnn_build_option_tuning_config:
            return detail::build_option_traits<build_option_type::tuning_config>::make_option(option);
        case cldnn_build_option_graph_dumps_dir:
            return detail::build_option_traits<build_option_type::graph_dumps_dir>::make_option(option);
        default: throw std::out_of_range("unsupported build option type");
        }
    }
};

/// @brief Compiled program build from @ref topology by @ref engine
struct program
{
    friend struct network;

public:
    /// @brief Builds executable program based on user-defined @p topology by specified @p engine.
    /// @param[in] engine The engine which will be used to build the program.
    /// @param[in] topology The user-defined topology on which the network will be based.
    /// @param[in] options Program build options. See @ref build_option and @ref build_options for details.
    program(engine const& engine, topology const& topology, build_options const& options = build_options())
        :_impl(check_status<cldnn_program>("program creation failed", [&](status_t* status)
            {
                auto options_refs = options.get_refs();
                return cldnn_build_program(engine.get(), topology.get(), options_refs.data(), options_refs.size(), status);
            }))
    {}

    /// @brief Retains the C API @ref cldnn_program handler stored in @p other.
    program(program const& other)
        :_impl(other._impl)
    {
        retain();
    }

    /// @brief Dereferences the counter of the underlying C API @ref cldnn_program handler.
    ~program()
    {
        release();
    }

    /// @brief Assigns new value by releasing previously referenced C API @ref cldnn_program handler and retaining the one referenced by @p other.
    program& operator=(const program& other)
    {
        if (_impl == other._impl) return *this;
        release();
        _impl = other._impl;
        retain();
        return *this;
    }

    /// @brief Checks whether @p lhs and @p rhs reference the same C API @ref cldnn_program handler
    friend bool operator==(const program& lhs, const program& rhs) { return lhs._impl == rhs._impl; }
    /// @brief Checks whether @p lhs and @p rhs reference different C API @ref cldnn_program handlers
    friend bool operator!=(const program& lhs, const program& rhs) { return !(lhs == rhs); }

    /// @brief Returns wrapped C API @ref cldnn_program handler.
    ::cldnn_program get() const { return _impl; }

private:

    ::cldnn_program _impl;

    program(::cldnn_program impl) : _impl(impl)
    {
        if (_impl == nullptr)
            throw std::invalid_argument("implementation pointer should not be null");
    }

    void retain()
    {
        check_status<void>("retain topology failed", [=](status_t* status) { cldnn_retain_program(_impl, status); });
    }
    void release()
    {
        check_status<void>("retain topology failed", [=](status_t* status) { cldnn_release_program(_impl, status); });
    }
};
/// @}
/// @}
}
