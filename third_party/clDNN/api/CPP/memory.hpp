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
#include <cstdint>
#include "cldnn_defs.h"
#include "compounds.h"
#include "layout.hpp"
#include "engine.hpp"
#include <memory>
#include <iterator>

namespace cldnn
{

/// @addtogroup cpp_api C++ API
/// @{

/// @defgroup cpp_memory Memory description and management
/// @{

template<typename T> struct pointer;

namespace details { struct memory_c_to_cpp_converter; }

/// @brief Represents buffer with particular @ref layout.
/// @details Usually allocated by @ref engine except cases when attached to user-allocated buffer.
struct memory
{
    friend struct data;
    friend struct mutable_data;
    friend struct network;
    friend struct network_output;
    friend struct details::memory_c_to_cpp_converter;

    /// Allocate memory on @p engine using specified @p layout
    static memory allocate(const engine& engine, const layout& layout)
    {
        size_t size = layout.bytes_count();
        if (size == 0) throw std::invalid_argument("size should be more than 0");
        return check_status<cldnn_memory>("memory allocation failed", [&](status_t* status)
        {
            return cldnn_allocate_memory(engine.get(), layout, status);
        });
    }

    /// Create memory object attached to the buffer allocated by user.
    /// @param ptr  The pointer to user allocated buffer.
    /// @param size Size (in bytes) of the buffer. Should be equal to @p layout.data_size()
    /// @note User is responsible for buffer deallocation. Buffer lifetime should be bigger than lifetime of the memory object.
    template<typename T>
    static memory attach(const cldnn::layout& layout, T* ptr, size_t size)
    {
        if (!ptr) throw std::invalid_argument("pointer should not be null");
        size_t data_size = size * sizeof(T);
        if (data_size != layout.bytes_count()) {
            std::string err_str("buffer size mismatch - input size " + std::to_string(data_size) + " layout size " + std::to_string(layout.bytes_count()));
            throw std::invalid_argument(err_str);
        }
        
        return check_status<cldnn_memory>("memory attach failed", [&](status_t* status)
        {
            return cldnn_attach_memory(layout, ptr, data_size, status);
        });
    }

    memory(const memory& other)
        :_impl(other._impl), _layout(other._layout)
        ,_size(other._size), _count(other._count)
    {
        retain();
    }

    memory& operator=(const memory& other)
    {
        if (_impl == other._impl) return *this;
        release();
        _impl = other._impl;
        _layout = other._layout;
        _size = other._size;
        _count = other._count;
        retain();
        return *this;
    }

    ~memory()
    {
        release();
    }

    friend bool operator==(const memory& lhs, const memory& rhs) { return lhs._impl == rhs._impl; }
    friend bool operator!=(const memory& lhs, const memory& rhs) { return !(lhs == rhs); }

    /// number of elements of _layout.data_type stored in memory
    size_t count() const { return _count; }

    /// number of bytes used by memory
    size_t size() const { return _size; }

    /// Associated @ref layout
    const layout& get_layout() const { return _layout; }

    /// Test if memory is allocated by @p engine
    bool is_allocated_by(const engine& engine) const
    {
        auto my_engine = check_status<cldnn_engine>("get memory engine failed", [&](status_t* status)
        {
            return cldnn_get_memory_engine(_impl, status);
        });
        return my_engine == engine.get();
    }

    bool is_the_same_buffer(const memory& other) const
    {
        return check_status<bool>("checking if two memories refers to the same buffer failed", [&](status_t* status)
        {
            return cldnn_is_the_same_buffer(_impl, other._impl, status) != 0;
        });
    }

    /// Creates the @ref pointer object to get an access memory data
    template<typename T> friend struct cldnn::pointer;
    template<typename T> cldnn::pointer<T> pointer() const;

    /// C API memory handle
    cldnn_memory get() const { return _impl; }

private:
    friend struct engine;
    cldnn_memory _impl;
    layout _layout;
    size_t _size;
    size_t _count;

    static layout get_layout_impl(cldnn_memory mem)
    {
        if (!mem) throw std::invalid_argument("mem");

        return check_status<layout>("get memory layout failed", [=](status_t* status)
        {
            return cldnn_get_memory_layout(mem, status);
        });
    }

    memory(cldnn_memory data)
        :_impl(data), _layout(get_layout_impl(data))
        , _size(_layout.bytes_count()), _count(_layout.count())
    {
        if (_impl == nullptr)
            throw std::invalid_argument("implementation pointer should not be null");
    }

    void retain()
    {
        check_status<void>("retain memory failed", [=](status_t* status) { cldnn_retain_memory(_impl, status); });
    }
    void release()
    {
        check_status<void>("release memory failed", [=](status_t* status) { cldnn_release_memory(_impl, status); });
    }

    template<typename T>
    T* lock() const
    {
        if (data_type_traits::align_of(_layout.data_type) % alignof(T) != 0)
        {
            throw std::logic_error("memory data type alignment do not match");
        }
        return check_status<T*>("memory lock failed", [=](status_t* status) { return static_cast<T*>(cldnn_lock_memory(_impl, status)); });
    }

    void unlock() const
    {
        check_status<void>("memory unlock failed", [=](status_t* status) { return cldnn_unlock_memory(_impl, status); });
    }
};

namespace details
{
//we need this hackish structure as long as primitives (which are used internally) use c++ api 'memory' (see: cldnn::data)
struct memory_c_to_cpp_converter
{
    //does not retain @p c_mem
    static memory convert(cldnn_memory c_mem)
    {
        return memory{ c_mem };
    }
};
}

/// @brief Helper class to get an access @ref memory data
/// @details
/// This class provides an access to @ref memory data following RAII idiom and exposes basic C++ collection members.
/// @ref memory object is locked on construction of pointer and "unlocked" on descruction.
/// Objects of this class could be used in many STL utility functions like copy(), transform(), etc.
/// As well as in range-for loops.
template<typename T>
struct pointer
{
    /// @brief Constructs pointer from @ref memory and locks @c (pin) ref@ memory object.
    pointer(const memory& mem)
        : _mem(mem)
        , _size(_mem.size()/sizeof(T))
        , _ptr(_mem.lock<T>())
    {}

    /// @brief Unlocks @ref memory
    ~pointer() { _mem.unlock(); }

    /// @brief Copy construction.
    pointer(const pointer& other) : pointer(other._mem){}

    /// @brief Copy assignment.
    pointer& operator=(const pointer& other)
    {
        if (this->_mem != other._mem)
            do_copy(other._mem);
        return *this;
    }

    /// @brief Returns the number of elements (of type T) stored in memory
    size_t size() const { return _size; }

#if defined(_SECURE_SCL) && (_SECURE_SCL > 0)
    typedef stdext::checked_array_iterator<T*> iterator;
    typedef stdext::checked_array_iterator<const T*> const_iterator;

    iterator begin() & { return stdext::make_checked_array_iterator(_ptr, size()); }
    iterator end() & { return stdext::make_checked_array_iterator(_ptr, size(), size()); }

    const_iterator begin() const& { return stdext::make_checked_array_iterator(_ptr, size()); }
    const_iterator end() const& { return stdext::make_checked_array_iterator(_ptr, size(), size()); }
#else
    typedef T* iterator;
    typedef const T* const_iterator;
    iterator begin() & { return _ptr; }
    iterator end() & { return _ptr + size(); }
    const_iterator begin() const& { return _ptr; }
    const_iterator end() const& { return _ptr + size(); }
#endif

    /// @brief Provides indexed access to pointed memory.
    T& operator[](size_t idx) const&
    {
        assert(idx < _size);
        return _ptr[idx];
    }

    /// @brief Returns the raw pointer to pointed memory.
    T* data() & { return _ptr; }
    /// @brief Returns the constant raw pointer to pointed memory
    const T* data() const& { return _ptr; }

    friend bool operator==(const pointer& lhs, const pointer& rhs) { return lhs._mem == rhs._mem; }
    friend bool operator!=(const pointer& lhs, const pointer& rhs) { return !(lhs == rhs); }

    // do not use this class as temporary object
    // ReSharper disable CppMemberFunctionMayBeStatic, CppMemberFunctionMayBeConst
    /// Prevents to use pointer as temporary object
    void data() && {}
    /// Prevents to use pointer as temporary object
    void begin() && {}
    /// Prevents to use pointer as temporary object
    void end() && {}
    /// Prevents to use pointer as temporary object
    void operator[](size_t idx) && {}
    // ReSharper restore CppMemberFunctionMayBeConst, CppMemberFunctionMayBeStatic

private:
    memory _mem;
    size_t _size;
    T* _ptr;

    //TODO implement exception safe code.
    void do_copy(const memory& mem)
    {
        auto ptr = mem.lock<T>();
        _mem.unlock();
        _mem = mem;
        _size = _mem.size() / sizeof(T);
        _ptr = ptr;
    }
};

#ifndef DOXYGEN_SHOULD_SKIP_THIS
template <typename T>
pointer<T> memory::pointer() const { return cldnn::pointer<T>(*this); }
#endif

/// @}

/// @}

}
