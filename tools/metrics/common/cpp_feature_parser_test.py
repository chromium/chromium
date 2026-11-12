# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for cpp_feature_parser."""

import textwrap
import unittest

import setup_modules  # pylint: disable=unused-import
import chromium_src.tools.metrics.common.cpp_feature_parser as cpp_feature_parser

FeatureState = cpp_feature_parser.FeatureState
FeatureStateDetail = cpp_feature_parser.FeatureStateDetail


class CppFeatureParserTest(unittest.TestCase):
  def test_parse_2arg_macro(self):
    detail = cpp_feature_parser.ParseCppFeatureMacro(
      'BASE_FEATURE(kMyFeature, base::FEATURE_ENABLED_BY_DEFAULT);',
      'kMyFeature, base::FEATURE_ENABLED_BY_DEFAULT',
      'MyFeature',
      {'IS_LINUX'},
    )
    self.assertEqual(detail, FeatureStateDetail(FeatureState.ENABLED))

    detail = cpp_feature_parser.ParseCppFeatureMacro(
      'BASE_FEATURE(kMyFeature, base::FEATURE_DISABLED_BY_DEFAULT);',
      'kMyFeature, base::FEATURE_DISABLED_BY_DEFAULT',
      'MyFeature',
      {'IS_LINUX'},
    )
    self.assertEqual(detail, FeatureStateDetail(FeatureState.DISABLED))

    detail = cpp_feature_parser.ParseCppFeatureMacro(
      'BASE_FEATURE(kMyFeature, ENABLED);',
      'kMyFeature, ENABLED',
      'MyFeature',
      {'IS_LINUX'},
    )
    self.assertEqual(detail, FeatureStateDetail(FeatureState.ENABLED))

    detail = cpp_feature_parser.ParseCppFeatureMacro(
      'BASE_FEATURE(kMyFeature, FeatureState::Disabled);',
      'kMyFeature, FeatureState::Disabled',
      'MyFeature',
      {'IS_LINUX'},
    )
    self.assertEqual(detail, FeatureStateDetail(FeatureState.DISABLED))

    detail = cpp_feature_parser.ParseCppFeatureMacro(
      'BASE_FEATURE(kMyFeature, '
      'base::FeatureState::FEATURE_ENABLED_BY_DEFAULT);',
      'kMyFeature, base::FeatureState::FEATURE_ENABLED_BY_DEFAULT',
      'MyFeature',
      {'IS_LINUX'},
    )
    self.assertEqual(detail, FeatureStateDetail(FeatureState.ENABLED))

    detail = cpp_feature_parser.ParseCppFeatureMacro(
      'BASE_FEATURE(kMyFeature, '
      'base::FeatureState::FEATURE_DISABLED_BY_DEFAULT);',
      'kMyFeature, base::FeatureState::FEATURE_DISABLED_BY_DEFAULT',
      'MyFeature',
      {'IS_LINUX'},
    )
    self.assertEqual(detail, FeatureStateDetail(FeatureState.DISABLED))

    # Mismatched feature name returns None.
    detail = cpp_feature_parser.ParseCppFeatureMacro(
      'BASE_FEATURE(kOtherFeature, base::FEATURE_ENABLED_BY_DEFAULT);',
      'kOtherFeature, base::FEATURE_ENABLED_BY_DEFAULT',
      'MyFeature',
      {'IS_LINUX'},
    )
    self.assertIsNone(detail)

  def test_parse_3arg_macro(self):
    detail = cpp_feature_parser.ParseCppFeatureMacro(
      'BASE_FEATURE(kMyVar, "MyFeature", base::FEATURE_ENABLED_BY_DEFAULT);',
      'kMyVar, "MyFeature", base::FEATURE_ENABLED_BY_DEFAULT',
      'MyFeature',
      {'IS_LINUX'},
    )
    self.assertEqual(detail, FeatureStateDetail(FeatureState.ENABLED))

    detail = cpp_feature_parser.ParseCppFeatureMacro(
      'BASE_FEATURE(kMyVar, "MyFeature", base::FEATURE_DISABLED_BY_DEFAULT);',
      'kMyVar, "MyFeature", base::FEATURE_DISABLED_BY_DEFAULT',
      'MyFeature',
      {'IS_LINUX'},
    )
    self.assertEqual(detail, FeatureStateDetail(FeatureState.DISABLED))

    # Mismatched feature string returns None.
    detail = cpp_feature_parser.ParseCppFeatureMacro(
      'BASE_FEATURE(kMyVar, "OtherFeature", base::FEATURE_ENABLED_BY_DEFAULT);',
      'kMyVar, "OtherFeature", base::FEATURE_ENABLED_BY_DEFAULT',
      'MyFeature',
      {'IS_LINUX'},
    )
    self.assertIsNone(detail)

  def test_parse_country_restricted_macro(self):
    full_macro = (
      'BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS('
      'kMyFeature, base::FEATURE_ENABLED_FOR_COUNTRIES, "us", "ca");'
    )
    args_str = 'kMyFeature, base::FEATURE_ENABLED_FOR_COUNTRIES, "us", "ca"'
    detail = cpp_feature_parser.ParseCppFeatureMacro(
      full_macro, args_str, 'MyFeature', {'IS_LINUX'}
    )
    self.assertEqual(
      detail,
      FeatureStateDetail(
        FeatureState.ENABLED_FOR_COUNTRIES, frozenset(['us', 'ca'])
      ),
    )

    full_macro = (
      'BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS('
      'kMyFeature, base::FEATURE_DISABLED_FOR_COUNTRIES, "fr");'
    )
    args_str = 'kMyFeature, base::FEATURE_DISABLED_FOR_COUNTRIES, "fr"'
    detail = cpp_feature_parser.ParseCppFeatureMacro(
      full_macro, args_str, 'MyFeature', {'IS_LINUX'}
    )
    self.assertEqual(
      detail,
      FeatureStateDetail(
        FeatureState.DISABLED_FOR_COUNTRIES, frozenset(['fr'])
      ),
    )

  def test_parse_conditional_macro(self):
    macro_text = (
      'BASE_FEATURE(kMyFeature, BUILDFLAG(IS_ANDROID) ? '
      'base::FEATURE_ENABLED_BY_DEFAULT : base::FEATURE_DISABLED_BY_DEFAULT);'
    )
    args_str = (
      'kMyFeature, BUILDFLAG(IS_ANDROID) ? '
      'base::FEATURE_ENABLED_BY_DEFAULT : base::FEATURE_DISABLED_BY_DEFAULT'
    )
    self.assertEqual(
      cpp_feature_parser.ParseCppFeatureMacro(
        macro_text, args_str, 'MyFeature', {'IS_ANDROID'}
      ),
      FeatureStateDetail(FeatureState.ENABLED),
    )
    self.assertEqual(
      cpp_feature_parser.ParseCppFeatureMacro(
        macro_text, args_str, 'MyFeature', {'IS_WIN'}
      ),
      FeatureStateDetail(FeatureState.DISABLED),
    )

    neg_macro = (
      'BASE_FEATURE(kMyFeature, !BUILDFLAG(IS_WIN) ? '
      'base::FEATURE_ENABLED_BY_DEFAULT : base::FEATURE_DISABLED_BY_DEFAULT);'
    )
    neg_args = (
      'kMyFeature, !BUILDFLAG(IS_WIN) ? '
      'base::FEATURE_ENABLED_BY_DEFAULT : base::FEATURE_DISABLED_BY_DEFAULT'
    )
    self.assertEqual(
      cpp_feature_parser.ParseCppFeatureMacro(
        neg_macro, neg_args, 'MyFeature', {'IS_WIN'}
      ),
      FeatureStateDetail(FeatureState.DISABLED),
    )
    self.assertEqual(
      cpp_feature_parser.ParseCppFeatureMacro(
        neg_macro, neg_args, 'MyFeature', {'IS_LINUX'}
      ),
      FeatureStateDetail(FeatureState.ENABLED),
    )

    enable_if_macro = 'BASE_FEATURE(kMyFeature, enable_if(!IS_IOS));'
    enable_if_args = 'kMyFeature, enable_if(!IS_IOS)'
    self.assertEqual(
      cpp_feature_parser.ParseCppFeatureMacro(
        enable_if_macro, enable_if_args, 'MyFeature', {'IS_IOS'}
      ),
      FeatureStateDetail(FeatureState.DISABLED),
    )
    self.assertEqual(
      cpp_feature_parser.ParseCppFeatureMacro(
        enable_if_macro, enable_if_args, 'MyFeature', {'IS_ANDROID'}
      ),
      FeatureStateDetail(FeatureState.ENABLED),
    )

    enable_if_or_macro = (
      'BASE_FEATURE(kMyFeature, enable_if(IS_ANDROID || BUILDFLAG(IS_IOS)));'
    )
    enable_if_or_args = 'kMyFeature, enable_if(IS_ANDROID || BUILDFLAG(IS_IOS))'
    self.assertEqual(
      cpp_feature_parser.ParseCppFeatureMacro(
        enable_if_or_macro, enable_if_or_args, 'MyFeature', {'IS_ANDROID'}
      ),
      FeatureStateDetail(FeatureState.ENABLED),
    )
    self.assertEqual(
      cpp_feature_parser.ParseCppFeatureMacro(
        enable_if_or_macro, enable_if_or_args, 'MyFeature', {'IS_IOS'}
      ),
      FeatureStateDetail(FeatureState.ENABLED),
    )
    self.assertEqual(
      cpp_feature_parser.ParseCppFeatureMacro(
        enable_if_or_macro, enable_if_or_args, 'MyFeature', {'IS_MAC'}
      ),
      FeatureStateDetail(FeatureState.DISABLED),
    )

    # Desktop Android and derived platform macros (e.g. IS_APPLE).
    desktop_android_macro = (
      'BASE_FEATURE(kMyFeature, BUILDFLAG(IS_DESKTOP_ANDROID) ? '
      'base::FEATURE_ENABLED_BY_DEFAULT : base::FEATURE_DISABLED_BY_DEFAULT);'
    )
    desktop_android_args = (
      'kMyFeature, BUILDFLAG(IS_DESKTOP_ANDROID) ? '
      'base::FEATURE_ENABLED_BY_DEFAULT : base::FEATURE_DISABLED_BY_DEFAULT'
    )
    self.assertEqual(
      cpp_feature_parser.ParseCppFeatureMacro(
        desktop_android_macro,
        desktop_android_args,
        'MyFeature',
        cpp_feature_parser.GetPlatformDefines('android_desktop'),
      ),
      FeatureStateDetail(FeatureState.ENABLED),
    )
    self.assertEqual(
      cpp_feature_parser.ParseCppFeatureMacro(
        desktop_android_macro,
        desktop_android_args,
        'MyFeature',
        cpp_feature_parser.GetPlatformDefines('android'),
      ),
      FeatureStateDetail(FeatureState.DISABLED),
    )

    apple_macro = (
      'BASE_FEATURE(kMyFeature, BUILDFLAG(IS_APPLE) ? '
      'base::FEATURE_ENABLED_BY_DEFAULT : base::FEATURE_DISABLED_BY_DEFAULT);'
    )
    apple_args = (
      'kMyFeature, BUILDFLAG(IS_APPLE) ? '
      'base::FEATURE_ENABLED_BY_DEFAULT : base::FEATURE_DISABLED_BY_DEFAULT'
    )
    self.assertEqual(
      cpp_feature_parser.ParseCppFeatureMacro(
        apple_macro,
        apple_args,
        'MyFeature',
        cpp_feature_parser.GetPlatformDefines('mac'),
      ),
      FeatureStateDetail(FeatureState.ENABLED),
    )
    self.assertEqual(
      cpp_feature_parser.ParseCppFeatureMacro(
        apple_macro,
        apple_args,
        'MyFeature',
        cpp_feature_parser.GetPlatformDefines('ios'),
      ),
      FeatureStateDetail(FeatureState.ENABLED),
    )
    self.assertEqual(
      cpp_feature_parser.ParseCppFeatureMacro(
        apple_macro,
        apple_args,
        'MyFeature',
        cpp_feature_parser.GetPlatformDefines('linux'),
      ),
      FeatureStateDetail(FeatureState.DISABLED),
    )

  def test_fullmatch_rejects_expressions(self):
    # Unsupported platform flag in ternary expression returns UNKNOWN.
    detail = cpp_feature_parser.ParseCppFeatureMacro(
      'BASE_FEATURE(kMyFeature, BUILDFLAG(IS_UNSUPPORTED_PLATFORM) ? '
      'base::FEATURE_DISABLED_BY_DEFAULT : base::FEATURE_ENABLED_BY_DEFAULT);',
      'kMyFeature, BUILDFLAG(IS_UNSUPPORTED_PLATFORM) ? '
      'base::FEATURE_DISABLED_BY_DEFAULT : base::FEATURE_ENABLED_BY_DEFAULT',
      'MyFeature',
      {'IS_ANDROID'},
    )
    self.assertEqual(detail, FeatureStateDetail(FeatureState.UNKNOWN))

    # Non-constant ternary branch returns UNKNOWN.
    detail = cpp_feature_parser.ParseCppFeatureMacro(
      'BASE_FEATURE(kMyFeature, BUILDFLAG(IS_ANDROID) ? '
      'kAndroidDefault : base::FEATURE_DISABLED_BY_DEFAULT);',
      'kMyFeature, BUILDFLAG(IS_ANDROID) ? '
      'kAndroidDefault : base::FEATURE_DISABLED_BY_DEFAULT',
      'MyFeature',
      {'IS_ANDROID'},
    )
    self.assertEqual(detail, FeatureStateDetail(FeatureState.UNKNOWN))

    # Unknown symbol in enable_if returns UNKNOWN.
    detail = cpp_feature_parser.ParseCppFeatureMacro(
      'BASE_FEATURE(kMyFeature, enable_if(kSomeCondition));',
      'kMyFeature, enable_if(kSomeCondition)',
      'MyFeature',
      {'IS_ANDROID'},
    )
    self.assertEqual(detail, FeatureStateDetail(FeatureState.UNKNOWN))

    # Unsupported operator in enable_if returns UNKNOWN.
    detail = cpp_feature_parser.ParseCppFeatureMacro(
      'BASE_FEATURE(kMyFeature, enable_if(IS_ANDROID == 1));',
      'kMyFeature, enable_if(IS_ANDROID == 1)',
      'MyFeature',
      {'IS_ANDROID'},
    )
    self.assertEqual(detail, FeatureStateDetail(FeatureState.UNKNOWN))

    # Helper function call returns UNKNOWN.
    detail = cpp_feature_parser.ParseCppFeatureMacro(
      'BASE_FEATURE(kMyFeature, GetDefaultState(vector<int>(1)));',
      'kMyFeature, GetDefaultState(vector<int>(1))',
      'MyFeature',
      {'IS_ANDROID'},
    )
    self.assertEqual(detail, FeatureStateDetail(FeatureState.UNKNOWN))

    # 2-param macro with string literal inside helper expression returns
    # UNKNOWN for that feature and None (not UNKNOWN) when querying another
    # feature whose name matches the string literal.
    detail = cpp_feature_parser.ParseCppFeatureMacro(
      'BASE_FEATURE(kMyFeature, GetDefaultState("MyFeature"));',
      'kMyFeature, GetDefaultState("MyFeature")',
      'MyFeature',
      {'IS_ANDROID'},
    )
    self.assertEqual(detail, FeatureStateDetail(FeatureState.UNKNOWN))

    detail = cpp_feature_parser.ParseCppFeatureMacro(
      'BASE_FEATURE(kOtherFeature, GetDefaultState("MyFeature"));',
      'kOtherFeature, GetDefaultState("MyFeature")',
      'MyFeature',
      {'IS_ANDROID'},
    )
    self.assertIsNone(detail)

  def test_extract_macro_feature_name(self):
    self.assertEqual(
      cpp_feature_parser.ExtractMacroFeatureName(
        'kMyFeature, base::FEATURE_ENABLED_BY_DEFAULT'
      ),
      'MyFeature',
    )
    self.assertEqual(
      cpp_feature_parser.ExtractMacroFeatureName(
        'kVar, "CustomName", base::FEATURE_ENABLED_BY_DEFAULT'
      ),
      'CustomName',
    )
    self.assertEqual(
      cpp_feature_parser.ExtractMacroFeatureName(
        'kMyFeature, base::FEATURE_ENABLED_FOR_COUNTRIES, "us"'
      ),
      'MyFeature',
    )
    self.assertEqual(
      cpp_feature_parser.ExtractMacroFeatureName(
        'kMyFeature, GetDefaultState("OtherString")'
      ),
      'MyFeature',
    )
    self.assertIsNone(
      cpp_feature_parser.ExtractMacroFeatureName(
        'myFeature, base::FEATURE_ENABLED_BY_DEFAULT'
      )
    )

  def test_validate_cpp_feature_macros_valid(self):
    content = textwrap.dedent("""\
      // Inline comment and #if blocks
      #if BUILDFLAG(IS_ANDROID)
      BASE_FEATURE(kMyFeature, /* default= */ base::FEATURE_ENABLED_BY_DEFAULT);
      #else
      BASE_FEATURE(kMyFeature, base::FEATURE_DISABLED_BY_DEFAULT);
      #endif

      // Valid platform-conditional expressions:
      BASE_FEATURE(kCondTernary,
          BUILDFLAG(IS_ANDROID) ? base::FEATURE_ENABLED_BY_DEFAULT
                                : base::FEATURE_DISABLED_BY_DEFAULT);
      BASE_FEATURE(kCondDesktopAndroid,
          BUILDFLAG(IS_DESKTOP_ANDROID) ? base::FEATURE_ENABLED_BY_DEFAULT
                                        : base::FEATURE_DISABLED_BY_DEFAULT);
      BASE_FEATURE(kCondApple,
          BUILDFLAG(IS_APPLE) ? base::FEATURE_ENABLED_BY_DEFAULT
                              : base::FEATURE_DISABLED_BY_DEFAULT);
      BASE_FEATURE(kCondEnableIf, enable_if(!IS_IOS));
      BASE_FEATURE(kCondEnableIfOr,
          enable_if(IS_ANDROID || BUILDFLAG(IS_IOS)));

      // Derived and secondary platform macros inside #if blocks:
      BASE_FEATURE(kDerivedIfdef,
      #if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_BSD)
          base::FEATURE_ENABLED_BY_DEFAULT
      #else
          base::FEATURE_DISABLED_BY_DEFAULT
      #endif
      );
      BASE_FEATURE(kOzoneIfdef,
      #if BUILDFLAG(IS_OZONE)
          base::FEATURE_ENABLED_BY_DEFAULT
      #else
          base::FEATURE_DISABLED_BY_DEFAULT
      #endif
      );
      BASE_FEATURE(kAndroidNotCastOrDesktop,
      #if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CAST_ANDROID) \\
          && !BUILDFLAG(IS_DESKTOP_ANDROID)
          base::FEATURE_ENABLED_BY_DEFAULT
      #else
          base::FEATURE_DISABLED_BY_DEFAULT
      #endif
      );

      // Commented out invalid macro should be ignored:
      // BASE_FEATURE(kOther, cond ? ENABLED : DISABLED);
    """)
    errors = cpp_feature_parser.ValidateCppFeatureMacros(content)
    self.assertEqual(errors, [])

  def test_validate_cpp_feature_macros_feature_state_qualified(self):
    content = textwrap.dedent("""\
      BASE_FEATURE(kFeatA, base::FeatureState::FEATURE_ENABLED_BY_DEFAULT);
      BASE_FEATURE(kFeatB, base::FeatureState::FEATURE_DISABLED_BY_DEFAULT);
    """)
    errors = cpp_feature_parser.ValidateCppFeatureMacros(content)
    self.assertEqual(errors, [])

  def test_validate_cpp_feature_macros_invalid_ternary(self):
    content = textwrap.dedent("""\
      BASE_FEATURE(kFeatUnsupportedFlag,
          BUILDFLAG(IS_UNSUPPORTED_PLATFORM) ? base::FEATURE_DISABLED_BY_DEFAULT
                                             : base::FEATURE_ENABLED_BY_DEFAULT);
      BASE_FEATURE(kFeatNonConstBranch,
          BUILDFLAG(IS_ANDROID) ? kAndroidDefault
                                : base::FEATURE_DISABLED_BY_DEFAULT);
      BASE_FEATURE(kFeatUnknownSymbol, enable_if(kSomeCondition));
      BASE_FEATURE(kFeatUnsupportedOp, enable_if(IS_ANDROID == 1));
    """)
    errors = cpp_feature_parser.ValidateCppFeatureMacros(content)
    self.assertEqual(len(errors), 4)
    self.assertEqual([line for line, _ in errors], [1, 4, 7, 8])
    for _, msg in errors:
      self.assertIn('Unable to parse default state', msg)

  def test_validate_cpp_feature_macros_invalid_function(self):
    content = textwrap.dedent("""\
      BASE_FEATURE(kMyFeature, GetDefaultState(vector<int>(1)));
      BASE_FEATURE(kStringHelper, GetDefaultState("StringHelper"));
      BASE_FEATURE(kValidFeature, base::FEATURE_ENABLED_BY_DEFAULT);
    """)
    errors = cpp_feature_parser.ValidateCppFeatureMacros(content)
    self.assertEqual(len(errors), 2)
    self.assertEqual([line for line, _ in errors], [1, 2])
    for _, msg in errors:
      self.assertIn('Unable to parse default state', msg)

  def test_validate_cpp_feature_macros_conflicting_definitions(self):
    content = textwrap.dedent("""\
      #if BUILDFLAG(IS_ANDROID)
      BASE_FEATURE(kMyFeature, base::FEATURE_ENABLED_BY_DEFAULT);
      BASE_FEATURE(kMyFeature, base::FEATURE_DISABLED_BY_DEFAULT);
      #endif
    """)
    errors = cpp_feature_parser.ValidateCppFeatureMacros(content)
    self.assertTrue(
      any('Conflicting BASE_FEATURE definitions' in msg for _, msg in errors)
    )

  def test_validate_cpp_feature_macros_unterminated_if(self):
    content = textwrap.dedent("""\
      #if BUILDFLAG(IS_ANDROID)
      BASE_FEATURE(kMyFeature, base::FEATURE_ENABLED_BY_DEFAULT);
    """)
    errors = cpp_feature_parser.ValidateCppFeatureMacros(content)
    self.assertEqual(len(errors), 1)
    self.assertIn(
      'C++ preprocessor error: Unterminated #if block', errors[0][1]
    )

  def test_validate_cpp_feature_macros_respects_changed_lines(self):
    content = textwrap.dedent("""\
      BASE_FEATURE(kInvalidOld, GetDefaultState(1));
      BASE_FEATURE(kValidNew, base::FEATURE_ENABLED_BY_DEFAULT);
    """)
    # Only line 2 is changed; line 1 should be ignored.
    errors = cpp_feature_parser.ValidateCppFeatureMacros(
      content, changed_lines={2}
    )
    self.assertEqual(errors, [])

  def test_validate_cpp_feature_macros_runtime_mutable(self):
    content = textwrap.dedent("""\
      BASE_RUNTIME_MUTABLE_FEATURE(kFeatA, base::FEATURE_ENABLED_BY_DEFAULT);
      BASE_RUNTIME_MUTABLE_FEATURE(kFeatB, GetDefaultState());
    """)
    errors = cpp_feature_parser.ValidateCppFeatureMacros(content)
    self.assertEqual([line for line, _ in errors], [2])

  def test_validate_cpp_feature_macros_ignores_wrapper_macros(self):
    # Wrapper macros whose names end in BASE_FEATURE must not be matched.
    content = textwrap.dedent("""\
      MAKE_STATIC_STORAGE_BASE_FEATURE(kFeatA, GetDefaultState());
    """)
    errors = cpp_feature_parser.ValidateCppFeatureMacros(content)
    self.assertEqual(errors, [])

  def test_validate_cpp_feature_macros_if_defined(self):
    content = textwrap.dedent("""\
      #if defined(IS_ANDROID)
      BASE_FEATURE(kMyFeature, base::FEATURE_ENABLED_BY_DEFAULT);
      #else
      BASE_FEATURE(kMyFeature, base::FEATURE_DISABLED_BY_DEFAULT);
      #endif
    """)
    errors = cpp_feature_parser.ValidateCppFeatureMacros(content)
    self.assertEqual(errors, [])

  def test_parse_enable_if_desktop(self):
    macro = 'BASE_FEATURE(kMyFeature, enable_if(IS_DESKTOP));'
    args = 'kMyFeature, enable_if(IS_DESKTOP)'
    expected = {
      'windows': cpp_feature_parser.FeatureState.ENABLED,
      'mac': cpp_feature_parser.FeatureState.ENABLED,
      'linux': cpp_feature_parser.FeatureState.ENABLED,
      'chromeos': cpp_feature_parser.FeatureState.ENABLED,
      'android': cpp_feature_parser.FeatureState.DISABLED,
      'android_desktop': cpp_feature_parser.FeatureState.DISABLED,
      'ios': cpp_feature_parser.FeatureState.DISABLED,
    }
    for platform, state in expected.items():
      with self.subTest(platform):
        detail = cpp_feature_parser.ParseCppFeatureMacro(
          macro,
          args,
          'MyFeature',
          cpp_feature_parser.GetPlatformDefines(platform),
        )
        assert detail is not None
        self.assertEqual(detail.state, state)


if __name__ == '__main__':
  unittest.main()
