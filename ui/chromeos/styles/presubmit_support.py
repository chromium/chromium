# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def _CheckSemanticColors(input_api, output_api):
    """Check that any colors present in HTML, CSS or JS files that are CSS
    variables and start with a known CSS context (present in the ChromeOS root
    semantic stylesheet) are valid.

    NOTE: Calculating the valid CSS variables is an expensive operation so a
    first attempt is made to ensure lines affected have a var() occurrence which
    indicates we need to verify the variable.
    """
    file_filter = lambda f: input_api.FilterSourceFile(
        f, files_to_check=(r'.+\.css', r'.+\.htm(l){1,}$', r'.+\.js$'),
        files_to_skip=[r'.*test.*'])
    affected_files = input_api.AffectedFiles(include_deletes=False,
                                             file_filter=file_filter)
    if not affected_files:
        return []

    # Ensure the tools/ path is available to import style_variable_generator.
    input_api.sys.path.append(
        input_api.os_path.join(input_api.change.RepositoryRoot(), 'tools'))
    from style_variable_generator.css_generator import CSSStyleGenerator
    style_root = [
        input_api.change.RepositoryRoot(), 'ui', 'chromeos', 'styles'
    ]

    def ExtractPrefixesAndVariables(file_set):
      # Identify the CSS variable prefixes currently in use by the semantic
      # stylesheets.
      css_prefixes = set()
      style_generator = CSSStyleGenerator()
      style_generator.AddJSONFilesToModel(cros_styles)
      for file_path in cros_styles:
          context = style_generator.in_file_to_context.get(file_path,
                                                           {}).get('CSS')
          if (not context or 'prefix' not in context):
              continue

          css_prefixes.add('--' + context['prefix'] + '-')

      valid_css_variables = style_generator.GetCSSVarNames()
      return css_prefixes, valid_css_variables

    cros_styles = [
        input_api.os_path.join(*style_root, 'cros_colors.json5'),
        input_api.os_path.join(*style_root, 'cros_palette.json5'),
        input_api.os_path.join(*style_root, 'cros_ref_colors.json5'),
        input_api.os_path.join(*style_root, 'cros_shadows.json5'),
        input_api.os_path.join(*style_root, 'cros_sys_colors.json5'),
        input_api.os_path.join(*style_root, 'cros_typography.json5'),
        input_api.os_path.join(*style_root, 'cros_gm3_shadows.json5'),
    ]

    cros_prefixes, cros_variables = ExtractPrefixesAndVariables(cros_styles)

    gm3_styles = [
        input_api.os_path.join(*style_root, 'cros_gm3_typography.json5'),
    ]

    gm3_prefixes, gm3_variables = ExtractPrefixesAndVariables(gm3_styles)

    cros_variables.update(gm3_variables)
    valid_css_variables = cros_variables

    css_prefixes = cros_prefixes | gm3_prefixes
    css_regex = input_api.re.compile('(' + '|'.join(css_prefixes) +
                                     '.+?)[^-a-zA-Z0-9_]+')

    def FindInvalidCSSVariables(file_path, line_num, input_line):
        """Find invalid usages of CSS variables on |input_line| and return
        output that is formatted nicely for a PRESUBMIT warning.
        """
        css_variable_matches = input_api.re.findall(css_regex, input_line)

        if not css_variable_matches:
            return []

        invalid_matches = []
        for potential_invalid_match in css_variable_matches:
            if potential_invalid_match not in valid_css_variables:
                invalid_matches.append(
                    '%s:%d: %s' %
                    (file_path, line_num, potential_invalid_match))

        return invalid_matches

    invalid_variables = []
    for f in affected_files:
        for line_num, line in f.ChangedContents():
            if any(prefix in line for prefix in css_prefixes):
                invalid_variables.extend(
                    FindInvalidCSSVariables(f.LocalPath(), line_num, line))

    if not invalid_variables:
        return []

    return [
        output_api.PresubmitPromptWarning('CSS variables identified that ' +
                                          'are not present in the root ' +
                                          'ChromeOS semantic stylesheet.',
                                          items=invalid_variables)
    ]
