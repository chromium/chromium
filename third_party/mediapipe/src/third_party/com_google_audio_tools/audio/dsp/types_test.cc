/*
 * Copyright 2020-2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "audio/dsp/types.h"

#include <complex>
#include <deque>
#include <type_traits>
#include <vector>

#include "absl/memory/memory.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace audio_dsp {
namespace {

using ::std::complex;
using ::testing::ElementsAre;

template <typename Type1, typename Type2>
void TypesMatch() {
  // The preprocessor parses EXPECT_TRUE(std::is_same<Type1, Type2>::value)
  // incorrectly as a two-argument macro function because of the comma in
  // "Type1, Type2". So we write it in two lines.
  bool match = std::is_same<Type1, Type2>::value;
  EXPECT_TRUE(match) << "Type1 (" << typeid(Type1).name()
      << ") does not match Type2 (" << typeid(Type2).name() << ").";
}

TEST(TypesTest, IsComplex) {
  EXPECT_FALSE(IsComplex<char>::Value);
  EXPECT_FALSE(IsComplex<int>::Value);
  EXPECT_FALSE(IsComplex<size_t>::Value);
  EXPECT_FALSE(IsComplex<float>::Value);
  EXPECT_FALSE(IsComplex<double>::Value);
  EXPECT_FALSE(IsComplex<long double>::Value);
  EXPECT_TRUE(IsComplex<complex<float>>::Value);
  EXPECT_TRUE(IsComplex<complex<double>>::Value);
  EXPECT_TRUE(IsComplex<complex<long double>>::Value);
}

TEST(TypesTest, HoldsComplex) {
  EXPECT_FALSE(HoldsComplex<std::vector<int>>::Value);
  EXPECT_FALSE(HoldsComplex<std::vector<float>>::Value);
  EXPECT_FALSE(HoldsComplex<std::vector<double>>::Value);
  EXPECT_FALSE(HoldsComplex<std::deque<int>>::Value);
  EXPECT_FALSE(HoldsComplex<std::deque<float>>::Value);
  EXPECT_FALSE(HoldsComplex<std::deque<double>>::Value);
  EXPECT_TRUE(HoldsComplex<std::vector<complex<float>>>::Value);
  EXPECT_TRUE(HoldsComplex<std::vector<complex<double>>>::Value);
  EXPECT_TRUE(HoldsComplex<std::deque<complex<float>>>::Value);
  EXPECT_TRUE(HoldsComplex<std::deque<complex<double>>>::Value);
}

TEST(TypesTest, RealType) {
  TypesMatch<RealType<char>::Type, char>();
  TypesMatch<RealType<int>::Type, int>();
  TypesMatch<RealType<size_t>::Type, size_t>();
  TypesMatch<RealType<float>::Type, float>();
  TypesMatch<RealType<double>::Type, double>();
  TypesMatch<RealType<long double>::Type, long double>();
  TypesMatch<RealType<complex<float>>::Type, float>();
  TypesMatch<RealType<complex<double>>::Type, double>();
  TypesMatch<RealType<complex<long double>>::Type, long double>();
}

TEST(TypesTest, ComplexType) {
  TypesMatch<ComplexType<float>::Type, complex<float>>();
  TypesMatch<ComplexType<double>::Type, complex<double>>();
  TypesMatch<ComplexType<long double>::Type, complex<long double>>();
  TypesMatch<ComplexType<complex<float>>::Type, complex<float>>();
  TypesMatch<ComplexType<complex<double>>::Type, complex<double>>();
  TypesMatch<ComplexType<complex<long double>>::Type, complex<long double>>();
}

TEST(TypesTest, FloatPromotion) {
  TypesMatch<double, FloatPromotion<char>::Type>();
  TypesMatch<double, FloatPromotion<int>::Type>();
  TypesMatch<double, FloatPromotion<size_t>::Type>();
  TypesMatch<float, FloatPromotion<float>::Type>();
  TypesMatch<double, FloatPromotion<double>::Type>();
  TypesMatch<long double, FloatPromotion<long double>::Type>();
  TypesMatch<complex<float>, FloatPromotion<complex<float>>::Type>();
  TypesMatch<complex<double>, FloatPromotion<complex<double>>::Type>();
  TypesMatch<complex<long double>,
      FloatPromotion<complex<long double>>::Type>();
}

TEST(TypesTest, DerefIfPointer) {
  {
    int x = 5;
    const int* y = &x;
    const int& z = x;
    // Arg: int.
    EXPECT_EQ(DerefIfPointer(x), 5);
    TypesMatch<int&, decltype(DerefIfPointer(x))>();
    // Arg: const int*.
    EXPECT_EQ(DerefIfPointer(y), 5);
    TypesMatch<const int&, decltype(DerefIfPointer(y))>();
    // Arg: const int&.
    EXPECT_EQ(DerefIfPointer(z), 5);
    TypesMatch<const int&, decltype(DerefIfPointer(z))>();
  }
  {
    auto x = absl::make_unique<std::vector<int>>();
    const std::vector<int>* y = x.get();
    x->assign({1, 2, 3});
    // Arg: unique_ptr<vector<int>>.
    EXPECT_THAT(DerefIfPointer(x), ElementsAre(1, 2, 3));
    TypesMatch<std::vector<int>&, decltype(DerefIfPointer(x))>();
    // Arg: vector<int>*.
    EXPECT_THAT(DerefIfPointer(x.get()), ElementsAre(1, 2, 3));
    TypesMatch<std::vector<int>&, decltype(DerefIfPointer(x.get()))>();
    // Arg: vector<int>&.
    EXPECT_THAT(DerefIfPointer(*x), ElementsAre(1, 2, 3));
    TypesMatch<std::vector<int>&, decltype(DerefIfPointer(*x))>();
    // Arg: const vector<int>*.
    EXPECT_THAT(DerefIfPointer(y), ElementsAre(1, 2, 3));
    TypesMatch<const std::vector<int>&, decltype(DerefIfPointer(y))>();
    // Arg: const vector<int>&.
    EXPECT_THAT(DerefIfPointer(*y), ElementsAre(1, 2, 3));
    TypesMatch<const std::vector<int>&, decltype(DerefIfPointer(*y))>();
    // Arg: vector<int>&&.
    EXPECT_THAT(DerefIfPointer(std::move(*x)), ElementsAre(1, 2, 3));
    TypesMatch<std::vector<int>&&, decltype(DerefIfPointer(std::move(*x)))>();
  }
}

}  // namespace
}  // namespace audio_dsp
