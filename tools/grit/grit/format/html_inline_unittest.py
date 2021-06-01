#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.format.html_inline'''

from __future__ import print_function

import os
import re
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

import unittest

from grit import util
from grit.format import html_inline


class HtmlInlineUnittest(unittest.TestCase):
  '''Unit tests for HtmlInline.'''

  def testGetResourceFilenames(self):
    '''Tests that all included files are returned by GetResourceFilenames.'''

    files = {
      'index.html': '''
      <!DOCTYPE HTML>
      <html>
        <head>
          <link rel="stylesheet" href="test.css">
          <link rel="stylesheet"
              href="really-long-long-long-long-long-test.css">
        </head>
        <body>
          <include src='test.html'>
          <include
              src="really-long-long-long-long-long-test-file-omg-so-long.html">
        </body>
      </html>
      ''',

      'test.html': '''
      <include src="test2.html">
      ''',

      'really-long-long-long-long-long-test-file-omg-so-long.html': '''
      <!-- This really long named resource should be included. -->
      ''',

      'test2.html': '''
      <!-- This second level resource should also be included. -->
      ''',

      'test.css': '''
      .image {
        background: url('test.png');
      }
      ''',

      'really-long-long-long-long-long-test.css': '''
      a:hover {
        font-weight: bold;  /* Awesome effect is awesome! */
      }
      ''',

      'test.png': 'PNG DATA',
    }

    source_resources = set()
    tmp_dir = util.TempDir(files)
    for filename in files:
      source_resources.add(tmp_dir.GetPath(filename))

    resources = html_inline.GetResourceFilenames(tmp_dir.GetPath('index.html'),
                                                 None)
    resources.add(tmp_dir.GetPath('index.html'))
    self.failUnlessEqual(resources, source_resources)
    tmp_dir.CleanUp()

  def testUnmatchedEndIfBlock(self):
    '''Tests that an unmatched </if> raises an exception.'''

    files = {
      'index.html': '''
      <!DOCTYPE HTML>
      <html>
        <if expr="lang == 'fr'">
          bonjour
        </if>
        <if expr='lang == "de"'>
          hallo
        </if>
        </if>
      </html>
      ''',
    }

    tmp_dir = util.TempDir(files)

    with self.assertRaises(Exception) as cm:
      html_inline.GetResourceFilenames(tmp_dir.GetPath('index.html'), None)
    self.failUnlessEqual(str(cm.exception), 'Unmatched </if>')
    tmp_dir.CleanUp()

  def testCompressedJavaScript(self):
    '''Tests that ".src=" doesn't treat as a tag.'''

    files = {
      'index.js': '''
      if(i<j)a.src="hoge.png";
      ''',
    }

    source_resources = set()
    tmp_dir = util.TempDir(files)
    for filename in files:
      source_resources.add(tmp_dir.GetPath(filename))

    resources = html_inline.GetResourceFilenames(tmp_dir.GetPath('index.js'),
                                                 None)
    resources.add(tmp_dir.GetPath('index.js'))
    self.failUnlessEqual(resources, source_resources)
    tmp_dir.CleanUp()

  def testInlineCSSImports(self):
    '''Tests that @import directives in inlined CSS files are inlined too.
    '''

    files = {
      'index.html': '''
      <html>
      <head>
      <link rel="stylesheet" href="css/test.css">
      </head>
      </html>
      ''',

      'css/test.css': '''
      @import url('test2.css');
      blink {
        display: none;
      }
      ''',

      'css/test2.css': '''
      .image {
        background: url('../images/test.png');
      }
      '''.strip(),

      'images/test.png': 'PNG DATA'
    }

    expected_inlined = '''
      <html>
      <head>
      <style>
      .image {
        background: url('data:image/png;base64,UE5HIERBVEE=');
      }
      blink {
        display: none;
      }
      </style>
      </head>
      </html>
      '''

    source_resources = set()
    tmp_dir = util.TempDir(files)
    for filename in files:
      source_resources.add(tmp_dir.GetPath(util.normpath(filename)))

    result = html_inline.DoInline(tmp_dir.GetPath('index.html'), None)
    resources = result.inlined_files
    resources.add(tmp_dir.GetPath('index.html'))
    self.failUnlessEqual(resources, source_resources)
    self.failUnlessEqual(expected_inlined,
                         util.FixLineEnd(result.inlined_data, '\n'))

    tmp_dir.CleanUp()

  def testInlineIgnoresPolymerBindings(self):
    '''Tests that polymer bindings are ignored when inlining.
    '''

    files = {
      'index.html': '''
      <html>
      <head>
      <link rel="stylesheet" href="test.css">
      </head>
      <body>
        <iron-icon src="[[icon]]"></iron-icon><!-- Should be ignored. -->
        <iron-icon src="{{src}}"></iron-icon><!-- Also ignored. -->
        <!-- [[image]] should be ignored. -->
        <div style="background: url([[image]]),
                                url('test.png');">
        </div>
        <div style="background: url('test.png'),
                                url([[image]]);">
        </div>
      </body>
      </html>
      ''',

      'test.css': '''
      .image {
        background: url('test.png');
        background-image: url([[ignoreMe]]);
        background-image: image-set(url({{alsoMe}}), 1x);
        background-image: image-set(
            url({{ignore}}) 1x,
            url('test.png') 2x);
      }
      ''',

      'test.png': 'PNG DATA'
    }

    expected_inlined = '''
      <html>
      <head>
      <style>
      .image {
        background: url('data:image/png;base64,UE5HIERBVEE=');
        background-image: url([[ignoreMe]]);
        background-image: image-set(url({{alsoMe}}), 1x);
        background-image: image-set(
            url({{ignore}}) 1x,
            url('data:image/png;base64,UE5HIERBVEE=') 2x);
      }
      </style>
      </head>
      <body>
        <iron-icon src="[[icon]]"></iron-icon><!-- Should be ignored. -->
        <iron-icon src="{{src}}"></iron-icon><!-- Also ignored. -->
        <!-- [[image]] should be ignored. -->
        <div style="background: url([[image]]),
                                url('data:image/png;base64,UE5HIERBVEE=');">
        </div>
        <div style="background: url('data:image/png;base64,UE5HIERBVEE='),
                                url([[image]]);">
        </div>
      </body>
      </html>
      '''

    source_resources = set()
    tmp_dir = util.TempDir(files)
    for filename in files:
      source_resources.add(tmp_dir.GetPath(util.normpath(filename)))

    result = html_inline.DoInline(tmp_dir.GetPath('index.html'), None)
    resources = result.inlined_files
    resources.add(tmp_dir.GetPath('index.html'))
    self.failUnlessEqual(resources, source_resources)
    self.failUnlessEqual(expected_inlined,
                         util.FixLineEnd(result.inlined_data, '\n'))

    tmp_dir.CleanUp()

  def testInlineCSSWithIncludeDirective(self):
    '''Tests that include directive in external css files also inlined'''

    files = {
      'index.html': '''
      <html>
      <head>
      <link rel="stylesheet" href="foo.css">
      </head>
      </html>
      ''',

      'foo.css': '''<include src="style.css">''',

      'style.css': '''
      <include src="style2.css">
      blink {
        display: none;
      }
      ''',
      'style2.css': '''h1 {}''',
    }

    expected_inlined = '''
      <html>
      <head>
      <style>
      h1 {}
      blink {
        display: none;
      }
      </style>
      </head>
      </html>
      '''

    source_resources = set()
    tmp_dir = util.TempDir(files)
    for filename in files:
      source_resources.add(tmp_dir.GetPath(filename))

    result = html_inline.DoInline(tmp_dir.GetPath('index.html'), None)
    resources = result.inlined_files
    resources.add(tmp_dir.GetPath('index.html'))
    self.failUnlessEqual(resources, source_resources)
    self.failUnlessEqual(expected_inlined,
                         util.FixLineEnd(result.inlined_data, '\n'))
    tmp_dir.CleanUp()

  def testCssIncludedFileNames(self):
    '''Tests that all included files from css are returned'''

    files = {
      'index.html': '''
      <!DOCTYPE HTML>
      <html>
        <head>
          <link rel="stylesheet" href="test.css">
        </head>
        <body>
        </body>
      </html>
      ''',

      'test.css': '''
      <include src="test2.css">
      ''',

      'test2.css': '''
      <include src="test3.css">
      .image {
        background: url('test.png');
      }
      ''',

      'test3.css': '''h1 {}''',

      'test.png': 'PNG DATA'
    }

    source_resources = set()
    tmp_dir = util.TempDir(files)
    for filename in files:
      source_resources.add(tmp_dir.GetPath(filename))

    resources = html_inline.GetResourceFilenames(tmp_dir.GetPath('index.html'),
                                                 None)
    resources.add(tmp_dir.GetPath('index.html'))
    self.failUnlessEqual(resources, source_resources)
    tmp_dir.CleanUp()

  def testInlineCSSLinks(self):
    '''Tests that only CSS files referenced via relative URLs are inlined.'''

    files = {
      'index.html': '''
      <html>
      <head>
      <link rel="stylesheet" href="foo.css">
      <link rel="stylesheet" href="chrome://resources/bar.css">
      </head>
      </html>
      ''',

      'foo.css': '''
      @import url(chrome://resources/blurp.css);
      blink {
        display: none;
      }
      ''',
    }

    expected_inlined = '''
      <html>
      <head>
      <style>
      @import url(chrome://resources/blurp.css);
      blink {
        display: none;
      }
      </style>
      <link rel="stylesheet" href="chrome://resources/bar.css">
      </head>
      </html>
      '''

    source_resources = set()
    tmp_dir = util.TempDir(files)
    for filename in files:
      source_resources.add(tmp_dir.GetPath(filename))

    result = html_inline.DoInline(tmp_dir.GetPath('index.html'), None)
    resources = result.inlined_files
    resources.add(tmp_dir.GetPath('index.html'))
    self.failUnlessEqual(resources, source_resources)
    self.failUnlessEqual(expected_inlined,
                         util.FixLineEnd(result.inlined_data, '\n'))
    tmp_dir.CleanUp()

  def testFilenameVariableExpansion(self):
    '''Tests that variables are expanded in filenames before inlining.'''

    files = {
      'index.html': '''
      <html>
      <head>
      <link rel="stylesheet" href="style[WHICH].css">
      <script src="script[WHICH].js"></script>
      </head>
      <include src="tmpl[WHICH].html">
      <img src="img[WHICH].png">
      </html>
      ''',
      'style1.css': '''h1 {}''',
      'tmpl1.html': '''<h1></h1>''',
      'script1.js': '''console.log('hello');''',
      'img1.png': '''abc''',
    }

    expected_inlined = '''
      <html>
      <head>
      <style>h1 {}</style>
      <script>console.log('hello');</script>
      </head>
      <h1></h1>
      <img src="data:image/png;base64,YWJj">
      </html>
      '''

    source_resources = set()
    tmp_dir = util.TempDir(files)
    for filename in files:
      source_resources.add(tmp_dir.GetPath(filename))

    def replacer(var, repl):
      return lambda filename: filename.replace('[%s]' % var, repl)

    # Test normal inlining.
    result = html_inline.DoInline(
        tmp_dir.GetPath('index.html'),
        None,
        filename_expansion_function=replacer('WHICH', '1'))
    resources = result.inlined_files
    resources.add(tmp_dir.GetPath('index.html'))
    self.failUnlessEqual(resources, source_resources)
    self.failUnlessEqual(expected_inlined,
                         util.FixLineEnd(result.inlined_data, '\n'))

    # Test names-only inlining.
    result = html_inline.DoInline(
        tmp_dir.GetPath('index.html'),
        None,
        names_only=True,
        filename_expansion_function=replacer('WHICH', '1'))
    resources = result.inlined_files
    resources.add(tmp_dir.GetPath('index.html'))
    self.failUnlessEqual(resources, source_resources)
    tmp_dir.CleanUp()

  def testWithCloseTags(self):
    '''Tests that close tags are removed.'''

    files = {
      'index.html': '''
      <html>
      <head>
      <link rel="stylesheet" href="style1.css"></link>
      <link rel="stylesheet" href="style2.css">
      </link>
      <link rel="stylesheet" href="style2.css"
      >
      </link>
      <script src="script1.js"></script>
      </head>
      <include src="tmpl1.html"></include>
      <include src="tmpl2.html">
      </include>
      <include src="tmpl2.html"
      >
      </include>
      <img src="img1.png">
      <include src='single-double-quotes.html"></include>
      <include src="double-single-quotes.html'></include>
      </html>
      ''',
      'style1.css': '''h1 {}''',
      'style2.css': '''h2 {}''',
      'tmpl1.html': '''<h1></h1>''',
      'tmpl2.html': '''<h2></h2>''',
      'script1.js': '''console.log('hello');''',
      'img1.png': '''abc''',
    }

    expected_inlined = '''
      <html>
      <head>
      <style>h1 {}</style>
      <style>h2 {}</style>
      <style>h2 {}</style>
      <script>console.log('hello');</script>
      </head>
      <h1></h1>
      <h2></h2>
      <h2></h2>
      <img src="data:image/png;base64,YWJj">
      <include src='single-double-quotes.html"></include>
      <include src="double-single-quotes.html'></include>
      </html>
      '''

    source_resources = set()
    tmp_dir = util.TempDir(files)
    for filename in files:
      source_resources.add(tmp_dir.GetPath(filename))

    # Test normal inlining.
    result = html_inline.DoInline(
        tmp_dir.GetPath('index.html'),
        None)
    resources = result.inlined_files
    resources.add(tmp_dir.GetPath('index.html'))
    self.failUnlessEqual(resources, source_resources)
    self.failUnlessEqual(expected_inlined,
                         util.FixLineEnd(result.inlined_data, '\n'))
    tmp_dir.CleanUp()

  def testCommentedJsInclude(self):
    '''Tests that <include> works inside a comment.'''

    files = {
      'include.js': '// <include src="other.js">',
      'other.js': '// Copyright somebody\nalert(1);',
    }

    expected_inlined = '// Copyright somebody\nalert(1);'

    source_resources = set()
    tmp_dir = util.TempDir(files)
    for filename in files:
      source_resources.add(tmp_dir.GetPath(filename))

    result = html_inline.DoInline(tmp_dir.GetPath('include.js'), None)
    resources = result.inlined_files
    resources.add(tmp_dir.GetPath('include.js'))
    self.failUnlessEqual(resources, source_resources)
    self.failUnlessEqual(expected_inlined,
                         util.FixLineEnd(result.inlined_data, '\n'))
    tmp_dir.CleanUp()

  def testCommentedJsIf(self):
    '''Tests that <if> works inside a comment.'''

    files = {
      'if.js': '''
      // <if expr="True">
      yep();
      // </if>

      // <if expr="False">
      nope();
      // </if>
      ''',
    }

    expected_inlined = '''
      // 
      yep();
      // 

      // 
      '''

    source_resources = set()
    tmp_dir = util.TempDir(files)
    for filename in files:
      source_resources.add(tmp_dir.GetPath(filename))

    class FakeGrdNode(object):
      def EvaluateCondition(self, cond):
        return eval(cond)

    result = html_inline.DoInline(tmp_dir.GetPath('if.js'), FakeGrdNode())
    resources = result.inlined_files

    resources.add(tmp_dir.GetPath('if.js'))
    self.failUnlessEqual(resources, source_resources)
    self.failUnlessEqual(expected_inlined,
                         util.FixLineEnd(result.inlined_data, '\n'))
    tmp_dir.CleanUp()

  def testImgSrcset(self):
    '''Tests that img srcset="" attributes are converted.'''

    # Note that there is no space before "img10.png" and that
    # "img11.png" has no descriptor.
    files = {
      'index.html': '''
      <html>
      <img src="img1.png" srcset="img2.png 1x, img3.png 2x">
      <img src="img4.png" srcset=" img5.png   1x , img6.png 2x ">
      <img src="chrome://theme/img11.png" srcset="img7.png 1x, '''\
          '''chrome://theme/img13.png 2x">
      <img srcset="img8.png 300w, img9.png 11E-2w,img10.png -1e2w">
      <img srcset="img11.png">
      <img srcset="img11.png, img2.png 1x">
      <img srcset="img2.png 1x, img11.png">
      </html>
      ''',
      'img1.png': '''a1''',
      'img2.png': '''a2''',
      'img3.png': '''a3''',
      'img4.png': '''a4''',
      'img5.png': '''a5''',
      'img6.png': '''a6''',
      'img7.png': '''a7''',
      'img8.png': '''a8''',
      'img9.png': '''a9''',
      'img10.png': '''a10''',
      'img11.png': '''a11''',
    }

    expected_inlined = '''
      <html>
      <img src="data:image/png;base64,YTE=" srcset="data:image/png;base64,'''\
          '''YTI= 1x,data:image/png;base64,YTM= 2x">
      <img src="data:image/png;base64,YTQ=" srcset="data:image/png;base64,'''\
          '''YTU= 1x,data:image/png;base64,YTY= 2x">
      <img src="chrome://theme/img11.png" srcset="data:image/png;base64,'''\
          '''YTc= 1x,chrome://theme/img13.png 2x">
      <img srcset="data:image/png;base64,YTg= 300w,data:image/png;base64,'''\
          '''YTk= 11E-2w,data:image/png;base64,YTEw -1e2w">
      <img srcset="data:image/png;base64,YTEx">
      <img srcset="data:image/png;base64,YTEx,data:image/png;base64,YTI= 1x">
      <img srcset="data:image/png;base64,YTI= 1x,data:image/png;base64,YTEx">
      </html>
      '''

    source_resources = set()
    tmp_dir = util.TempDir(files)
    for filename in files:
      source_resources.add(tmp_dir.GetPath(filename))

    # Test normal inlining.
    result = html_inline.DoInline(
        tmp_dir.GetPath('index.html'),
        None)
    resources = result.inlined_files
    resources.add(tmp_dir.GetPath('index.html'))
    self.failUnlessEqual(resources, source_resources)
    self.failUnlessEqual(expected_inlined,
                         util.FixLineEnd(result.inlined_data, '\n'))
    tmp_dir.CleanUp()

  def testImgSrcsetIgnoresI18n(self):
    '''Tests that $i18n{...} strings are ignored when inlining.
    '''

    src_html = '''
      <html>
      <head></head>
      <body>
        <img srcset="$i18n{foo}">
      </body>
      </html>
      '''

    files = {
      'index.html': src_html,
    }

    expected_inlined = src_html

    source_resources = set()
    tmp_dir = util.TempDir(files)
    for filename in files:
      source_resources.add(tmp_dir.GetPath(util.normpath(filename)))

    result = html_inline.DoInline(tmp_dir.GetPath('index.html'), None)
    resources = result.inlined_files
    resources.add(tmp_dir.GetPath('index.html'))
    self.failUnlessEqual(resources, source_resources)
    self.failUnlessEqual(expected_inlined,
                         util.FixLineEnd(result.inlined_data, '\n'))
    tmp_dir.CleanUp()

  def testSourceSrcset(self):
    '''Tests that source srcset="" attributes are converted.'''

    # Note that there is no space before "img10.png" and that
    # "img11.png" has no descriptor.
    files = {
      'index.html': '''
      <html>
      <source src="img1.png" srcset="img2.png 1x, img3.png 2x">
      <source src="img4.png" srcset=" img5.png   1x , img6.png 2x ">
      <source src="chrome://theme/img11.png" srcset="img7.png 1x, '''\
          '''chrome://theme/img13.png 2x">
      <source srcset="img8.png 300w, img9.png 11E-2w,img10.png -1e2w">
      <source srcset="img11.png">
      </html>
      ''',
      'img1.png': '''a1''',
      'img2.png': '''a2''',
      'img3.png': '''a3''',
      'img4.png': '''a4''',
      'img5.png': '''a5''',
      'img6.png': '''a6''',
      'img7.png': '''a7''',
      'img8.png': '''a8''',
      'img9.png': '''a9''',
      'img10.png': '''a10''',
      'img11.png': '''a11''',
    }

    expected_inlined = '''
      <html>
      <source src="data:image/png;base64,YTE=" srcset="data:image/png;'''\
          '''base64,YTI= 1x,data:image/png;base64,YTM= 2x">
      <source src="data:image/png;base64,YTQ=" srcset="data:image/png;'''\
          '''base64,YTU= 1x,data:image/png;base64,YTY= 2x">
      <source src="chrome://theme/img11.png" srcset="data:image/png;'''\
          '''base64,YTc= 1x,chrome://theme/img13.png 2x">
      <source srcset="data:image/png;base64,YTg= 300w,data:image/png;'''\
          '''base64,YTk= 11E-2w,data:image/png;base64,YTEw -1e2w">
      <source srcset="data:image/png;base64,YTEx">
      </html>
      '''

    source_resources = set()
    tmp_dir = util.TempDir(files)
    for filename in files:
      source_resources.add(tmp_dir.GetPath(filename))

    # Test normal inlining.
    result = html_inline.DoInline(tmp_dir.GetPath('index.html'), None)
    resources = result.inlined_files
    resources.add(tmp_dir.GetPath('index.html'))
    self.failUnlessEqual(resources, source_resources)
    self.failUnlessEqual(expected_inlined,
                         util.FixLineEnd(result.inlined_data, '\n'))
    tmp_dir.CleanUp()

  def testConditionalInclude(self):
    '''Tests that output and dependency generation includes only files not'''\
        ''' blocked by  <if> macros.'''

    files = {
      'index.html': '''
      <html>
      <if expr="True">
        <img src="img1.png" srcset="img2.png 1x, img3.png 2x">
      </if>
      <if expr="False">
        <img src="img4.png" srcset=" img5.png 1x, img6.png 2x ">
      </if>
      <if expr="True">
        <img src="chrome://theme/img11.png" srcset="img7.png 1x, '''\
            '''chrome://theme/img13.png 2x">
      </if>
      <img srcset="img8.png 300w, img9.png 11E-2w,img10.png -1e2w">
      </html>
      ''',
      'img1.png': '''a1''',
      'img2.png': '''a2''',
      'img3.png': '''a3''',
      'img4.png': '''a4''',
      'img5.png': '''a5''',
      'img6.png': '''a6''',
      'img7.png': '''a7''',
      'img8.png': '''a8''',
      'img9.png': '''a9''',
      'img10.png': '''a10''',
    }

    expected_inlined = '''
      <html>
      <img src="data:image/png;base64,YTE=" srcset="data:image/png;base64,'''\
          '''YTI= 1x,data:image/png;base64,YTM= 2x">
      <img src="chrome://theme/img11.png" srcset="data:image/png;base64,'''\
          '''YTc= 1x,chrome://theme/img13.png 2x">
      <img srcset="data:image/png;base64,YTg= 300w,data:image/png;base64,'''\
          '''YTk= 11E-2w,data:image/png;base64,YTEw -1e2w">
      </html>
      '''

    expected_files = [
      'index.html',
      'img1.png',
      'img2.png',
      'img3.png',
      'img7.png',
      'img8.png',
      'img9.png',
      'img10.png'
    ]

    source_resources = set()
    tmp_dir = util.TempDir(files)
    for filename in expected_files:
      source_resources.add(tmp_dir.GetPath(filename))

    class FakeGrdNode(object):
      def EvaluateCondition(self, cond):
        return eval(cond)

    # Test normal inlining.
    result = html_inline.DoInline(
        tmp_dir.GetPath('index.html'),
        FakeGrdNode())
    resources = result.inlined_files
    resources.add(tmp_dir.GetPath('index.html'))
    self.failUnlessEqual(resources, source_resources)

    # ignore whitespace
    expected_inlined = re.sub(r'\s+', ' ', expected_inlined)
    actually_inlined = re.sub(r'\s+', ' ',
                              util.FixLineEnd(result.inlined_data, '\n'))
    self.failUnlessEqual(expected_inlined, actually_inlined);
    tmp_dir.CleanUp()

  def testPreprocessOnlyEvaluatesIncludeAndIf(self):
    '''Tests that preprocess_only=true evaluates <include> and <if> only.  '''

    files = {
      'index.html': '''
      <html>
        <head>
          <link rel="stylesheet" href="not_inlined.css">
          <script src="also_not_inlined.js">
        </head>
        <body>
          <include src="inline_this.html">
          <if expr="True">
            <p>'if' should be evaluated.</p>
          </if>
        </body>
      </html>
      ''',
      'not_inlined.css': ''' /* <link> should not be inlined. */ ''',
      'also_not_inlined.js': ''' // <script> should not be inlined. ''',
      'inline_this.html': ''' <p>'include' should be inlined.</p> '''
    }

    expected_inlined = '''
      <html>
        <head>
          <link rel="stylesheet" href="not_inlined.css">
          <script src="also_not_inlined.js">
        </head>
        <body>
          <p>'include' should be inlined.</p>
          <p>'if' should be evaluated.</p>
        </body>
      </html>
      '''

    source_resources = set()
    tmp_dir = util.TempDir(files)
    source_resources.add(tmp_dir.GetPath('index.html'))
    source_resources.add(tmp_dir.GetPath('inline_this.html'))

    result = html_inline.DoInline(tmp_dir.GetPath('index.html'), None,
                                  preprocess_only=True)
    resources = result.inlined_files
    resources.add(tmp_dir.GetPath('index.html'))
    self.failUnlessEqual(resources, source_resources)

    # Ignore whitespace
    expected_inlined = re.sub(r'\s+', ' ', expected_inlined)
    actually_inlined = re.sub(r'\s+', ' ',
                              util.FixLineEnd(result.inlined_data, '\n'))
    self.failUnlessEqual(expected_inlined, actually_inlined)

    tmp_dir.CleanUp()

  def testPreprocessOnlyAppliesRecursively(self):
    '''Tests that preprocess_only=true propagates to included files. '''

    files = {
      'index.html': '''
      <html>
        <include src="outer_include.html">
      </html>
      ''',
      'outer_include.html': '''
      <include src="inner_include.html">
      <link rel="stylesheet" href="not_inlined.css">
      ''',
      'inner_include.html': ''' <p>This should be inlined in index.html</p> ''',
      'not_inlined.css': ''' /* This should not be inlined. */ '''
    }

    expected_inlined = '''
      <html>
        <p>This should be inlined in index.html</p>
        <link rel="stylesheet" href="not_inlined.css">
      </html>
      '''

    source_resources = set()
    tmp_dir = util.TempDir(files)
    source_resources.add(tmp_dir.GetPath('index.html'))
    source_resources.add(tmp_dir.GetPath('outer_include.html'))
    source_resources.add(tmp_dir.GetPath('inner_include.html'))

    result = html_inline.DoInline(tmp_dir.GetPath('index.html'), None,
                                  preprocess_only=True)
    resources = result.inlined_files
    resources.add(tmp_dir.GetPath('index.html'))
    self.failUnlessEqual(resources, source_resources)

    # Ignore whitespace
    expected_inlined = re.sub(r'\s+', ' ', expected_inlined)
    actually_inlined = re.sub(r'\s+', ' ',
                              util.FixLineEnd(result.inlined_data, '\n'))
    self.failUnlessEqual(expected_inlined, actually_inlined)

    tmp_dir.CleanUp()

if __name__ == '__main__':
  unittest.main()
