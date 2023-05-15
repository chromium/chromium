# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import math
import re
from style_variable_generator.color import Color, ColorVar, ColorRGBVar
from style_variable_generator.css_generator import CSSStyleGenerator
from style_variable_generator.model import Modes, VariableType


class ViewsStyleGenerator(CSSStyleGenerator):
    '''Generator for Views Variables'''
    @staticmethod
    def GetName():
        return 'Views'

    def GetParameters(self):
        return {
            'colors': self._CreateColorList(),
            'opacities': self.model.opacities,
        }

    def GetFilters(self):
        return {
            'to_const_name': self._ToConstName,
            'cpp_color': self._CppColor,
            'alpha_to_hex': self._AlphaToHex,
            'cpp_opacity': self._CppOpacity,
            'to_css_var_name': self.ToCSSVarName,
            'css_color_rgb': self.CSSColorRGB,
        }

    def GetGlobals(self):
        globals = {
            'Modes':
            Modes,
            'out_file_path':
            None,
            'namespace_name':
            self.generator_options.get(
                'cpp_namespace',
                os.path.splitext(os.path.basename(self.out_file_path))[0]),
            'header_file':
            None,
            'in_files':
            self.GetInputFiles(),
            'css_color_var':
            self.CSSColorVar,
        }
        if self.out_file_path:
            globals['out_file_path'] = self.out_file_path
            header_file = self.out_file_path.replace(".cc", ".h")
            header_file = re.sub(r'.*gen/', '', header_file)
            globals['header_file'] = header_file

        return globals

    def DefaultPreblend(self):
        # CSSGenerator sets this to false, we set it back to true since
        # views does not do runtime blends.
        return True

    def _CreateColorList(self):
        color_list = []
        for name, mode_values in self.model.colors.items():
            color_list.append({'name': name, 'mode_values': mode_values})

        return color_list

    def _ToConstName(self, var_name):
        return 'k%s' % re.sub(r'[_\-\.]', '', var_name.title())

    def _AlphaToHex(self, opacity):
        return '0x%X' % math.floor(opacity.a * 255)

    def _CppOpacity(self, opacity):
        if opacity.a != -1:
            return self._AlphaToHex(opacity)
        elif opacity.var:
            return ('GetOpacity(OpacityName::%s, is_dark_mode)' %
                    self._ToConstName(opacity.var))
        raise ValueError('Invalid opacity: ' + repr(opacity))

    def _CppColor(self, c):
        '''Returns the C++ color representation of |c|'''
        assert (isinstance(c, Color))

        if isinstance(c, ColorVar):
            return ('ResolveColor(ColorName::%s, is_dark_mode)' %
                    self._ToConstName(c.var))

        if isinstance(c, ColorRGBVar):
            return (
                'SkColorSetA(ResolveColor(' +
                'ColorName::%s, is_dark_mode), %s)' %
                (self._ToConstName(c.ToVar()), self._CppOpacity(c.opacity)))

        if c.opacity.a != 1:
            return 'SkColorSetARGB(%s, 0x%X, 0x%X, 0x%X)' % (self._CppOpacity(
                c.opacity), c.r, c.g, c.b)
        else:
            return 'SkColorSetRGB(0x%X, 0x%X, 0x%X)' % (c.r, c.g, c.b)


class ViewsCCStyleGenerator(ViewsStyleGenerator):
    @staticmethod
    def GetName():
        return 'ViewsCC'

    def Render(self):
        return self.ApplyTemplate(self, 'templates/views_generator_cc.tmpl',
                                  self.GetParameters())


class ViewsHStyleGenerator(ViewsStyleGenerator):
    @staticmethod
    def GetName():
        return 'ViewsH'

    def Render(self):
        return self.ApplyTemplate(self, 'templates/views_generator_h.tmpl',
                                  self.GetParameters())
