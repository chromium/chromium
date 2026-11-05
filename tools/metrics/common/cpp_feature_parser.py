# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Library for parsing and validating Chromium C++ BASE_FEATURE macros."""

from collections.abc import Iterable
import dataclasses
import enum
import re

import setup_modules  # pylint: disable=unused-import
from chromium_src.tools.metrics.common import cpp_preprocessor


class FeatureState(enum.Enum):
  ENABLED = 'ENABLED'
  DISABLED = 'DISABLED'
  UNKNOWN = 'UNKNOWN'
  ENABLED_FOR_COUNTRIES = 'ENABLED_FOR_COUNTRIES'
  DISABLED_FOR_COUNTRIES = 'DISABLED_FOR_COUNTRIES'


@dataclasses.dataclass(frozen=True)
class FeatureStateDetail:
  state: FeatureState
  countries: frozenset[str] = frozenset()


_ENABLED_STATE_REGEX = re.compile(
  r'(?:base::)?(?:FeatureState::)?FEATURE_ENABLED_BY_DEFAULT|ENABLED|'
  r'FeatureState::Enabled'
)
_DISABLED_STATE_REGEX = re.compile(
  r'(?:base::)?(?:FeatureState::)?FEATURE_DISABLED_BY_DEFAULT|DISABLED|'
  r'FeatureState::Disabled'
)
_ENABLED_FOR_COUNTRIES_REGEX = re.compile(
  r'(?:base::)?(?:FeatureState::)?FEATURE_ENABLED_FOR_COUNTRIES'
)
_DISABLED_FOR_COUNTRIES_REGEX = re.compile(
  r'(?:base::)?(?:FeatureState::)?FEATURE_DISABLED_FOR_COUNTRIES'
)

# The leading \b avoids matching wrapper macros whose names end in
# BASE_FEATURE (e.g. MAKE_STATIC_STORAGE_BASE_FEATURE).
CPP_FEATURE_STATE_REGEX = re.compile(
  r'\b(?:BASE_FEATURE(?:_WITH_COUNTRY_RESTRICTIONS)?'
  r'|BASE_RUNTIME_MUTABLE_FEATURE)\(([^;]*?)\);',
  re.DOTALL,
)
# Matches the `enable_if(<condition>)` helpers defined in
# components/omnibox/common/omnibox_features.cc and
# components/omnibox/common/omnibox_feature_configs.cc, which return ENABLED if
# the condition holds and DISABLED otherwise.
_ENABLE_IF_REGEX = re.compile(r'enable_if\s*\((.*)\)', re.DOTALL)
# Splits a ternary `<condition> ? <if_true> : <if_false>` on ':' but not on
# '::' (e.g. in base::FEATURE_ENABLED_BY_DEFAULT).
_TERNARY_ELSE_SPLIT_REGEX = re.compile(r'(?<!:):(?!:)')

PLATFORM_TO_CONDITION = {
  'windows': 'IS_WIN',
  'mac': 'IS_MAC',
  'linux': 'IS_LINUX',
  'chromeos': 'IS_CHROMEOS',
  'android': 'IS_ANDROID',
  'android_webview': 'IS_ANDROID',
  'android_desktop': 'IS_ANDROID',
  'ios': 'IS_IOS',
}

DERIVED_PLATFORM_CONDITIONS = {
  'IS_APPLE': {'IS_MAC', 'IS_IOS'},
  'IS_POSIX': {'IS_MAC', 'IS_LINUX', 'IS_CHROMEOS', 'IS_ANDROID', 'IS_IOS'},
  'IS_OZONE': {'IS_LINUX', 'IS_CHROMEOS'},
  'IS_CHROMEOS_DEVICE': {'IS_CHROMEOS'},
  # Matches the `constexpr bool IS_DESKTOP = !IS_ANDROID && !IS_IOS` alias in
  # components/omnibox/common/omnibox_feature_configs.cc.
  'IS_DESKTOP': {'IS_WIN', 'IS_MAC', 'IS_LINUX', 'IS_CHROMEOS'},
}

PLATFORM_EXTRA_DEFINES = {
  'android_desktop': {'IS_DESKTOP_ANDROID'},
}

# CppPreprocessor evaluates unknown symbols to True, while
# _EvaluatePlatformCondition evaluates symbols outside known_symbols to None
# (falling back to UNKNOWN). Known symbols include all primary platform
# conditions, derived platform macros, and other platform/OS buildflags from
# Chromium build/build_config.h and build flag headers so they evaluate to
# False rather than True when checking supported platforms.
KNOWN_PLATFORM_SYMBOLS = frozenset(
  [
    *PLATFORM_TO_CONDITION.values(),
    *DERIVED_PLATFORM_CONDITIONS.keys(),
    'IS_DESKTOP_ANDROID',
    # build/build_config.h OS flags:
    'IS_AIX',
    'IS_ASMJS',
    'IS_BSD',
    'IS_FREEBSD',
    'IS_FUCHSIA',
    'IS_NETBSD',
    'IS_OPENBSD',
    'IS_QNX',
    'IS_SOLARIS',
    'IS_IOS_MACCATALYST',
    'IS_IOS_TVOS',
    'IS_WATCHOS',
    # //build:chromecast_buildflags:
    'IS_CASTOS',
    'IS_CAST_ANDROID',
    # //build/config/chromebox_for_meetings & //build:chromeos_buildflags:
    'PLATFORM_CFM',
    'IS_REVEN',
    'IS_CHROMEOS_WITH_HW_DETAILS',
    # //build:ios_buildflags & //build:robolectric_buildflags:
    'IS_IOS_APP_EXTENSION',
    'IS_ROBOLECTRIC',
  ]
)


def GetPlatformDefines(platform: str) -> set[str]:
  """Returns the active preprocessor defines for a given platform."""
  define_str = PLATFORM_TO_CONDITION[platform]
  return (
    {define_str}
    | PLATFORM_EXTRA_DEFINES.get(platform, set())
    | {
      macro
      for macro, triggers in DERIVED_PLATFORM_CONDITIONS.items()
      if define_str in triggers
    }
  )


def ParseCountryRestrictions(args_str: str) -> list[str]:
  """Parses country codes from a country-restricted BASE_FEATURE macro.

  Expected input format for args_str is comma-separated arguments:
    kFeature, default_state, "us", "ca", ...
  where args[0] is the feature variable name, args[1] is default state,
  and args[2:] are variadic quoted country code string literals.

  Args:
    args_str: Comma-separated arguments inside the macro parentheses.

  Returns:
    List of parsed country code strings (e.g. ['us', 'ca']).
  """
  args = args_str.split(',')
  if len(args) < 3:
    return []
  return [a.strip().strip('"') for a in args[2:]]


def ParseCountryRestrictedFeatureDetail(
  full_macro_text: str, args_str: str, state_arg: str
) -> FeatureStateDetail | None:
  """Parses country restrictions for a country-restricted BASE_FEATURE macro."""
  if not full_macro_text.startswith('BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS'):
    return None
  countries = frozenset(ParseCountryRestrictions(args_str))
  if _ENABLED_FOR_COUNTRIES_REGEX.fullmatch(state_arg):
    return FeatureStateDetail(FeatureState.ENABLED_FOR_COUNTRIES, countries)
  if _DISABLED_FOR_COUNTRIES_REGEX.fullmatch(state_arg):
    return FeatureStateDetail(FeatureState.DISABLED_FOR_COUNTRIES, countries)
  return FeatureStateDetail(FeatureState.UNKNOWN)


def _EvaluatePlatformCondition(
  expression: str, defines: Iterable[str]
) -> bool | None:
  """Evaluates a C++ platform condition against `defines`.

  Supports `BUILDFLAG(IS_X)`, bare `IS_X` identifiers (e.g. the
  `constexpr bool IS_ANDROID = !!BUILDFLAG(IS_ANDROID)` aliases in
  omnibox_features.cc), `!`, `&&`, `||` and parentheses.

  Unlike CppPreprocessor, which treats unknown symbols as true, this returns
  None for any symbol outside KNOWN_PLATFORM_SYMBOLS (e.g. runtime variables
  such as `kDefaultEnabled` or unrecognized buildflags), so that callers can
  fall back to UNKNOWN.

  Args:
    expression: The C++ condition expression.
    defines: Active preprocessor defines for the platform.

  Returns:
    The value of the condition, or None if it cannot be evaluated.
  """
  return cpp_preprocessor.evaluate_condition(
    expression,
    true_symbols=defines,
    known_symbols=KNOWN_PLATFORM_SYMBOLS,
  )


def _ParseSimpleState(state_arg: str) -> FeatureState | None:
  """Returns the state for a single enabled/disabled state constant, or None."""
  if _ENABLED_STATE_REGEX.fullmatch(state_arg):
    return FeatureState.ENABLED
  if _DISABLED_STATE_REGEX.fullmatch(state_arg):
    return FeatureState.DISABLED
  return None


def _ParseConditionalState(
  state_arg: str, defines: Iterable[str]
) -> FeatureState | None:
  """Resolves a platform-conditional state argument against `defines`.

  Handles `enable_if(<condition>)` and
  `<condition> ? <state constant> : <state constant>`.

  Args:
    state_arg: The state argument of a BASE_FEATURE macro.
    defines: Active preprocessor defines for the platform.

  Returns:
    The resolved state, or None if `state_arg` is not a supported conditional
    expression or its condition cannot be evaluated.
  """
  if enable_if_match := _ENABLE_IF_REGEX.fullmatch(state_arg):
    condition = _EvaluatePlatformCondition(enable_if_match.group(1), defines)
    if condition is None:
      return None
    return FeatureState.ENABLED if condition else FeatureState.DISABLED

  if state_arg.count('?') != 1:
    return None
  condition_str, branches_str = state_arg.split('?')
  branches = _TERNARY_ELSE_SPLIT_REGEX.split(branches_str)
  if len(branches) != 2:
    return None
  condition = _EvaluatePlatformCondition(condition_str, defines)
  if condition is None:
    return None
  return _ParseSimpleState(branches[0 if condition else 1].strip())


def ParseCppFeatureMacro(
  full_macro_text: str,
  args_str: str,
  feature_name: str,
  defines: Iterable[str],
) -> FeatureStateDetail | None:
  """Parses a C++ BASE_FEATURE macro invocation for a specific feature.

  Legitimate macro formats in Chromium source code include:
    1. 2-param BASE_FEATURE (identifier-derived name with 'k' prefix):
         BASE_FEATURE(kFeatureName, base::FEATURE_ENABLED_BY_DEFAULT);
       where args_str is 'kFeatureName, base::FEATURE_..._BY_DEFAULT'.
    2. 3-param BASE_FEATURE (explicit quoted string name):
         BASE_FEATURE(kVar, "FeatureName", base::FEATURE_...);
       where args_str is 'kVar, "FeatureName", base::FEATURE_...'.
    3. Country-restricted macro (variadic country strings):
         BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS(
             kFeatureName, base::FEATURE_ENABLED_FOR_COUNTRIES, "us", "ca");
       where args_str is 'kFeatureName, base::FEATURE_..., "us", "ca"'.

  BASE_RUNTIME_MUTABLE_FEATURE accepts the same 2-param and 3-param forms as
  BASE_FEATURE.

  In 2-param and 3-param macros, the state argument may also be a platform
  conditional such as `BUILDFLAG(IS_ANDROID) ? base::FEATURE_ENABLED_BY_DEFAULT
  : base::FEATURE_DISABLED_BY_DEFAULT` or `enable_if(!IS_IOS)`, which is
  resolved against `defines`.

  Args:
    full_macro_text: The complete macro text including macro name.
    args_str: The arguments string inside the macro parentheses.
    feature_name: The name of the feature to match against.
    defines: Active preprocessor defines for the platform.

  Returns:
    FeatureStateDetail if this macro matches feature_name, or None otherwise.
  """
  # Split macro arguments by comma. Assumes macro arguments in Chromium do not
  # contain commas within quoted strings or compound expressions.
  args = [a.strip() for a in args_str.split(',')]

  # In 3-param BASE_FEATURE(kVar, "Name", state), args[1] is a quoted string
  # literal for the feature name. Requiring a full match avoids misclassifying
  # 2-param macros whose state expression contains a string literal.
  quoted_string_match = (
    re.fullmatch(r'"([^"]*)"', args[1]) if len(args) > 1 else None
  )

  if quoted_string_match:
    # 3-param macro: quoted string must match feature_name;
    # state_arg is index 2.
    if quoted_string_match.group(1) != feature_name:
      return None
    if len(args) < 3:
      return FeatureStateDetail(FeatureState.UNKNOWN)
    state_arg = args[2]
  else:
    # 2-param or country-restricted macro: first arg must match
    # 'k' + feature_name; state_arg is index 1.
    if not args or args[0] != 'k' + feature_name:
      return None
    if len(args) < 2:
      return FeatureStateDetail(FeatureState.UNKNOWN)
    state_arg = args[1]
    country_detail = ParseCountryRestrictedFeatureDetail(
      full_macro_text, args_str, state_arg
    )
    if country_detail is not None:
      return country_detail

  if (state := _ParseSimpleState(state_arg)) is not None:
    return FeatureStateDetail(state)
  if (state := _ParseConditionalState(state_arg, defines)) is not None:
    return FeatureStateDetail(state)
  return FeatureStateDetail(FeatureState.UNKNOWN)


def ExtractMacroFeatureName(args_str: str) -> str | None:
  """Extracts the feature name string from macro arguments."""
  args = [a.strip() for a in args_str.split(',')]
  if len(args) > 1:
    quoted_match = re.fullmatch(r'"([^"]*)"', args[1])
    if quoted_match:
      return quoted_match.group(1)
  if args and args[0].startswith('k') and len(args[0]) > 1:
    return args[0][1:]
  return None


def ValidateCppFeatureMacros(
  content: str, changed_lines: set[int] | None = None
) -> list[tuple[int, str]]:
  """Validates BASE_FEATURE and country-restricted feature declarations.

  Args:
    content: The full content of a C++ source file.
    changed_lines: Optional set of 1-based changed line numbers. If provided,
      only macros overlapping these lines are validated.

  Returns:
    A list of (line_number, error_message) tuples.
  """
  comment_stripped = cpp_preprocessor.RemoveComments(
    content, preserve_newlines=True
  )

  errors: list[tuple[int, str]] = []
  touched_features: dict[str, int] = {}
  invalid_features: set[str] = set()
  has_valid_touched_macro = False
  first_touched_line = None

  for match in CPP_FEATURE_STATE_REGEX.finditer(comment_stripped):
    start_line = comment_stripped.count('\n', 0, match.start()) + 1
    end_line = comment_stripped.count('\n', 0, match.end()) + 1

    if changed_lines is not None and not changed_lines.intersection(
      range(start_line, end_line + 1)
    ):
      continue

    if first_touched_line is None:
      first_touched_line = start_line

    full_macro_text = match.group(0)
    args_str = match.group(1)
    feature_name = ExtractMacroFeatureName(args_str)
    if feature_name is None:
      continue

    has_preprocessor_directive = any(
      line.strip().startswith('#') for line in args_str.splitlines()
    )

    if has_preprocessor_directive:
      touched_features.setdefault(feature_name, start_line)
      has_valid_touched_macro = True
    else:
      is_valid_across_platforms = True
      for platform in PLATFORM_TO_CONDITION:
        defines = GetPlatformDefines(platform)
        detail = ParseCppFeatureMacro(
          full_macro_text, args_str, feature_name, defines
        )
        if detail is None or detail.state == FeatureState.UNKNOWN:
          is_valid_across_platforms = False
          break
      if not is_valid_across_platforms:
        invalid_features.add(feature_name)
        errors.append(
          (
            start_line,
            f'Unable to parse default state for feature "{feature_name}". '
            'Default state must be a literal constant (e.g. '
            'base::FEATURE_ENABLED_BY_DEFAULT or '
            'base::FEATURE_DISABLED_BY_DEFAULT); use #if/#else '
            'preprocessor blocks instead of ternary or helper expressions.',
          )
        )
      else:
        touched_features.setdefault(feature_name, start_line)
        has_valid_touched_macro = True

  for feature_name in invalid_features:
    touched_features.pop(feature_name, None)

  if has_valid_touched_macro and touched_features:
    for platform, define_str in PLATFORM_TO_CONDITION.items():
      defines = GetPlatformDefines(platform)
      platform_label = (
        'IS_DESKTOP_ANDROID' if 'IS_DESKTOP_ANDROID' in defines else define_str
      )
      processor = cpp_preprocessor.CppPreprocessor(
        defines=defines, known_symbols=KNOWN_PLATFORM_SYMBOLS
      )
      try:
        preprocessed = processor.preprocess(content)
      except ValueError as e:
        errors.append((first_touched_line or 1, f'C++ preprocessor error: {e}'))
        break

      clean_preprocessed = cpp_preprocessor.RemoveComments(
        preprocessed, preserve_newlines=False
      )
      feature_states: dict[str, set[FeatureStateDetail]] = {
        f: set() for f in touched_features
      }
      for match in CPP_FEATURE_STATE_REGEX.finditer(clean_preprocessed):
        macro_text = match.group(0)
        macro_args = match.group(1)
        fname = ExtractMacroFeatureName(macro_args)
        if fname in feature_states:
          det = ParseCppFeatureMacro(macro_text, macro_args, fname, defines)
          if det is not None:
            feature_states[fname].add(det)

      for fname, line_num in touched_features.items():
        states = feature_states[fname]
        if len(states) > 1:
          errors.append(
            (
              line_num,
              f'Conflicting BASE_FEATURE definitions for "{fname}" on '
              f'{platform_label}.',
            )
          )
        elif any(s.state == FeatureState.UNKNOWN for s in states):
          errors.append(
            (
              line_num,
              f'Unable to parse default state for feature "{fname}" on '
              f'{platform_label}.',
            )
          )

  unique_errors: list[tuple[int, str]] = []
  seen: set[tuple[int, str]] = set()
  for err in errors:
    if err not in seen:
      seen.add(err)
      unique_errors.append(err)
  unique_errors.sort(key=lambda x: x[0])
  return unique_errors
