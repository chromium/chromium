#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import html_checker
from os import path as os_path
import re
from sys import path as sys_path
import test_util
import unittest

_HERE = os_path.dirname(os_path.abspath(__file__))
sys_path.append(os_path.join(_HERE, '..', '..'))

from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi


class HtmlCheckerTest(unittest.TestCase):
  def setUp(self):
    super(HtmlCheckerTest, self).setUp()

    self.checker = html_checker.HtmlChecker(MockInputApi(), MockOutputApi())

  def ShouldFailCheck(self, line, checker):
    """Checks that the |checker| flags |line| as a style error."""
    error = checker(1, line)
    self.assertNotEqual('', error, 'Should be flagged as style error: ' + line)
    highlight = test_util.GetHighlight(line, error).strip()

  def ShouldPassCheck(self, line, checker):
    """Checks that the |checker| doesn't flag |line| as a style error."""
    error = checker(1, line)
    self.assertEqual('', error, 'Should not be flagged as style error: ' + line)

  def testClassesUseDashFormCheckFails(self):
    lines = [
      ' <a class="Foo-bar" href="classBar"> ',
      '<b class="foo-Bar"> ',
      '<i class="foo_bar" >',
      ' <hr class="fooBar"> ',
    ]
    for line in lines:
      self.ShouldFailCheck(line, self.checker.ClassesUseDashFormCheck)

  def testClassesUseDashFormCheckPasses(self):
    lines = [
      ' class="abc" ',
      'class="foo-bar"',
      '<div class="foo-bar" id="classBar"',
      '<div class="foo $i18n{barBazQux}" id="classBar"',
    ]
    for line in lines:
      self.ShouldPassCheck(line, self.checker.ClassesUseDashFormCheck)

  def testSingleQuoteCheckFails(self):
    lines = [
      """ <a href='classBar'> """,
      """<a foo$="bar" href$='classBar'>""",
      """<a foo="bar" less="more" href='classBar' kittens="cats">""",
      """<a cats href='classBar' dogs>""",
      """<a cats\n href='classBat\nclassBaz'\n dogs>""",
    ]
    for line in lines:
      self.ShouldFailCheck(line, self.checker.DoNotUseSingleQuotesCheck)

  def testSingleQuoteCheckPasses(self):
    lines = [
      """<b id="super-valid">SO VALID!</b>""",
      """<a text$="i ain't got invalid quotes">i don't</a>""",
      """<span>[[i18n('blah')]]</span> """,
      """<a cats href="classBar" dogs>""",
      """<a cats\n href="classBar"\n dogs>""",
    ]
    for line in lines:
      self.ShouldPassCheck(line, self.checker.DoNotUseSingleQuotesCheck)

  def testDoNotCloseSingleTagsCheckFails(self):
    lines = [
      "<input/>",
      ' <input id="a" /> ',
      "<div/>",
      "<br/>",
      "<br />",
    ]
    for line in lines:
      self.ShouldFailCheck(line, self.checker.DoNotCloseSingleTagsCheck)

  def testDoNotCloseSingleTagsCheckPasses(self):
    lines = [
      "<input>",
      "<link>",
      "<div></div>",
      '<input text="/">',
    ]
    for line in lines:
      self.ShouldPassCheck(line, self.checker.DoNotCloseSingleTagsCheck)

  def testDoNotUseBrElementCheckFails(self):
    lines = [
      " <br>",
      "<br  >  ",
      "<br\>",
      '<br name="a">',
    ]
    for line in lines:
      self.ShouldFailCheck(
          line, self.checker.DoNotUseBrElementCheck)

  def testDoNotUseBrElementCheckPasses(self):
    lines = [
      "br",
      "br>",
      "<browser-switch-app></browser-switch-app>",
      "give me a break"
    ]
    for line in lines:
      self.ShouldPassCheck(
          line, self.checker.DoNotUseBrElementCheck)

  def testDoNotUseInputTypeButtonCheckFails(self):
    lines = [
      '<input type="button">',
      ' <input id="a" type="button" >',
      '<input type="button" id="a"> ',
    ]
    for line in lines:
      self.ShouldFailCheck(line, self.checker.DoNotUseInputTypeButtonCheck)

  def testDoNotUseInputTypeButtonCheckPasses(self):
    lines = [
      "<input>",
      '<input type="text">',
      '<input type="result">',
      '<input type="submit">',
      "<button>",
      '<button type="button">',
      '<button type="reset">',
      '<button type="submit">',

    ]
    for line in lines:
      self.ShouldPassCheck(line, self.checker.DoNotUseInputTypeButtonCheck)

  def testI18nContentJavaScriptCaseCheckFails(self):
    lines = [
      ' i18n-content="foo-bar" ',
      'i18n-content="foo_bar"',
      'i18n-content="FooBar"',
      'i18n-content="_foo"',
      'i18n-content="foo_"',
      'i18n-content="-foo"',
      'i18n-content="foo-"',
      'i18n-content="Foo"',
    ]
    for line in lines:
      self.ShouldFailCheck(line, self.checker.I18nContentJavaScriptCaseCheck)

  def testI18nContentJavaScriptCaseCheckPasses(self):
    lines = [
      ' i18n-content="abc" ',
      'i18n-content="fooBar"',
      'i18n-content="validName" attr="invalidName_"',
      '<div i18n-content="exampleTitle"',
    ]
    for line in lines:
      self.ShouldPassCheck(line, self.checker.I18nContentJavaScriptCaseCheck)

  def testImportCorrectPolymerHtmlFails(self):
    bad_url = 'chrome://resources/polymer/v1_0/polymer/polymer.html'
    lines = [
      '<link rel="import" href="%s">' % bad_url,
      '<link href="%s" rel="import">' % bad_url,
    ]
    for line in lines:
      self.ShouldFailCheck(line, self.checker.ImportCorrectPolymerHtml)

  def testImportCorrectPolymerHtmlPasses(self):
    good_url = 'chrome://resources/html/polymer.html'
    lines = [
      '<link rel="import" href="%s">' % good_url,
      '<link href="%s" rel="import">' % good_url,
    ]
    for line in lines:
      self.ShouldPassCheck(line, self.checker.ImportCorrectPolymerHtml)

  def testLabelCheckFails(self):
    lines = [
      ' <label for="abc"',
      " <label for=    ",
      " <label\tfor=    ",
      ' <label\n blah="1" blee="3"\n for="goop"',
    ]
    for line in lines:
      self.ShouldFailCheck(line, self.checker.LabelCheck)

  def testLabelCheckPasses(self):
    lines = [
      ' my-for="abc" ',
      ' myfor="abc" ',
      " <for",
      ' <paper-tooltip for="id-name"',
    ]
    for line in lines:
      self.ShouldPassCheck(line, self.checker.LabelCheck)

  def testQuotePolymerBindingsFails(self):
    lines = [
      "<a href=[[blah]]>",
      "<div class$=[[class_]]>",
      "<settings-checkbox prefs={{prefs}}",
      "<paper-button actionable$=[[isActionable_(a,b)]]>",
    ]
    for line in lines:
      self.ShouldFailCheck(line, self.checker.QuotePolymerBindings)

  def testQuotePolymerBindingsPasses(self):
    lines = [
      '<a href="[[blah]]">',
      '<span id="blah">[[text]]</span>',
      '<setting-checkbox prefs="{{prefs}}">',
      '<paper-input tab-index="[[tabIndex_]]">',
      '<div style="font: [[getFont_(item)]]">',
    ]
    for line in lines:
      self.ShouldPassCheck(line, self.checker.QuotePolymerBindings)


if __name__ == '__main__':
  unittest.main()
