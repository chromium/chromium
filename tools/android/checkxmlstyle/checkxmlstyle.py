# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script that is used by PRESUBMIT.py to check Android XML files.

This file checks for the following:
  - Colors are defined as RRGGBB or AARRGGBB
  - No (A)RGB values are referenced outside color_palette.xml
  - No duplicate (A)RGB values are referenced in color_palette.xml
  - Colors in semantic_colors are only referecing colors in color_palette.xml
  - XML namspace "app" is used for "http://schemas.android.com/apk/res-auto"
  - Android text attributes are only defined in text appearance styles
  - Warning on adding new text appearance styles
"""

from collections import defaultdict
import os
import re
import xml.etree.ElementTree as ET

import helpers

def CheckStyleOnUpload(input_api, output_api):
  """Returns result for all the presubmit upload checks for XML files."""
  result = _CommonChecks(input_api, output_api)
  result.extend(_CheckNewTextAppearance(input_api, output_api))
  return result


def CheckStyleOnCommit(input_api, output_api):
  """Returns result for all the presubmit commit checks for XML files."""
  return _CommonChecks(input_api, output_api)


def IncludedFiles(input_api, allow_list=helpers.INCLUDED_PATHS):
  # Filter out XML files outside included paths and files that were deleted.
  files = lambda f: input_api.FilterSourceFile(f, allow_list)
  return input_api.AffectedFiles(include_deletes=False, file_filter=files)


class LazyColorStateListSet:
  """
  This class has two jobs. It allows us to delay searching directories for color
  state list files unless we actually find a color reference to check against.
  And additionally it's a convenient mock point for tests to specify files in a
  slightly more robust way than relying on real color state list file names.
  """
  _color_set_or_none = None

  def get(self):
    if self._color_set_or_none != None:
      return self._color_set_or_none

    self._color_set_or_none = set()
    for color_dir in helpers.COLOR_STATE_LIST_DIRS:
      if not os.path.isdir(color_dir):
        continue
      for color_file in os.listdir(color_dir):
        if '.' in color_file:
          self._color_set_or_none.add(color_file[:color_file.index('.')])
    return self._color_set_or_none


def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  result = []
  result.extend(_CheckColorFormat(input_api, output_api))
  result.extend(_CheckColorReferences(input_api, output_api))
  result.extend(_CheckDuplicateColors(input_api, output_api))
  result.extend(_CheckSemanticColorsReferences(input_api, output_api))
  result.extend(_CheckColorPaletteReferences(input_api, output_api))
  result.extend(_CheckNonDynamicColorReference(input_api, output_api))
  result.extend(_CheckXmlNamespacePrefixes(input_api, output_api))
  result.extend(_CheckTextAppearance(input_api, output_api))
  result.extend(_CheckLineSpacingAttribute(input_api, output_api))
  result.extend(_CheckButtonCompatWidgetUsage(input_api, output_api))
  result.extend(_CheckStringResourceQuotesPunctuations(input_api, output_api))
  result.extend(_CheckStringResourceEllipsisPunctuations(input_api, output_api))
  result.extend(_CheckImportantForAccessibility(input_api, output_api))
  result.extend(_CheckBadStyleReference(input_api, output_api))
  # Add more checks here
  return result


### color resources below ###
def _CheckColorFormat(input_api, output_api):
  """Checks color (A)RGB values are of format either RRGGBB or AARRGGBB."""
  errors = []
  for f in IncludedFiles(input_api):
    # Ignore vector drawable xmls
    contents = input_api.ReadFile(f)
    if '<vector' in contents:
      continue
    for line_number, line in f.ChangedContents():
      color = helpers.COLOR_PATTERN.search(line)
      if color and not helpers.VALID_COLOR_PATTERN.match(color.group(2)):
        errors.append(
            '  %s:%d\n    \t%s' % (f.LocalPath(), line_number, line.strip()))
  if errors:
    return [
        output_api.PresubmitError(
            '''
  Android Color Reference Check failed:
    Your new code added (A)RGB values for colors that are not well
    formatted, listed below.

    This is banned, please define colors in format of #RRGGBB for opaque
    colors or #AARRGGBB for translucent colors. Note that they should be
    defined in chrome/android/java/res/values/color_palette.xml.

    If the new added color is a one-off color, please contact UX for approval
    and then add it to ui/android/java/res/values/one_off_colors.xml

    See https://crbug.com/775198 for more information.
  ''', errors)
    ]
  return []


def _CheckColorReferences(input_api, output_api):
  """
  Checks no (A)RGB values are defined outside color_palette.xml
  or one_off_colors.xml.
  """
  errors = []
  warnings = []
  for f in IncludedFiles(input_api):
    if (f.LocalPath() == helpers.COLOR_PALETTE_RELATIVE_PATH
        or f.LocalPath() == helpers.ONE_OFF_COLORS_RELATIVE_PATH):
      continue
    # Ignore new references in vector/shape drawable xmls
    contents = input_api.ReadFile(f)
    is_vector_drawable = '<vector' in contents or '<shape' in contents
    for line_number, line in f.ChangedContents():
      if helpers.COLOR_PATTERN.search(line):
        issue = '  %s:%d\n    \t%s' % (f.LocalPath(), line_number, line.strip())
        if is_vector_drawable:
          warnings.append(issue)
        else:
          errors.append(issue)
  result = []
  if errors:
    result += [
        output_api.PresubmitError(
            '''
  Android Color Reference Check failed:
    Your new code contains hard coded hex color values in a resource file. You
    likely should be using a @macro or color state list to support dynamic
    colors, see
    https://chromium.googlesource.com/chromium/src/+/main/docs/ui/android/dynamic_colors.md

    In the cases where you purposefully want fixed colors (like incognito), at
    the very least @color references to color_palette.xml or one_off_colors.xml
    will be necessary.

    If the new added color is a one-off color, please contact UX for approval
    and then add it to ui/android/java/res/values/one_off_colors.xml

    See https://crbug.com/775198 for more information.
  ''', errors)
    ]
  if warnings:
    result += [
        output_api.PresubmitPromptWarning(
            '''
  Android Color Reference Check warning:
    Your new code contains hard coded hex color values in a resource file. You
    likely should be using a @macro or color state list to support dynamic
    colors, see
    https://chromium.googlesource.com/chromium/src/+/main/docs/ui/android/dynamic_colors.md

    In the cases where you purposefully want fixed colors (like incognito), at
    the very least @color references to color_palette.xml or one_off_colors.xml
    will be necessary.

    Only bypass this check if you are confident that you should be using a HEX
    reference, e.g. you are adding an illustration or a shadow using XML rather
    than a PNG/9-patch.

    Please contact src/chrome/android/java/res/OWNERS for questions.
  ''', warnings)
    ]
  return result

def _CheckDuplicateColors(input_api, output_api):
  """
  Checks colors defined by (A)RGB values in color_palette.xml and
  one_off_colors.xml are unique.
  """
  errors = []
  for f in IncludedFiles(input_api):
    if (f.LocalPath() != helpers.COLOR_PALETTE_RELATIVE_PATH
        and f.LocalPath() != helpers.ONE_OFF_COLORS_RELATIVE_PATH):
      continue
    colors = defaultdict(int)
    contents = input_api.ReadFile(f)
    # Get count for each color defined.
    for line in contents.splitlines(False):
      color = helpers.COLOR_PATTERN.search(line)
      if color:
        colors[color.group(2)] += 1

    # Check duplicates in changed contents.
    for line_number, line in f.ChangedContents():
      color = helpers.COLOR_PATTERN.search(line)
      if color and colors[color.group(2)] > 1:
        errors.append(
            '  %s:%d\n    \t%s' % (f.LocalPath(), line_number, line.strip()))
  if errors:
    return [
        output_api.PresubmitError(
            '''
  Android Duplicate Color Declaration Check failed:
    Your new code added new colors by (A)RGB values that are already defined in
    ui/android/java/res/values/color_palette.xml or
    ui/android/java/res/values/one_off_colors.xml, listed below.

    This is banned, please reference the existing color resource from
    color_palette.xml or one_off_colors.xml using @color/... and if needed,
    give the existing color resource a more general name (e.g. baseline_neutral_90).

    See https://crbug.com/775198 for more information.
  ''', errors)
    ]
  return []


def _CheckColorPaletteReferences(input_api, output_api):
  """
  Checks colors defined in color_palette.xml are not references in colors.xml.
  """
  warnings = []
  color_palette = None

  for f in IncludedFiles(input_api):
    if not f.LocalPath().endswith('/colors.xml'):
      continue

    if color_palette is None:
      color_palette = _colorXml2Dict(
          input_api.ReadFile(helpers.COLOR_PALETTE_PATH))
    for line_number, line in f.ChangedContents():
      r = helpers.COLOR_REFERENCE_PATTERN.search(line)
      if not r:
        continue
      color = r.group()
      if _removePrefix(color) in color_palette:
        warnings.append(
            '  %s:%d\n    \t%s' % (f.LocalPath(), line_number, line.strip()))

  if warnings:
    return [
        output_api.PresubmitPromptWarning(
            '''
  Android Color Palette Reference Check warning:
    Your new color values added in colors.xml are defined in color_palette.xml.

    We can recommend using semantic colors already defined in
    ui/android/java/res/values/semantic_colors_non_adaptive.xml
    or ui/android/java/res/values/semantic_colors_adaptive.xml if possible.

    See https://crbug.com/775198 for more information.
  ''', warnings)
    ]
  return []


def _CheckSemanticColorsReferences(input_api, output_api):
  """
  Checks colors defined in semantic_colors_non_adaptive.xml only referencing
  resources in self or color_palette.xml.
  """
  errors = []
  usable_colors = None

  for f in IncludedFiles(input_api):
    if not f.LocalPath().endswith('/semantic_colors_non_adaptive.xml'):
      continue

    if usable_colors is None:
      color_palette = _colorXml2Dict(
        input_api.ReadFile(helpers.COLOR_PALETTE_PATH))
      self_palette = _colorXml2Dict(input_api.ReadFile(f.AbsoluteLocalPath()))
      usable_colors = {**color_palette, **self_palette}
    for line_number, line in f.ChangedContents():
      r = helpers.COLOR_REFERENCE_PATTERN.search(line)
      if not r:
        continue
      color_ref = r.group()
      if _removePrefix(color_ref) not in usable_colors:
        errors.append(
            '  %s:%d\n    \t%s' % (f.LocalPath(), line_number, line.strip()))

  if errors:
    return [
        output_api.PresubmitError(
            '''
  Android Semantic Color Reference Check failed:
    Your new color values added in semantic_colors_non_adaptive.xml are not
    defined in ui/android/java/res/values/color_palette.xml, listed below.

    This is banned. Colors in semantic colors can only reference
    the existing color resource from color_palette.xml.

    See https://crbug.com/775198 for more information.
  ''', errors)
    ]
  return []


def _CheckNonDynamicColorReference(
    input_api, output_api, lazy_color_state_list_set=LazyColorStateListSet()):
  """
  Checks for @color references that will not work with dynamic colors.
  """

  warnings = []
  for f in IncludedFiles(input_api,
                         allow_list=helpers.DYNAMIC_COLOR_INCLUDED_PATHS):
    for line_number, line in f.ChangedContents():
      r = helpers.COLOR_REFERENCE_PATTERN.search(line)
      if not r:
        continue

      color_name = r.group(1)
      if color_name not in lazy_color_state_list_set.get():
        issue = '  %s:%d\n    \t%s' % (f.LocalPath(), line_number, line.strip())
        warnings.append(issue)

  if warnings:
    return [
        output_api.PresubmitPromptWarning(
            '''
Dynamic Color Reference Check warning:
  Your new code is using @color references. These will not correctly support
  dynamic colors. Instead you should use a @macro that routes into an ?attr.
  Note using color references is currently okay for incognito code, as it should
  not be dynamically colored. See
  https://chromium.googlesource.com/chromium/src/+/main/docs/ui/android/dynamic_colors.md.
          ''', warnings)
    ]

  return []


def _CheckXmlNamespacePrefixes(input_api, output_api):
  """Checks consistency of prefixes used for XML namespace names."""
  errors = []
  for f in IncludedFiles(input_api):
    for line_number, line in f.ChangedContents():
      xml_app_namespace = helpers.XML_APP_NAMESPACE_PATTERN.search(line)
      if xml_app_namespace and not xml_app_namespace.group(1) == 'app':
        errors.append(
            '  %s:%d\n    \t%s' % (f.LocalPath(), line_number, line.strip()))
  if errors:
    return [
        output_api.PresubmitError(
            '''
  XML Namespace Prefixes Check failed:
    Your new code added new xml namespace declaration that is not consistent
    with other XML files. Namespace "http://schemas.android.com/apk/res-auto"
    should use 'app' prefix:

    xmlns:app="http://schemas.android.com/apk/res-auto"

    See https://crbug.com/850616 for more information.
  ''', errors)
    ]
  return []


### text appearance below ###
def _CheckTextAppearance(input_api, output_api):
  """Checks text attributes are only used for text appearance styles in XMLs."""
  text_attributes = [
      'android:textColor', 'android:textSize', 'android:textStyle',
      'android:fontFamily', 'android:textAllCaps']
  namespace = {'android': 'http://schemas.android.com/apk/res/android'}
  errors = []
  differences = False
  for f in IncludedFiles(input_api):
    try:
      root = ET.fromstring(input_api.ReadFile(f))
    except ET.ParseError:
      print('*' * 80)
      print('Parse error processing file:', f)
      print('*' * 80)
      raise
    # Check if there are text attributes defined outside text appearances.
    for attribute in text_attributes:
      # Get style name that contains text attributes but is not text appearance.
      invalid_styles = []
      for style in root.findall('style') + root.findall('.//style'):
        name = style.get('name')
        is_text_appearance = helpers.TEXT_APPEARANCE_STYLE_PATTERN.search(name)
        item = style.find(".//item[@name='"+attribute+"']")
        if is_text_appearance is None and item is not None:
          invalid_styles.append(name)
      # Append error messages.
      contents = input_api.ReadFile(f)
      style_count = 0
      widget_count = len(root.findall('[@'+attribute+']', namespace)) + len(
          root.findall('.//*[@'+attribute+']', namespace))
      for line_number, line in enumerate(contents.splitlines(False)):
        # Error for text attributes in non-text-appearance style.
        if (style_count < len(invalid_styles) and
            invalid_styles[style_count] in line):
          errors.append('  %s:%d contains attribute %s\n    \t%s' % (
              f.LocalPath(), line_number+1, attribute, line.strip()))
          style_count += 1
          if f.ChangedContents():
            differences = True
        # Error for text attributes in layout.
        if widget_count > 0 and attribute in line:
          errors.append('  %s:%d contains attribute %s\n    \t%s' % (
              f.LocalPath(), line_number+1, attribute, line.strip()))
          widget_count -= 1
          if f.ChangedContents():
            differences = True
  # TODO(huayinz): Change the path on the error message to the corresponding
  # styles.xml when this check applies to all resource directories.
  if errors:
    message = ('''
  Android Text Appearance Check failed:
    Your modified files contain Android text attributes defined outside
    text appearance styles, listed below.

    It is recommended to use the pre-defined text appearance styles in
      src/ui/android/java/res/values-v17/styles.xml

    And to use
      android:textAppearance="@style/SomeTextAppearance"
    in the XML layout whenever possible.

    If your text appearance absolutely has to deviate from the existing
    pre-defined text appearance style, you will need UX approval for adding a
    new text appearance style.

    If your approved text appearance style is a common text appreance style,
    please define it in src/ui/android/java/res/values-v17/styles.xml.

    Otherwise, if your approved text appearance is feature-specific, in
    chrome/android/java/res/values*/styles.xml, please define
      <style name="TextAppearance.YourTextAppearanceName>
        <item name="android:textColor">...</item>
        <item name="android:textSize">...</item>
        ...
      </style>

    Please contact arminaforoughi@chromium.org for UX approval, and
    src/chrome/android/java/res/OWNERS for questions.
    See https://crbug.com/775198 for more information.
  ''')
    if differences:
      return [output_api.PresubmitError(message, errors)]
    else:
      # Report a warning instead of an error when running "presubmit --all" or
      # "presubmit --files" so that these can run error free.
      return [output_api.PresubmitPromptWarning(message, errors)]
  return []


def _CheckNewTextAppearance(input_api, output_api):
  """Checks whether a new text appearance style is defined."""
  errors = []
  for f in IncludedFiles(input_api):
    for line_number, line in f.ChangedContents():
      if '<style name="TextAppearance.' in line:
        errors.append(
            '  %s:%d\n    \t%s' % (f.LocalPath(), line_number, line.strip()))
  if errors:
    return [
        output_api.PresubmitPromptWarning(
            '''
  New Text Appearance in styles.xml Check failed:
    Your new code added, edited or removed a text appearance style.
    If you are removing or editing an existing text appearance style, or your
    new text appearance style is approved by UX, please bypass this check.

    Otherwise, please contact arminaforoughi@chromium.org for UX approval, and
    src/chrome/android/java/res/OWNERS for questions.
    See https://crbug.com/775198 for more information.
  ''', errors)
    ]
  return []

### unfavored layout attributes below ###
def _CheckLineSpacingAttribute(input_api, output_api):
  """
  Encourage using TextViewWithLeading rather than android:lineSpacingExtra
  and android:lineSpacingMultiplier.
  """
  warnings = []
  attributes = ['android:lineSpacingExtra', 'android:lineSpacingMultiplier']
  for f in IncludedFiles(input_api):
    for line_number, line in f.ChangedContents():
      for attribute in attributes:
        if attribute in line:
          warnings.append(
              '  %s:%d\n    \t%s' % (f.LocalPath(), line_number, line.strip()))

  if warnings:
    return [
      output_api.PresubmitPromptWarning(
          '''
  Android XML Widget Check warning:
    Your new code is using android:lineSpacingExtra
    or android:lineSpacingMultiplier, listed below.

    Use org.chromium.ui.widget.TextViewWithLeading instead of
    using android:lineSpacingExtra or android:lineSpacingMultiplier if possible;
    TextViewWithLeading is a TextView with the added leading property, which can
    perform the calculation to setup leading correctly.

    See https://crbug.com/1069805 for more information.
  ''', warnings)
    ]

  return []

### important for accessibility below ###
def _CheckImportantForAccessibility(input_api, output_api):
  """
  Encourage android:importantForAccessibility="no" rather than
  tools:ignore="ContentDescription" for images that don't need content
  descriptions.
  """
  warnings = []
  attributes = ['tools:ignore="ContentDescription"']
  for f in IncludedFiles(input_api):
    for line_number, line in f.ChangedContents():
      for attribute in attributes:
        if attribute in line:
          warnings.append(
            '  %s:%d\n    \t%s' % (f.LocalPath(), line_number, line.strip()))

  if warnings:
    return [
      output_api.PresubmitPromptWarning(
          '''
  Android XML Widget Check warning:
    Your new code is using tools:ignore="ContentDescription", listed below.

    Use android:importantForAccessibility="no" instead of tools:ignore="ContentDescription"
    in your ImageView unless it is important for accessibility and a content description is set
    in Java.

    See https://crbug.com/1245341 for more information.
  ''', warnings)
    ]

  return []


### bad style reference below ###
def _CheckBadStyleReference(input_api, output_api):
  """Checks whether style attribute reference could work."""
  errors = []
  for f in IncludedFiles(input_api):
    for line_number, line in f.ChangedContents():
      match = helpers.KNOWN_STYLE_ATTRIBUTE.search(line)
      if match and not helpers.STYLE_REF_PREFIX.search(match.group(2)):
        errors.append('  %s:%d\n    \t%s' %
                      (f.LocalPath(), line_number, line.strip()))
  if errors:
    return [
        output_api.PresubmitPromptWarning(
            '''
  Style Reference Check failed:
    Your modified resource file has declared a style attribute, but does not
    prefix the style reference with a ? (for attributes) or @ (for styles). It's
    very likely this style is not being resolved correctly at runtime.
  ''', errors)
    ]
  return []


### unfavored android widgets below ###
def _CheckButtonCompatWidgetUsage(input_api, output_api):
  """Encourage using ButtonCompat rather than Button, AppButtonCompat"""
  warnings = []

  for f in IncludedFiles(input_api):
    # layout resource files
    for line_number, line in f.ChangedContents():
      if (re.search(r'<Button$', line) or
          re.search(r'<android.support.v7.widget.AppCompatButton$', line)):
        warnings.append(
            '  %s:%d\n    \t%s' % (f.LocalPath(), line_number, line.strip()))

  if warnings:
    return [
        output_api.PresubmitPromptWarning(
            '''
  Android Widget Check warning:
    Your new code is using Button or AppCompatButton, listed below.

    Use org.chromium.ui.widget.ButtonCompat instead of Button and
    AppCompatButton if possible; ButtonCompat is a Material-styled button with a
    customizable background color. On L devices, this is a true Material button.
    On earlier devices, the button is similar but lacks ripples and a shadow.

    See https://crbug.com/775198 and https://crbug.com/908651 for
    more information.
  ''', warnings)
    ]

  return []


### String resource check ###
def _CheckStringResourceQuotesPunctuations(input_api, output_api):
  """Check whether inappropriate quotes are used"""
  warning = '''
  Android String Resources Check failed:
    Your new string is using one or more of generic quotes (\u0022 \\u0022, \u0027 \\u0027,
    \u0060 \\u0060, \u00B4 \\u00B4), which is not encouraged. Instead, quotations marks
    (\u201C \\u201C, \u201D \\u201D, \u2018 \\u2018, \u2019 \\u2019) are usually preferred (see
    https://material.io/archive/guidelines/style/writing.html#writing-capitalization-punctuation).

    Use prime (\u2032 \\u2032) only in abbreviations for feet, arcminutes, and minutes.
    Use double-prime (\u2033 \\u2033) only in abbreviations for inches, arcseconds, and seconds.
    Use the right single quotation mark (\u2019 \\u2019) for apostrophes.

    Please reach out to the UX designer/writer in your team to double check
    which punctuation should be correctly used. Ignore this warning if UX has confirmed.

    Reach out to writing-strings@chromium.org if you have any question about writing strings.
  '''
  return _checkStringResourcePunctuations(
      re.compile(u'[\u0022\u0027\u0060\u00B4]'), warning, input_api, output_api)


def _CheckStringResourceEllipsisPunctuations(input_api, output_api):
  """Check whether inappropriate ellipsis are used"""
  warning = '''
  Android String Resources Check failed:
    Your new string appears to use three periods(\u002E \\u002E) to represent
    an ellipsis, which is not encouraged. Instead, an ellipsis mark
    (\u2026 \\u2026) is usually preferred.

    Please reach out to the UX designer/writer in your team to double check
    which punctuation should be correctly used. Ignore this warning if UX has confirmed.

    Reach out to writing-strings@chromium.org if you have any question about writing strings.
  '''
  return _checkStringResourcePunctuations(re.compile(u'[\u002E]{3}'), warning,
                                          input_api, output_api)


### helpers ###
def _colorXml2Dict(content):
  dct = dict()
  tree = ET.fromstring(content)
  for child in tree:
    dct[child.attrib['name']] = child.text
  return dct


def _removePrefix(color, prefix='@color/'):
  if color.startswith(prefix):
    return color[len(prefix):]
  return color


def _checkStringResourcePunctuations(regex, warning, input_api, output_api):
  """Check whether inappropriate punctuations are used"""
  warnings = []
  result = []
  # Removing placeholders for parsing purpose:
  # placeholders will be parsed as children of the parent node.
  ph = re.compile(r'<ph>.*</ph>')
  for f in IncludedFiles(input_api, helpers.INCLUDED_GRD_PATHS):
    contents = input_api.ReadFile(f)

    contents = re.sub(ph, '', contents)
    tree = ET.fromstring(contents)

    # some grds don't contain release and messages tags
    if tree.find('release') is None:
      continue
    if tree.find('release').find('messages') is None:
      continue

    messages = tree.find('release').find('messages')

    quotes = set()
    for child in messages:
      if child.tag == 'message':
        lines = child.text.split('\n')
        quotes.update(l for l in lines if regex.search(l))

    # Only report the lines in the changed contents of the current workspace
    for line_number, line in f.ChangedContents():
      lineWithoutPh = re.sub(ph, '', line)
      if lineWithoutPh in quotes:
        warnings.append('  %s:%d\n    \t%s' %
                        (f.LocalPath(), line_number, line))

  if warnings:
    result += [output_api.PresubmitPromptWarning(warning, warnings)]
  return result
