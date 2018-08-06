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

#include <vector>
#include <cassert>
#include <iterator>
#include <cstring>
#include <string>

#include "meta_utils.hpp"


namespace cldnn {

/// @addtogroup cpp_api C++ API
/// @{

/// @cond CPP_HELPERS

/// @defgroup cpp_helpers Helpers
/// @{

template<typename T>
class mutable_array_ref
{
public:
    typedef size_t size_type;

    mutable_array_ref() :_data(nullptr), _size(0) {}
    mutable_array_ref(T& val) :_data(&val), _size(1) {}
    mutable_array_ref(T* data, size_t size) :_data(data), _size(size) {}

    template<size_t N>
    mutable_array_ref(T(&arr)[N]) : _data(arr), _size(N) {}

    mutable_array_ref(const mutable_array_ref& other) : _data(other._data), _size(other._size) {}

    mutable_array_ref& operator=(const mutable_array_ref& other)
    {
        if (this == &other)
            return *this;
        _data = other._data;
        _size = other._size;
        return *this;
    }

    T* data() const { return _data; }
    size_t size() const { return _size; }
    bool empty() const { return _size == 0; }

#if defined(_SECURE_SCL) && (_SECURE_SCL > 0)
    typedef stdext::checked_array_iterator<T*> iterator;
    typedef stdext::checked_array_iterator<const T*> const_iterator;
    iterator begin() const { return stdext::make_checked_array_iterator(_data, _size); }
    iterator end() const { return stdext::make_checked_array_iterator(_data, _size, _size); }
    const_iterator cbegin() const { return stdext::make_checked_array_iterator(_data, _size); }
    const_iterator cend() const { return stdext::make_checked_array_iterator(_data, _size, _size); }
#else
    typedef T* iterator;
    typedef T* const_iterator;
    iterator begin() const { return _data; }
    iterator end() const { return _data + _size; }
    const_iterator cbegin() const { return _data; }
    const_iterator cend() const { return _data + _size; }
#endif


    T& operator[](size_t idx) const
    {
        assert(idx < _size);
        return _data[idx];
    }

    T& at(size_t idx) const
    {
        if (idx >= _size) throw std::out_of_range("idx");
        return _data[idx];
    }

    std::vector<T> vector() const { return std::vector<T>(_data, _data + _size); }
private:
    T* _data;
    size_t _size;
};

template<typename T>
class array_ref
{
public:
    typedef size_t size_type;

    array_ref() :_data(nullptr), _size(0) {}
    array_ref(const T& val) :_data(&val), _size(1) {}
    array_ref(const T* data, size_t size) :_data(data), _size(size) {}

    template<typename A>
    array_ref(const std::vector<T, A>& vec) : _data(vec.data()), _size(vec.size()) {}

    template<size_t N>
    array_ref(const T(&arr)[N]) : _data(arr), _size(N) {}

    array_ref(const mutable_array_ref<T>& other) : _data(other.data()), _size(other.size()){}

    array_ref(const array_ref& other) : _data(other._data), _size(other._size) {}

    array_ref& operator=(const array_ref& other)
    {
        if (this == &other)
            return *this;
        _data = other._data;
        _size = other._size;
        return *this;
    }

    const T* data() const { return _data; }
    size_t size() const { return _size; }
    bool empty() const { return _size == 0; }

#if defined(_SECURE_SCL) && (_SECURE_SCL > 0)
    typedef stdext::checked_array_iterator<const T*> iterator;
    typedef stdext::checked_array_iterator<const T*> const_iterator;
    iterator begin() const { return stdext::make_checked_array_iterator(_data, _size); }
    iterator end() const { return stdext::make_checked_array_iterator(_data, _size, _size); }
    const_iterator cbegin() const { return stdext::make_checked_array_iterator(_data, _size); }
    const_iterator cend() const { return stdext::make_checked_array_iterator(_data, _size, _size); }
#else
    typedef const T* iterator;
    typedef const T* const_iterator;
    iterator begin() const { return _data; }
    iterator end() const { return _data + _size; }
    const_iterator cbegin() const { return _data; }
    const_iterator cend() const { return _data + _size; }
#endif

    const T& operator[](size_t idx) const
    {
        assert(idx < _size);
        return _data[idx];
    }

    const T& at(size_t idx) const
    {
        if (idx >= _size) throw std::out_of_range("idx");
        return _data[idx];
    }

    std::vector<T> vector() const { return std::vector<T>(_data, _data + _size); }
private:
    const T* _data;
    size_t _size;
};

// NOTE: It seems that clang before version 3.9 has bug that treates non-member template function with deleted function
//       body as non-template or non-specializable (specializations are treated as redefinitions).
//template<typename Char> size_t basic_strlen(const Char* str) = delete;
template<typename Char> size_t basic_strlen(const Char*)
{
    static_assert(meta::always_false<Char>::value, "basic_strlen<Char> for selected Char type is deleted.");
    return 0;
}

template<>
inline size_t basic_strlen(const char* str) { return std::strlen(str); }

template<>
inline size_t basic_strlen(const wchar_t* str) { return std::wcslen(str); }

template<typename Char>
class basic_string_ref
{
public:
    typedef const Char* iterator;
    typedef const Char* const_iterator;
    typedef size_t size_type;

private:
    const Char* _data;
    size_t _size;
public:
    basic_string_ref() :_data(nullptr), _size(0) {}
    basic_string_ref(const Char* str) : _data(str), _size(basic_strlen(str)) {}

    template<typename T, typename A>
    basic_string_ref(const std::basic_string<Char, T, A>& str) : _data(str.c_str()), _size(str.size()) {}

    basic_string_ref(const basic_string_ref& other) : _data(other._data), _size(other._size) {}

    basic_string_ref& operator=(const basic_string_ref& other)
    {
        if (this == &other)
            return *this;
        _data = other._data;
        _size = other._size;
        return *this;
    }

    const Char* data() const { return _data; }
    const Char* c_str() const { return _data; }
    size_t size() const { return _size; }
    size_t length() const { return _size; }
    bool empty() const { return _size == 0; }

    iterator begin() const { return _data; }
    iterator end() const { return _data + _size; }
    const_iterator cbegin() const { return begin(); }
    const_iterator cend() const { return end(); }

    const Char& operator[](size_t idx)
    {
        assert(idx < _size);
        return _data[idx];
    }

    std::basic_string<Char> str() const { return std::basic_string<Char>(_data, _size); }
    operator std::basic_string<Char>() const { return str(); }
};

typedef basic_string_ref<char> string_ref;
typedef basic_string_ref<wchar_t> wstring_ref;

/// @}

/// @endcond

/// @}
}
