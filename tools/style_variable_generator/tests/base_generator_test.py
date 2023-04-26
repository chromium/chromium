#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
from pathlib import Path

if len(Path(__file__).parents) > 2:
    sys.path += [str(Path(__file__).parents[2])]

from style_variable_generator.base_generator import BaseGenerator, Modes
import unittest


class BaseGeneratorTest(unittest.TestCase):
    def setUp(self):
        self.generator = BaseGenerator()

    def ResolveOpacity(self, name, mode=Modes.LIGHT):
        opacity_model = self.generator.model.opacities
        opacity = opacity_model.Resolve(name, mode)
        return opacity_model.ResolveOpacity(opacity, mode).a

    def ResolveRGBA(self, name, mode=Modes.LIGHT):
        return repr(self.generator.model.colors.ResolveToRGBA(name, mode))

    def testMissingColor(self):
        # google_grey_900 is missing.
        self.generator.AddJSONToModel('''
{
  colors: {
    cros_default_text_color: {
      light: "$google_grey_900",
      dark: "rgb(255, 255, 255)",
    },
  },
}
        ''')
        self.assertRaises(ValueError, self.generator.model.Validate)

        # Add google_grey_900.
        self.generator.AddJSONToModel('''
{
  colors: {
    google_grey_900: "rgb(255, 255, 255)",
  }
}
        ''')
        self.generator.model.Validate()

    def testMissingDefaultModeColor(self):
        # google_grey_900 is missing in the default mode (light).
        self.generator.AddJSONToModel('''
{
  colors: {
    google_grey_900: { dark: "rgb(255, 255, 255)", },
  }
}
        ''')
        self.assertRaises(ValueError, self.generator.model.Validate)

    def testDuplicateKeys(self):
        self.generator.AddJSONToModel('''
{
  colors: {
    google_grey_900: { light: "rgb(255, 255, 255)", },
  }
}
        ''')
        self.generator.model.Validate()

        # Add google_grey_900's dark mode as if in a separate file. This counts
        # as a redefinition/conflict and causes an error.
        self.assertRaises(
            ValueError, self.generator.AddJSONToModel, '''
{
  colors: {
    google_grey_900: { dark: "rgb(255, 255, 255)", }
  }
}
        ''')

    def testBadNames(self):
        # Add a bad color name.
        self.assertRaises(
            ValueError, self.generator.AddJSONToModel, '''
{
  colors: {
    Google+grey: { dark: "rgb(255, 255, 255)", }
  }
}
        ''')

    def testSimpleOpacity(self):
        # Reference a missing opacity.
        self.generator.AddJSONToModel('''
{
  colors: {
    google_grey_900: "rgba(255, 255, 255, $disabled_opacity)",
  },
  opacities: {
    disabled_opacity: 0.5,
  },
}
        ''')

        self.assertEqual(self.ResolveRGBA('google_grey_900'),
                         'rgba(255, 255, 255, 0.5)')

        self.generator.model.Validate()

    def testReferenceOpacity(self):
        # Add a reference opacity.
        self.generator.AddJSONToModel('''
{
  colors: {
    google_grey_900: "rgba(255, 255, 255, $disabled_opacity)",
  },
  opacities: {
    disabled_opacity: "$second_opacity",
    second_opacity: { "light": "$another_opacity", "dark": 1 },
    another_opacity: 0.5,
  },
}
        ''')
        self.assertEqual(self.ResolveRGBA('google_grey_900'),
                         'rgba(255, 255, 255, 0.5)')
        self.assertEqual(self.ResolveOpacity('disabled_opacity'), 0.5)
        self.assertEqual(self.ResolveOpacity('disabled_opacity', Modes.DARK),
                         1)

    def testMissingOpacity(self):
        # Reference a missing opacity.
        self.generator.AddJSONToModel('''
{
  colors: {
    google_grey_900: "rgba(255, 255, 255, $missing_opacity)",
  },
  opacities: {
    disabled_opacity: 0.5,
  },
}
        ''')
        self.assertRaises(ValueError, self.generator.model.Validate)

        self.generator.AddJSONToModel('''
{
  opacities: {
    missing_opacity: 0.5,
  },
}
        ''')
        self.generator.model.Validate()

    def testSelfReferenceColor(self):
        self.generator.AddJSONToModel('''
{
  colors: {
    google_grey_900: "$google_grey_900",
  }
}
        ''')
        self.assertRaises(ValueError, self.generator.model.Validate)

    def testSelfReferenceOpacity(self):
        self.generator.AddJSONToModel('''
{
  opacities: {
    some_opacity: "$some_opacity",
  }
}
        ''')
        self.assertRaises(ValueError, self.generator.model.Validate)

    def testBlend(self):
        self.generator.AddJSONToModel('''
{
  colors: {
    expect_color_white: "blend($white, #202124)",
    expect_color_lighter: "blend(rgba($white.rgb, 0.06), rgba(32, 33, 36, 0.6))"
  }
}
        ''')
        self.assertEqual(self.ResolveRGBA('expect_color_white'),
                         'rgba(255, 255, 255, 1)')
        self.assertEqual(self.ResolveRGBA('expect_color_lighter'),
                         'rgba(53, 54, 57, 0.624)')

    def testBlendVariableReference(self):
        self.generator.AddJSONToModel('''
{
  colors: {
    google_grey_900: "#202124",
    bg_color_elevation_3: "blend(rgba($white.rgb, 0.08), $google_grey_900)",
  }
}
        ''')
        self.assertEqual(self.ResolveRGBA('bg_color_elevation_3'),
                         'rgba(50, 51, 54, 1)')

    def testBlendMoreVariableReferences(self):
        self.generator.AddJSONToModel('''
{
  colors: {
    color_a: "blend(rgba($white.rgb, 0.5), $black)",
    color_b: "blend(rgba($color_a.rgb, 0.5), rgba($black.rgb, 0.5))",
    color_c: "blend($color_a, blend(rgba($color_b.rgb, 0.5), $white))",
  }
}
        ''')
        self.assertEqual(self.ResolveRGBA('color_a'), 'rgba(128, 128, 128, 1)')
        self.assertEqual(self.ResolveRGBA('color_b'), 'rgba(85, 85, 85, 0.75)')
        # Same as color_a because color_a has alpha 1.
        self.assertEqual(self.ResolveRGBA('color_c'), 'rgba(128, 128, 128, 1)')

    def testBlendVariableReferenceLightDarkModes(self):
        self.generator.AddJSONToModel('''
{
  colors: {
    google_blue_300: "#8ab4f8",
    google_blue_600: "#1a73e8",
    color_prominent: {
      light: "$google_blue_600",
      dark: "$google_blue_300",
    },
    color_prominent_dull: {
      light: "blend(rgba($white.rgb, 0.08), $color_prominent)",
      dark: "blend(rgba($black.rgb, 0.08), $color_prominent)",
    },
  }
}
        ''')
        self.assertEqual(self.ResolveRGBA('color_prominent_dull', Modes.LIGHT),
                         'rgba(44, 126, 234, 1)')
        self.assertEqual(self.ResolveRGBA('color_prominent_dull', Modes.DARK),
                         'rgba(127, 166, 228, 1)')

    def testBlendNested(self):
        self.generator.AddJSONToModel('''
{
  colors: {
    expect_color_black: "blend(blend($black, $white), $white)",
  }
}
        ''')
        self.assertEqual(self.ResolveRGBA('expect_color_black'),
                         'rgba(0, 0, 0, 1)')

    def testMissingBlendColor(self):
        self.generator.AddJSONToModel('''
{
  colors: {
    bg_color_elevation_10: "blend($white, $google_grey_900)",
  }
}
        ''')
        self.assertRaises(ValueError, self.generator.model.Validate)

    def testInvertedColors(self):
        # Add an inverted color.
        self.generator.AddJSONToModel('''
{
  colors: {
    alert: { light: "$white", dark: "$black", generate_inverted: true },
  },
}
        ''')
        self.assertEqual(self.ResolveRGBA('alert', Modes.LIGHT),
                         'rgba(255, 255, 255, 1)')
        self.assertEqual(self.ResolveRGBA('alert', Modes.DARK),
                         'rgba(0, 0, 0, 1)')
        self.assertEqual(self.ResolveRGBA('alert_inverted', Modes.LIGHT),
                         'rgba(0, 0, 0, 1)')
        self.assertEqual(self.ResolveRGBA('alert_inverted', Modes.DARK),
                         'rgba(255, 255, 255, 1)')

    def testInvertedWithSingleMode(self):
        # Add an inverted color without a dark mode.
        self.assertRaises(
            ValueError, self.generator.AddJSONToModel, '''
{
  colors: {
    alert: { light: "$white", generate_inverted: true },
  },
}
        ''')

    def testIllegalSuffix(self):
        self.generator.AddJSONToModel('''
{
  colors: {
    some_inverted: "$black",
  }
}
        ''')
        self.assertRaises(ValueError, self.generator.model.Validate)

    def testPerModeColors(self):
        # Add a per-mode color.
        self.generator.AddJSONToModel('''
{
  colors: {
    alert: {
      light: "$white",
      dark: "$black",
      debug: "$black",
      generate_per_mode: true
    },
  },
}
        ''')
        self.assertEqual(self.ResolveRGBA('alert_dark'), 'rgba(0, 0, 0, 1)')
        self.assertEqual(self.ResolveRGBA('alert_light'),
                         'rgba(255, 255, 255, 1)')
        self.assertEqual(self.ResolveRGBA('alert_debug'), 'rgba(0, 0, 0, 1)')
        self.assertEqual(self.ResolveRGBA('alert'), 'rgba(255, 255, 255, 1)')

    def testAlias(self):
        self.generator.AddJSONToModel('''
        {
          legacy_mappings: {
            legacy: "$white",
          },
        }
        ''')
        self.assertEqual(self.generator.model.legacy_mappings['legacy'],
                         '$white')
        self.generator.model.Validate()


if __name__ == '__main__':
    unittest.main()
