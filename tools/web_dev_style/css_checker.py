# Copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for Chromium WebUI resources.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools, and see
https://chromium.googlesource.com/chromium/src/+/master/styleguide/web/web.md
for the rules we're checking against here.
"""

# TODO(dbeam): Real CSS parser? https://github.com/danbeam/css-py/tree/css3

class CSSChecker(object):
  DISABLE_PREFIX = 'csschecker-disable'
  DISABLE_FORMAT = DISABLE_PREFIX + '(-[a-z]+)+ [a-z-]+(-[a-z-]+)*'
  DISABLE_LINE = DISABLE_PREFIX + '-line'

  def __init__(self, input_api, output_api, file_filter=None):
    self.input_api = input_api
    self.output_api = output_api
    self.file_filter = file_filter

  def RemoveAtBlocks(self, s):
    re = self.input_api.re

    def _remove_comments(s):
      return re.sub(r'/\*.*\*/', '', s)

    lines = s.splitlines()
    i = 0
    while i < len(lines):
      line = _remove_comments(lines[i]).strip()
      if (len(line) > 0 and line[0] == '@' and
          not line[1:].startswith(("apply", "page")) and
          line[-1] == '{' and not re.match("\d+x\b", line[1:])):
        j = i
        open_brackets = 1
        while open_brackets > 0:
          j += 1
          inner_line = _remove_comments(lines[j]).strip()
          if not inner_line:
            continue
          if inner_line[-1] == '{':
            open_brackets += 1
          elif inner_line[-1] == '}':
            # Ignore single line keyframes (from { height: 0; }).
            if not re.match(r'\s*(from|to|\d+%)\s*{', inner_line):
              open_brackets -= 1
          elif len(inner_line) > 1 and inner_line[-2:] == '};':
            # End of mixin. TODO(dbeam): worth detecting ": {" start?
            open_brackets -= 1
        del lines[j]  # Later index first, as indices shift with deletion.
        del lines[i]
      else:
        i += 1
    return '\n'.join(lines)

  def RunChecks(self):
    # We use this a lot, so make a nick name variable.
    re = self.input_api.re

    def _collapseable_hex(s):
      return (len(s) == 6 and s[0] == s[1] and s[2] == s[3] and s[4] == s[5])

    def _is_gray(s):
      return s[0] == s[1] == s[2] if len(s) == 3 else s[0:2] == s[2:4] == s[4:6]

    def _extract_inline_style(s):
      return '\n'.join(re.findall(r'<style\b[^>]*>([^<]*)<\/style>', s))

    def _remove_comments_except_for_disables(s):
      return re.sub(r'/\*(?! %s \*/$).*?\*/' % self.DISABLE_FORMAT, '', s,
                    flags=re.DOTALL | re.MULTILINE)

    def _remove_grit(s):
      return re.sub(r"""
          <if[^>]+>.*?<\s*/\s*if[^>]*>|  # <if> contents </if>
          <include[^>]+>                 # <include>
          """, '', s, flags=re.DOTALL | re.VERBOSE)

    mixin_shim_reg = r'[\w-]+_-_[\w-]+'

    def _remove_mixins_and_valid_vars(s):
      valid_vars = r'--(?!' + mixin_shim_reg + r')[\w-]+:\s*'
      mixin_or_value = r'({.*?}|[^;}]+);?\s*'
      return re.sub(valid_vars + mixin_or_value, '', s, flags=re.DOTALL)

    def _remove_disable(content, lstrip=False):
      prefix_reg = ('\s*' if lstrip else '')
      disable_reg = '/\* %s \*/' % self.DISABLE_FORMAT
      return re.sub(prefix_reg + disable_reg, '', content, re.MULTILINE)

    def _remove_template_expressions(s):
      return re.sub(r'\$i18n(Raw)?{[^}]*}', '', s, flags=re.DOTALL)

    def _rgb_from_hex(s):
      if len(s) == 3:
        r, g, b = s[0] + s[0], s[1] + s[1], s[2] + s[2]
      else:
        r, g, b = s[0:2], s[2:4], s[4:6]
      return int(r, base=16), int(g, base=16), int(b, base=16)

    def _strip_prefix(s):
      return re.sub(r'^-(?:o|ms|moz|khtml|webkit)-', '', s)

    def alphabetize_props(contents):
      errors = []
      # TODO(dbeam): make this smart enough to detect issues in mixins.
      strip_rule = lambda t: _remove_disable(t).strip()
      for rule in re.finditer(r'{(.*?)}', contents, re.DOTALL):
        semis = map(strip_rule, rule.group(1).split(';'))[:-1]
        rules = filter(lambda r: ': ' in r, semis)
        props = map(lambda r: r[0:r.find(':')], rules)
        if props != sorted(props):
          errors.append('    %s;\n' % (';\n    '.join(rules)))
      return errors

    def braces_have_space_before_and_nothing_after(line):
      brace_space_reg = re.compile(r"""
          (?:^|\S){|  # selector{ or selector\n{ or
          {\s*\S+\s*  # selector { with stuff after it
          $           # must be at the end of a line
          """,
          re.VERBOSE)
      return brace_space_reg.search(line)

    def classes_use_dashes(line):
      # Intentionally dumbed down version of CSS 2.1 grammar for class without
      # non-ASCII, escape chars, or whitespace.
      class_reg = re.compile(r"""
          (?<!')\.(-?[\w-]+).*  # ., then maybe -, then alpha numeric and -
          [,{]\s*$              # selectors should end with a , or {
          """,
          re.VERBOSE)
      m = class_reg.search(line)
      if not m:
        return False
      class_name = m.group(1)
      return class_name.lower() != class_name or '_' in class_name

    end_mixin_reg = re.compile(r'\s*};\s*$')

    def close_brace_on_new_line(line):
      # Ignore single frames in a @keyframe, i.e. 0% { margin: 50px; }
      frame_reg = re.compile(r"""
          \s*(from|to|\d+%)\s*{     # 50% {
          \s*[\w-]+:                # rule:
          (\s*[\w\(\), -\.]+)+\s*;  # value;
          \s*}\s*                   # }
          """,
          re.VERBOSE)
      return ('}' in line and re.search(r'[^ }]', line) and
              not frame_reg.match(line) and not end_mixin_reg.match(line))

    def colons_have_space_after(line):
      colon_space_reg = re.compile(r"""
          (?<!data)    # ignore data URIs
          :(?!//)      # ignore url(http://), etc.
          \S[^;]+;\s*  # only catch one-line rules for now
          """,
          re.VERBOSE)
      return colon_space_reg.search(line)

    def favor_single_quotes(line):
      return '"' in line

    # Shared between hex_could_be_shorter and rgb_if_not_gray.
    hex_reg = re.compile(r"""
        \#([a-fA-F0-9]{3}|[a-fA-F0-9]{6})  # pound followed by 3 or 6 hex digits
        (?=[^\w-]|$)                       # no more alphanum chars or at EOL
        (?!.*(?:{.*|,\s*)$)                # not in a selector
        """,
        re.VERBOSE)

    def hex_could_be_shorter(line):
      m = hex_reg.search(line)
      return (m and _is_gray(m.group(1)) and _collapseable_hex(m.group(1)))

    def rgb_if_not_gray(line):
      m = hex_reg.search(line)
      return (m and not _is_gray(m.group(1)))

    small_seconds_reg = re.compile(r"""
        (?:^|[^\w-])   # start of a line or a non-alphanumeric char
        (0?\.[0-9]+)s  # 1.0s
        (?!-?[\w-])    # no following - or alphanumeric chars
        """,
        re.VERBOSE)

    def milliseconds_for_small_times(line):
      return small_seconds_reg.search(line)

    def suggest_ms_from_s(line):
      ms = int(float(small_seconds_reg.search(line).group(1)) * 1000)
      return ' (replace with %dms)' % ms

    def no_data_uris_in_source_files(line):
      return re.search(r'\(\s*\s*data:', line)

    def no_mixin_shims(line):
      return re.search(r'--' + mixin_shim_reg + r'\s*:', line)

    def no_quotes_in_url(line):
      return re.search('url\s*\(\s*["\']', line, re.IGNORECASE)

    def one_rule_per_line(line):
      line = _remove_disable(line)
      one_rule_reg = re.compile(r"""
          [\w-](?<!data):  # a rule: but no data URIs
          (?!//)[^;]+;     # value; ignoring colons in protocols:// and };
          \s*[^ }]\s*      # any non-space after the end colon
          """,
          re.VERBOSE)
      return one_rule_reg.search(line) and not end_mixin_reg.match(line)

    def pseudo_elements_double_colon(contents):
      pseudo_elements = ['after',
                         'before',
                         'calendar-picker-indicator',
                         'color-swatch',
                         'color-swatch-wrapper',
                         'date-and-time-container',
                         'date-and-time-value',
                         'datetime-edit',
                         'datetime-edit-ampm-field',
                         'datetime-edit-day-field',
                         'datetime-edit-hour-field',
                         'datetime-edit-millisecond-field',
                         'datetime-edit-minute-field',
                         'datetime-edit-month-field',
                         'datetime-edit-second-field',
                         'datetime-edit-text',
                         'datetime-edit-week-field',
                         'datetime-edit-year-field',
                         'details-marker',
                         'file-upload-button',
                         'first-letter',
                         'first-line',
                         'inner-spin-button',
                         'input-placeholder',
                         'input-speech-button',
                         'media-slider-container',
                         'media-slider-thumb',
                         'meter-bar',
                         'meter-even-less-good-value',
                         'meter-inner-element',
                         'meter-optimum-value',
                         'meter-suboptimum-value',
                         'progress-bar',
                         'progress-inner-element',
                         'progress-value',
                         'resizer',
                         'scrollbar',
                         'scrollbar-button',
                         'scrollbar-corner',
                         'scrollbar-thumb',
                         'scrollbar-track',
                         'scrollbar-track-piece',
                         'search-cancel-button',
                         'search-decoration',
                         'search-results-button',
                         'search-results-decoration',
                         'selection',
                         'slider-container',
                         'slider-runnable-track',
                         'slider-thumb',
                         'textfield-decoration-container',
                         'validation-bubble',
                         'validation-bubble-arrow',
                         'validation-bubble-arrow-clipper',
                         'validation-bubble-heading',
                         'validation-bubble-message',
                         'validation-bubble-text-block']
      pseudo_reg = re.compile(r"""
          (?<!:):       # a single colon, i.e. :after but not ::after
          ([a-zA-Z-]+)  # a pseudo element, class, or function
          (?=[^{}]+?{)  # make sure a selector, not inside { rules }
          """,
          re.MULTILINE | re.VERBOSE)
      errors = []
      for p in re.finditer(pseudo_reg, contents):
        pseudo = p.group(1).strip().splitlines()[0]
        if _strip_prefix(pseudo.lower()) in pseudo_elements:
          errors.append('    :%s (should be ::%s)' % (pseudo, pseudo))
      return errors

    def one_selector_per_line(contents):
      any_reg = re.compile(r"""
          :(?:-webkit-)?any\(.*?\)  # :-webkit-any(a, b, i) selector
          """,
          re.DOTALL | re.VERBOSE)
      multi_sels_reg = re.compile(r"""
          (?:}\s*)?            # ignore 0% { blah: blah; }, from @keyframes
          ([^,]+,(?=[^{}]+?{)  # selector junk {, not in a { rule }
          .*[,{])\s*$          # has to end with , or {
          """,
          re.MULTILINE | re.VERBOSE)
      errors = []
      for b in re.finditer(multi_sels_reg, re.sub(any_reg, '', contents)):
        errors.append('    ' + b.group(1).strip().splitlines()[-1:][0])
      return errors

    def suggest_rgb_from_hex(line):
      suggestions = ['rgb(%d, %d, %d)' % _rgb_from_hex(h.group(1))
          for h in re.finditer(hex_reg, line)]
      return ' (replace with %s)' % ', '.join(suggestions)

    def suggest_short_hex(line):
      h = hex_reg.search(line).group(1)
      return ' (replace with #%s)' % (h[0] + h[2] + h[4])

    prefixed_logical_axis_reg = re.compile(r"""
        -webkit-(min-|max-|)logical-(height|width):
        """, re.VERBOSE)

    def suggest_unprefixed_logical_axis(line):
      prefix, prop = prefixed_logical_axis_reg.search(line).groups()
      block_or_inline = 'block' if prop == 'height' else 'inline'
      return ' (replace with %s)' % (prefix + block_or_inline + '-size')

    def prefixed_logical_axis(line):
      return prefixed_logical_axis_reg.search(line)

    prefixed_logical_side_reg = re.compile(r"""
        -webkit-(margin|padding|border)-(before|after|start|end)
        (?!-collapse)(-\w+|):
        """, re.VERBOSE)

    def suggest_unprefixed_logical_side(line):
      prop, pos, suffix = prefixed_logical_side_reg.search(line).groups()
      if pos == 'before' or pos == 'after':
        block_or_inline = 'block'
      else:
        block_or_inline = 'inline'
      if pos == 'start' or pos == 'before':
        start_or_end = 'start'
      else:
        start_or_end = 'end'
      return ' (replace with %s)' % (
        prop + '-' + block_or_inline + '-' + start_or_end + suffix)

    def prefixed_logical_side(line):
      return prefixed_logical_side_reg.search(line)

    _LEFT_RIGHT_REG = '(?:(border|margin|padding)-|(text-align): )' \
                      '(left|right)' \
                      '(?:(-[a-z-^:]+):)?(?!.*/\* %s left-right \*/)' % \
                      self.DISABLE_LINE

    def start_end_instead_of_left_right(line):
      return re.search(_LEFT_RIGHT_REG, line, re.IGNORECASE)

    def suggest_start_end_from_left_right(line):
      groups = re.search(_LEFT_RIGHT_REG, line, re.IGNORECASE).groups()
      prop_start, text_align, left_right, prop_end = groups
      start_end = {'left': 'start', 'right': 'end'}[left_right]
      if text_align:
        return ' (replace with text-align: %s)' % start_end
      prop = '%s-inline-%s%s' % (prop_start, start_end, prop_end or '')
      return ' (replace with %s)' % prop

    def zero_width_lengths(contents):
      hsl_reg = re.compile(r"""
          hsl\([^\)]*       # hsl(maybestuff
          (?:[, ]|(?<=\())  # a comma or space not followed by a (
          (?:0?\.?)?0%      # some equivalent to 0%
          """,
          re.VERBOSE)
      zeros_reg = re.compile(r"""
          ^.*(?:^|[^0-9.])              # start/non-number
          (?:\.0|0(?:\.0?               # .0, 0, or 0.0
          |px|em|%|in|cm|mm|pc|pt|ex))  # a length unit
          (?!svg|png|jpg)(?:\D|$)       # non-number/end
          (?=[^{}]+?}).*$               # only { rules }
          """,
          re.MULTILINE | re.VERBOSE)
      errors = []
      for z in re.finditer(zeros_reg, contents):
        first_line = z.group(0).strip().splitlines()[0]
        if not hsl_reg.search(first_line):
          errors.append('    ' + first_line)
      return errors

    # NOTE: Currently multi-line checks don't support 'after'. Instead, add
    # suggestions while parsing the file so another pass isn't necessary.
    added_or_modified_files_checks = [
        { 'desc': 'Alphabetize properties and list vendor specific (i.e. '
                  '-webkit) above standard.',
          'test': alphabetize_props,
          'multiline': True,
        },
        { 'desc': 'Start braces ({) end a selector, have a space before them '
                  'and no rules after.',
          'test': braces_have_space_before_and_nothing_after,
        },
        { 'desc': 'Classes use .dash-form.',
          'test': classes_use_dashes,
        },
        { 'desc': 'Always put a rule closing brace (}) on a new line.',
          'test': close_brace_on_new_line,
        },
        { 'desc': 'Colons (:) should have a space after them.',
          'test': colons_have_space_after,
        },
        { 'desc': 'Use single quotes (\') instead of double quotes (") in '
                  'strings.',
          'test': favor_single_quotes,
        },
        { 'desc': 'Use abbreviated hex (#rgb) when in form #rrggbb.',
          'test': hex_could_be_shorter,
          'after': suggest_short_hex,
        },
        { 'desc': 'Use milliseconds for time measurements under 1 second.',
          'test': milliseconds_for_small_times,
          'after': suggest_ms_from_s,
        },
        { 'desc': "Don't use data URIs in source files. Use grit instead.",
          'test': no_data_uris_in_source_files,
        },
        { 'desc': "Don't override custom properties created by Polymer's mixin "
                  "shim. Set mixins or documented custom properties directly.",
          'test': no_mixin_shims,
        },
        { 'desc': "Don't use quotes in url().",
          'test': no_quotes_in_url,
        },
        { 'desc': 'One rule per line (what not to do: color: red; margin: 0;).',
          'test': one_rule_per_line,
        },
        { 'desc': 'One selector per line (what not to do: a, b {}).',
          'test': one_selector_per_line,
          'multiline': True,
        },
        { 'desc': 'Pseudo-elements should use double colon (i.e. ::after).',
          'test': pseudo_elements_double_colon,
          'multiline': True,
        },
        { 'desc': 'Use rgb() over #hex when not a shade of gray (like #333).',
          'test': rgb_if_not_gray,
          'after': suggest_rgb_from_hex,
        },
        { 'desc': 'Unprefix logical axis property.',
          'test': prefixed_logical_axis,
          'after': suggest_unprefixed_logical_axis,
        },
        { 'desc': 'Unprefix logical side property.',
          'test': prefixed_logical_side,
          'after': suggest_unprefixed_logical_side,
        },
        {
          'desc': 'Use -start/end instead of -left/right ' \
                  '(https://goo.gl/gQYY7z, add /* %s left-right */ to ' \
                  'suppress)' % self.DISABLE_LINE,
          'test': start_end_instead_of_left_right,
          'after': suggest_start_end_from_left_right,
        },
        { 'desc': 'Use "0" for zero-width lengths (i.e. 0px -> 0)',
          'test': zero_width_lengths,
          'multiline': True,
        },
    ]

    results = []
    affected_files = self.input_api.AffectedFiles(include_deletes=False,
                                                  file_filter=self.file_filter)
    files = []
    for f in affected_files:
      path = f.LocalPath()

      is_html = path.endswith('.html')
      if not is_html and not path.endswith('.css'):
        continue

      file_contents = '\n'.join(f.NewContents())

      # Remove all /*comments*/, @at-keywords, and grit <if|include> tags; we're
      # not using a real parser. TODO(dbeam): Check alpha in <if> blocks.

      file_contents = _remove_grit(file_contents)  # Must be done first.

      if is_html:
        # The <style> extraction regex can't handle <if> nor /* <tag> */.
        prepped_html = _remove_comments_except_for_disables(file_contents)
        file_contents = _extract_inline_style(prepped_html)

      file_contents = self.RemoveAtBlocks(file_contents)

      if not is_html:
        file_contents = _remove_comments_except_for_disables(file_contents)

      file_contents = _remove_mixins_and_valid_vars(file_contents)
      file_contents = _remove_template_expressions(file_contents)

      files.append((path, file_contents))

    for f in files:
      file_errors = []
      for check in added_or_modified_files_checks:
        # If the check is multiline, it receives the whole file and gives us
        # back a list of things wrong. If the check isn't multiline, we pass it
        # each line and the check returns something truthy if there's an issue.
        if ('multiline' in check and check['multiline']):
          assert not 'after' in check
          check_errors = check['test'](f[1])
          if len(check_errors) > 0:
            file_errors.append('- %s\n%s' %
                (check['desc'], '\n'.join(check_errors).rstrip()))
        else:
          check_errors = []
          lines = f[1].splitlines()
          for lnum, line in enumerate(lines):
            if check['test'](line):
              error = '    ' + _remove_disable(line, lstrip=True).strip()
              if 'after' in check:
                error += check['after'](line)
              check_errors.append(error)
          if len(check_errors) > 0:
            file_errors.append('- %s\n%s' %
                (check['desc'], '\n'.join(check_errors)))
      if file_errors:
        results.append(self.output_api.PresubmitPromptWarning(
            '%s:\n%s' % (f[0], '\n\n'.join(file_errors))))

    return results
