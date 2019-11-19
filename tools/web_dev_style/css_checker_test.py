#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import css_checker
from os import path as os_path
import re
from sys import path as sys_path
import unittest

_HERE = os_path.dirname(os_path.abspath(__file__))
sys_path.append(os_path.join(_HERE, '..', '..'))

from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi, MockFile


class CssCheckerTest(unittest.TestCase):
  def setUp(self):
    super(CssCheckerTest, self).setUp()

    self.input_api = MockInputApi()
    self.checker = css_checker.CSSChecker(self.input_api, MockOutputApi())

  def _create_file(self, contents, filename):
    self.input_api.files.append(MockFile(filename, contents.splitlines()))

  def VerifyContentIsValid(self, contents, filename='fake.css'):
    self._create_file(contents, filename)
    results = self.checker.RunChecks()
    self.assertEqual(len(results), 0)

  def VerifyContentsProducesOutput(self, contents, output, filename='fake.css'):
    self._create_file(contents, filename)
    results = self.checker.RunChecks()
    self.assertEqual(len(results), 1)
    self.assertEqual(results[0].message, filename + ':\n' + output.strip())

  def testCssAlphaWithAtBlock(self):
    self.VerifyContentsProducesOutput("""
<include src="../shared/css/cr/ui/overlay.css">
<include src="chrome://resources/totally-cool.css" />

/* A hopefully safely ignored comment and @media statement. /**/
@media print {
  div {
    display: block;
    color: red;
  }
}

.rule {
  z-index: 5;
<if expr="not is macosx">
  background-image: url(chrome://resources/BLAH); /* TODO(dbeam): Fix this. */
  background-color: rgb(235, 239, 249);
</if>
<if expr="is_macosx">
  background-color: white;
  background-image: url(chrome://resources/BLAH2);
</if>
  color: black;
}

<if expr="is_macosx">
.language-options-right {
  visibility: hidden;
  opacity: 1; /* TODO(dbeam): Fix this. */
}
</if>

@media (prefers-color-scheme: dark) {
  a[href] {
    z-index: 3;
    color: blue;
  }
}""", """
- Alphabetize properties and list vendor specific (i.e. -webkit) above standard.
    display: block;
    color: red;

    z-index: 5;
    color: black;

    z-index: 3;
    color: blue;""")

  def testCssStringWithAt(self):
    self.VerifyContentIsValid("""
#logo {
  background-image: url(images/google_logo.png@2x);
}

body.alternate-logo #logo {
  -webkit-mask-image: url(images/google_logo.png@2x);
  background: none;
  @apply(--some-variable);
}

div {
  margin-inline-start: 5px;
}

.stuff1 {
}

.stuff2 {
}
      """)

  def testCssAlphaWithNonStandard(self):
    self.VerifyContentsProducesOutput("""
div {
  /* A hopefully safely ignored comment and @media statement. /**/
  color: red;
  -webkit-margin-before-collapse: discard;
}""", """
- Alphabetize properties and list vendor specific (i.e. -webkit) above standard.
    color: red;
    -webkit-margin-before-collapse: discard;""")

  def testCssAlphaWithLongerDashedProps(self):
    self.VerifyContentsProducesOutput("""
div {
  border-inline-start: 5px;  /* A hopefully removed comment. */
  border: 5px solid red;
}""", """
- Alphabetize properties and list vendor specific (i.e. -webkit) above standard.
    border-inline-start: 5px;
    border: 5px solid red;""")

  def testCssAlphaWithVariables(self):
    self.VerifyContentIsValid("""
#id {
  --zzyxx-xylophone: 3px;
  --ignore-me: {
    /* TODO(dbeam): fix this by creating a "sort context". If we simply strip
     * off the mixin, the inside contents will be compared to the outside
     * contents, which isn't what we want. */
    visibility: hidden;
    color: black;
  };
  --aardvark-animal: var(--zzyxz-xylophone);
}
""")

  def testCssBracesHaveSpaceBeforeAndNothingAfter(self):
    self.VerifyContentsProducesOutput("""
/* Hello! */div/* Comment here*/{
  display: block;
}

blah /* hey! */
{
  rule: value;
}

.mixed-in {
  display: none;
  --css-mixin: {
    color: red;
  };  /* This should be ignored. */
}

.this.is { /* allowed */
  rule: value;
}""", """
- Start braces ({) end a selector, have a space before them and no rules after.
    div{
    {""")

  def testCssClassesUseDashes(self):
    self.VerifyContentsProducesOutput("""
.className,
.ClassName,
.class-name /* We should not catch this. */,
.class_name,
[i18n-values*='.innerHTML:'] {
  display: block;
}""", """
 - Classes use .dash-form.
    .className,
    .ClassName,
    .class_name,""")

  def testCssCloseBraceOnNewLine(self):
    self.VerifyContentsProducesOutput("""
@-webkit-keyframe blah {
  from { height: rotate(-10turn); }
  100% { height: 500px; }
}

#id { /* $i18n{*} and $i18nRaw{*} should be ignored. */
  rule: $i18n{someValue};
  rule2: $i18nRaw{someValue};
  --css-mixin: {
    color: red;
  };
}

.paper-wrapper {
  --paper-thinger: {
    background: blue;
  };
}

#rule {
  rule: value; }""", """
- Always put a rule closing brace (}) on a new line.
    rule: value; }""")

  def testCssColonsHaveSpaceAfter(self):
    self.VerifyContentsProducesOutput("""
div:not(.class):not([attr=5]), /* We should not catch this. */
div:not(.class):not([attr]) /* Nor this. */ {
  background: url(data:image/jpeg,asdfasdfsadf); /* Ignore this. */
  background: -webkit-linear-gradient(left, red,
                                      80% blah blee blar);
  color: red;
  display:block;
}""", """
- Colons (:) should have a space after them.
    display:block;

- Don't use data URIs in source files. Use grit instead.
    background: url(data:image/jpeg,asdfasdfsadf);""")

  def testCssFavorSingleQuotes(self):
    self.VerifyContentsProducesOutput("""
html[dir="rtl"] body,
html[dir=ltr] body /* TODO(dbeam): Require '' around rtl in future? */ {
  font-family: "Open Sans";
<if expr="is_macosx">
  blah: blee;
</if>
}""", """
- Use single quotes (') instead of double quotes (") in strings.
    html[dir="rtl"] body,
    font-family: "Open Sans";""")

  def testCssHexCouldBeShorter(self):
    self.VerifyContentsProducesOutput("""
#abc,
#abc-,
#abc-ghij,
#abcdef-,
#abcdef-ghij,
#aaaaaa,
#bbaacc {
  background-color: #336699; /* Ignore short hex rule if not gray. */
  color: #999999;
  color: #666;
}""", """
- Use abbreviated hex (#rgb) when in form #rrggbb.
    color: #999999; (replace with #999)

- Use rgb() over #hex when not a shade of gray (like #333).
    background-color: #336699; (replace with rgb(51, 102, 153))""")

  def testCssUseMillisecondsForSmallTimes(self):
    self.VerifyContentsProducesOutput("""
.transition-0s /* This is gross but may happen. */ {
  transform: one 0.2s;
  transform: two .1s;
  transform: tree 1s;
  transform: four 300ms;
}""", """
- Use milliseconds for time measurements under 1 second.
    transform: one 0.2s; (replace with 200ms)
    transform: two .1s; (replace with 100ms)""")

  def testCssNoDataUrisInSourceFiles(self):
    self.VerifyContentsProducesOutput("""
img {
  background: url( data:image/jpeg,4\/\/350|\/|3|2 );
}""", """
- Don't use data URIs in source files. Use grit instead.
    background: url( data:image/jpeg,4\/\/350|\/|3|2 );""")

  def testCssNoMixinShims(self):
    self.VerifyContentsProducesOutput("""
:host {
  --good-property: red;
  --not-okay-mixin_-_not-okay-property: green;
}""", """
- Don't override custom properties created by Polymer's mixin shim. Set \
mixins or documented custom properties directly.
    --not-okay-mixin_-_not-okay-property: green;""")

  def testCssNoQuotesInUrl(self):
    self.VerifyContentsProducesOutput("""
img {
  background: url('chrome://resources/images/blah.jpg');
  background: url("../../folder/hello.png");
}""", """
- Use single quotes (') instead of double quotes (") in strings.
    background: url("../../folder/hello.png");

- Don't use quotes in url().
    background: url('chrome://resources/images/blah.jpg');
    background: url("../../folder/hello.png");""")

  def testCssOneRulePerLine(self):
    self.VerifyContentsProducesOutput("""
a:not([hidden]):not(.custom-appearance):not([version=1]):first-of-type,
a:not([hidden]):not(.custom-appearance):not([version=1]):first-of-type ~
    input[type='checkbox']:not([hidden]),
div {
  background: url(chrome://resources/BLAH);
  rule: value; /* rule: value; */
  rule: value; rule: value;
}

.remix {
  --dj: {
    spin: that;
  };
}
""", """
- One rule per line (what not to do: color: red; margin: 0;).
    rule: value; rule: value;""")

  def testCssOneSelectorPerLine(self):
    self.VerifyContentsProducesOutput("""
a,
div,a,
div,/* Hello! */ span,
#id.class([dir=rtl):not(.class):any(a, b, d) {
  rule: value;
}

a,
div,a {
  some-other: rule here;
}""", """
- One selector per line (what not to do: a, b {}).
    div,a,
    div, span,
    div,a {""")

  def testCssPseudoElementDoubleColon(self):
    self.VerifyContentsProducesOutput("""
a:href,
br::after,
::-webkit-scrollbar-thumb,
a:not([empty]):hover:focus:active, /* shouldn't catch here and above */
abbr:after,
.tree-label:empty:after,
b:before,
:-WebKit-ScrollBar {
  rule: value;
}""", """
- Pseudo-elements should use double colon (i.e. ::after).
    :after (should be ::after)
    :after (should be ::after)
    :before (should be ::before)
    :-WebKit-ScrollBar (should be ::-WebKit-ScrollBar)
    """)

  def testCssRgbIfNotGray(self):
    self.VerifyContentsProducesOutput("""
#abc,
#aaa,
#aabbcc {
  background: -webkit-linear-gradient(left, from(#abc), to(#def));
  color: #bad;
  color: #bada55;
}""", """
- Use rgb() over #hex when not a shade of gray (like #333).
    background: -webkit-linear-gradient(left, from(#abc), to(#def)); """
"""(replace with rgb(170, 187, 204), rgb(221, 238, 255))
    color: #bad; (replace with rgb(187, 170, 221))
    color: #bada55; (replace with rgb(186, 218, 85))""")

  def testPrefixedLogicalAxis(self):
    self.VerifyContentsProducesOutput("""
.test {
  -webkit-logical-height: 50%;
  -webkit-logical-width: 50%;
  -webkit-max-logical-height: 200px;
  -webkit-max-logical-width: 200px;
  -webkit-min-logical-height: 100px;
  -webkit-min-logical-width: 100px;
}
""", """
- Unprefix logical axis property.
    -webkit-logical-height: 50%; (replace with block-size)
    -webkit-logical-width: 50%; (replace with inline-size)
    -webkit-max-logical-height: 200px; (replace with max-block-size)
    -webkit-max-logical-width: 200px; (replace with max-inline-size)
    -webkit-min-logical-height: 100px; (replace with min-block-size)
    -webkit-min-logical-width: 100px; (replace with min-inline-size)""")

  def testPrefixedLogicalSide(self):
    self.VerifyContentsProducesOutput("""
.test {
  -webkit-border-after: 1px solid blue;
  -webkit-border-after-color: green;
  -webkit-border-after-style: dotted;
  -webkit-border-after-width: 10px;
  -webkit-border-before: 2px solid blue;
  -webkit-border-before-color: green;
  -webkit-border-before-style: dotted;
  -webkit-border-before-width: 20px;
  -webkit-border-end: 3px solid blue;
  -webkit-border-end-color: green;
  -webkit-border-end-style: dotted;
  -webkit-border-end-width: 30px;
  -webkit-border-start: 4px solid blue;
  -webkit-border-start-color: green;
  -webkit-border-start-style: dotted;
  -webkit-border-start-width: 40px;
  -webkit-margin-after: 1px;
  -webkit-margin-after-collapse: discard;
  -webkit-margin-before: 2px;
  -webkit-margin-before-collapse: discard;
  -webkit-margin-end: 3px;
  -webkit-margin-end-collapse: discard;
  -webkit-margin-start: 4px;
  -webkit-margin-start-collapse: discard;
  -webkit-padding-after: 1px;
  -webkit-padding-before: 2px;
  -webkit-padding-end: 3px;
  -webkit-padding-start: 4px;
}
""", """
- Unprefix logical side property.
    -webkit-border-after: 1px solid blue; (replace with border-block-end)
    -webkit-border-after-color: green; (replace with border-block-end-color)
    -webkit-border-after-style: dotted; (replace with border-block-end-style)
    -webkit-border-after-width: 10px; (replace with border-block-end-width)
    -webkit-border-before: 2px solid blue; (replace with border-block-start)
    -webkit-border-before-color: green; (replace with border-block-start-color)
    -webkit-border-before-style: dotted; (replace with border-block-start-style)
    -webkit-border-before-width: 20px; (replace with border-block-start-width)
    -webkit-border-end: 3px solid blue; (replace with border-inline-end)
    -webkit-border-end-color: green; (replace with border-inline-end-color)
    -webkit-border-end-style: dotted; (replace with border-inline-end-style)
    -webkit-border-end-width: 30px; (replace with border-inline-end-width)
    -webkit-border-start: 4px solid blue; (replace with border-inline-start)
    -webkit-border-start-color: green; (replace with border-inline-start-color)
    -webkit-border-start-style: dotted; (replace with border-inline-start-style)
    -webkit-border-start-width: 40px; (replace with border-inline-start-width)
    -webkit-margin-after: 1px; (replace with margin-block-end)
    -webkit-margin-before: 2px; (replace with margin-block-start)
    -webkit-margin-end: 3px; (replace with margin-inline-end)
    -webkit-margin-start: 4px; (replace with margin-inline-start)
    -webkit-padding-after: 1px; (replace with padding-block-end)
    -webkit-padding-before: 2px; (replace with padding-block-start)
    -webkit-padding-end: 3px; (replace with padding-inline-end)
    -webkit-padding-start: 4px; (replace with padding-inline-start)""")

  def testStartEndInsteadOfLeftRight(self):
    self.VerifyContentsProducesOutput("""
.inline-node {
  --var-is-ignored-left: 10px;
  --var-is-ignored-right: 10px;
  border-left-color: black;
  border-right: 1px solid blue;  /* csschecker-disable-line left-right */
  margin-right: 5px;
  padding-left: 10px;    /* csschecker-disable-line some-other-thing */
  text-align: right;
}""", """
- Use -start/end instead of -left/right (https://goo.gl/gQYY7z, add /* csschecker-disable-line left-right */ to suppress)
    border-left-color: black; (replace with border-inline-start-color)
    margin-right: 5px; (replace with margin-inline-end)
    padding-left: 10px; (replace with padding-inline-start)
    text-align: right; (replace with text-align: end)
""")

  def testCssZeroWidthLengths(self):
    self.VerifyContentsProducesOutput("""
@-webkit-keyframe anim {
  0% { /* Ignore key frames */
    width: 0px;
  }
  10% {
    width: 10px;
  }
  50% { background-image: url(blah.svg); }
  100% {
    width: 100px;
  }
}

#logo {
  background-image: url(images/google_logo.png@2x);
}

body.alternate-logo #logo {
  -webkit-mask-image: url(images/google_logo.png@2x);
}

/* http://crbug.com/359682 */
#spinner-container #spinner {
  -webkit-animation-duration: 1.0s;
  background-image: url(images/google_logo0.svg);
}

.media-button.play > .state0.active,
.media-button[state='0'] > .state0.normal /* blah */, /* blee */
.media-button[state='0']:not(.disabled):hover > .state0.hover {
  -webkit-animation: anim 0s;
  -webkit-animation-duration: anim 0ms;
  -webkit-transform: scale(0%);
  background-position-x: 0em;
  background-position-y: 0ex;
  border-width: 0em;
  color: hsl(0, 0%, 85%); /* Shouldn't trigger error. */
  opacity: .0;
  opacity: 0.0;
  opacity: 0.;
}

@page {
  border-width: 0mm;
  height: 0cm;
  width: 0in;
}""", """
- Use "0" for zero-width lengths (i.e. 0px -> 0)
    width: 0px;
    -webkit-transform: scale(0%);
    background-position-x: 0em;
    background-position-y: 0ex;
    border-width: 0em;
    opacity: .0;
    opacity: 0.0;
    opacity: 0.;
    border-width: 0mm;
    height: 0cm;
    width: 0in;
""")

  def testInlineStyleInHtml(self):
    self.VerifyContentsProducesOutput("""<!doctype html>
<html>
<head>
  <!-- Don't warn about problems outside of style tags
    html,
    body {
      margin: 0;
      height: 100%;
    }
  -->
  <style>
    body {
      flex-direction:column;
    }
  </style>
</head>
</html>""", """
- Colons (:) should have a space after them.
    flex-direction:column;
""", filename='test.html')

  def testInlineStyleInHtmlWithIncludes(self):
    self.VerifyContentsProducesOutput("""<!doctype html>
<html>
  <style include="fake-shared-css other-shared-css">
    body {
      flex-direction:column;
    }
  </style>
</head>
</html>""", """
- Colons (:) should have a space after them.
    flex-direction:column;
""", filename='test.html')

  def testInlineStyleInHtmlWithTagsInComments(self):
    self.VerifyContentsProducesOutput("""<!doctype html>
<html>
  <style>
    body {
      /* You better ignore the <tag> in this comment! */
      flex-direction:column;
    }
  </style>
</head>
</html>""", """
- Colons (:) should have a space after them.
    flex-direction:column;
""", filename='test.html')

  def testRemoveAtBlocks(self):
    self.assertEqual(self.checker.RemoveAtBlocks("""
@media (prefers-color-scheme: dark) {
  .magic {
    color: #000;
  }
}"""), """
  .magic {
    color: #000;
  }""")

    self.assertEqual(self.checker.RemoveAtBlocks("""
@media (prefers-color-scheme: dark) {
  .magic {
    --mixin-definition: {
      color: red;
    };
  }
}"""), """
  .magic {
    --mixin-definition: {
      color: red;
    };
  }""")

    self.assertEqual(self.checker.RemoveAtBlocks("""
@keyframes jiggle {
  from { left: 0; }
  50% { left: 100%; }
  to { left: 10%; }
}"""), """
  from { left: 0; }
  50% { left: 100%; }
  to { left: 10%; }""")

    self.assertEqual(self.checker.RemoveAtBlocks("""
@media print {
  .rule1 {
    color: black;
  }
  .rule2 {
    margin: 1in;
  }
}"""), """
  .rule1 {
    color: black;
  }
  .rule2 {
    margin: 1in;
  }""")

    self.assertEqual(self.checker.RemoveAtBlocks("""
@media (prefers-color-scheme: dark) {
  .rule1 {
    color: gray;
  }
  .rule2 {
    margin: .5in;
  }
  @keyframe dark-fade {
    0% { background: black; }
    100% { background: darkgray; }
  }
}"""), """
  .rule1 {
    color: gray;
  }
  .rule2 {
    margin: .5in;
  }
    0% { background: black; }
    100% { background: darkgray; }""")

    self.assertEqual(self.checker.RemoveAtBlocks("""
@-webkit-keyframe anim {
  0% { /* Ignore key frames */
    width: 0px;
  }
  10% {
    width: 10px;
  }
  50% { background-image: url(blah.svg); }
  100% {
    width: 100px;
  }
}"""), """
  0% { /* Ignore key frames */
    width: 0px;
  }
  10% {
    width: 10px;
  }
  50% { background-image: url(blah.svg); }
  100% {
    width: 100px;
  }""")


if __name__ == '__main__':
  unittest.main()
