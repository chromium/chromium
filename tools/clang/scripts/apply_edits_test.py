#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import apply_edits


def _FindPHB(filepath):
  return apply_edits._FindPrimaryHeaderBasename(filepath)


class FindPrimaryHeaderBasenameTest(unittest.TestCase):
  def testNoOpOnHeader(self):
    self.assertIsNone(_FindPHB('bar.h'))
    self.assertIsNone(_FindPHB('foo/bar.h'))

  def testStripDirectories(self):
    self.assertEqual('bar', _FindPHB('foo/bar.cc'))

  def testStripPlatformSuffix(self):
    self.assertEqual('bar', _FindPHB('bar_posix.cc'))
    self.assertEqual('bar', _FindPHB('bar_unittest.cc'))

  def testStripTestSuffix(self):
    self.assertEqual('bar', _FindPHB('bar_browsertest.cc'))
    self.assertEqual('bar', _FindPHB('bar_unittest.cc'))

  def testStripPlatformAndTestSuffix(self):
    self.assertEqual('bar', _FindPHB('bar_uitest_aura.cc'))
    self.assertEqual('bar', _FindPHB('bar_linux_unittest.cc'))

  def testNoSuffixStrippingWithoutUnderscore(self):
    self.assertEqual('barunittest', _FindPHB('barunittest.cc'))


def _ApplyEdit(old_contents_string,
               edit,
               contents_filepath="some_file.cc",
               last_edit=None):
  if last_edit is not None:
    assert (last_edit > edit)  # Test or prod caller should ensure.
  ba = bytearray()
  ba.extend(old_contents_string.encode('utf-8'))
  return apply_edits._ApplySingleEdit(contents_filepath,
                                      old_contents_string.encode("utf-8"), edit,
                                      last_edit).decode("utf-8")


def _InsertHeader(old_contents,
                  contents_filepath='foo/impl.cc',
                  new_header_path='new/header.h'):
  edit = apply_edits.Edit("include-user-header", -1, -1,
                          new_header_path.encode("utf-8"))
  return _ApplyEdit(old_contents, edit, contents_filepath)


class InsertIncludeHeaderTest(unittest.TestCase):
  def _assertEqualContents(self, expected, actual):
    if expected != actual:
      print("####################### EXPECTED:")
      print(expected)
      print("####################### ACTUAL:")
      print(actual)
      print("####################### END.")
    self.assertEqual(expected, actual)

  def testSkippingCppComments(self):
    old_contents = '''
// Copyright info here.

#include "old/header.h"
    '''
    expected_new_contents = '''
// Copyright info here.

#include "new/header.h"
#include "old/header.h"
    '''
    new_header_line = '#include "new/header.h'
    self._assertEqualContents(expected_new_contents,
                              _InsertHeader(old_contents))

  def testSkippingCppComments_DocCommentForStruct(self):
    """ This is a regression test for https://crbug.com/1175684 """
    old_contents = '''
// Copyright blah blah...

#ifndef SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_FILTER_H_
#define SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_FILTER_H_

#include <stdint.h>

// Doc comment for a struct.
// Multiline.
struct sock_filter {
  uint16_t code;
};
    '''
    expected_new_contents = '''
// Copyright blah blah...

#ifndef SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_FILTER_H_
#define SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_FILTER_H_

#include <stdint.h>

#include "new/header.h"

// Doc comment for a struct.
// Multiline.
struct sock_filter {
  uint16_t code;
};
    '''
    new_header_line = '#include "new/header.h'
    self._assertEqualContents(expected_new_contents,
                              _InsertHeader(old_contents))

  def testSkippingCppComments_DocCommentForStruct2(self):
    """ This is a regression test for https://crbug.com/1175684 """
    old_contents = '''
// Copyright blah blah...

// Doc comment for a struct.
struct sock_filter {
  uint16_t code;
};
    '''
    expected_new_contents = '''
// Copyright blah blah...

#include "new/header.h"

// Doc comment for a struct.
struct sock_filter {
  uint16_t code;
};
    '''
    new_header_line = '#include "new/header.h'
    self._assertEqualContents(expected_new_contents,
                              _InsertHeader(old_contents))

  def testSkippingCppComments_DocCommentForStruct3(self):
    """ This is a regression test for https://crbug.com/1175684 """
    old_contents = '''
// Doc comment for a struct.
struct sock_filter {
  uint16_t code;
};
    '''
    expected_new_contents = '''
#include "new/header.h"

// Doc comment for a struct.
struct sock_filter {
  uint16_t code;
};
    '''
    new_header_line = '#include "new/header.h'
    self._assertEqualContents(expected_new_contents,
                              _InsertHeader(old_contents))

  def testSkippingCppComments_DocCommentForInclude(self):
    """ This is a regression test for https://crbug.com/1175684 """
    old_contents = '''
// Copyright blah blah...

// System includes.
#include <stdint.h>

// Doc comment for a struct.
struct sock_filter {
  uint16_t code;
};
    '''
    expected_new_contents = '''
// Copyright blah blah...

// System includes.
#include <stdint.h>

#include "new/header.h"

// Doc comment for a struct.
struct sock_filter {
  uint16_t code;
};
    '''
    new_header_line = '#include "new/header.h'
    self._assertEqualContents(expected_new_contents,
                              _InsertHeader(old_contents))

  def testSkippingCppComments_DocCommentForWholeFile(self):
    """ This is a regression test for https://crbug.com/1175684 """
    old_contents = '''
// Copyright blah blah...

// Doc comment for the whole file.

struct sock_filter {
  uint16_t code;
};
    '''
    expected_new_contents = '''
// Copyright blah blah...

// Doc comment for the whole file.

#include "new/header.h"

struct sock_filter {
  uint16_t code;
};
    '''
    new_header_line = '#include "new/header.h'
    self._assertEqualContents(expected_new_contents,
                              _InsertHeader(old_contents))

  def testSkippingOldStyleComments(self):
    old_contents = '''
/* Copyright
 * info here.
 */

#include "old/header.h"
    '''
    expected_new_contents = '''
/* Copyright
 * info here.
 */

#include "new/header.h"
#include "old/header.h"
    '''
    self._assertEqualContents(expected_new_contents,
                              _InsertHeader(old_contents))

  def testSkippingOldStyleComments_NoWhitespaceAtLineStart(self):
    old_contents = '''
/* Copyright
* info here.
*/

#include "old/header.h"
    '''
    expected_new_contents = '''
/* Copyright
* info here.
*/

#include "new/header.h"
#include "old/header.h"
    '''
    self._assertEqualContents(expected_new_contents,
                              _InsertHeader(old_contents))

  def testSkippingSystemHeaders(self):
    old_contents = '''
#include <string>
#include <vector>  // blah

#include "old/header.h"
    '''
    expected_new_contents = '''
#include <string>
#include <vector>  // blah

#include "new/header.h"
#include "old/header.h"
    '''
    self._assertEqualContents(expected_new_contents,
                              _InsertHeader(old_contents))

  def testSkippingPrimaryHeader(self):
    old_contents = '''
// Copyright info here.

#include "foo/impl.h"

#include "old/header.h"
    '''
    expected_new_contents = '''
// Copyright info here.

#include "foo/impl.h"

#include "new/header.h"
#include "old/header.h"
    '''
    self._assertEqualContents(expected_new_contents,
                              _InsertHeader(old_contents))

  def testSimilarNonPrimaryHeader_WithPrimaryHeader(self):
    old_contents = '''
// Copyright info here.

#include "primary/impl.h"  // This is the primary header.

#include "unrelated/impl.h"  // This is *not* the primary header.
#include "zzz/foo.h"
    '''
    expected_new_contents = '''
// Copyright info here.

#include "primary/impl.h"  // This is the primary header.

#include "unrelated/impl.h"  // This is *not* the primary header.
#include "new/header.h"
#include "zzz/foo.h"
    '''
    self._assertEqualContents(expected_new_contents,
                              _InsertHeader(old_contents))

  def testSimilarNonPrimaryHeader_NoPrimaryHeader(self):
    old_contents = '''
// Copyright info here.

#include "unrelated/impl.h"  // This is *not* the primary header.
#include "zzz/foo.h"
    '''
    expected_new_contents = '''
// Copyright info here.

#include "unrelated/impl.h"  // This is *not* the primary header.
#include "new/header.h"
#include "zzz/foo.h"
    '''
    self._assertEqualContents(expected_new_contents,
                              _InsertHeader(old_contents))

  def testSkippingIncludeGuards(self):
    old_contents = '''
#ifndef FOO_IMPL_H_
#define FOO_IMPL_H_

#include "old/header.h"

#endif FOO_IMPL_H_
    '''
    expected_new_contents = '''
#ifndef FOO_IMPL_H_
#define FOO_IMPL_H_

#include "new/header.h"
#include "old/header.h"

#endif FOO_IMPL_H_
    '''
    self._assertEqualContents(
        expected_new_contents,
        _InsertHeader(old_contents, 'foo/impl.h', 'new/header.h'))

  def testSkippingIncludeGuards2(self):
    # This test is based on base/third_party/valgrind/memcheck.h
    old_contents = '''
#ifndef __MEMCHECK_H
#define __MEMCHECK_H

#include "old/header.h"

#endif
    '''
    expected_new_contents = '''
#ifndef __MEMCHECK_H
#define __MEMCHECK_H

#include "new/header.h"
#include "old/header.h"

#endif
    '''
    self._assertEqualContents(expected_new_contents,
                              _InsertHeader(old_contents))

  def testSkippingIncludeGuards3(self):
    # This test is based on base/third_party/xdg_mime/xdgmime.h
    old_contents = '''
#ifndef __XDG_MIME_H__
#define __XDG_MIME_H__

#include "old/header.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef void (*XdgMimeCallback) (void *user_data);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __XDG_MIME_H__ */
    '''
    expected_new_contents = '''
#ifndef __XDG_MIME_H__
#define __XDG_MIME_H__

#include "new/header.h"
#include "old/header.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef void (*XdgMimeCallback) (void *user_data);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __XDG_MIME_H__ */
    '''
    self._assertEqualContents(expected_new_contents,
                              _InsertHeader(old_contents))

  @unittest.skip(
      "Failing test due to regex (in apply_edits.py) not working as expected, please fix."
  )
  def testSkippingIncludeGuards4(self):
    # This test is based on ash/first_run/desktop_cleaner.h and/or
    # components/subresource_filter/core/common/scoped_timers.h and/or
    # device/gamepad/abstract_haptic_gamepad.h
    old_contents = '''
#ifndef ASH_FIRST_RUN_DESKTOP_CLEANER_
#define ASH_FIRST_RUN_DESKTOP_CLEANER_

#include "old/header.h"

namespace ash {
}  // namespace ash

#endif  // ASH_FIRST_RUN_DESKTOP_CLEANER_
    '''
    expected_new_contents = '''
#ifndef ASH_FIRST_RUN_DESKTOP_CLEANER_
#define ASH_FIRST_RUN_DESKTOP_CLEANER_

#include "new/header.h"
#include "old/header.h"

namespace ash {
}  // namespace ash

#endif  // ASH_FIRST_RUN_DESKTOP_CLEANER_
    '''
    self._assertEqualContents(expected_new_contents,
                              _InsertHeader(old_contents))

  @unittest.skip(
      "Failing test due to regex (in apply_edits.py) not working as expected, please fix."
  )
  def testSkippingIncludeGuards5(self):
    # This test is based on third_party/weston/include/GLES2/gl2.h (the |extern
    # "C"| part has been removed to make the test trickier to handle right -
    # otherwise it is easy to see that the header has to be included before the
    # |extern "C"| part).
    #
    # The tricky parts below include:
    # 1. upper + lower case characters allowed in the guard name
    # 2. Having to recognize that GL_APIENTRYP is *not* a guard
    old_contents = '''
#ifndef __gles2_gl2_h_
#define __gles2_gl2_h_ 1

#include <GLES2/gl2platform.h>

#ifndef GL_APIENTRYP
#define GL_APIENTRYP GL_APIENTRY*
#endif

#endif
    '''
    expected_new_contents = '''
#ifndef __gles2_gl2_h_
#define __gles2_gl2_h_ 1

#include <GLES2/gl2platform.h>

#include "new/header.h"

#ifndef GL_APIENTRYP
#define GL_APIENTRYP GL_APIENTRY*
#endif

#endif
    '''
    self._assertEqualContents(expected_new_contents,
                              _InsertHeader(old_contents))

  @unittest.skip(
      "Failing test due to regex (in apply_edits.py) not working as expected, please fix."
  )
  def testSkippingIncludeGuards6(self):
    # This test is based on ios/third_party/blink/src/html_token.h
    old_contents = '''
#ifndef HTMLToken_h
#define HTMLToken_h

#include <stddef.h>
#include <vector>

// ...

#endif
    '''
    expected_new_contents = '''
#ifndef HTMLToken_h
#define HTMLToken_h

#include <stddef.h>
#include <vector>

#include "new/header.h"

// ...

#endif
    '''
    self._assertEqualContents(expected_new_contents,
                              _InsertHeader(old_contents))

  def testNoOpIfAlreadyPresent(self):
    # This tests that the new header won't be inserted (and duplicated)
    # if it is already included.
    old_contents = '''
// Copyright info here.

#include "old/header.h"
#include "new/header.h"
#include "new/header2.h"
    '''
    expected_new_contents = '''
// Copyright info here.

#include "old/header.h"
#include "new/header.h"
#include "new/header2.h"
    '''
    self._assertEqualContents(expected_new_contents,
                              _InsertHeader(old_contents))

  def testNoOpIfAlreadyPresent_WithTrailingComment(self):
    # This tests that the new header won't be inserted (and duplicated)
    # if it is already included.
    old_contents = '''
// Copyright info here.

#include "old/header.h"
#include "new/header.h" // blah
#include "new/header2.h"
    '''
    expected_new_contents = '''
// Copyright info here.

#include "old/header.h"
#include "new/header.h" // blah
#include "new/header2.h"
    '''
    self._assertEqualContents(expected_new_contents,
                              _InsertHeader(old_contents))

  def testNoOldHeaders(self):
    # This tests that an extra new line is inserted after the new header
    # when there are no old headers immediately below.
    old_contents = '''
#include <vector>

struct S {};
    '''
    expected_new_contents = '''
#include <vector>

#include "new/header.h"

struct S {};
    '''
    self._assertEqualContents(expected_new_contents,
                              _InsertHeader(old_contents))

  def testPlatformIfDefs(self):
    # This test is based on
    # //base/third_party/double_conversion/double-conversion/utils.h
    # We need to insert the new header in a non-conditional part.
    old_contents = '''
#ifndef DOUBLE_CONVERSION_UTILS_H_
#define DOUBLE_CONVERSION_UTILS_H_

#include <cstdlib>
#include <cstring>

#ifndef DOUBLE_CONVERSION_UNREACHABLE
#ifdef _MSC_VER
void DOUBLE_CONVERSION_NO_RETURN abort_noreturn();
inline void abort_noreturn() { abort(); }
#define DOUBLE_CONVERSION_UNREACHABLE()   (abort_noreturn())
#else
#define DOUBLE_CONVERSION_UNREACHABLE()   (abort())
#endif
#endif

namespace double_conversion {
    '''
    expected_new_contents = '''
#ifndef DOUBLE_CONVERSION_UTILS_H_
#define DOUBLE_CONVERSION_UTILS_H_

#include <cstdlib>
#include <cstring>

#include "new/header.h"

#ifndef DOUBLE_CONVERSION_UNREACHABLE
#ifdef _MSC_VER
void DOUBLE_CONVERSION_NO_RETURN abort_noreturn();
inline void abort_noreturn() { abort(); }
#define DOUBLE_CONVERSION_UNREACHABLE()   (abort_noreturn())
#else
#define DOUBLE_CONVERSION_UNREACHABLE()   (abort())
#endif
#endif

namespace double_conversion {
    '''
    self._assertEqualContents(expected_new_contents,
                              _InsertHeader(old_contents))

  def testNoOldIncludesAndIfDefs(self):
    # Artificial test: no old #includes + some #ifdefs.  The main focus of the
    # test is ensuring that the new header will be inserted into the
    # unconditional part of the file.
    old_contents = '''
#ifndef NDEBUG
#include "base/logging.h"
#endif

void foo();
    '''
    expected_new_contents = '''
#include "new/header.h"

#ifndef NDEBUG
#include "base/logging.h"
#endif

void foo();
    '''
    self._assertEqualContents(expected_new_contents,
                              _InsertHeader(old_contents))

  def testNoOldIncludesAndIfDefs2(self):
    # Artificial test: no old #includes + some #ifdefs.  The main focus of the
    # test is ensuring that the new header will be inserted into the
    # unconditional part of the file.
    old_contents = '''
#if BUILDFLAG(IS_WIN)
#include "foo_win.h"
#endif

void foo();
    '''
    expected_new_contents = '''
#include "new/header.h"

#if BUILDFLAG(IS_WIN)
#include "foo_win.h"
#endif

void foo();
    '''
    self._assertEqualContents(expected_new_contents,
                              _InsertHeader(old_contents))

  def testUtf8BomMarker(self):
    # Test based on
    # //chrome/browser/ui/views/payments/payment_sheet_view_controller.cc
    # which at some point began as follows:
    # 00000000: efbb bf2f 2f20 436f 7079 7269 6768 7420  ...// Copyright
    #
    # Previous versions of apply_edits.py would not skip the BOM marker when
    # figuring out where to insert the new include header.
    old_contents = u'''\ufeff// Copyright

#include "old/header.h"
    '''
    expected_new_contents = u'''\ufeff// Copyright

#include "new/header.h"
#include "old/header.h"
    '''
    actual = bytearray()
    actual.extend(old_contents.encode('utf-8'))
    expected = bytearray()
    expected.extend(expected_new_contents.encode('utf-8'))
    # Test sanity check (i.e. not an assertion about code under test).
    utf8_bom = [0xef, 0xbb, 0xbf]
    self._assertEqualContents(list(actual[0:3]), utf8_bom)
    self._assertEqualContents(list(expected[0:3]), utf8_bom)
    # Actual test.
    edit = apply_edits.Edit('include-user-header', -1, -1, b"new/header.h")
    actual = apply_edits._ApplySingleEdit("foo/impl.cc", actual, edit, None)
    self._assertEqualContents(expected, actual)


def _CreateReplacement(content_string, old_substring, new_substring):
  """ Test helper for creating an Edit object with the right offset, etc. """
  b_content_string = content_string.encode("utf-8")
  b_old_string = old_substring.encode("utf-8")
  b_new_string = new_substring.encode("utf-8")
  offset = b_content_string.find(b_old_string)
  return apply_edits.Edit('r', offset, len(b_old_string), b_new_string)


class ApplyReplacementTest(unittest.TestCase):
  def testBasics(self):
    old_text = "123 456 789"
    r = _CreateReplacement(old_text, "456", "foo")
    new_text = _ApplyEdit(old_text, r)
    self.assertEqual("123 foo 789", new_text)

  def testMiddleListElementRemoval(self):
    old_text = "(123, 456, 789)  // foobar"
    r = _CreateReplacement(old_text, "456", "")
    new_text = _ApplyEdit(old_text, r)
    self.assertEqual("(123,  789)  // foobar", new_text)

  def testFinalElementRemoval(self):
    old_text = "(123, 456, 789)  // foobar"
    r = _CreateReplacement(old_text, "789", "")
    new_text = _ApplyEdit(old_text, r)
    self.assertEqual("(123, 456)  // foobar", new_text)

  def testConflictingReplacement(self):
    old_text = "123 456 789"
    last = _CreateReplacement(old_text, "456", "foo")
    edit = _CreateReplacement(old_text, "456", "bar")
    expected_msg_regex = 'Conflicting replacement text'
    expected_msg_regex += '.*some_file.cc at offset 4, length 3'
    expected_msg_regex += '.*"bar" != "foo"'
    with self.assertRaisesRegex(ValueError, expected_msg_regex):
      _ApplyEdit(old_text, edit, last_edit=last)

  def testUnrecognizedEditDirective(self):
    old_text = "123 456 789"
    edit = apply_edits.Edit('unknown_directive', 123, 456, "foo")
    expected_msg_regex = 'Unrecognized edit directive "unknown_directive"'
    expected_msg_regex += '.*some_file.cc'
    with self.assertRaisesRegex(ValueError, expected_msg_regex):
      _ApplyEdit(old_text, edit)

  def testOverlappingReplacement(self):
    old_text = "123 456 789"
    last = _CreateReplacement(old_text, "456 789", "foo")
    edit = _CreateReplacement(old_text, "123 456", "bar")
    expected_msg_regex = 'Overlapping replacements'
    expected_msg_regex += '.*some_file.cc'
    expected_msg_regex += '.*offset 0, length 7.*"bar"'
    expected_msg_regex += '.*offset 4, length 7.*"foo"'
    with self.assertRaisesRegex(ValueError, expected_msg_regex):
      _ApplyEdit(old_text, edit, last_edit=last)


if __name__ == '__main__':
  unittest.main()
