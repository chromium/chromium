// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/template_expressions.h"

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

// Tip: ui_base_unittests --gtest_filter='TemplateExpressionsTest.*' to run
// these tests.

TEST(TemplateExpressionsTest, ReplaceTemplateExpressionsPieces) {
  TemplateReplacements substitutions;
  substitutions["test"] = "word";
  substitutions["5"] = "number";

  EXPECT_EQ("", ReplaceTemplateExpressions("", substitutions));
  EXPECT_EQ("word", ReplaceTemplateExpressions("$i18n{test}", substitutions));
  EXPECT_EQ("number ", ReplaceTemplateExpressions("$i18n{5} ", substitutions));
  EXPECT_EQ("multiple: word, number.",
            ReplaceTemplateExpressions("multiple: $i18n{test}, $i18n{5}.",
                                       substitutions));
}

TEST(TemplateExpressionsTest,
     ReplaceTemplateExpressionsConsecutiveDollarSignsPieces) {
  TemplateReplacements substitutions;
  substitutions["a"] = "x";
  EXPECT_EQ("$ $$ $$$", ReplaceTemplateExpressions("$ $$ $$$", substitutions));
  EXPECT_EQ("$x", ReplaceTemplateExpressions("$$i18n{a}", substitutions));
  EXPECT_EQ("$$x", ReplaceTemplateExpressions("$$$i18n{a}", substitutions));
  EXPECT_EQ("$i1812", ReplaceTemplateExpressions("$i1812", substitutions));
}

TEST(TemplateExpressionsTest, ReplaceTemplateExpressionsEscaping) {
  static TemplateReplacements substitutions;
  substitutions["punctuationSample"] = "a\"b'c<d>e&f";
  substitutions["htmlSample"] = "<div>hello</div>";
  EXPECT_EQ(
      "a&quot;b&#39;c&lt;d&gt;e&amp;f",
      ReplaceTemplateExpressions("$i18n{punctuationSample}", substitutions));
  EXPECT_EQ("&lt;div&gt;hello&lt;/div&gt;",
            ReplaceTemplateExpressions("$i18n{htmlSample}", substitutions));
  EXPECT_EQ(
      "multiple: &lt;div&gt;hello&lt;/div&gt;, a&quot;b&#39;c&lt;d&gt;e&amp;f.",
      ReplaceTemplateExpressions(
          "multiple: $i18n{htmlSample}, $i18n{punctuationSample}.",
          substitutions));
}

TEST(TemplateExpressionsTest, ReplaceTemplateExpressionsRaw) {
  static TemplateReplacements substitutions;
  substitutions["rawSample"] = "<a href=\"example.com\">hello</a>";
  EXPECT_EQ("<a href=\"example.com\">hello</a>",
            ReplaceTemplateExpressions("$i18nRaw{rawSample}", substitutions));
}

TEST(TemplateExpressionsTest, ReplaceTemplateExpressionsPolymerQuoting) {
  static TemplateReplacements substitutions;
  substitutions["singleSample"] = "don't do it";
  substitutions["doubleSample"] = "\"moo\" said the cow";
  // This resolves |Call('don\'t do it')| to Polymer, which is presented as
  // |don't do it| to the user.
  EXPECT_EQ("<div>[[Call('don\\'t do it')]]",
            ReplaceTemplateExpressions(
                "<div>[[Call('$i18nPolymer{singleSample}')]]", substitutions));
  // This resolves |Call('\"moo\" said the cow')| to Polymer, which is
  // presented as |"moo" said the cow| to the user.
  EXPECT_EQ("<div>[[Call('&quot;moo&quot; said the cow')]]",
            ReplaceTemplateExpressions(
                "<div>[[Call('$i18nPolymer{doubleSample}')]]", substitutions));
}

TEST(TemplateExpressionsTest, ReplaceTemplateExpressionsPolymerMixed) {
  static TemplateReplacements substitutions;
  substitutions["punctuationSample"] = "a\"b'c<d>e&f,g";
  substitutions["htmlSample"] = "<div>hello</div>";
  EXPECT_EQ("a&quot;b\\'c<d>e&f\\\\,g",
            ReplaceTemplateExpressions("$i18nPolymer{punctuationSample}",
                                       substitutions));
  EXPECT_EQ("<div>hello</div>", ReplaceTemplateExpressions(
                                    "$i18nPolymer{htmlSample}", substitutions));
  EXPECT_EQ("multiple: <div>hello</div>, a&quot;b\\'c<d>e&f\\\\,g.",
            ReplaceTemplateExpressions("multiple: $i18nPolymer{htmlSample}, "
                                       "$i18nPolymer{punctuationSample}.",
                                       substitutions));
}

struct TestCase {
  const char* js_in;
  const char* expected_out;
};

TEST(TemplateExpressionsTest, JSNoReplacementOutsideTemplate) {
  TemplateReplacements substitutions;
  substitutions["test"] = "word";
  substitutions["5"] = "number";

  const TestCase kTestCases[] = {
      // No substitutions should occur in normal JS code.
      {"console.log('hello world');", "console.log('hello world');"},
      // Has HTML content but nothing to substitute.
      {"Polymer({\n"
       "  _template: html`\n"
       "    <button on-click=\"onClick_\">Button Name</button>\n"
       "  `,\n"
       "  is: 'foo-element',\n"
       "  onClick_: function() { console.log('hello'); },\n"
       "});",
       "Polymer({\n"
       "  _template: html`\n"
       "    <button on-click=\"onClick_\">Button Name</button>\n"
       "  `,\n"
       "  is: 'foo-element',\n"
       "  onClick_: function() { console.log('hello'); },\n"
       "});"},
      // Basic substitution with template on 1 line.
      {"Polymer({\n"
       "  _template: html`<div>$i18n{test}</div>`,\n"
       "  is: 'foo-element',\n"
       "});",
       "Polymer({\n"
       "  _template: html`<div>word</div>`,\n"
       "  is: 'foo-element',\n"
       "});"},
      // Test case in which only the first $i18n{...} should be substituted,
      // since the second is not in the HTML template string.
      {"Polymer({\n"
       "  _template: html`\n"
       "    <button on-click=\"onClick_\">$i18n{test}</button>\n"
       "  `,\n"
       "  is: 'foo-element',\n"
       "  onClick_: function() { console.log($i18n{5}); },\n"
       "});",
       "Polymer({\n"
       "  _template: html`\n"
       "    <button on-click=\"onClick_\">word</button>\n"
       "  `,\n"
       "  is: 'foo-element',\n"
       "  onClick_: function() { console.log($i18n{5}); },\n"
       "});"},
      // Test case with multiple valid substitutions.
      {"Polymer({\n"
       "  _template: html`\n"
       "    <button on-click=\"onClick_\">$i18n{test}</button>\n"
       "    <span>$i18n{5}</span>\n"
       "  `,\n"
       "  is: 'foo-element',\n"
       "  onClick_: function() { console.log('hello'); },\n"
       "});",
       "Polymer({\n"
       "  _template: html`\n"
       "    <button on-click=\"onClick_\">word</button>\n"
       "    <span>number</span>\n"
       "  `,\n"
       "  is: 'foo-element',\n"
       "  onClick_: function() { console.log('hello'); },\n"
       "});"},
      // Test cases verifying escaped backticks are not detected as the end of
      // the template.
      {"Polymer({\n"
       "  _template: html`<div>backtick\\`,$i18n{test}</div>`,\n"
       "  is: 'foo-element',\n"
       "});",
       "Polymer({\n"
       "  _template: html`<div>backtick\\`,word</div>`,\n"
       "  is: 'foo-element',\n"
       "});"},
      {"Polymer({\n"
       "  _template: html`<div>backtick\\`,$i18n{test}</div>\\\\`,\n"
       "  is: 'foo-element',\n"
       "});",
       "Polymer({\n"
       "  _template: html`<div>backtick\\`,word</div>\\\\`,\n"
       "  is: 'foo-element',\n"
       "});"}};

  std::string formatted;
  for (const TestCase test_case : kTestCases) {
    ASSERT_TRUE(ReplaceTemplateExpressionsInJS(test_case.js_in, substitutions,
                                               &formatted));
    EXPECT_EQ(test_case.expected_out, formatted);
    formatted.clear();
  }
}

TEST(TemplateExpressionsTest, JSReplacementsEscape) {
  TemplateReplacements substitutions;
  substitutions["backtickSample"] =
      "`, attached: function() { alert(1); },_template: html`";
  substitutions["dollarSignSample"] = "5$";
  substitutions["punctuationSample"] = "a\"b'c<d>e&f";
  substitutions["htmlSample"] = "<div>hello</div>";

  const TestCase kTestCases[] = {
      // Substitution with a backtick in the replacement.
      {"Polymer({\n"
       "  _template: html`\n"
       "    <span>\n"
       "      $i18n{backtickSample}\n"
       "    </span>\n"
       "    <button on-click=\"onClick_\">Button</button>\n"
       "  `,\n"
       "  is: 'foo-element',\n"
       "  onClick_: function() { console.log('hello'); },\n"
       "});",
       "Polymer({\n"
       "  _template: html`\n"
       "    <span>\n"
       "      \\`, attached: function() { alert(1); },_template: html\\`\n"
       "    </span>\n"
       "    <button on-click=\"onClick_\">Button</button>\n"
       "  `,\n"
       "  is: 'foo-element',\n"
       "  onClick_: function() { console.log('hello'); },\n"
       "});"},
      // Backtick in one replacement, HTML escapes in other replacements
      {"Polymer({\n"
       "  _template: html`\n"
       "    <span>\n"
       "      $i18n{backtickSample}\n"
       "    </span>\n"
       "    <button on-click=\"onClick_\">\n"
       "      $i18n{punctuationSample}.\n"
       "    </button>\n"
       "    <div>$i18n{htmlSample}</div>\n"
       "  `,\n"
       "  is: 'foo-element',\n"
       "  onClick_: function() { console.log('hello'); },\n"
       "});",
       "Polymer({\n"
       "  _template: html`\n"
       "    <span>\n"
       "      \\`, attached: function() { alert(1); },_template: html\\`\n"
       "    </span>\n"
       "    <button on-click=\"onClick_\">\n"
       "      a&quot;b&#39;c&lt;d&gt;e&amp;f.\n"
       "    </button>\n"
       "    <div>&lt;div&gt;hello&lt;/div&gt;</div>\n"
       "  `,\n"
       "  is: 'foo-element',\n"
       "  onClick_: function() { console.log('hello'); },\n"
       "});"},
      // Replacement contains a '$' that isn't accompanied by a subsequent '{',
      // so should be replaced correctly.
      {"Polymer({\n"
       "  _template: html`\n"
       "    <div>Price is: $i18n{dollarSignSample}</div>\n"
       "  `,\n"
       "  is: 'foo-element',\n"
       "});",
       "Polymer({\n"
       "  _template: html`\n"
       "    <div>Price is: 5$</div>\n"
       "  `,\n"
       "  is: 'foo-element',\n"
       "});"}};
  std::string formatted;
  for (const TestCase test_case : kTestCases) {
    ASSERT_TRUE(ReplaceTemplateExpressionsInJS(test_case.js_in, substitutions,
                                               &formatted));
    EXPECT_EQ(test_case.expected_out, formatted);
    formatted.clear();
  }
}

TEST(TemplateExpressionsTest, JSReplacementsError) {
  TemplateReplacements substitutions;
  substitutions["test"] = "${foo + bar}";
  substitutions["testa"] = "5$";
  substitutions["testb"] = "{a + b}";

  // All these cases should fail.
  const TestCase kTestCases[] = {
      // Nested templates not allowed.
      {"Polymer({\n"
       "  _template: html`\n"
       "    _template: html`\n"
       "      <span>Hello</span>\n"
       "    `,\n"
       "    <div>World</div>\n"
       "  `,\n"
       "  is: 'foo-element',\n"
       "});",
       ""},
      // 2 starts, one end.
      {"Polymer({\n"
       "  _template: html`\n"
       "  _template: html`\n"
       "    <span>Hello</span>\n"
       "    <div>World</div>\n"
       "  `,\n"
       "  is: 'foo-element',\n"
       "});",
       ""},
      // Replacement contains "${".
      {"Polymer({\n"
       "  _template: html`\n"
       "    <div>$i18n{test}</div>\n"
       "  `,\n"
       "  is: 'foo-element',\n"
       "});",
       ""},
      // 2 replacements, when combined, create "${".
      {"Polymer({\n"
       "  _template: html`\n"
       "    <div>$i18n{testa}$i18n{testb}</div>\n"
       "  `,\n"
       "  is: 'foo-element',\n"
       "});",
       ""},
      // Replacement, when combined with content preceding it, creates "${".
      {"Polymer({\n"
       "  _template: html`\n"
       "    <div>Price is: $$i18n{testb}</div>\n"
       "  `,\n"
       "  is: 'foo-element',\n"
       "});",
       ""},
      // HTML _template string is not terminated.
      {"Polymer({\n"
       "  _template: html`\n"
       "    <div>Price is: $i18n{testa}</div>\n"
       "    <span>Fake ending</span>\\\\\\`,\n"
       "  is: 'foo-element',\n"
       "});",
       ""},
  };

  std::string formatted;
  for (const TestCase test_case : kTestCases) {
    ASSERT_FALSE(ReplaceTemplateExpressionsInJS(test_case.js_in, substitutions,
                                                &formatted));
    formatted.clear();
  }
}

TEST(TemplateExpressionsTest, JSMultipleTemplates) {
  TemplateReplacements substitutions;
  substitutions["test"] = "word";
  substitutions["5"] = "number";

  const TestCase kTestCases[] = {
      // Only the second template has substitutions
      {"Polymer({\n"
       "  _template: html`<div>Hello</div>`,\n"
       "  is: 'foo-element',\n"
       "});"
       "Polymer({\n"
       "  _template: html`<div>$i18n{5}$i18n{test}</div>`,\n"
       "  is: 'bar-element',\n"
       "});",
       "Polymer({\n"
       "  _template: html`<div>Hello</div>`,\n"
       "  is: 'foo-element',\n"
       "});"
       "Polymer({\n"
       "  _template: html`<div>numberword</div>`,\n"
       "  is: 'bar-element',\n"
       "});"},
      // 2 templates, both with substitutions.
      {"Polymer({\n"
       "  _template: html`<div>$i18n{test}</div>`,\n"
       "  is: 'foo-element',\n"
       "});"
       "Polymer({\n"
       "  _template: html`<div>$i18n{5}</div>`,\n"
       "  is: 'bar-element',\n"
       "});",
       "Polymer({\n"
       "  _template: html`<div>word</div>`,\n"
       "  is: 'foo-element',\n"
       "});"
       "Polymer({\n"
       "  _template: html`<div>number</div>`,\n"
       "  is: 'bar-element',\n"
       "});"},
      // 2 minified templates, both with substitutions.
      {"Polymer({_template:html`<div>$i18n{test}</div>`,is:'foo-element',});\n"
       "Polymer({_template:html`<div>$i18n{5}</div>`,is:'bar-element',});",
       "Polymer({_template:html`<div>word</div>`,is:'foo-element',});\n"
       "Polymer({_template:html`<div>number</div>`,is:'bar-element',});"}};

  std::string formatted;
  for (const TestCase test_case : kTestCases) {
    ASSERT_TRUE(ReplaceTemplateExpressionsInJS(test_case.js_in, substitutions,
                                               &formatted));
    EXPECT_EQ(test_case.expected_out, formatted);
    formatted.clear();
  }
}

}  // namespace ui
