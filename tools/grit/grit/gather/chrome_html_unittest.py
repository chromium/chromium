#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.gather.chrome_html'''

from __future__ import print_function

import os
import re
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

import unittest

from grit import lazy_re
from grit import util
from grit.gather import chrome_html


_NEW_LINE = lazy_re.compile('(\r\n|\r|\n)', re.MULTILINE)


def StandardizeHtml(text):
  '''Standardizes the newline format and png mime type in Html text.'''
  return _NEW_LINE.sub('\n', text).replace('data:image/x-png;',
                                           'data:image/png;')


class ChromeHtmlUnittest(unittest.TestCase):
  '''Unit tests for ChromeHtml.'''

  def testFileResources(self):
    '''Tests inlined image file resources with available high DPI assets.'''

    tmp_dir = util.TempDir({
      'index.html': '''
      <!DOCTYPE HTML>
      <html>
        <head>
          <link rel="stylesheet" href="test.css">
        </head>
        <body>
          <!-- Don't need a body. -->
        </body>
      </html>
      ''',

      'test.css': '''
      .image {
        background: url('test.png');
      }
      ''',

      'test.png': 'PNG DATA',

      '1.4x/test.png': '1.4x PNG DATA',

      '1.8x/test.png': '1.8x PNG DATA',
    })

    html = chrome_html.ChromeHtml(tmp_dir.GetPath('index.html'))
    html.SetDefines({'scale_factors': '1.4x,1.8x'})
    html.SetAttributes({'flattenhtml': 'true'})
    html.Parse()
    self.failUnlessEqual(StandardizeHtml(html.GetData('en', 'utf-8')),
                         StandardizeHtml('''
      <!DOCTYPE HTML>
      <html>
        <head>
          <style>
      .image {
        background: -webkit-image-set(url('data:image/png;base64,UE5HIERBVEE=') 1x, url('data:image/png;base64,MS40eCBQTkcgREFUQQ==') 1.4x, url('data:image/png;base64,MS44eCBQTkcgREFUQQ==') 1.8x);
      }
      </style>
        </head>
        <body>
          <!-- Don't need a body. -->
        </body>
      </html>
      '''))
    tmp_dir.CleanUp()

  def testFileResourcesImageTag(self):
    '''Tests inlined image file resources with available high DPI assets on
    an image tag.'''

    tmp_dir = util.TempDir({
      'index.html': '''
      <!DOCTYPE HTML>
      <html>
        <body>
          <img id="foo" src="test.png">
        </body>
      </html>
      ''',

      'test.png': 'PNG DATA',

      '2x/test.png': '2x PNG DATA',
    })

    html = chrome_html.ChromeHtml(tmp_dir.GetPath('index.html'))
    html.SetDefines({'scale_factors': '2x'})
    html.SetAttributes({'flattenhtml': 'true'})
    html.Parse()
    self.failUnlessEqual(StandardizeHtml(html.GetData('en', 'utf-8')),
                         StandardizeHtml('''
      <!DOCTYPE HTML>
      <html>
        <body>
          <img id="foo" src="data:image/png;base64,UE5HIERBVEE=" style="content: -webkit-image-set(url('data:image/png;base64,UE5HIERBVEE=') 1x, url('data:image/png;base64,MnggUE5HIERBVEE=') 2x);">
        </body>
      </html>
      '''))
    tmp_dir.CleanUp()

  def testFileResourcesNoFlatten(self):
    '''Tests non-inlined image file resources with available high DPI assets.'''

    tmp_dir = util.TempDir({
      'test.css': '''
      .image {
        background: url('test.png');
      }
      ''',

      'test.png': 'PNG DATA',

      '1.4x/test.png': '1.4x PNG DATA',

      '1.8x/test.png': '1.8x PNG DATA',
    })

    html = chrome_html.ChromeHtml(tmp_dir.GetPath('test.css'))
    html.SetDefines({'scale_factors': '1.4x,1.8x'})
    html.SetAttributes({'flattenhtml': 'false'})
    html.Parse()
    self.failUnlessEqual(StandardizeHtml(html.GetData('en', 'utf-8')),
                         StandardizeHtml('''
      .image {
        background: -webkit-image-set(url('test.png') 1x, url('1.4x/test.png') 1.4x, url('1.8x/test.png') 1.8x);
      }
      '''))
    tmp_dir.CleanUp()

  def testFileResourcesNoFlattenSubdir(self):
    '''Tests non-inlined image file resources w/high DPI assets in subdirs.'''

    tmp_dir = util.TempDir({
      'test.css': '''
      .image {
        background: url('sub/test.png');
      }
      ''',

      'sub/test.png': 'PNG DATA',

      'sub/1.4x/test.png': '1.4x PNG DATA',

      'sub/1.8x/test.png': '1.8x PNG DATA',
    })

    html = chrome_html.ChromeHtml(tmp_dir.GetPath('test.css'))
    html.SetDefines({'scale_factors': '1.4x,1.8x'})
    html.SetAttributes({'flattenhtml': 'false'})
    html.Parse()
    self.failUnlessEqual(StandardizeHtml(html.GetData('en', 'utf-8')),
                         StandardizeHtml('''
      .image {
        background: -webkit-image-set(url('sub/test.png') 1x, url('sub/1.4x/test.png') 1.4x, url('sub/1.8x/test.png') 1.8x);
      }
      '''))
    tmp_dir.CleanUp()

  def testFileResourcesPreprocess(self):
    '''Tests preprocessed image file resources with available high DPI
    assets.'''

    tmp_dir = util.TempDir({
      'test.css': '''
      .image {
        background: url('test.png');
      }
      ''',

      'test.png': 'PNG DATA',

      '1.4x/test.png': '1.4x PNG DATA',

      '1.8x/test.png': '1.8x PNG DATA',
    })

    html = chrome_html.ChromeHtml(tmp_dir.GetPath('test.css'))
    html.SetDefines({'scale_factors': '1.4x,1.8x'})
    html.SetAttributes({'flattenhtml': 'false', 'preprocess': 'true'})
    html.Parse()
    self.failUnlessEqual(StandardizeHtml(html.GetData('en', 'utf-8')),
                         StandardizeHtml('''
      .image {
        background: -webkit-image-set(url('test.png') 1x, url('1.4x/test.png') 1.4x, url('1.8x/test.png') 1.8x);
      }
      '''))
    tmp_dir.CleanUp()

  def testFileResourcesDoubleQuotes(self):
    '''Tests inlined image file resources if url() filename is double quoted.'''

    tmp_dir = util.TempDir({
      'test.css': '''
      .image {
        background: url("test.png");
      }
      ''',

      'test.png': 'PNG DATA',

      '2x/test.png': '2x PNG DATA',
    })

    html = chrome_html.ChromeHtml(tmp_dir.GetPath('test.css'))
    html.SetDefines({'scale_factors': '2x'})
    html.SetAttributes({'flattenhtml': 'true'})
    html.Parse()
    self.failUnlessEqual(StandardizeHtml(html.GetData('en', 'utf-8')),
                         StandardizeHtml('''
      .image {
        background: -webkit-image-set(url("data:image/png;base64,UE5HIERBVEE=") 1x, url("data:image/png;base64,MnggUE5HIERBVEE=") 2x);
      }
      '''))
    tmp_dir.CleanUp()

  def testFileResourcesNoQuotes(self):
    '''Tests inlined image file resources when url() filename is unquoted.'''

    tmp_dir = util.TempDir({
      'test.css': '''
      .image {
        background: url(test.png);
      }
      ''',

      'test.png': 'PNG DATA',

      '2x/test.png': '2x PNG DATA',
    })

    html = chrome_html.ChromeHtml(tmp_dir.GetPath('test.css'))
    html.SetDefines({'scale_factors': '2x'})
    html.SetAttributes({'flattenhtml': 'true'})
    html.Parse()
    self.failUnlessEqual(StandardizeHtml(html.GetData('en', 'utf-8')),
                         StandardizeHtml('''
      .image {
        background: -webkit-image-set(url(data:image/png;base64,UE5HIERBVEE=) 1x, url(data:image/png;base64,MnggUE5HIERBVEE=) 2x);
      }
      '''))
    tmp_dir.CleanUp()

  def testFileResourcesSubdirs(self):
    '''Tests inlined image file resources if url() filename is in a subdir.'''

    tmp_dir = util.TempDir({
      'test.css': '''
      .image {
        background: url('some/sub/path/test.png');
      }
      ''',

      'some/sub/path/test.png': 'PNG DATA',

      'some/sub/path/2x/test.png': '2x PNG DATA',
    })

    html = chrome_html.ChromeHtml(tmp_dir.GetPath('test.css'))
    html.SetDefines({'scale_factors': '2x'})
    html.SetAttributes({'flattenhtml': 'true'})
    html.Parse()
    self.failUnlessEqual(StandardizeHtml(html.GetData('en', 'utf-8')),
                         StandardizeHtml('''
      .image {
        background: -webkit-image-set(url('data:image/png;base64,UE5HIERBVEE=') 1x, url('data:image/png;base64,MnggUE5HIERBVEE=') 2x);
      }
      '''))
    tmp_dir.CleanUp()

  def testFileResourcesNoFile(self):
    '''Tests inlined image file resources without available high DPI assets.'''

    tmp_dir = util.TempDir({
      'index.html': '''
      <!DOCTYPE HTML>
      <html>
        <head>
          <link rel="stylesheet" href="test.css">
        </head>
        <body>
          <!-- Don't need a body. -->
        </body>
      </html>
      ''',

      'test.css': '''
      .image {
        background: url('test.png');
      }
      ''',

      'test.png': 'PNG DATA',
    })

    html = chrome_html.ChromeHtml(tmp_dir.GetPath('index.html'))
    html.SetDefines({'scale_factors': '2x'})
    html.SetAttributes({'flattenhtml': 'true'})
    html.Parse()
    self.failUnlessEqual(StandardizeHtml(html.GetData('en', 'utf-8')),
                         StandardizeHtml('''
      <!DOCTYPE HTML>
      <html>
        <head>
          <style>
      .image {
        background: url('data:image/png;base64,UE5HIERBVEE=');
      }
      </style>
        </head>
        <body>
          <!-- Don't need a body. -->
        </body>
      </html>
      '''))
    tmp_dir.CleanUp()

  def testFileResourcesMultipleBackgrounds(self):
    '''Tests inlined image file resources with two url()s.'''

    tmp_dir = util.TempDir({
      'test.css': '''
      .image {
        background: url(test.png), url(test.png);
      }
      ''',

      'test.png': 'PNG DATA',

      '2x/test.png': '2x PNG DATA',
    })

    html = chrome_html.ChromeHtml(tmp_dir.GetPath('test.css'))
    html.SetDefines({'scale_factors': '2x'})
    html.SetAttributes({'flattenhtml': 'true'})
    html.Parse()
    self.failUnlessEqual(StandardizeHtml(html.GetData('en', 'utf-8')),
                         StandardizeHtml('''
      .image {
        background: -webkit-image-set(url(data:image/png;base64,UE5HIERBVEE=) 1x, url(data:image/png;base64,MnggUE5HIERBVEE=) 2x), -webkit-image-set(url(data:image/png;base64,UE5HIERBVEE=) 1x, url(data:image/png;base64,MnggUE5HIERBVEE=) 2x);
      }
      '''))
    tmp_dir.CleanUp()

  def testFileResourcesMultipleBackgroundsWithNewline1(self):
    '''Tests inlined image file resources with line break after first url().'''

    tmp_dir = util.TempDir({
      'test.css': '''
      .image {
        background: url(test.png),
                    url(test.png);
      }
      ''',

      'test.png': 'PNG DATA',

      '2x/test.png': '2x PNG DATA',
    })

    html = chrome_html.ChromeHtml(tmp_dir.GetPath('test.css'))
    html.SetDefines({'scale_factors': '2x'})
    html.SetAttributes({'flattenhtml': 'true'})
    html.Parse()
    self.failUnlessEqual(StandardizeHtml(html.GetData('en', 'utf-8')),
                         StandardizeHtml('''
      .image {
        background: -webkit-image-set(url(data:image/png;base64,UE5HIERBVEE=) 1x, url(data:image/png;base64,MnggUE5HIERBVEE=) 2x),
                    -webkit-image-set(url(data:image/png;base64,UE5HIERBVEE=) 1x, url(data:image/png;base64,MnggUE5HIERBVEE=) 2x);
      }
      '''))
    tmp_dir.CleanUp()

  def testFileResourcesMultipleBackgroundsWithNewline2(self):
    '''Tests inlined image file resources with line break before first url()
    and before second url().'''

    tmp_dir = util.TempDir({
      'test.css': '''
      .image {
        background:
          url(test.png),
          url(test.png);
      }
      ''',

      'test.png': 'PNG DATA',

      '2x/test.png': '2x PNG DATA',
    })

    html = chrome_html.ChromeHtml(tmp_dir.GetPath('test.css'))
    html.SetDefines({'scale_factors': '2x'})
    html.SetAttributes({'flattenhtml': 'true'})
    html.Parse()
    self.failUnlessEqual(StandardizeHtml(html.GetData('en', 'utf-8')),
                         StandardizeHtml('''
      .image {
        background: -webkit-image-set(url(data:image/png;base64,UE5HIERBVEE=) 1x, url(data:image/png;base64,MnggUE5HIERBVEE=) 2x),
          -webkit-image-set(url(data:image/png;base64,UE5HIERBVEE=) 1x, url(data:image/png;base64,MnggUE5HIERBVEE=) 2x);
      }
      '''))
    tmp_dir.CleanUp()

  def testFileResourcesCRLF(self):
    '''Tests inlined image file resource when url() is preceded by a Windows
    style line break.'''

    tmp_dir = util.TempDir({
      'test.css': '''
      .image {
        background:\r\nurl(test.png);
      }
      ''',

      'test.png': 'PNG DATA',

      '2x/test.png': '2x PNG DATA',
    })

    html = chrome_html.ChromeHtml(tmp_dir.GetPath('test.css'))
    html.SetDefines({'scale_factors': '2x'})
    html.SetAttributes({'flattenhtml': 'true'})
    html.Parse()
    self.failUnlessEqual(StandardizeHtml(html.GetData('en', 'utf-8')),
                         StandardizeHtml('''
      .image {
        background: -webkit-image-set(url(data:image/png;base64,UE5HIERBVEE=) 1x, url(data:image/png;base64,MnggUE5HIERBVEE=) 2x);
      }
      '''))
    tmp_dir.CleanUp()

  def testThemeResources(self):
    '''Tests inserting high DPI chrome://theme references.'''

    tmp_dir = util.TempDir({
      'index.html': '''
      <!DOCTYPE HTML>
      <html>
        <head>
          <link rel="stylesheet" href="test.css">
        </head>
        <body>
          <!-- Don't need a body. -->
        </body>
      </html>
      ''',

      'test.css': '''
      .image {
        background: url('chrome://theme/IDR_RESOURCE_NAME');
        content: url('chrome://theme/IDR_RESOURCE_NAME_WITH_Q?$1');
      }
      ''',
    })

    html = chrome_html.ChromeHtml(tmp_dir.GetPath('index.html'))
    html.SetDefines({'scale_factors': '2x'})
    html.SetAttributes({'flattenhtml': 'true'})
    html.Parse()
    self.failUnlessEqual(StandardizeHtml(html.GetData('en', 'utf-8')),
                         StandardizeHtml('''
      <!DOCTYPE HTML>
      <html>
        <head>
          <style>
      .image {
        background: -webkit-image-set(url('chrome://theme/IDR_RESOURCE_NAME') 1x, url('chrome://theme/IDR_RESOURCE_NAME@2x') 2x);
        content: -webkit-image-set(url('chrome://theme/IDR_RESOURCE_NAME_WITH_Q?$1') 1x, url('chrome://theme/IDR_RESOURCE_NAME_WITH_Q@2x?$1') 2x);
      }
      </style>
        </head>
        <body>
          <!-- Don't need a body. -->
        </body>
      </html>
      '''))
    tmp_dir.CleanUp()

  def testRemoveUnsupportedScale(self):
    '''Tests removing an unsupported scale factor from an explicit image-set.'''

    tmp_dir = util.TempDir({
      'index.html': '''
      <!DOCTYPE HTML>
      <html>
        <head>
          <link rel="stylesheet" href="test.css">
        </head>
        <body>
          <!-- Don't need a body. -->
        </body>
      </html>
      ''',

      'test.css': '''
      .image {
        background: -webkit-image-set(url('test.png') 1x,
                                      url('test1.4.png') 1.4x,
                                      url('test1.8.png') 1.8x);
      }
      ''',

      'test.png': 'PNG DATA',

      'test1.4.png': '1.4x PNG DATA',

      'test1.8.png': '1.8x PNG DATA',
    })

    html = chrome_html.ChromeHtml(tmp_dir.GetPath('index.html'))
    html.SetDefines({'scale_factors': '1.8x'})
    html.SetAttributes({'flattenhtml': 'true'})
    html.Parse()
    self.failUnlessEqual(StandardizeHtml(html.GetData('en', 'utf-8')),
                         StandardizeHtml('''
      <!DOCTYPE HTML>
      <html>
        <head>
          <style>
      .image {
        background: -webkit-image-set(url('data:image/png;base64,UE5HIERBVEE=') 1x,
                                      url('data:image/png;base64,MS44eCBQTkcgREFUQQ==') 1.8x);
      }
      </style>
        </head>
        <body>
          <!-- Don't need a body. -->
        </body>
      </html>
      '''))
    tmp_dir.CleanUp()

  def testExpandVariablesInFilename(self):
    '''
    Tests variable substitution in filenames while flattening images
    with multiple scale factors.
    '''

    tmp_dir = util.TempDir({
      'index.html': '''
      <!DOCTYPE HTML>
      <html>
        <head>
          <link rel="stylesheet" href="test.css">
        </head>
        <body>
          <!-- Don't need a body. -->
        </body>
      </html>
      ''',

      'test.css': '''
      .image {
        background: url('test[WHICH].png');
      }
      ''',

      'test1.png': 'PNG DATA',
      '1.4x/test1.png': '1.4x PNG DATA',
      '1.8x/test1.png': '1.8x PNG DATA',
    })

    def replacer(var, repl):
      return lambda filename: filename.replace('[%s]' % var, repl)

    html = chrome_html.ChromeHtml(tmp_dir.GetPath('index.html'))
    html.SetDefines({'scale_factors': '1.4x,1.8x'})
    html.SetAttributes({'flattenhtml': 'true'})
    html.SetFilenameExpansionFunction(replacer('WHICH', '1'));
    html.Parse()
    self.failUnlessEqual(StandardizeHtml(html.GetData('en', 'utf-8')),
                         StandardizeHtml('''
      <!DOCTYPE HTML>
      <html>
        <head>
          <style>
      .image {
        background: -webkit-image-set(url('data:image/png;base64,UE5HIERBVEE=') 1x, url('data:image/png;base64,MS40eCBQTkcgREFUQQ==') 1.4x, url('data:image/png;base64,MS44eCBQTkcgREFUQQ==') 1.8x);
      }
      </style>
        </head>
        <body>
          <!-- Don't need a body. -->
        </body>
      </html>
      '''))
    tmp_dir.CleanUp()


if __name__ == '__main__':
  unittest.main()
