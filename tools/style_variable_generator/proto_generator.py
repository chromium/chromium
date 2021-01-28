# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import collections
from base_generator import Color, Modes, BaseGenerator, VariableType


class BaseProtoStyleGenerator(BaseGenerator):
    '''Base Generator for Protobuf formats'''

    def GetParameters(self):
        return {
            'fields': self._CreateFieldList(),
        }

    def GetFilters(self):
        return {
            'proto_color': self._ProtoColor,
        }

    def GetGlobals(self):
        return {
            'Modes': Modes,
            'in_files': self.in_file_to_context.keys(),
        }

    def _CreateFieldList(self):
        field_value_map = dict()
        field_id_map = dict()
        field_list = []
        for ctx in self.in_file_to_context.values():
            field_name = ctx['field_name']
            field_id = ctx['field_id']
            if field_name in field_id_map and field_id_map.get(
                    field_name) != field_id:
                raise Exception(
                    'Proto field "%s" declared > 1 times with differing ids' %
                    field_name)
            field_id_map[field_name] = field_id
            field = {'name': field_name, 'id': field_id, 'values': []}
            if field_name not in field_value_map:
                field_list.append(field)
                field_value_map[field_name] = field['values']

        # Order fields by key
        field_list.sort(key=lambda x: x['id'])

        # Populate each field with its corresponding colors.
        color_model = self.model[VariableType.COLOR]
        for name, mode_values in color_model.items():
            color_item = {
                'name': name,
                'mode_values': {
                    Modes.LIGHT: color_model.ResolveToRGBA(name, Modes.LIGHT),
                    Modes.DARK: color_model.ResolveToRGBA(name, Modes.DARK),
                }
            }
            field_value_map[self.context_map[name]['field_name']].append(
                color_item)

        return field_list

    def _ProtoColor(self, c):
        '''Returns the proto color representation of |c|'''
        assert (isinstance(c, Color))

        def AlphaToInt(alpha):
            return int(alpha * 255)

        return '0x%X%02X%02X%02X' % (AlphaToInt(c.a), c.r, c.g, c.b)


class ProtoStyleGenerator(BaseProtoStyleGenerator):
    @staticmethod
    def GetName():
        return 'proto'

    def Render(self):
        self.Validate()
        return self.ApplyTemplate(self, 'proto_generator.tmpl',
                                  self.GetParameters())


class ProtoJSONStyleGenerator(BaseProtoStyleGenerator):
    @staticmethod
    def GetName():
        return 'protojson'

    def GetContextKey(self):
        return ProtoStyleGenerator.GetName()

    def Render(self):
        self.Validate()
        return self.ApplyTemplate(self, 'proto_json_generator.tmpl',
                                  self.GetParameters())
