/*
// Copyright (c) 2017 Intel Corporation
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

#include <type_traits>

namespace cldnn
{

struct primitive;

namespace meta
{

//helper struct to tell wheter type T is any of given types U...
//termination case when U... is empty -> return std::false_type
template <class T, class... U>
struct is_any_of : public std::false_type {};

//helper struct to tell whether type is any of given types (U, Rest...)
//recurrence case when at least one type U is present -> returns std::true_type if std::same<T, U>::value is true, otherwise call is_any_of<T, Rest...> recurrently
template <class T, class U, class... Rest>
struct is_any_of<T, U, Rest...> : public std::conditional<std::is_same<T, U>::value, std::true_type, is_any_of<T, Rest...>>::type {};

template <class T>
struct always_false : public std::false_type {};

template <typename Ty, Ty Val>
struct always_false_ty_val : public std::false_type {};

template <typename Ty, Ty ... Vals>
struct val_tuple {};

template <bool... Values>
struct all : public std::true_type {};

template <bool Val, bool... Values>
struct all<Val, Values...> : public std::integral_constant<bool, Val && all<Values...>::value> {};

}
}