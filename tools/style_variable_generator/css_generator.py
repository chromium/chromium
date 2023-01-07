# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from style_variable_generator.base_generator import Color, BaseGenerator
from style_variable_generator.model import Modes, VariableType
import collections


class CSSStyleGenerator(BaseGenerator):
    '''Generator for CSS Variables'''

    @staticmethod
    def GetName():
        return 'CSS'

    def Render(self):
        return self.ApplyTemplate(self, 'templates/css_generator.tmpl',
                                  self.GetParameters())

    def GetParameters(self):
        if self.generate_single_mode:
            resolved_colors = self.model.colors.Flatten(resolve_missing=True)
            resolved_opacities = self.model.opacities.Flatten(
                resolve_missing=True)
            colors = {
                Modes.DEFAULT: resolved_colors[self.generate_single_mode]
            }
            opacities = {
                Modes.DEFAULT: resolved_opacities[self.generate_single_mode]
            }
        else:
            colors = self.model.colors.Flatten()
            opacities = self.model.opacities.Flatten()

        return {
            'opacities': opacities,
            'colors': colors,
            'typefaces': self.model.typefaces,
            'font_families': self.model.font_families,
            'untyped_css': self.model.untyped_css,
        }

    def GetFilters(self):
        return {
            'to_css_var_name': self.ToCSSVarName,
            'css_color': self._CSSColor,
            'css_opacity': self._CSSOpacity,
            'css_color_rgb': self.CSSColorRGB,
            'process_simple_ref': self.ProcessSimpleRef,
        }

    def GetGlobals(self):
        return {
            'css_color_var':
            self.CSSColorVar,
            'in_files':
            self.GetInputFiles(),
            'dark_mode_selector':
            self.generator_options.get('dark_mode_selector', None),
            'suppress_sources_comment':
            self.generator_options.get('suppress_sources_comment', False),
            'Modes':
            Modes,
        }

    def AddGeneratedVars(self, var_names, variable):
        def AddVarNames(name, variations):
            for v in variations:
                var_name = v.replace('$css_name', self.ToCSSVarName(name))
                if var_name in var_names:
                    raise ValueError(name + " is defined multiple times")
                var_names[var_name] = name

        variable_type = variable.variable_type
        if variable_type == VariableType.OPACITY:
            AddVarNames(variable.name, ['$css_name'])
        elif variable_type == VariableType.COLOR:
            AddVarNames(variable.name, ['$css_name', '$css_name-rgb'])
        elif variable_type == VariableType.UNTYPED_CSS:
            AddVarNames(variable.name, ['$css_name'])
        elif variable_type == VariableType.FONT_FAMILY:
            AddVarNames(variable.name, ['$css_name'])
        elif variable_type == VariableType.TYPEFACE:
            AddVarNames(variable.name, [
                '$css_name-font',
                '$css_name-font-family',
                '$css_name-font-size',
                '$css_name-font-weight',
                '$css_name-line-height',
            ])
        else:
            raise ValueError("GetGeneratedVars() for '%s' not implemented")

    def GetCSSVarNames(self):
        '''Returns a map of all generated names to the model names that
           generated them.
        '''
        var_names = dict()
        for variable in self.model.variable_map.values():
            self.AddGeneratedVars(var_names, variable)

        return var_names

    def ProcessSimpleRef(self, value):
        '''If |value| is a simple '$other_variable' reference, returns a
           CSS variable that points to '$other_variable'.'''
        if value.startswith('$'):
            ref_name = value[1:]
            assert ref_name in self.model.variable_map
            value = 'var({0})'.format(self.ToCSSVarName(ref_name))

        return value

    def _GetCSSVarPrefix(self, name):
        prefix = self.model.variable_map[name].context.get(
            CSSStyleGenerator.GetName(), {}).get('prefix')
        return prefix + '-' if prefix else ''

    def ToCSSVarName(self, name):
        # This handles old_semantic_names as well as new.token-names.
        var_name = name.translate(str.maketrans('-_.', '_--'))

        return '--%s%s' % (self._GetCSSVarPrefix(name), var_name)

    def _CSSOpacity(self, opacity):
        if opacity.var:
            return 'var(%s)' % self.ToCSSVarName(opacity.var)

        return ('%f' % opacity.a).rstrip('0').rstrip('.')

    def _CSSColor(self, c):
        '''Returns the CSS color representation of |c|'''
        assert (isinstance(c, Color))
        if c.var:
            return 'var(%s)' % self.ToCSSVarName(c.var)

        if c.rgb_var:
            if c.opacity.a != 1:
                return 'rgba(var(%s-rgb), %g)' % (self.ToCSSVarName(
                    c.RGBVarToVar()), self._CSSOpacity(c.opacity))
            else:
                return 'rgb(var(%s-rgb))' % self.ToCSSVarName(c.RGBVarToVar())

        elif c.a != 1:
            return 'rgba(%d, %d, %d, %g)' % (c.r, c.g, c.b,
                                             self._CSSOpacity(c.opacity))
        else:
            return 'rgb(%d, %d, %d)' % (c.r, c.g, c.b)

    def CSSColorRGB(self, c):
        '''Returns the CSS rgb representation of |c|'''
        if c.var:
            return 'var(%s-rgb)' % self.ToCSSVarName(c.var)

        if c.rgb_var:
            return 'var(%s-rgb)' % self.ToCSSVarName(c.RGBVarToVar())

        return '%d, %d, %d' % (c.r, c.g, c.b)

    def CSSColorVar(self, name, color):
        '''Returns the CSS color representation given a color name and color'''
        if color.var:
            return 'var(%s)' % self.ToCSSVarName(color.var)
        if color.opacity and color.opacity.a != 1:
            return 'rgba(var(%s-rgb), %s)' % (self.ToCSSVarName(name),
                                              self._CSSOpacity(color.opacity))
        else:
            return 'rgb(var(%s-rgb))' % self.ToCSSVarName(name)
