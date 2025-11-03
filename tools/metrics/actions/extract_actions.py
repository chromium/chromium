#!/usr/bin/env python3
#
# Copyright 2012 The Chromium Authors
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

import ast
import copy
import logging
import os
import re
import sys
from typing import Dict, List, Optional, Tuple
from xml.dom import minidom

if sys.version_info.major == 2:
  from HTMLParser import HTMLParser
else:
  from html.parser import HTMLParser

import action_utils
import actions_model

# Import the metrics/common module for pretty print xml.
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import presubmit_util
import xml_utils

USER_METRICS_ACTION_RE = re.compile(
    r"""
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
    re.VERBOSE | re.DOTALL  # Verbose syntax and makes . also match new lines.
)
USER_METRICS_ACTION_RE_JS = re.compile(
    r"""
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
    re.VERBOSE | re.DOTALL  # Verbose syntax and makes . also match new lines.
)
USER_METRICS_ACTION_RE_DEVTOOLS = re.compile(
    r"""
  InspectorFrontendHost\.recordUserMetricsAction     # Start of function call.
  \(                          # Opening parenthesis.
  \s*                         # Any amount of whitespace, including new lines.
  (.+?)                       # A sequence of characters for the param.
  \s*                         # Any amount of whitespace, including new lines.
  \)                          # Closing parenthesis.
  """,
    re.VERBOSE | re.DOTALL  # Verbose syntax and makes . also match new lines.
)
COMPUTED_ACTION_RE = re.compile(r'RecordComputedAction')

# Files that are known to use content::RecordComputedAction(), which means
# they require special handling code in this script.
# To add a new file, add it to this list and add the appropriate logic to
# generate the known actions to AddComputedActions() below.
KNOWN_COMPUTED_USERS = (
    'back_forward_menu_model.cc',
    'user_metrics.cc',  # method definition
    'external_metrics.cc',  # see AddChromeOSActions()
    'render_thread_impl.cc',  # impl of RenderThread::RecordComputedAction()
    # browser side impl for RenderThread::RecordComputedAction()
    'render_process_host_impl.cc',
    'mock_render_thread.cc',  # mock of RenderThread::RecordComputedAction()
    'pdf_view_web_plugin_client.cc',  # see AddPDFPluginActions()
    'blink_platform_impl.cc',  # see WebKit/public/platform/Platform.h
    'devtools_ui_bindings.cc',  # see AddDevToolsActions()
    'sharing_hub_bubble_controller.cc',  # share targets
    'sharing_hub_sub_menu_model.cc',  # share targets
    'sharing_hub_bubble_controller_desktop_impl.cc',
    'bookmark_metrics.cc',  # see AddBookmarkUsageActions()
    'accelerator_tracker.cc',
    'child_thread_impl.cc',
    'customize_toolbar_handler.cc',
    'feature_promo_controller.cc',
    'feature_promo_lifecycle.cc',
    'metrics_handler.cc',
    'performance_controls_metrics.cc',
    'pinned_action_toolbar_button.cc',
    'pinned_action_toolbar_button_menu_model.cc',
    'pinned_toolbar_actions_model.cc',
    'side_panel_util.cc',
    'stats.cc',
    'toast_metrics.cc',
    'whats_new_handler.cc',
)

# The path to the root of the repository.
REPOSITORY_ROOT = os.path.join(os.path.dirname(__file__), '..', '..', '..')

number_of_files_total = 0

# Tags that need to be inserted to each 'action' tag and their default content.
TAGS = {
    'description': 'Please enter the description of the metric.',
    'owner': ('Please list the metric\'s owners. Add more owner tags as '
              'needed.')
}

SHARE_TARGETS = {
    'CopyURLSelected', 'QRCodeSelected', 'ScreenshotSelected',
    'SendTabToSelfSelected', 'CastSelected', 'SavePageSelected',
    'ThirdPartyAppSelected', 'FollowSelected', 'UnfollowSelected'
}


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

  # Actions for sharing_hub_bubble_controller.cc and
  # sharing_hub_sub_menu_model.cc.
  for share_target in SHARE_TARGETS:
    actions.add('SharingHubDesktop.%s' % share_target)


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


def AddBookmarkUsageActions(actions):
  """Add actions related to bookmarks usage.

  Arguments
    actions: set of actions to add to.
  """
  actions.add('Bookmarks.Added')
  actions.add('Bookmarks.Added.AccountStorage')
  actions.add('Bookmarks.Added.LocalStorage')
  actions.add('Bookmarks.Added.LocalStorageSyncing')
  actions.add('Bookmarks.FolderAdded')
  actions.add('Bookmarks.FolderAdded.AccountStorage')
  actions.add('Bookmarks.FolderAdded.LocalStorage')
  actions.add('Bookmarks.FolderAdded.LocalStorageSyncing')
  actions.add('Bookmarks.Opened')
  actions.add('Bookmarks.Opened.AccountStorage')
  actions.add('Bookmarks.Opened.LocalStorage')
  actions.add('Bookmarks.Opened.LocalStorageSyncing')


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
    self.__pos = match.end()

    param_string = match.group(1)
    try:
      evaluated = ast.literal_eval(param_string)
      return evaluated if isinstance(evaluated, str) else None
    except (ValueError, SyntaxError):
      # The action is not a literal string, so we ignore it.
      return None


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

  if os.name == 'nt':
    # TODO(crbug.com/40941175): Remove when Windows bots have LongPathsEnabled.
    # Windows APIs limits path names to 260 characters unless the Windows
    # property LongPathsEnabled is set to 1. As a workaround, the "\\?\"
    # disables all string parsing by the Windows API and thus allows us to
    # exceed Windows' path length limit of 260 characters.
    path = '\\\\?\\' + os.path.abspath(path)

  try:
    content = open(path, encoding='utf-8').read()
  except UnicodeDecodeError:
    # If the file is not UTF-8, it's not a Chrome source file, ignore it.
    return

  finder = ActionNameFinder(path, content, action_re)
  while True:
    try:
      action_name = finder.FindNextAction()
      if not action_name:
        break
      actions.add(action_name)
    except InvalidStatementException as e:
      logging.warning(str(e))

  if action_re != USER_METRICS_ACTION_RE:
    return

  line_number = 0
  for line in open(path, encoding='utf-8'):
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
        assert (attrs['type'] == 'radio')
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
    parser.feed(open(path, encoding='utf-8').read())
    # An exception can be thrown by parser.close(), so do it in the try to
    # ensure the path of the file being parsed gets printed if that happens.
    close_called = True
    parser.close()
  except Exception as e:
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

  finder = ActionNameFinder(path,
                            open(path, encoding='utf-8').read(),
                            USER_METRICS_ACTION_RE_DEVTOOLS)
  while True:
    try:
      action_name = finder.FindNextAction()
      if not action_name:
        break
      actions.add(action_name)
    except InvalidStatementException as e:
      logging.warning(str(e))


def WalkDirectory(root_path, actions, extensions, callback):
  """Walk directory chooses which files to process based on a set
   of extensions, and runs the callback function on them.

    It's important to know that `extensions` should be a tuple,
    and if it's not, it will be converted into one. This is to correct
    for Python automatically converting ('foo') to 'foo'.

    Note: Files starting with a `.` will be ignored by default. See
    comments in implementation.
  """

  # Convert `extensions` to tuple if it is not one already
  if type(extensions) != tuple:
    extensions = (extensions, )

  for path, dirs, files in os.walk(root_path):
    if 'third_party' in dirs:
      dirs.remove('third_party')
    if '.svn' in dirs:
      dirs.remove('.svn')
    if '.git' in dirs:
      dirs.remove('.git')
    for file in files:
      """splitext() returns an empty extension |ext| for files
      starting with `.`, as a result, files starting with a `.` will
      be ignored (unless the |extensions| tuple includes an empty
      element). Beware of allowing the callback() to run on all files
      that start with a `.`: the callback needs to be resilient to
      different file formats (binary, ASCII, etc.) and may also end
      up processing many files that don't need to be processed, wasting
      time.
      """
      filename, ext = os.path.splitext(file)
      if ext in extensions and not filename.endswith('test'):
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
      os.path.join(REPOSITORY_ROOT, 'third_party/blink/renderer/core'))
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


def ExtractVariants(
    variants_node: minidom.Element) -> List[action_utils.Variant]:
  """Extracts a list of variants from a <variants> or <token> node."""
  variants = []
  for variant_node in xml_utils.IterElementsWithTag(variants_node, 'variant',
                                                    1):
    name = variant_node.getAttribute('name')
    summary = variant_node.getAttribute('summary')
    if not summary:
      summary = xml_utils.GetTextFromChildNodes(variant_node)
    variants.append(action_utils.Variant(name, summary))
  return variants


def ExtractTokens(
    action: minidom.Element, variants_dict: Dict[str,
                                                 List[action_utils.Variant]]
) -> List[action_utils.Token]:
  """Extracts tokens and variants from the given action element.

  Args:
    action: A DOM Element corresponding to a action.
    variants_dict: A dictionary of variants extracted from the tree.

  Returns:
    A tuple where the first element is a list of extracted Tokens, and the
      second indicates if any errors were detected while extracting them.
  """
  tokens_seen = set()
  tokens = []
  action_name = action.getAttribute('name')

  for token_node in xml_utils.IterElementsWithTag(action, 'token', 1):
    token_key = token_node.getAttribute('key')
    if token_key in tokens_seen:
      logging.error(f'Histogram {action_name} contains duplicate token key '
                    f'{token_key}, please ensure token keys are unique.')
      continue
    tokens_seen.add(token_key)

    token_key_format = '{' + token_key + '}'
    if token_key_format not in action_name:
      logging.error(
          f'User Action {action_name} includes a token tag but the token key '
          f'is not present in action name. Please insert the token key into '
          f'the action name in order for the token to be added.')
      continue

    token = action_utils.Token(key=token_key)

    # If 'variants' attribute is set for the <token>, get the list of Variant
    # objects from from the |variants_dict| (out-of-line variants).
    if token_node.hasAttribute('variants'):
      variants_name = token_node.getAttribute('variants')
      if variants_name not in variants_dict:
        logging.error(
            f'The variants attribute {variants_name} of token key {token_key} '
            f'of action {action_name} does not have a corresponding '
            f'<variants> tag.')
      token.variants_name = variants_name

    # Extract any inline variants.
    else:
      token.variants = ExtractVariants(token_node)
    tokens.append(token)

  return tokens


def _ExtractText(parent_dom: minidom.Element, tag_name: str) -> List[str]:
  """Extract the text enclosed by |tag_name| under |parent_dom|

  Args:
    parent_dom: The parent Element under which text node is searched for.
    tag_name: The name of the tag which contains a text node.

  Returns:
    A (list of) string enclosed by |tag_name| under |parent_dom|.
  """
  texts = []
  for node in parent_dom.getElementsByTagName(tag_name):
    text = xml_utils.GetTextFromChildNodes(node)
    if text:
      texts.append(text)
  return texts


def ParseActionFile(
    file_content: str
) -> Tuple[Dict[str, action_utils.Action], List[minidom.Node], Dict[
    str, List[action_utils.Variant]]]:
  """Parse the XML data currently stored in the file.

  Args:
    file_content: a string containing the action XML file content.

  Returns:
    (actions_dict, comment_nodes, variants_dict):
      - actions_dict is a dict from user action name to Action object.
      - comment_nodes is a list of top-level comment nodes.
      - variants_dict is a dict of Variant objects.
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
  variants_dict = {}
  for variants_dom in dom.getElementsByTagName('variants'):
    variants_name = variants_dom.getAttribute('name')
    variants_dict[variants_name] = ExtractVariants(variants_dom)

  # Get each user action data.
  for action_dom in dom.getElementsByTagName('action'):
    action_name = action_dom.getAttribute('name')
    not_user_triggered = bool(action_dom.getAttribute('not_user_triggered'))

    owners = _ExtractText(action_dom, 'owner')
    # There is only one description for each user action. Get the first element
    # of the returned list.
    description_list = _ExtractText(action_dom, 'description')
    if len(description_list) > 1:
      logging.error(
          'User action "%s" has more than one description. Exactly '
          'one description is needed for each user action. Please '
          'fix.', action_name)
      sys.exit(1)
    description = description_list[0] if description_list else None
    # There is at most one obsolete tag for each user action.
    obsolete_list = _ExtractText(action_dom, 'obsolete')
    if len(obsolete_list) > 1:
      logging.error(
          'User action "%s" has more than one obsolete tag. At most '
          'one obsolete tag can be added for each user action. Please'
          ' fix.', action_name)
      sys.exit(1)
    obsolete = obsolete_list[0] if obsolete_list else None

    tokens = ExtractTokens(action_dom, variants_dict)

    actions_dict[action_name] = action_utils.Action(action_name, description,
                                                    owners, not_user_triggered,
                                                    obsolete, tokens)


  return actions_dict, comment_nodes, variants_dict


def _CreateActionTag(doc: minidom.Document,
                     action: action_utils.Action) -> Optional[minidom.Element]:
  """Create a new action tag.

  Format of an action tag:
  <action name="name.{tokenName}" not_user_triggered="true">
    <obsolete>Deprecated.</obsolete>
    <owner>Owner</owner>
    <description>Description.</description>
    <token key="tokenName">
      <variant name="V1" summary="Variant number one.">
      <variant name="V2" summary="Variant number two.">
    </token>
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
    An action tag Element with proper children elements.
  """

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
    description_dom.appendChild(doc.createTextNode(TAGS.get('description', '')))

  for token in action.tokens:
    if token.implicit:
      continue
    token_dom = doc.createElement('token')
    token_dom.setAttribute('key', token.key)
    if token.variants_name:
      token_dom.setAttribute('variants', token.variants_name)
    else:
      for variant in token.variants:
        variant_dom = doc.createElement('variant')
        variant_dom.setAttribute('name', variant.name)
        variant_dom.setAttribute('summary', variant.summary)
        token_dom.appendChild(variant_dom)
    action_dom.appendChild(token_dom)

  return action_dom


def PrettyPrint(actions_dict: Dict[str, action_utils.Action],
                comment_nodes: List[minidom.Node],
                variants_dict: Dict[str, List[action_utils.Variant]]) -> str:
  """Given a list of actions, create a well-printed minidom document.

  Args:
    actions_dict: A mapping from action name to Action object.
    comment_nodes: A list of top-level comment nodes.
    variants_dict: A mapping from variants dict to shared <variants> tags.

  Returns:
    A well-printed minidom document that represents the input action data.
  """
  doc = minidom.Document()

  # Attach top-level comments.
  for node in comment_nodes:
    doc.appendChild(node)

  actions_element = doc.createElement('actions')
  doc.appendChild(actions_element)

  # Attach variants node based on updated |variants_dict|.
  for name, variants in sorted(variants_dict.items()):
    variants_tag = doc.createElement('variants')
    variants_tag.setAttribute('name', name)
    for variant in variants:
      variant_tag = doc.createElement('variant')
      variant_tag.setAttribute('name', variant.name)
      variant_tag.setAttribute('summary', variant.summary)
      variants_tag.appendChild(variant_tag)
    actions_element.appendChild(variants_tag)

  # Attach action node based on updated |actions_dict|.
  for _, action in sorted(actions_dict.items()):
    action_tag = _CreateActionTag(doc, action)
    if action_tag:
      actions_element.appendChild(action_tag)

  return actions_model.PrettifyTree(doc)


def UpdateXml(original_xml):
  actions_dict, comment_nodes, variants_dict = ParseActionFile(original_xml)

  expanded_actions_dict = copy.deepcopy(actions_dict)
  # Created a deep copy of the actions dictionary to expand variants into. This
  # is to avoid modifying the original Action objects in case of variants
  # present in the xml file, which would remove their token definitions.
  if variants_dict:
    try:
      action_utils.CreateActionsFromVariants(expanded_actions_dict,
                                             variants_dict)
    except Exception as e:
      logging.warning(str(e))

  actions = set()
  AddComputedActions(actions)
  AddWebUIActions(actions)
  AddDevToolsActions(actions)
  AddLiteralActions(actions)
  AddAutomaticResetBannerActions(actions)
  AddBookmarkManagerActions(actions)
  AddBookmarkUsageActions(actions)
  AddChromeOSActions(actions)
  AddExtensionActions(actions)
  AddHistoryPageActions(actions)
  AddPDFPluginActions(actions)

  for action_name in actions:
    if action_name not in expanded_actions_dict:
      actions_dict[action_name] = action_utils.Action(action_name, None, [])

  return PrettyPrint(actions_dict, comment_nodes, variants_dict)


def main(argv):
  presubmit_util.DoPresubmitMain(argv,
                                 'actions.xml',
                                 'actions.old.xml',
                                 UpdateXml,
                                 script_name='extract_actions.py')


if '__main__' == __name__:
  sys.exit(main(sys.argv))
