# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import math
import os
import re
from style_variable_generator.color import Color, ColorBlend, ColorVar, ColorRGBVar
from style_variable_generator.css_generator import CSSStyleGenerator
from style_variable_generator.model import Modes, VariableType


class ColorMappingsStyleGenerator(CSSStyleGenerator):
    '''Generator for ColorMappings Variables'''

    @staticmethod
    def GetName():
        return 'ColorMappings'

    def GetParameters(self):
        return {
            'color_mappings': self._CreateMappings(),
            'colors': self.model.colors
        }

    def GetFilters(self):
        return {
            'to_color_id_name': self._ToColorIdName,
            'color_mixer_color': self._ColorMixerColor,
            'to_css_var_name': self.ToCSSVarName,
            'cpp_opacity': self._CppOpacity,
        }

    def GetGlobals(self):
        globals = {
            'Modes': Modes,
            'out_file_path': None,
            'namespace_name':
            self.generator_options.get('cpp_namespace', None),
            'header_file': None,
            'color_id_start_value':
            self.generator_options['color_id_start_value'],
            'in_files': self.GetInputFiles(),
        }
        if self.out_file_path:
            globals['out_file_path'] = self.out_file_path
            header_file = self.out_file_path.replace(".cc", ".h")
            header_file = re.sub(r'.*gen/', '', header_file)
            globals['header_file'] = header_file

        return globals

    def ShouldResolveBlendedColors(self):
        return False

    def _CreateMappings(self):
        mappings = collections.defaultdict(list)
        for name, mode_values in self.model.colors.items():
            set_name = self.model.variable_map[name].context['ColorMappings'][
                'set_name']
            mappings[set_name].append({
                'name': name,
                'mode_values': mode_values
            })

        return mappings

    def _ToColorIdName(self, var_name):
        return 'k%s' % (re.sub(r'[_\-\.]', '', var_name.title()))

    def _CppOpacity(self, opacity, mode):
        return math.floor(255 *
                          self.model.opacities.ResolveOpacity(opacity, mode).a)

    def _ColorMixerColor(self, c, mode):
        '''Returns the C++ ColorMappings representation of |c|'''
        assert (isinstance(c, Color))

        if isinstance(c, ColorBlend) and c.blendPercentage:
            return 'ui::GetResultingPaintColor(ui::SetAlpha(%s, 0x%X), %s)' % (
                self._ColorMixerColor(c.blended_colors[0], mode),
                math.floor(255 * (float(c.blendPercentage) / 100)),
                self._ColorMixerColor(c.blended_colors[1], mode))
        elif isinstance(c, ColorBlend):
            return 'ui::GetResultingPaintColor(%s, %s)' % (
                self._ColorMixerColor(c.blended_colors[0], mode),
                self._ColorMixerColor(c.blended_colors[1], mode))

        if isinstance(c, ColorVar):
            return '{%s}' % self._ToColorIdName(c.var)

        if isinstance(c, ColorRGBVar):
            return ('ui::SetAlpha({%s}, 0x%X)' % (self._ToColorIdName(
                c.ToVar()), self._CppOpacity(c.opacity, mode)))

        if c.opacity.a != 1:
            return '{SkColorSetARGB(0x%X, 0x%X, 0x%X, 0x%X)}' % (
                self._CppOpacity(c.opacity, mode), c.r, c.g, c.b)
        else:
            return '{SkColorSetRGB(0x%X, 0x%X, 0x%X)}' % (c.r, c.g, c.b)


class ColorMappingsCCStyleGenerator(ColorMappingsStyleGenerator):
    @staticmethod
    def GetName():
        return 'ColorMappingsCC'

    def Render(self):
        return self.ApplyTemplate(
            self, 'templates/color_mappings_generator_cc.tmpl',
            self.GetParameters())


class ColorMappingsHStyleGenerator(ColorMappingsStyleGenerator):
    @staticmethod
    def GetName():
        return 'ColorMappingsH'

    def Render(self):
        return self.ApplyTemplate(self,
                                  'templates/color_mappings_generator_h.tmpl',
                                  self.GetParameters())
