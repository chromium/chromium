# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Downloads, extracts, and loads symbols for the current version of Google Chrome
running under the debugger. This only works for official, Google Chrome builds.

To use, add this to .lldbinit:
    command script import /path/to/tools/lldb/lldb_chrome_symbols.py

If you are attached to any Google Chrome process, type:
    (lldb) getchromesyms

And the symbols will automatically be added to the debugger. Symbols by default
are stored in /tmp because they are large. If you want to change the location,
add this to .lldbinit, after the `script import`:
    script lldb_chrome_symbols.SYMBOL_STORE = '/path/to/symbols'

Note that because Google Chrome is codesigned with certain security options,
you need to either have System Integrity Protection Debugging disabled to be
able to attach to the process, or force re-sign the binary to remove the
codesigning options.
"""

import glob
import lldb
import os.path
import plistlib
import sys

sys.path.insert(1,
                os.path.join(os.path.dirname(os.path.dirname(__file__)), 'mac'))
import download_symbols

SYMBOL_STORE = '/tmp/lldb_chrome_symbols'


def get_chrome_symbols(debugger, command, result, internal_dict):
    modules = debugger.GetSelectedTarget().module_iter()
    chrome_framework = [
        m for m in modules
        if m.GetFileSpec().GetFilename() == 'Google Chrome Framework'
    ]
    if len(chrome_framework) != 1:
        print('Could not find a "Google Chrome Framework" module in the ' \
              'current target',
            file=result)
        return
    chrome_framework = chrome_framework[0]

    # Extract the version information from the path.
    # framework_parent_dir is:
    # /Applications/Google Chrome Canary.app/Contents/Frameworks/Google Chrome Framework.framework/Versions/91.0.4449.6
    framework_parent_dir = chrome_framework.GetFileSpec().GetDirectory()
    chrome_version = os.path.basename(framework_parent_dir)
    # Get the outer .app bundle path.
    outer_bundle = os.path.dirname(
        os.path.dirname(
            os.path.dirname(
                os.path.dirname(os.path.dirname(framework_parent_dir)))))

    chrome_arch = chrome_framework.GetTriple().split('-')[0]
    chrome_channel = _get_channel(outer_bundle, chrome_version)

    if os.path.exists(SYMBOL_STORE):
        dsym_dir = download_symbols.get_symbol_directory(
            chrome_version, chrome_channel, chrome_arch, SYMBOL_STORE)
        if os.path.exists(dsym_dir):
            print('Adding existing symbols from {}'.format(dsym_dir),
                  file=result)
            _add_symbols(debugger, dsym_dir)
            return
    else:
        os.mkdir(SYMBOL_STORE)

    dsym_dir = download_symbols.download_chrome_symbols(chrome_version,
                                                        chrome_channel,
                                                        chrome_arch,
                                                        SYMBOL_STORE)
    if dsym_dir:
        _add_symbols(debugger, dsym_dir)


def _get_channel(outer_bundle, version):
    """Looks up the Chrome release channel by reading the Info.plist key. The
    target's Chrome `version` will be checked against the `outer_bundle`'s."""
    info_plist = os.path.join(outer_bundle, 'Contents', 'Info.plist')
    # Reading the Info.plist may fail if this is a core or minidump, but
    # there is no way to test in the lldb API if the target is one.
    try:
        with open(info_plist, 'rb') as f:
            plist = plistlib.load(f)
            if plist['KSVersion'] != version:
                # The on-disk bundle version does not match the target version,
                # so guess the channel.
                return None
            kschannel = plist['KSChannelID']

        if kschannel == '':
            return 'stable'

        channels = ('extended', 'stable', 'beta', 'dev', 'canary')
        for channel in channels:
            if channel in kschannel:
                return channel
    except:
        pass
    return None


def _add_symbols(debugger, dsym_dir):
    """Adds the dSYMs in `dsym_dir` to the `debugger`. This filters the symbols
    to just the relevant modules that are loaded in the process because lldb
    prints an error when adding symbols that are not used.
    """
    dsyms = glob.glob(os.path.join(dsym_dir, '*.dSYM'))
    wanted_dsyms = set([
        m.GetFileSpec().GetFilename() + '.dSYM'
        for m in debugger.GetSelectedTarget().module_iter()
    ])
    for dsym in dsyms:
        if os.path.basename(dsym) in wanted_dsyms:
            debugger.HandleCommand('target symbol add "{}"'.format(dsym))


def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand('command script add -f ' \
            'lldb_chrome_symbols.get_chrome_symbols getchromesyms'
    )
