#!/usr/bin/env python
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Extract UserMetrics "actions" strings from the Chrome source.

This program generates the list of known actions we expect to see in the
user behavior logs.  It walks the Chrome source, looking for calls to
UserMetrics functions, extracting actions and warning on improper calls,
as well as generating the lists of possible actions in situations where
there are many possible actions.

See also:
  base/metrics/user_metrics.h

After extracting all actions, the content will go through a pretty print
function to make sure it's well formatted. If the file content needs to be
changed, a window will be prompted asking for user's consent. The old version
will also be saved in a backup file.
"""

from __future__ import print_function

__author__ = 'evanm (Evan Martin)'

from HTMLParser import HTMLParser
import logging
import os
import re
import shutil
import sys
from xml.dom import minidom

import action_utils
import actions_print_style

# Import the metrics/common module for pretty print xml.
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import presubmit_util
import diff_util
import pretty_print_xml

USER_METRICS_ACTION_RE = re.compile(r"""
  [^a-zA-Z]                   # Preceded by a non-alphabetical character.
  (?:                         # Begin non-capturing group.
  UserMetricsAction           # C++ / Objective C function name.
  |                           # or...
  RecordUserAction\.record    # Java function name.
  )                           # End non-capturing group.
  \(                          # Opening parenthesis.
  \s*                         # Any amount of whitespace, including new lines.
  (.+?)                       # A sequence of characters for the param.
  \)                          # Closing parenthesis.
  """,
  re.VERBOSE | re.DOTALL      # Verbose syntax and makes . also match new lines.
)
USER_METRICS_ACTION_RE_JS = re.compile(r"""
  chrome\.send                # Start of function call.
  \(                          # Opening parenthesis.
  \s*                         # Any amount of whitespace, including new lines.
  # WebUI message handled by CoreOptionsHandler.
  'coreOptionsUserMetricsAction'
  ,                           # Separator after first parameter.
  \s*                         # Any amount of whitespace, including new lines.
  \[                          # Opening bracket for arguments for C++ function.
  \s*                         # Any amount of whitespace, including new lines.
  (.+?)                       # A sequence of characters for the param.
  \s*                         # Any amount of whitespace, including new lines.
  \]                          # Closing bracket.
  \s*                         # Any amount of whitespace, including new lines.
  \)                          # Closing parenthesis.
  """,
  re.VERBOSE | re.DOTALL      # Verbose syntax and makes . also match new lines.
)
USER_METRICS_ACTION_RE_DEVTOOLS = re.compile(r"""
  InspectorFrontendHost\.recordUserMetricsAction     # Start of function call.
  \(                          # Opening parenthesis.
  \s*                         # Any amount of whitespace, including new lines.
  (.+?)                       # A sequence of characters for the param.
  \s*                         # Any amount of whitespace, including new lines.
  \)                          # Closing parenthesis.
  """,
  re.VERBOSE | re.DOTALL      # Verbose syntax and makes . also match new lines.
)
COMPUTED_ACTION_RE = re.compile(r'RecordComputedAction')
QUOTED_STRING_RE = re.compile(r"""('[^']+'|"[^"]+")$""")

# Files that are known to use content::RecordComputedAction(), which means
# they require special handling code in this script.
# To add a new file, add it to this list and add the appropriate logic to
# generate the known actions to AddComputedActions() below.
KNOWN_COMPUTED_USERS = (
  'back_forward_menu_model.cc',
  'options_page_view.cc',
  'render_view_host.cc',  # called using webkit identifiers
  'user_metrics.cc',  # method definition
  'new_tab_ui.cc',  # most visited clicks 1-9
  'extension_metrics_module.cc', # extensions hook for user metrics
  'language_options_handler_common.cc', # languages and input methods in CrOS
  'cros_language_options_handler.cc', # languages and input methods in CrOS
  'external_metrics.cc',  # see AddChromeOSActions()
  'core_options_handler.cc',  # see AddWebUIActions()
  'browser_render_process_host.cc',  # see AddRendererActions()
  'render_thread_impl.cc',  # impl of RenderThread::RecordComputedAction()
  'render_process_host_impl.cc',  # browser side impl for
                                  # RenderThread::RecordComputedAction()
  'mock_render_thread.cc',  # mock of RenderThread::RecordComputedAction()
  'ppb_pdf_impl.cc',  # see AddClosedSourceActions()
  'pepper_pdf_host.cc',  # see AddClosedSourceActions()
  'record_user_action.cc', # see RecordUserAction.java
  'blink_platform_impl.cc', # see WebKit/public/platform/Platform.h
  'devtools_ui_bindings.cc', # see AddDevToolsActions()
)

# Language codes used in Chrome. The list should be updated when a new
# language is added to app/l10n_util.cc, as follows:
#
# % (cat app/l10n_util.cc | \
#    perl -n0e 'print $1 if /kAcceptLanguageList.*?\{(.*?)\}/s' | \
#    perl -nle 'print $1, if /"(.*)"/'; echo 'es-419') | \
#   sort | perl -pe "s/(.*)\n/'\$1', /" | \
#   fold -w75 -s | perl -pe 's/^/  /;s/ $//'; echo
#
# The script extracts language codes from kAcceptLanguageList, but es-419
# (Spanish in Latin America) is an exception.
LANGUAGE_CODES = (
  'af', 'am', 'ar', 'az', 'be', 'bg', 'bh', 'bn', 'br', 'bs', 'ca', 'co',
  'cs', 'cy', 'da', 'de', 'de-AT', 'de-CH', 'de-DE', 'el', 'en', 'en-AU',
  'en-CA', 'en-GB', 'en-NZ', 'en-US', 'en-ZA', 'eo', 'es', 'es-419', 'et',
  'eu', 'fa', 'fi', 'fil', 'fo', 'fr', 'fr-CA', 'fr-CH', 'fr-FR', 'fy',
  'ga', 'gd', 'gl', 'gn', 'gu', 'ha', 'haw', 'he', 'hi', 'hr', 'hu', 'hy',
  'ia', 'id', 'is', 'it', 'it-CH', 'it-IT', 'ja', 'jw', 'ka', 'kk', 'km',
  'kn', 'ko', 'ku', 'ky', 'la', 'ln', 'lo', 'lt', 'lv', 'mk', 'ml', 'mn',
  'mo', 'mr', 'ms', 'mt', 'nb', 'ne', 'nl', 'nn', 'no', 'oc', 'om', 'or',
  'pa', 'pl', 'ps', 'pt', 'pt-BR', 'pt-PT', 'qu', 'rm', 'ro', 'ru', 'sd',
  'sh', 'si', 'sk', 'sl', 'sn', 'so', 'sq', 'sr', 'st', 'su', 'sv', 'sw',
  'ta', 'te', 'tg', 'th', 'ti', 'tk', 'to', 'tr', 'tt', 'tw', 'ug', 'uk',
  'ur', 'uz', 'vi', 'xh', 'yi', 'yo', 'zh', 'zh-CN', 'zh-TW', 'zu',
)

# Input method IDs used in Chrome OS. The list should be updated when a
# new input method is added to
# chromeos/ime/input_methods.txt in the Chrome tree, as
# follows:
#
# % sort chromeos/ime/input_methods.txt | \
#   perl -ne "print \"'\$1', \" if /^([^#]+?)\s/" | \
#   fold -w75 -s | perl -pe 's/^/  /;s/ $//'; echo
#
# The script extracts input method IDs from input_methods.txt.
INPUT_METHOD_IDS = (
  'xkb:am:phonetic:arm', 'xkb:be::fra', 'xkb:be::ger', 'xkb:be::nld',
  'xkb:bg::bul', 'xkb:bg:phonetic:bul', 'xkb:br::por', 'xkb:by::bel',
  'xkb:ca::fra', 'xkb:ca:eng:eng', 'xkb:ca:multix:fra', 'xkb:ch::ger',
  'xkb:ch:fr:fra', 'xkb:cz::cze', 'xkb:cz:qwerty:cze', 'xkb:de::ger',
  'xkb:de:neo:ger', 'xkb:dk::dan', 'xkb:ee::est', 'xkb:es::spa',
  'xkb:es:cat:cat', 'xkb:fi::fin', 'xkb:fr::fra', 'xkb:gb:dvorak:eng',
  'xkb:gb:extd:eng', 'xkb:ge::geo', 'xkb:gr::gre', 'xkb:hr::scr',
  'xkb:hu::hun', 'xkb:il::heb', 'xkb:is::ice', 'xkb:it::ita', 'xkb:jp::jpn',
  'xkb:latam::spa', 'xkb:lt::lit', 'xkb:lv:apostrophe:lav', 'xkb:mn::mon',
  'xkb:no::nob', 'xkb:pl::pol', 'xkb:pt::por', 'xkb:ro::rum', 'xkb:rs::srp',
  'xkb:ru::rus', 'xkb:ru:phonetic:rus', 'xkb:se::swe', 'xkb:si::slv',
  'xkb:sk::slo', 'xkb:tr::tur', 'xkb:ua::ukr', 'xkb:us::eng',
  'xkb:us:altgr-intl:eng', 'xkb:us:colemak:eng', 'xkb:us:dvorak:eng',
  'xkb:us:intl:eng',
)

# The path to the root of the repository.
REPOSITORY_ROOT = os.path.join(os.path.dirname(__file__), '..', '..', '..')

number_of_files_total = 0

# Tags that need to be inserted to each 'action' tag and their default content.
TAGS = {'description': 'Please enter the description of the metric.',
        'owner': ('Please list the metric\'s owners. Add more owner tags as '
                  'needed.')}


def AddComputedActions(actions):
  """Add computed actions to the actions list.

  Arguments:
    actions: set of actions to add to.
  """

  # Actions for back_forward_menu_model.cc.
  for dir in ('BackMenu_', 'ForwardMenu_'):
    actions.add(dir + 'ShowFullHistory')
    actions.add(dir + 'Popup')
    for i in range(1, 20):
      actions.add(dir + 'HistoryClick' + str(i))
      actions.add(dir + 'ChapterClick' + str(i))

  # Actions for new_tab_ui.cc.
  for i in range(1, 10):
    actions.add('MostVisited%d' % i)

  # Actions for language_options_handler.cc (Chrome OS specific).
  for input_method_id in INPUT_METHOD_IDS:
    actions.add('LanguageOptions_DisableInputMethod_%s' % input_method_id)
    actions.add('LanguageOptions_EnableInputMethod_%s' % input_method_id)
  for language_code in LANGUAGE_CODES:
    actions.add('LanguageOptions_UiLanguageChange_%s' % language_code)
    actions.add('LanguageOptions_SpellCheckLanguageChange_%s' % language_code)


def AddPDFPluginActions(actions):
  """Add actions that are sent by the PDF plugin.

  Arguments
    actions: set of actions to add to.
  """
  actions.add('PDF.LoadFailure')
  actions.add('PDF.LoadSuccess')
  actions.add('PDF.PreviewDocumentLoadFailure')
  actions.add('PDF.PrintPage')
  actions.add('PDF.ZoomFromBrowser')
  actions.add('PDF_Unsupported_3D')
  actions.add('PDF_Unsupported_Attachment')
  actions.add('PDF_Unsupported_Bookmarks')
  actions.add('PDF_Unsupported_Digital_Signature')
  actions.add('PDF_Unsupported_Movie')
  actions.add('PDF_Unsupported_Portfolios_Packages')
  actions.add('PDF_Unsupported_Rights_Management')
  actions.add('PDF_Unsupported_Screen')
  actions.add('PDF_Unsupported_Shared_Form')
  actions.add('PDF_Unsupported_Shared_Review')
  actions.add('PDF_Unsupported_Sound')
  actions.add('PDF_Unsupported_XFA')

def AddBookmarkManagerActions(actions):
  """Add actions that are used by BookmarkManager.

  Arguments
    actions: set of actions to add to.
  """
  actions.add('BookmarkManager_Command_AddPage')
  actions.add('BookmarkManager_Command_Copy')
  actions.add('BookmarkManager_Command_Cut')
  actions.add('BookmarkManager_Command_Delete')
  actions.add('BookmarkManager_Command_Edit')
  actions.add('BookmarkManager_Command_Export')
  actions.add('BookmarkManager_Command_Import')
  actions.add('BookmarkManager_Command_NewFolder')
  actions.add('BookmarkManager_Command_OpenIncognito')
  actions.add('BookmarkManager_Command_OpenInNewTab')
  actions.add('BookmarkManager_Command_OpenInNewWindow')
  actions.add('BookmarkManager_Command_OpenInSame')
  actions.add('BookmarkManager_Command_Paste')
  actions.add('BookmarkManager_Command_ShowInFolder')
  actions.add('BookmarkManager_Command_Sort')
  actions.add('BookmarkManager_Command_UndoDelete')
  actions.add('BookmarkManager_Command_UndoGlobal')
  actions.add('BookmarkManager_Command_UndoNone')

  actions.add('BookmarkManager_NavigateTo_BookmarkBar')
  actions.add('BookmarkManager_NavigateTo_Mobile')
  actions.add('BookmarkManager_NavigateTo_Other')
  actions.add('BookmarkManager_NavigateTo_Recent')
  actions.add('BookmarkManager_NavigateTo_Search')
  actions.add('BookmarkManager_NavigateTo_SubFolder')

def AddChromeOSActions(actions):
  """Add actions reported by non-Chrome processes in Chrome OS.

  Arguments:
    actions: set of actions to add to.
  """
  # Actions sent by Chrome OS update engine.
  actions.add('Updater.ServerCertificateChanged')
  actions.add('Updater.ServerCertificateFailed')

def AddExtensionActions(actions):
  """Add actions reported by extensions via chrome.metricsPrivate API.

  Arguments:
    actions: set of actions to add to.
  """
  # Actions sent by Chrome OS File Browser.
  actions.add('FileBrowser.CreateNewFolder')
  actions.add('FileBrowser.PhotoEditor.Edit')
  actions.add('FileBrowser.PhotoEditor.View')
  actions.add('FileBrowser.SuggestApps.ShowDialog')

  # Actions sent by Google Now client.
  actions.add('GoogleNow.MessageClicked')
  actions.add('GoogleNow.ButtonClicked0')
  actions.add('GoogleNow.ButtonClicked1')
  actions.add('GoogleNow.Dismissed')

  # Actions sent by Chrome Connectivity Diagnostics.
  actions.add('ConnectivityDiagnostics.LaunchSource.OfflineChromeOS')
  actions.add('ConnectivityDiagnostics.LaunchSource.WebStore')
  actions.add('ConnectivityDiagnostics.UA.LogsShown')
  actions.add('ConnectivityDiagnostics.UA.PassingTestsShown')
  actions.add('ConnectivityDiagnostics.UA.SettingsShown')
  actions.add('ConnectivityDiagnostics.UA.TestResultExpanded')
  actions.add('ConnectivityDiagnostics.UA.TestSuiteRun')


class InvalidStatementException(Exception):
  """Indicates an invalid statement was found."""


class ActionNameFinder:
  """Helper class to find action names in source code file."""

  def __init__(self, path, contents, action_re):
    self.__path = path
    self.__pos = 0
    self.__contents = contents
    self.__action_re = action_re

  def FindNextAction(self):
    """Finds the next action name in the file.

    Returns:
      The name of the action found or None if there are no more actions.
    Raises:
      InvalidStatementException if the next action statement is invalid
      and could not be parsed. There may still be more actions in the file,
      so FindNextAction() can continue to be called to find following ones.
    """
    match = self.__action_re.search(self.__contents, pos=self.__pos)
    if not match:
      return None
    match_start = match.start()
    self.__pos = match.end()

    match = QUOTED_STRING_RE.match(match.group(1))
    if not match:
      if self.__action_re == USER_METRICS_ACTION_RE_JS:
        return None
      self._RaiseException(match_start, self.__pos)

    # Remove surrounding quotation marks.
    return match.group(1)[1:-1]

  def _RaiseException(self, match_start, match_end):
    """Raises an InvalidStatementException for the specified code range."""
    line_number = self.__contents.count('\n', 0, match_start) + 1
    # Add 1 to |match_start| since the RE checks the preceding character.
    statement = self.__contents[match_start + 1:match_end]
    raise InvalidStatementException(
      '%s uses UserMetricsAction incorrectly on line %d:\n%s' %
      (self.__path, line_number, statement))


def GrepForActions(path, actions):
  """Grep a source file for calls to UserMetrics functions.

  Arguments:
    path: path to the file
    actions: set of actions to add to
  """
  global number_of_files_total
  number_of_files_total = number_of_files_total + 1

  # Check the extension, using the regular expression for C++ syntax by default.
  ext = os.path.splitext(path)[1].lower()
  if ext == '.js':
    action_re = USER_METRICS_ACTION_RE_JS
  else:
    action_re = USER_METRICS_ACTION_RE

  finder = ActionNameFinder(path, open(path).read(), action_re)
  while True:
    try:
      action_name = finder.FindNextAction()
      if not action_name:
        break
      actions.add(action_name)
    except InvalidStatementException, e:
      logging.warning(str(e))

  if action_re != USER_METRICS_ACTION_RE:
    return

  line_number = 0
  for line in open(path):
    line_number = line_number + 1
    if COMPUTED_ACTION_RE.search(line):
      # Warn if this file shouldn't be calling RecordComputedAction.
      if os.path.basename(path) not in KNOWN_COMPUTED_USERS:
        logging.warning('%s has RecordComputedAction statement on line %d' %
                        (path, line_number))

class WebUIActionsParser(HTMLParser):
  """Parses an HTML file, looking for all tags with a 'metric' attribute.
  Adds user actions corresponding to any metrics found.

  Arguments:
    actions: set of actions to add to
  """
  def __init__(self, actions):
    HTMLParser.__init__(self)
    self.actions = actions

  def handle_starttag(self, tag, attrs):
    # We only care to examine tags that have a 'metric' attribute.
    attrs = dict(attrs)
    if not 'metric' in attrs:
      return

    # Boolean metrics have two corresponding actions.  All other metrics have
    # just one corresponding action.  By default, we check the 'dataType'
    # attribute.
    is_boolean = ('dataType' in attrs and attrs['dataType'] == 'boolean')
    if 'type' in attrs and attrs['type'] in ('checkbox', 'radio'):
      if attrs['type'] == 'checkbox':
        is_boolean = True
      else:
        # Radio buttons are boolean if and only if their values are 'true' or
        # 'false'.
        assert(attrs['type'] == 'radio')
        if 'value' in attrs and attrs['value'] in ['true', 'false']:
          is_boolean = True

    if is_boolean:
      self.actions.add(attrs['metric'] + '_Enable')
      self.actions.add(attrs['metric'] + '_Disable')
    else:
      self.actions.add(attrs['metric'])

def GrepForWebUIActions(path, actions):
  """Grep a WebUI source file for elements with associated metrics.

  Arguments:
    path: path to the file
    actions: set of actions to add to
  """
  close_called = False
  try:
    parser = WebUIActionsParser(actions)
    parser.feed(open(path).read())
    # An exception can be thrown by parser.close(), so do it in the try to
    # ensure the path of the file being parsed gets printed if that happens.
    close_called = True
    parser.close()
  except Exception, e:
    print("Error encountered for path %s" % path)
    raise e
  finally:
    if not close_called:
      parser.close()

def GrepForDevToolsActions(path, actions):
  """Grep a DevTools source file for calls to UserMetrics functions.

  Arguments:
    path: path to the file
    actions: set of actions to add to
  """
  global number_of_files_total
  number_of_files_total = number_of_files_total + 1

  ext = os.path.splitext(path)[1].lower()
  if ext != '.js':
    return

  finder = ActionNameFinder(path, open(path).read(),
      USER_METRICS_ACTION_RE_DEVTOOLS)
  while True:
    try:
      action_name = finder.FindNextAction()
      if not action_name:
        break
      actions.add(action_name)
    except InvalidStatementException, e:
      logging.warning(str(e))

def WalkDirectory(root_path, actions, extensions, callback):
  for path, dirs, files in os.walk(root_path):
    if '.svn' in dirs:
      dirs.remove('.svn')
    if '.git' in dirs:
      dirs.remove('.git')
    for file in files:
      ext = os.path.splitext(file)[1]
      if ext in extensions:
        callback(os.path.join(path, file), actions)

def AddLiteralActions(actions):
  """Add literal actions specified via calls to UserMetrics functions.

  Arguments:
    actions: set of actions to add to.
  """
  EXTENSIONS = ('.cc', '.cpp', '.mm', '.c', '.m', '.java')

  # Walk the source tree to process all files.
  ash_root = os.path.normpath(os.path.join(REPOSITORY_ROOT, 'ash'))
  WalkDirectory(ash_root, actions, EXTENSIONS, GrepForActions)
  chrome_root = os.path.normpath(os.path.join(REPOSITORY_ROOT, 'chrome'))
  WalkDirectory(chrome_root, actions, EXTENSIONS, GrepForActions)
  content_root = os.path.normpath(os.path.join(REPOSITORY_ROOT, 'content'))
  WalkDirectory(content_root, actions, EXTENSIONS, GrepForActions)
  components_root = os.path.normpath(os.path.join(REPOSITORY_ROOT,
                    'components'))
  WalkDirectory(components_root, actions, EXTENSIONS, GrepForActions)
  net_root = os.path.normpath(os.path.join(REPOSITORY_ROOT, 'net'))
  WalkDirectory(net_root, actions, EXTENSIONS, GrepForActions)
  webkit_root = os.path.normpath(os.path.join(REPOSITORY_ROOT, 'webkit'))
  WalkDirectory(os.path.join(webkit_root, 'glue'), actions, EXTENSIONS,
                GrepForActions)
  WalkDirectory(os.path.join(webkit_root, 'port'), actions, EXTENSIONS,
                GrepForActions)
  webkit_core_root = os.path.normpath(
                     os.path.join(REPOSITORY_ROOT,
                                  'third_party/blink/renderer/core'))
  WalkDirectory(webkit_core_root, actions, EXTENSIONS, GrepForActions)

def AddWebUIActions(actions):
  """Add user actions defined in WebUI files.

  Arguments:
    actions: set of actions to add to.
  """
  resources_root = os.path.join(REPOSITORY_ROOT, 'chrome', 'browser',
                                'resources')
  WalkDirectory(resources_root, actions, ('.html'), GrepForWebUIActions)
  WalkDirectory(resources_root, actions, ('.js'), GrepForActions)

def AddDevToolsActions(actions):
  """Add user actions defined in DevTools frontend files.

  Arguments:
    actions: set of actions to add to.
  """
  resources_root = os.path.join(REPOSITORY_ROOT, 'third_party', 'blink',
                                'renderer', 'devtools', 'front_end')
  WalkDirectory(resources_root, actions, ('.js'), GrepForDevToolsActions)

def AddHistoryPageActions(actions):
  """Add actions that are used in History page.

  Arguments
    actions: set of actions to add to.
  """
  actions.add('HistoryPage_BookmarkStarClicked')
  actions.add('HistoryPage_EntryMenuRemoveFromHistory')
  actions.add('HistoryPage_EntryLinkClick')
  actions.add('HistoryPage_EntryLinkRightClick')
  actions.add('HistoryPage_SearchResultClick')
  actions.add('HistoryPage_EntryMenuShowMoreFromSite')
  actions.add('HistoryPage_NewestHistoryClick')
  actions.add('HistoryPage_NewerHistoryClick')
  actions.add('HistoryPage_OlderHistoryClick')
  actions.add('HistoryPage_Search')
  actions.add('HistoryPage_InitClearBrowsingData')
  actions.add('HistoryPage_RemoveSelected')
  actions.add('HistoryPage_SearchResultRemove')
  actions.add('HistoryPage_ConfirmRemoveSelected')
  actions.add('HistoryPage_CancelRemoveSelected')

def AddAutomaticResetBannerActions(actions):
  """Add actions that are used for the automatic profile settings reset banners
  in chrome://settings.

  Arguments
    actions: set of actions to add to.
  """
  # These actions relate to the the automatic settings reset banner shown as
  # a result of the reset prompt.
  actions.add('AutomaticReset_WebUIBanner_BannerShown')
  actions.add('AutomaticReset_WebUIBanner_ManuallyClosed')
  actions.add('AutomaticReset_WebUIBanner_ResetClicked')

  # These actions relate to the the automatic settings reset banner shown as
  # a result of settings hardening.
  actions.add('AutomaticSettingsReset_WebUIBanner_BannerShown')
  actions.add('AutomaticSettingsReset_WebUIBanner_ManuallyClosed')
  actions.add('AutomaticSettingsReset_WebUIBanner_LearnMoreClicked')
  actions.add('AutomaticSettingsReset_WebUIBanner_ResetClicked')


class Error(Exception):
  pass


def _ExtractText(parent_dom, tag_name):
  """Extract the text enclosed by |tag_name| under |parent_dom|

  Args:
    parent_dom: The parent Element under which text node is searched for.
    tag_name: The name of the tag which contains a text node.

  Returns:
    A (list of) string enclosed by |tag_name| under |parent_dom|.
  """
  texts = []
  for child_dom in parent_dom.getElementsByTagName(tag_name):
    text_dom = child_dom.childNodes
    if text_dom.length != 1:
      raise Error('More than 1 child node exists under %s' % tag_name)
    if text_dom[0].nodeType != minidom.Node.TEXT_NODE:
      raise Error('%s\'s child node is not a text node.' % tag_name)
    texts.append(text_dom[0].data)
  return texts


def ParseActionFile(file_content):
  """Parse the XML data currently stored in the file.

  Args:
    file_content: a string containing the action XML file content.

  Returns:
    (actions_dict, comment_nodes, suffixes):
      - actions_dict is a dict from user action name to Action object.
      - comment_nodes is a list of top-level comment nodes.
      - suffixes is a list of <action-suffix> DOM elements.
  """
  dom = minidom.parseString(file_content)

  comment_nodes = []
  # Get top-level comments. It is assumed that all comments are placed before
  # <actions> tag. Therefore the loop will stop if it encounters a non-comment
  # node.
  for node in dom.childNodes:
    if node.nodeType == minidom.Node.COMMENT_NODE:
      comment_nodes.append(node)
    else:
      break

  actions_dict = {}
  # Get each user action data.
  for action_dom in dom.getElementsByTagName('action'):
    action_name = action_dom.getAttribute('name')
    not_user_triggered = bool(action_dom.getAttribute('not_user_triggered'))

    owners = _ExtractText(action_dom, 'owner')
    # There is only one description for each user action. Get the first element
    # of the returned list.
    description_list = _ExtractText(action_dom, 'description')
    if len(description_list) > 1:
      logging.error('User action "%s" has more than one description. Exactly '
                    'one description is needed for each user action. Please '
                    'fix.', action_name)
      sys.exit(1)
    description = description_list[0] if description_list else None
    # There is at most one obsolete tag for each user action.
    obsolete_list = _ExtractText(action_dom, 'obsolete')
    if len(obsolete_list) > 1:
      logging.error('User action "%s" has more than one obsolete tag. At most '
                    'one obsolete tag can be added for each user action. Please'
                    ' fix.', action_name)
      sys.exit(1)
    obsolete = obsolete_list[0] if obsolete_list else None
    actions_dict[action_name] = action_utils.Action(action_name, description,
        owners, not_user_triggered, obsolete)

  suffixes = dom.getElementsByTagName('action-suffix')
  action_utils.CreateActionsFromSuffixes(actions_dict, suffixes)

  return actions_dict, comment_nodes, suffixes


def _CreateActionTag(doc, action):
  """Create a new action tag.

  Format of an action tag:
  <action name="name" not_user_triggered="true">
    <obsolete>Deprecated.</obsolete>
    <owner>Owner</owner>
    <description>Description.</description>
  </action>

  not_user_triggered is an optional attribute. If set, it implies that the
  belonging action is not a user action. A user action is an action that
  is logged exactly once right after a user has made an action.

  <obsolete> is an optional tag. It's added to actions that are no longer used
  any more.

  If action_name is in actions_dict, the values to be inserted are based on the
  corresponding Action object. If action_name is not in actions_dict, the
  default value from TAGS is used.

  Args:
    doc: The document under which the new action tag is created.
    action: An Action object representing the data to be inserted.

  Returns:
    An action tag Element with proper children elements, or None if a tag should
    not be created for this action (e.g. if it comes from a suffix).
  """
  if action.from_suffix:
    return None

  action_dom = doc.createElement('action')
  action_dom.setAttribute('name', action.name)

  # Add not_user_triggered attribute.
  if action.not_user_triggered:
    action_dom.setAttribute('not_user_triggered', 'true')

  # Create obsolete tag.
  if action.obsolete:
    obsolete_dom = doc.createElement('obsolete')
    action_dom.appendChild(obsolete_dom)
    obsolete_dom.appendChild(doc.createTextNode(action.obsolete))

  # Create owner tag.
  if action.owners:
    # If owners for this action is not None, use the stored value. Otherwise,
    # use the default value.
    for owner in action.owners:
      owner_dom = doc.createElement('owner')
      owner_dom.appendChild(doc.createTextNode(owner))
      action_dom.appendChild(owner_dom)
  else:
    # Use default value.
    owner_dom = doc.createElement('owner')
    owner_dom.appendChild(doc.createTextNode(TAGS.get('owner', '')))
    action_dom.appendChild(owner_dom)

  # Create description tag.
  description_dom = doc.createElement('description')
  action_dom.appendChild(description_dom)
  if action.description:
    # If description for this action is not None, use the store value.
    # Otherwise, use the default value.
    description_dom.appendChild(doc.createTextNode(action.description))
  else:
    description_dom.appendChild(doc.createTextNode(
        TAGS.get('description', '')))

  return action_dom


def PrettyPrint(actions_dict, comment_nodes, suffixes):
  """Given a list of actions, create a well-printed minidom document.

  Args:
    actions_dict: A mappting from action name to Action object.
    comment_nodes: A list of top-level comment nodes.
    suffixes: A list of <action-suffix> tags to be appended as-is.

  Returns:
    A well-printed minidom document that represents the input action data.
  """
  doc = minidom.Document()

  # Attach top-level comments.
  for node in comment_nodes:
    doc.appendChild(node)

  actions_element = doc.createElement('actions')
  doc.appendChild(actions_element)

  # Attach action node based on updated |actions_dict|.
  for _, action in sorted(actions_dict.items()):
    action_tag = _CreateActionTag(doc, action)
    if action_tag:
      actions_element.appendChild(action_tag)

  for suffix_tag in suffixes:
    actions_element.appendChild(suffix_tag)

  return actions_print_style.GetPrintStyle().PrettyPrintXml(doc)


def UpdateXml(original_xml):
  actions_dict, comment_nodes, suffixes = ParseActionFile(original_xml)

  actions = set()
  AddComputedActions(actions)
  AddWebUIActions(actions)
  AddDevToolsActions(actions)

  AddLiteralActions(actions)

  # print("Scanned {0} number of files".format(number_of_files_total))
  # print("Found {0} entries".format(len(actions)))

  AddAutomaticResetBannerActions(actions)
  AddBookmarkManagerActions(actions)
  AddChromeOSActions(actions)
  AddExtensionActions(actions)
  AddHistoryPageActions(actions)
  AddPDFPluginActions(actions)

  for action_name in actions:
    if action_name not in actions_dict:
      actions_dict[action_name] = action_utils.Action(action_name, None, [])

  return PrettyPrint(actions_dict, comment_nodes, suffixes)


def main(argv):
  presubmit_util.DoPresubmitMain(argv, 'actions.xml', 'actions.old.xml',
                                 'extract_actions.py', UpdateXml)

if '__main__' == __name__:
  sys.exit(main(sys.argv))
