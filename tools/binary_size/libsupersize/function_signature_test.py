#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import re
import unittest

import function_signature


class AnalyzeTest(unittest.TestCase):

  def testParseJavaFunctionSignature(self):
    # Java method with no args
    SIG = 'org.ClassName java.util.List getCameraInfo()'
    actual = function_signature.ParseJava(SIG)
    self.assertEqual('ClassName#getCameraInfo', actual[2])
    self.assertEqual('org.ClassName#getCameraInfo(): java.util.List', actual[0])
    self.assertEqual('org.ClassName#getCameraInfo', actual[1])
    self.assertEqual(actual, function_signature.ParseJava(actual[0]))

    # Java method with args
    SIG = 'org.ClassName int readShort(int,int)'
    actual = function_signature.ParseJava(SIG)
    self.assertEqual('ClassName#readShort', actual[2])
    self.assertEqual('org.ClassName#readShort', actual[1])
    self.assertEqual('org.ClassName#readShort(int,int): int', actual[0])
    self.assertEqual(actual, function_signature.ParseJava(actual[0]))

    # Java <init> method
    SIG = 'org.ClassName$Inner <init>(byte[])'
    actual = function_signature.ParseJava(SIG)
    self.assertEqual('ClassName$Inner#<init>', actual[2])
    self.assertEqual('org.ClassName$Inner#<init>', actual[1])
    self.assertEqual('org.ClassName$Inner#<init>(byte[])', actual[0])
    self.assertEqual(actual, function_signature.ParseJava(actual[0]))

    # Java Class
    SIG = 'org.ClassName'
    actual = function_signature.ParseJava(SIG)
    self.assertEqual('ClassName', actual[2])
    self.assertEqual('org.ClassName', actual[1])
    self.assertEqual('org.ClassName', actual[0])
    self.assertEqual(actual, function_signature.ParseJava(actual[0]))

    # Java Field
    SIG = 'org.ClassName some.Type mField'
    actual = function_signature.ParseJava(SIG)
    self.assertEqual('ClassName#mField', actual[2])
    self.assertEqual('org.ClassName#mField', actual[1])
    self.assertEqual('org.ClassName#mField: some.Type', actual[0])
    self.assertEqual(actual, function_signature.ParseJava(actual[0]))

    # Class merging: Method
    SIG = 'org.NewClass int org.OldClass.readShort(int,int)'
    actual = function_signature.ParseJava(SIG)
    self.assertEqual('OldClass#readShort', actual[2])
    self.assertEqual('org.OldClass#readShort', actual[1])
    self.assertEqual('org.NewClass#org.OldClass.readShort(int,int): int',
                     actual[0])
    self.assertEqual(actual, function_signature.ParseJava(actual[0]))

    # Class merging: Field
    SIG = 'org.NewClass some.Type org.OldClass.mField'
    actual = function_signature.ParseJava(SIG)
    self.assertEqual('OldClass#mField', actual[2])
    self.assertEqual('org.OldClass#mField', actual[1])
    self.assertEqual('org.NewClass#org.OldClass.mField: some.Type', actual[0])
    self.assertEqual(actual, function_signature.ParseJava(actual[0]))

  def testParseFunctionSignature(self):
    def check(ret_part, name_part, params_part, after_part='',
              name_without_templates=None):
      if name_without_templates is None:
        name_without_templates = re.sub(r'<.*?>', '<>', name_part) + after_part

      signature = ''.join((name_part, params_part, after_part))
      got_full_name, got_template_name, got_name = (
          function_signature.Parse(signature))
      self.assertEqual(name_without_templates, got_name)
      self.assertEqual(name_part + after_part, got_template_name)
      self.assertEqual(name_part + params_part + after_part, got_full_name)
      if ret_part:
        signature = ''.join((ret_part, name_part, params_part, after_part))
        got_full_name, got_template_name, got_name = (
            function_signature.Parse(signature))
        self.assertEqual(name_without_templates, got_name)
        self.assertEqual(name_part + after_part, got_template_name)
        self.assertEqual(name_part + params_part + after_part, got_full_name)

    check('bool ',
          'foo::Bar<unsigned int, int>::Do<unsigned int>',
          '(unsigned int)')
    check('base::internal::CheckedNumeric<int>& ',
          'base::internal::CheckedNumeric<int>::operator+=<int>',
          '(int)')
    check('base::internal::CheckedNumeric<int>& ',
          'b::i::CheckedNumeric<int>::MathOp<b::i::CheckedAddOp, int>',
          '(int)')
    check('', '(anonymous namespace)::GetBridge', '(long long)')
    check('', 'operator delete', '(void*)')
    check('', 'b::i::DstRangeRelationToSrcRangeImpl<long long, long long, '
              'std::__ndk1::numeric_limits, (b::i::Integer)1>::Check',
          '(long long)')
    check('', 'cc::LayerIterator::operator cc::LayerIteratorPosition const',
          '()',
          ' const')
    check('decltype ({parm#1}((SkRecords::NoOp)())) ',
          'SkRecord::Record::visit<SkRecords::Draw&>',
          '(SkRecords::Draw&)',
          ' const')
    check('', 'base::internal::BindStateBase::BindStateBase',
          '(void (*)(), void (*)(base::internal::BindStateBase const*))')
    check('int ', 'std::__ndk1::__c11_atomic_load<int>',
          '(std::__ndk1::<int> volatile*, std::__ndk1::memory_order)')
    check('std::basic_ostream<char, std::char_traits<char> >& ',
          'std::operator<< <std::char_traits<char> >',
          '(std::basic_ostream<char, std::char_traits<char> >&, char)',
          name_without_templates='std::operator<< <>')
    check('',
          'std::basic_istream<char, std::char_traits<char> >'
          '::operator>>',
          '(unsigned int&)',
          name_without_templates='std::basic_istream<>::operator>>')
    check('',
          'std::operator><std::allocator<char> >', '()',
          name_without_templates='std::operator><>')
    check('',
          'std::operator>><std::allocator<char> >',
          '(std::basic_istream<char, std::char_traits<char> >&)',
          name_without_templates='std::operator>><>')
    check('',
          'std::basic_istream<char>::operator>', '(unsigned int&)',
          name_without_templates='std::basic_istream<>::operator>')
    check('v8::internal::SlotCallbackResult ',
          'v8::internal::UpdateTypedSlotHelper::UpdateCodeTarget'
          '<v8::PointerUpdateJobTraits<(v8::Direction)1>::Foo(v8::Heap*, '
          'v8::MemoryChunk*)::{lambda(v8::SlotType, unsigned char*)#2}::'
          'operator()(v8::SlotType, unsigned char*, unsigned char*) '
          'const::{lambda(v8::Object**)#1}>',
          '(v8::RelocInfo, v8::Foo<(v8::PointerDirection)1>::Bar(v8::Heap*)::'
          '{lambda(v8::SlotType)#2}::operator()(v8::SlotType) const::'
          '{lambda(v8::Object**)#1})',
          name_without_templates=(
              'v8::internal::UpdateTypedSlotHelper::UpdateCodeTarget<>'))
    check('',
          'WTF::StringAppend<WTF::String, WTF::String>::operator WTF::String',
          '()',
          ' const')
    # Make sure []s are not removed from the name part.
    check('', 'Foo', '()', ' [virtual thunk]')
    # Template function that accepts an anonymous lambda.
    check('',
          'blink::FrameView::ForAllNonThrottledFrameViews<blink::FrameView::Pre'
          'Paint()::{lambda(FrameView&)#2}>',
          '(blink::FrameView::PrePaint()::{lambda(FrameView&)#2} const&)', '')

    # Test with multiple template args.
    check('int ', 'Foo<int()>::bar<a<b> >', '()',
          name_without_templates='Foo<>::bar<>')

    # SkArithmeticImageFilter.cpp has class within function body. e.g.:
    #   ArithmeticFP::onCreateGLSLInstance() looks like:
    # class ArithmeticFP {
    #   GrGLSLFragmentProcessor* onCreateGLSLInstance() const {
    #     class GLSLFP {
    #       void emitCode(EmitArgs& args) { ... }
    #     };
    #     ...
    #   }
    # };
    SIG = '(anonymous namespace)::Foo::Baz() const::GLSLFP::onData(Foo, Bar)'
    got_full_name, got_template_name, got_name = (
        function_signature.Parse(SIG))
    self.assertEqual('(anonymous namespace)::Foo::Baz', got_name)
    self.assertEqual('(anonymous namespace)::Foo::Baz', got_template_name)
    self.assertEqual(SIG, got_full_name)

    # Top-level lambda.
    # Note: Inline lambdas do not seem to be broken into their own symbols.
    SIG = 'cc::{lambda(cc::PaintOp*)#63}::_FUN(cc::PaintOp*)'
    got_full_name, got_template_name, got_name = (
        function_signature.Parse(SIG))
    self.assertEqual('cc::$lambda#63', got_name)
    self.assertEqual('cc::$lambda#63', got_template_name)
    self.assertEqual('cc::$lambda#63(cc::PaintOp*)', got_full_name)

    SIG = 'cc::$_63::__invoke(cc::PaintOp*)'
    got_full_name, got_template_name, got_name = (
        function_signature.Parse(SIG))
    self.assertEqual('cc::$lambda#63', got_name)
    self.assertEqual('cc::$lambda#63', got_template_name)
    self.assertEqual('cc::$lambda#63(cc::PaintOp*)', got_full_name)

    # Data members
    check('', 'blink::CSSValueKeywordsHash::findValueImpl', '(char const*)',
          '::value_word_list')
    check('', 'foo::Bar<Z<Y> >::foo<bar>', '(abc)', '::var<baz>',
          name_without_templates='foo::Bar<>::foo<>::var<>')

    # ABI Tag Attributes
    SIG = 'std::make_unique[abi:v15000]<Foo>(Bar const*&)'
    got_full_name, got_template_name, got_name = function_signature.Parse(SIG)
    self.assertEqual('std::make_unique<>', got_name)
    self.assertEqual('std::make_unique<Foo>', got_template_name)
    self.assertEqual(SIG, got_full_name)

    SIG = 'foo::kBar[abi:baz]'
    got_full_name, got_template_name, got_name = function_signature.Parse(SIG)
    self.assertEqual('foo::kBar', got_name)
    self.assertEqual('foo::kBar', got_template_name)
    self.assertEqual(SIG, got_full_name)

    # Make sure operator[] is not considered an attribute.
    check('', 'foo::operator[]', '(abc)')

    SIG = 'foo<char []>::operator[][abi:v1500]<Bar[99]>()'
    got_full_name, got_template_name, got_name = function_signature.Parse(SIG)
    self.assertEqual('foo<>::operator[]<>', got_name)
    self.assertEqual('foo<char []>::operator[]<Bar[99]>', got_template_name)
    self.assertEqual(SIG, got_full_name)


if __name__ == '__main__':
  logging.basicConfig(level=logging.DEBUG,
                      format='%(levelname).1s %(relativeCreated)6d %(message)s')
  unittest.main()
