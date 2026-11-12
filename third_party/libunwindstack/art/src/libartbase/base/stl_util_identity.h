/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_LIBARTBASE_BASE_STL_UTIL_IDENTITY_H_
#define ART_LIBARTBASE_BASE_STL_UTIL_IDENTITY_H_

namespace art {

// Use to suppress type deduction for a function argument.
// See std::identity<> for more background:
// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2005/n1856.html#20.2.2 - move/forward helpers
//
// e.g. "template <typename X> void bar(identity<X>::type foo);
//     bar(5); // compilation error
//     bar<int>(5); // ok
// or "template <typename T> void foo(T* x, typename Identity<T*>::type y);
//     Base b;
//     Derived d;
//     foo(&b, &d);  // Use implicit Derived* -> Base* conversion.
// If T was deduced from both &b and &d, there would be a mismatch, i.e. deduction failure.
template <typename T>
struct Identity {
  using type = T;
};

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_STL_UTIL_IDENTITY_H_
