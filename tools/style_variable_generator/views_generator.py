# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from base_generator import Color, Modes, BaseGenerator, VariableType


class ViewsStyleGenerator(BaseGenerator):
    '''Generator for Views Variables'''

    @staticmethod
    def GetName():
        return 'Views'

    def Render(self):
        self.Validate()
        return self.ApplyTemplate(self, 'views_generator_h.tmpl',
                                  self.GetParameters())

    def GetParameters(self):
        return {
            'colors': self._CreateColorList(),
        }

    def GetFilters(self):
        return {
            'to_const_name': self._ToConstName,
            'cpp_color': self._CppColor,
        }

    def GetGlobals(self):
        globals = {
            'Modes': Modes,
            'out_file_path': None,
            'namespace_name': None,
            'in_files': self.in_file_to_context.keys(),
        }
        if self.out_file_path:
            globals['out_file_path'] = self.out_file_path
            globals['namespace_name'] = os.path.splitext(
                os.path.basename(self.out_file_path))[0]
        return globals

    def _CreateColorList(self):
        color_list = []
        for name, mode_values in self.model[VariableType.COLOR].items():
            color_list.append({'name': name, 'mode_values': mode_values})

        return color_list

    def _ToConstName(self, var_name):
        return 'k%s' % var_name.title().replace('_', '')

    def _CppColor(self, c):
        '''Returns the C++ color representation of |c|'''
        assert (isinstance(c, Color))

        def AlphaToInt(alpha):
            return int(alpha * 255)

        if c.var:
            return ('ResolveColor(ColorName::%s, is_dark_mode)' %
                    self._ToConstName(c.var))

        if c.rgb_var:
            return (
                'SkColorSetA(ResolveColor(ColorName::%s, is_dark_mode), 0x%X)'
                % (self._ToConstName(c.RGBVarToVar()), AlphaToInt(c.a)))

        if c.a != 1:
            return 'SkColorSetARGB(0x%X, 0x%X, 0x%X, 0x%X)' % (AlphaToInt(c.a),
                                                               c.r, c.g, c.b)
        else:
            return 'SkColorSetRGB(0x%X, 0x%X, 0x%X)' % (c.r, c.g, c.b)
