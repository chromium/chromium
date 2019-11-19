# Copyright (c) 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
    LLDB Support for Chromium types in Xcode

    Add the following to your ~/.lldbinit:
    command script import {Path to SRC Root}/tools/lldb/lldb_chrome.py
"""

import lldb


def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand('type summary add -F ' +
        'lldb_chrome.basestring16_SummaryProvider base::string16')


def basestring16_SummaryProvider(valobj, internal_dict):
    s = valobj.GetValueForExpressionPath('.__r_.__value_.__s')
    l = valobj.GetValueForExpressionPath('.__r_.__value_.__l')
    size = s.GetChildMemberWithName('__size_').GetValueAsUnsigned(0)
    is_short_string = size & 128 == 0  # Assumes _LIBCPP_BIG_ENDIAN is defined.
    if is_short_string:
        length = size >> 1
        data = s.GetChildMemberWithName('__data_').GetPointeeData(0, length)
    else:
        length = l.GetChildMemberWithName('__size_').GetValueAsUnsigned(0)
        data = l.GetChildMemberWithName('__data_').GetPointeeData(0, length)
    error = lldb.SBError()
    bytes_to_read = 2 * length
    if not bytes_to_read:
        return '""'
    byte_string = data.ReadRawData(error, 0, bytes_to_read)
    if error.fail:
        return 'Summary error: %s' % error.description
    else:
        return '"' + byte_string.decode('utf-16') + '"'
