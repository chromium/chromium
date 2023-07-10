# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import collections
from style_variable_generator.color import ColorRGB, ParseColor, ColorBlend, ColorRGBVar, ColorVar
from style_variable_generator.opacity import Opacity
from abc import ABC, abstractmethod


def full_token_name(name, context):
    namespace = context['token_namespace']
    if namespace:
        return f'{namespace}.{name}'
    return name


class Modes:
    LIGHT = 'light'
    DARK = 'dark'
    DEBUG = 'debug'
    # The mode that colors will fallback to when not specified in a
    # non-default mode. An error will be raised if a color in any mode is
    # not specified in the default mode.
    DEFAULT = LIGHT
    ALL = [LIGHT, DARK, DEBUG]


class VariableType:
    COLOR = 'color'
    OPACITY = 'opacity'
    UNTYPED_CSS = 'untyped_css'
    TYPEFACE = 'typeface'
    FONT_FAMILY = 'font_family'
    FONT_FACE = 'font_face'
    LEGACY_MAPPING = 'legacy_mappings'


class StyleVariable(object):
    '''An intermediate representation of a single variable that the generator
       knows about.

       Some JSON entries will generate multiple StyleVariables (e.g when
       generate_per_mode is true), and different Generators may create multiple
       per-platform variables (e.g CSS generates an var and var-rgb).
    '''

    def __init__(self, variable_type, name, json_value, context):
        if not re.match(r'^[a-z0-9_\.\-]+$', name):
            raise ValueError(name + ' is not a valid variable name ' +
                             '(lower case, 0-9, _)')
        self.variable_type = variable_type
        self.name = name
        self.json_value = json_value
        self.context = context or {}


class Submodel(ABC):
    '''Abstract Base Class for all Submodels.'''

    @abstractmethod
    def Add(self, name, value_obj, context):
        '''Adds the a variable represented by |value_obj| to the submodel.
        Returns a list of |StyleVariable| objects representing the variables
        added.
        '''
        assert False

    # Submodels are expected to provide dict-like interfaces.
    @abstractmethod
    def keys(self):
        assert False

    @abstractmethod
    def items(self):
        assert False

    @abstractmethod
    def __getitem__(self, key):
        assert False


class ModeKeyedModel(collections.OrderedDict, Submodel):
    def __init__(self):
        # A map of all variables to their |StyleVariable| object.
        self.variable_map = dict()

    def Add(self, name, value_obj, context):
        if name not in self:
            self[name] = {}

        if isinstance(value_obj, dict):
            for mode in value_obj:
                value = self._CreateValue(value_obj[mode])
                if mode == 'default':
                    mode = Modes.DEFAULT
                assert mode in Modes.ALL, f"Invalid mode '{mode}' used in' \
                    'definition for '{name}'"

                assert mode not in self[
                    name], f"{mode} mode for '{name}' defined multiple times"
                self[name][mode] = value
        else:
            self[name][Modes.DEFAULT] = self._CreateValue(value_obj)

        variable = StyleVariable(self.variable_type, name, value_obj, context)
        self.variable_map[name] = variable
        return [variable]

    # Returns the value that |name| will have in |mode|. Resolves to the default
    # mode's value if the a value for |mode| isn't specified. Always returns a
    # value.
    def Resolve(self, name, mode):
        if mode in self[name]:
            return self[name][mode]

        return self[name][Modes.DEFAULT]

    def Flatten(self, resolve_missing=False):
        '''Builds a name to variable dictionary for each mode.
        If |resolve_missing| is true, colors that aren't specified in |mode|
        will be resolved to their default mode value.'''
        flattened = {}
        for mode in Modes.ALL:
            variables = collections.OrderedDict()
            for name, mode_values in self.items():
                if resolve_missing:
                    variables[name] = self.Resolve(name, mode)
                else:
                    if mode in mode_values:
                        variables[name] = mode_values[mode]
            flattened[mode] = variables

        return flattened


class OpacityModel(ModeKeyedModel):
    '''A dictionary of opacity names to their values in each mode.
       e.g OpacityModel['disabled_opacity'][Modes.LIGHT] = Opacity(...)
    '''

    def __init__(self):
        super().__init__()
        self.variable_type = VariableType.OPACITY

    def Add(self, name, value_obj, context):
        name = full_token_name(name, context)
        return super().Add(name, value_obj, context)

    # Returns a float from 0-1 representing the concrete value of |opacity|.
    def ResolveOpacity(self, opacity, mode):
        if opacity.a != -1:
            return opacity

        return self.ResolveOpacity(self.Resolve(opacity.var, mode), mode)

    def _CreateValue(self, value):
        return Opacity(value)


class ColorModel(ModeKeyedModel):
    '''A dictionary of color names to their values in each mode.
       e.g ColorModel['blue'][Modes.LIGHT] = Color(...)
    '''

    def __init__(self, opacity_model):
        super().__init__()
        self.opacity_model = opacity_model
        self.variable_type = VariableType.COLOR

    def Add(self, name, value_obj, context):
        added = []
        # If a color has generate_per_mode set, a separate variable will be
        # created for each mode, suffixed by mode name.
        # (e.g my_color_light, my_color_debug)
        generate_per_mode = False
        # If a color has generate_inverted set, a |color_name|_inverted will be
        # generated which uses the dark color for light mode and vice versa.
        generate_inverted = False
        if isinstance(value_obj, dict):
            generate_per_mode = value_obj.pop('generate_per_mode', None)
            generate_inverted = value_obj.pop('generate_inverted', None)
        elif isinstance(self._CreateValue(value_obj), ColorBlend):
            # A blended color could evaluate to different colors in different
            # modes, so add it to all the modes.
            value_obj = {mode: value_obj for mode in Modes.ALL}

        generated_context = dict(context)
        generated_context['generated'] = True

        if generate_per_mode or generate_inverted:
            for mode, value in value_obj.items():
                per_mode_name = name + '_' + mode
                added += ModeKeyedModel.Add(self, per_mode_name, value,
                                            generated_context)
                value_obj[mode] = '$' + per_mode_name
        if generate_inverted:
            if Modes.LIGHT not in value_obj or Modes.DARK not in value_obj:
                raise ValueError(
                    'generate_inverted requires both dark and light modes to be'
                    ' set')
            added += ModeKeyedModel.Add(
                self, name + '_inverted', {
                    Modes.LIGHT: '$' + name + '_dark',
                    Modes.DARK: '$' + name + '_light'
                }, generated_context)

        added += ModeKeyedModel.Add(self, name, value_obj, context)
        return added

    # Returns a Color that is the final RGBA value for |name| in |mode|.
    def ResolveToRGBA(self, name, mode):
        return self._ResolveColorToRGBA(self.Resolve(name, mode), mode)

    # Returns a Color that is the hexadecimal string for |name| in |mode|.
    def ResolveToHexString(self, name, mode):
        color = self._ResolveColorToRGBA(self.Resolve(name, mode), mode)
        opacity = int(float(repr(color.opacity)) * 255)
        return '#{:02x}{:02x}{:02x}{:02x}'.format(color.r, color.g, color.b,
                                                  opacity)

    # Returns a Color that is the final RGBA value for |color| in |mode|.
    def _ResolveColorToRGBA(self, color, mode):
        if isinstance(color, ColorVar):
            return self.ResolveToRGBA(color.var, mode)

        if isinstance(color, ColorBlend) and len(color.blended_colors) == 2:
            return self._BlendColors(color.blended_colors[0],
                                     color.blended_colors[1], mode)

        result = ColorRGB()
        assert color.opacity
        result.opacity = self.opacity_model.ResolveOpacity(color.opacity, mode)

        rgb = color
        if isinstance(color, ColorRGBVar):
            rgb = self.ResolveToRGBA(color.ToVar(), mode)

        (result.r, result.g, result.b) = (rgb.r, rgb.g, rgb.b)
        return result

    def _ProcessBlendedColors(self, default_preblend):
        # Calculate the final RGBA for all blended colors because the
        # generator's subclasses can't blend yet.
        temp_model = {}
        for name, value in self.items():
            for mode, color in value.items():
                context = self.variable_map[name].context
                should_preblend = context.get('CSS',
                                              {}).get('preblend',
                                                      default_preblend)
                if isinstance(color, ColorBlend) and should_preblend:
                    assert len(color.blended_colors) == 2
                    if name not in temp_model:
                        temp_model[name] = {}
                    temp_model[name][mode] = self.ResolveToRGBA(name, mode)

        for name, value in temp_model.items():
            for mode, color in value.items():
                self[name][mode] = temp_model[name][mode]

    # Returns a Color that is the final RGBA value for |color_a| over |color_b|
    # in |mode|.
    def _BlendColors(self, color_a, color_b, mode):
        # TODO(b/206887565): Check for circular references.
        color_a_res = self._ResolveColorToRGBA(color_a, mode)
        (alpha_a, r_a, g_a, b_a) = (color_a_res.opacity.a, color_a_res.r,
                                    color_a_res.g, color_a_res.b)
        color_b_res = self._ResolveColorToRGBA(color_b, mode)
        (alpha_b, r_b, g_b, b_b) = (color_b_res.opacity.a, color_b_res.r,
                                    color_b_res.g, color_b_res.b)

        # Blend using the formula for "A over B" from
        # https://wikipedia.org/wiki/Alpha_compositing.
        alpha_out = alpha_a + (alpha_b * (1 - alpha_a))
        r_out = round(
            (r_a * alpha_a + r_b * alpha_b * (1 - alpha_a)) / alpha_out)
        g_out = round(
            (g_a * alpha_a + g_b * alpha_b * (1 - alpha_a)) / alpha_out)
        b_out = round(
            (b_a * alpha_a + b_b * alpha_b * (1 - alpha_a)) / alpha_out)

        return ColorRGB((r_out, g_out, b_out), Opacity(alpha_out))

    def _CreateValue(self, value):
        return ParseColor(value) or ColorRGB()


class SimpleModel(collections.OrderedDict, Submodel):
    def __init__(self, variable_type, check_func=None):
        self.variable_type = variable_type
        self.check_func = check_func

    def Add(self, name, value_obj, context):
        if self.check_func:
            self.check_func(name, value_obj, context)
        self[name] = value_obj
        return [StyleVariable(self.variable_type, name, value_obj, context)]


# A simple model where all variables are prefixed with the current namespace.
class NamespacedModel(SimpleModel):
    def Add(self, name, value_obj, context):
        name = full_token_name(name, context)
        return super().Add(name, value_obj, context)


# A color model where all variables are prefixed with the current namespace.
class NamespacedColorModel(ColorModel):
    def Add(self, name, value_obj, context):
        name = full_token_name(name, context)
        return super().Add(name, value_obj, context)


# A color model specifically for storing mappings of arbitrary css vars to some
# known StyleVariable.
class LegacyMappingsModel(ColorModel):
    def Add(self, name, value_obj, context):
        if isinstance(value_obj, dict):
            raise ValueError(
                'Legacy mappings can only be singular references.')
        return super().Add(name, value_obj, context)


class Model(object):
    def __init__(self):
        # A map of all variables to their |StyleVariable| object.
        self.variable_map = dict()

        # A map of |VariableType| to its underlying model.
        self.submodels = dict()

        self.opacities = OpacityModel()
        self.submodels[VariableType.OPACITY] = self.opacities

        self.colors = NamespacedColorModel(self.opacities)
        self.submodels[VariableType.COLOR] = self.colors

        self.untyped_css = NamespacedModel(VariableType.UNTYPED_CSS)
        self.submodels[VariableType.UNTYPED_CSS] = self.untyped_css

        self.legacy_mappings = LegacyMappingsModel(self.opacities)
        self.submodels[VariableType.LEGACY_MAPPING] = self.legacy_mappings

        def CheckTypeFace(name, value_obj, context):
            assert value_obj['font_family']
            assert value_obj['font_size']
            assert value_obj['font_weight']
            assert value_obj['line_height']

        self.typefaces = NamespacedModel(VariableType.TYPEFACE, CheckTypeFace)
        self.submodels[VariableType.TYPEFACE] = self.typefaces

        def CheckFontFamily(name, value_obj, context):
            assert name.startswith('font_family_')

        self.font_families = NamespacedModel(VariableType.FONT_FAMILY,
                                             CheckFontFamily)
        self.submodels[VariableType.FONT_FAMILY] = self.font_families

        def CheckFontFace(name, value_obj, context):
            assert name.startswith('face_')

        self.font_faces = NamespacedModel(VariableType.FONT_FACE,
                                          CheckFontFace)
        self.submodels[VariableType.FONT_FACE] = self.font_faces

    def Add(self, variable_type, name, value_obj, context):
        '''Adds a new variable to the submodel for |variable_type|.
        '''
        try:
            added = self.submodels[variable_type].Add(name, value_obj, context)
        except ValueError as err:
            raise ValueError(
                f'Error parsing {variable_type} "{name}": {value_obj}'
            ) from err

        for var in added:
            if var.name in self.variable_map:
                raise ValueError('Variable name "%s" is reused' % name)
            self.variable_map[var.name] = var


    def PostProcess(self, default_preblend=True):
        '''Called after all variables have been added to perform operations that
           require a complete worldview.
        '''

        # Resolve blended colors after all the files are added because some
        # color dependencies are between different files.
        self.colors._ProcessBlendedColors(default_preblend)

        self.Validate()

    def Validate(self):
        colors = self.colors
        color_names = set(colors.keys())
        opacities = self.opacities
        opacity_names = set(opacities.keys())

        def CheckColorReference(name, referrer):
            if name == referrer:
                raise ValueError("{0} refers to itself".format(name))
            if name not in color_names:
                raise ValueError("Cannot find color %s referenced by %s" %
                                 (name, referrer))

        def CheckOpacityReference(name, referrer):
            if name == referrer:
                raise ValueError("{0} refers to itself".format(name))
            if name not in opacity_names:
                raise ValueError("Cannot find opacity %s referenced by %s" %
                                 (name, referrer))

        def CheckColor(color, name):
            if isinstance(color, ColorVar):
                CheckColorReference(color.var, name)
            if isinstance(color, ColorRGBVar):
                CheckColorReference(color.ToVar(), name)
            if isinstance(color,
                          (ColorRGB, ColorRGBVar)) and color.opacity.var:
                CheckOpacityReference(color.opacity.var, name)
            if isinstance(color, ColorBlend):
                assert len(color.blended_colors) == 2
                CheckColor(color.blended_colors[0], name)
                CheckColor(color.blended_colors[1], name)

        RESERVED_SUFFIXES = ['_' + s for s in Modes.ALL + ['rgb', 'inverted']]

        # Check all colors in all modes refer to colors that exist in the
        # default mode.
        for name, mode_values in colors.items():
            for suffix in RESERVED_SUFFIXES:
                if not self.variable_map[name].context.get(
                        'generated') and name.endswith(suffix):
                    raise ValueError(
                        'Variable name "%s" uses a reserved suffix: %s' %
                        (name, suffix))
            if Modes.DEFAULT not in mode_values:
                raise ValueError("Color %s not defined for default mode" %
                                 name)

            for mode, color in mode_values.items():
                CheckColor(color, name)

        for name, mode_values in opacities.items():
            for mode, opacity in mode_values.items():
                if opacity.var:
                    CheckOpacityReference(opacity.var, name)

        # TODO(b/206887565): Check for circular references.
