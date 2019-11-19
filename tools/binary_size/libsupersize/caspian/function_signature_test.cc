// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/binary_size/libsupersize/caspian/function_signature.h"

#include <string>
#include <string_view>
#include <tuple>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"

namespace {
std::tuple<std::string, std::string, std::string> PrettyDebug(
    std::tuple<std::string_view, std::string_view, std::string_view> tuple) {
  return std::make_tuple(std::string(std::get<0>(tuple)),
                         std::string(std::get<1>(tuple)),
                         std::string(std::get<2>(tuple)));
}

TEST(AnalyzeTest, StringSplit) {
  std::string input = "a//b/cd";
  std::vector<std::string_view> expected_output = {"a", "", "b", "cd"};
  EXPECT_EQ(expected_output, caspian::SplitBy(input, '/'));

  input = "a";
  expected_output = {"a"};
  EXPECT_EQ(expected_output, caspian::SplitBy(input, '/'));

  input = "";
  expected_output = {""};
  EXPECT_EQ(expected_output, caspian::SplitBy(input, '/'));

  input = "/";
  expected_output = {"", ""};
  EXPECT_EQ(expected_output, caspian::SplitBy(input, '/'));
}

TEST(AnalyzeTest, FindLastCharOutsideOfBrackets) {
  EXPECT_EQ(caspian::FindLastCharOutsideOfBrackets("(a)a", 'a'), 3u);
  EXPECT_EQ(caspian::FindLastCharOutsideOfBrackets("abc(a)a", 'a'), 6u);
  EXPECT_EQ(caspian::FindLastCharOutsideOfBrackets("(b)aaa", 'b'),
            std::string::npos);
  EXPECT_EQ(caspian::FindLastCharOutsideOfBrackets("", 'b'), std::string::npos);

  EXPECT_EQ(caspian::FindLastCharOutsideOfBrackets("a(a)a", 'a', 4u), 0u);
  EXPECT_EQ(caspian::FindLastCharOutsideOfBrackets("a<<>", '<', 4u), 2u);
}

TEST(AnalyzeTest, FindParameterListParen) {
  EXPECT_EQ(caspian::FindParameterListParen("a()"), 1u);
  EXPECT_EQ(
      caspian::FindParameterListParen(
          "bool foo::Bar<unsigned int, int>::Do<unsigned int>(unsigned int)"),
      50u);
  EXPECT_EQ(caspian::FindParameterListParen(
                "std::basic_ostream<char, std::char_traits<char> >& "
                "std::operator<< <std::char_traits<char> "
                ">(std::basic_ostream<char, std::char_traits<char> >&, char)"),
            92u);
}

TEST(AnalyzeTest, FindReturnValueSpace) {
  EXPECT_EQ(caspian::FindReturnValueSpace("bool a()", 6u), 4u);
  EXPECT_EQ(caspian::FindReturnValueSpace("operator delete(void*)", 15),
            std::string::npos);
  EXPECT_EQ(
      caspian::FindReturnValueSpace(
          "bool foo::Bar<unsigned int, int>::Do<unsigned int>(unsigned int)",
          50u),
      4u);
  EXPECT_EQ(caspian::FindReturnValueSpace(
                "std::basic_ostream<char, std::char_traits<char> >& "
                "std::operator<< <std::char_traits<char> "
                ">(std::basic_ostream<char, std::char_traits<char> >&, char)",
                92u),
            50u);
}

TEST(AnalyzeTest, NormalizeTopLevelGccLambda) {
  EXPECT_EQ(caspian::NormalizeTopLevelGccLambda(
                "cc::{lambda(PaintOp*)#63}::_FUN()", 31u),
            "cc::$lambda#63()");
}

TEST(AnalyzeTest, NormalizeTopLevelClangLambda) {
  // cc::$_21::__invoke() -> cc::$lambda#21()
  EXPECT_EQ(caspian::NormalizeTopLevelClangLambda("cc::$_21::__invoke()", 18u),
            "cc::$lambda#21()");
}

TEST(AnalyzeTest, ParseJavaFunctionSignature) {
  std::deque<std::string> owned_strings;
  // Java method with no args
  auto do_test = [&owned_strings](std::string sig, std::string exp_full_name,
                                  std::string exp_template_name,
                                  std::string exp_name) {
    auto actual = caspian::ParseJava(sig, &owned_strings);
    EXPECT_EQ(exp_full_name, std::string(std::get<0>(actual)));
    EXPECT_EQ(exp_template_name, std::string(std::get<1>(actual)));
    EXPECT_EQ(exp_name, std::string(std::get<2>(actual)));
    // Ensure that ParseJava() is idempotent w.r.t. |full_name| output.
    EXPECT_EQ(PrettyDebug(actual), PrettyDebug(caspian::ParseJava(
                                       std::get<0>(actual), &owned_strings)));
  };
  do_test("org.ClassName java.util.List getCameraInfo()",
          "org.ClassName#getCameraInfo(): java.util.List",
          "org.ClassName#getCameraInfo", "ClassName#getCameraInfo");

  // Java method with args
  do_test("org.ClassName int readShort(int,int)",
          "org.ClassName#readShort(int,int): int", "org.ClassName#readShort",
          "ClassName#readShort");

  // Java <init> method
  do_test("org.ClassName$Inner <init>(byte[])",
          "org.ClassName$Inner#<init>(byte[])", "org.ClassName$Inner#<init>",
          "ClassName$Inner#<init>");

  // Java Class
  do_test("org.ClassName", "org.ClassName", "org.ClassName", "ClassName");

  // Java field
  do_test("org.ClassName some.Type mField", "org.ClassName#mField: some.Type",
          "org.ClassName#mField", "ClassName#mField");
}

TEST(AnalyzeTest, ParseFunctionSignature) {
  std::deque<std::string> owned_strings;
  auto check = [&owned_strings](std::string ret_part, std::string name_part,
                                std::string params_part,
                                std::string after_part = "",
                                std::string name_without_templates = "") {
    if (name_without_templates.empty()) {
      name_without_templates = name_part;
      // Heuristic to drop templates: std::vector<int> -> std::vector<>
      RE2::GlobalReplace(&name_without_templates, "<.*?>", "<>");
      name_without_templates += after_part;
    }
    std::string signature = name_part + params_part + after_part;
    auto result = caspian::ParseCpp(signature, &owned_strings);
    EXPECT_EQ(name_without_templates, std::get<2>(result));
    EXPECT_EQ(name_part + after_part, std::get<1>(result));
    EXPECT_EQ(name_part + params_part + after_part, std::get<0>(result));

    if (!ret_part.empty()) {
      // Parse should be unchanged when we prepend |ret_part|
      signature = ret_part + name_part + params_part + after_part;
      result = caspian::ParseCpp(signature, &owned_strings);
      EXPECT_EQ(name_without_templates, std::get<2>(result));
      EXPECT_EQ(name_part + after_part, std::get<1>(result));
      EXPECT_EQ(name_part + params_part + after_part, std::get<0>(result));
    }
  };

  check("bool ", "foo::Bar<unsigned int, int>::Do<unsigned int>",
        "(unsigned int)");
  check("base::internal::CheckedNumeric<int>& ",
        "base::internal::CheckedNumeric<int>::operator+=<int>", "(int)");
  check("base::internal::CheckedNumeric<int>& ",
        "b::i::CheckedNumeric<int>::MathOp<b::i::CheckedAddOp, int>", "(int)");
  check("", "(anonymous namespace)::GetBridge", "(long long)");
  check("", "operator delete", "(void*)");
  check("",
        "b::i::DstRangeRelationToSrcRangeImpl<long long, long long, "
        "std::__ndk1::numeric_limits, (b::i::Integer)1>::Check",
        "(long long)");
  check("", "cc::LayerIterator::operator cc::LayerIteratorPosition const", "()",
        " const");
  check("decltype ({parm#1}((SkRecords::NoOp)())) ",
        "SkRecord::Record::visit<SkRecords::Draw&>", "(SkRecords::Draw&)",
        " const");
  check("", "base::internal::BindStateBase::BindStateBase",
        "(void (*)(), void (*)(base::internal::BindStateBase const*))");
  check("int ", "std::__ndk1::__c11_atomic_load<int>",
        "(std::__ndk1::<int> volatile*, std::__ndk1::memory_order)");
  check("std::basic_ostream<char, std::char_traits<char> >& ",
        "std::operator<< <std::char_traits<char> >",
        "(std::basic_ostream<char, std::char_traits<char> >&, char)", "",
        "std::operator<< <>");
  check("",
        "std::basic_istream<char, std::char_traits<char> >"
        "::operator>>",
        "(unsigned int&)", "", "std::basic_istream<>::operator>>");
  check("", "std::operator><std::allocator<char> >", "()", "",
        "std::operator><>");
  check("", "std::operator>><std::allocator<char> >",
        "(std::basic_istream<char, std::char_traits<char> >&)", "",
        "std::operator>><>");
  check("", "std::basic_istream<char>::operator>", "(unsigned int&)", "",
        "std::basic_istream<>::operator>");
  check("v8::internal::SlotCallbackResult ",
        "v8::internal::UpdateTypedSlotHelper::UpdateCodeTarget"
        "<v8::PointerUpdateJobTraits<(v8::Direction)1>::Foo(v8::Heap*, "
        "v8::MemoryChunk*)::{lambda(v8::SlotType, unsigned char*)#2}::"
        "operator()(v8::SlotType, unsigned char*, unsigned char*) "
        "const::{lambda(v8::Object**)#1}>",
        "(v8::RelocInfo, v8::Foo<(v8::PointerDirection)1>::Bar(v8::Heap*)::"
        "{lambda(v8::SlotType)#2}::operator()(v8::SlotType) const::"
        "{lambda(v8::Object**)#1})",
        "", "v8::internal::UpdateTypedSlotHelper::UpdateCodeTarget<>");
  check("", "WTF::StringAppend<WTF::String, WTF::String>::operator WTF::String",
        "()", " const");
  // Make sure []s are not removed from the name part.
  check("", "Foo", "()", " [virtual thunk]");
  // Template function that accepts an anonymous lambda.
  check("",
        "blink::FrameView::ForAllNonThrottledFrameViews<blink::FrameView::Pre"
        "Paint()::{lambda(FrameView&)#2}>",
        "(blink::FrameView::PrePaint()::{lambda(FrameView&)#2} const&)", "");

  // Test with multiple template args.
  check("int ", "Foo<int()>::bar<a<b> >", "()", "", "Foo<>::bar<>");

  // See function_signature_test.py for full comment
  std::string sig =
      "(anonymous namespace)::Foo::Baz() const::GLSLFP::onData(Foo, Bar)";
  auto ret = caspian::ParseCpp(sig, &owned_strings);
  EXPECT_EQ("(anonymous namespace)::Foo::Baz", std::get<2>(ret));
  EXPECT_EQ("(anonymous namespace)::Foo::Baz", std::get<1>(ret));
  EXPECT_EQ(sig, std::get<0>(ret));

  // Top-level lambda.
  // Note: Inline lambdas do not seem to be broken into their own symbols.
  sig = "cc::{lambda(cc::PaintOp*)#63}::_FUN(cc::PaintOp*)";
  ret = caspian::ParseCpp(sig, &owned_strings);
  EXPECT_EQ("cc::$lambda#63", std::get<2>(ret));
  EXPECT_EQ("cc::$lambda#63", std::get<1>(ret));
  EXPECT_EQ("cc::$lambda#63(cc::PaintOp*)", std::get<0>(ret));

  sig = "cc::$_63::__invoke(cc::PaintOp*)";
  ret = caspian::ParseCpp(sig, &owned_strings);
  EXPECT_EQ("cc::$lambda#63", std::get<2>(ret));
  EXPECT_EQ("cc::$lambda#63", std::get<1>(ret));
  EXPECT_EQ("cc::$lambda#63(cc::PaintOp*)", std::get<0>(ret));

  // Data members
  check("", "blink::CSSValueKeywordsHash::findValueImpl", "(char const*)",
        "::value_word_list");
  check("", "foo::Bar<Z<Y> >::foo<bar>", "(abc)", "::var<baz>",
        "foo::Bar<>::foo<>::var<>");
}
}  // namespace
