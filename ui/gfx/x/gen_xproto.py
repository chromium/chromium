# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script generates header/source files with C++ language bindings
# for the X11 protocol and its extensions.  The protocol information
# is obtained from xcbproto which provides XML files describing the
# wire format.  However, we don't parse the XML here; xcbproto ships
# with xcbgen, a python library that parses the files into python data
# structures for us.
#
# The generated header and source files will look like this:

# #ifndef GEN_UI_GFX_X_XPROTO_H_
# #define GEN_UI_GFX_X_XPROTO_H_
#
# #include <array>
# #include <cstddef>
# #include <cstdint>
# #include <cstring>
# #include <vector>
#
# #include "base/component_export.h"
# #include "ui/gfx/x/xproto_types.h"
#
# namespace x11 {
#
# class Connection;
#
# class COMPONENT_EXPORT(X11) XProto {
#  public:
#   explicit XProto(Connection* connection);
#
#   Connection* connection() const { return connection_; }
#
#   struct RGB {
#     uint16_t red{};
#     uint16_t green{};
#     uint16_t blue{};
#   };
#
#   struct QueryColorsRequest {
#     uint32_t cmap{};
#     std::vector<uint32_t> pixels{};
#   };
#
#   struct QueryColorsReply {
#     uint16_t colors_len{};
#     std::vector<RGB> colors{};
#   };
#
#   using QueryColorsResponse = Response<QueryColorsReply>;
#
#   Future<QueryColorsReply> QueryColors(const QueryColorsRequest& request);
#
#  private:
#   Connection* const connection_;
# };
#
# }  // namespace x11
#
# #endif  // GEN_UI_GFX_X_XPROTO_H_

# #include "xproto.h"
#
# #include <xcb/xcb.h>
# #include <xcb/xcbext.h>
#
# #include "base/notreached.h"
# #include "base/check_op.h"
# #include "ui/gfx/x/xproto_internal.h"
#
# namespace x11 {
#
# XProto::XProto(Connection* connection) : connection_(connection) {}
#
# Future<XProto::QueryColorsReply>
# XProto::QueryColors(
#     const XProto::QueryColorsRequest& request) {
#   WriteBuffer buf;
#
#   auto& cmap = request.cmap;
#   auto& pixels = request.pixels;
#   size_t pixels_len = pixels.size();
#
#   // major_opcode
#   uint8_t major_opcode = 91;
#   Write(&major_opcode, &buf);
#
#   // pad0
#   Pad(&buf, 1);
#
#   // length
#   // Caller fills in length for writes.
#   Pad(&buf, sizeof(uint16_t));
#
#   // cmap
#   Write(&cmap, &buf);
#
#   // pixels
#   DCHECK_EQ(static_cast<size_t>(pixels_len), pixels.size());
#   for (auto& pixels_elem : pixels) {
#     Write(&pixels_elem, &buf);
#   }
#
#   return x11::SendRequest<XProto::QueryColorsReply>(connection_, &buf);
# }
#
# template<> COMPONENT_EXPORT(X11)
# std::unique_ptr<XProto::QueryColorsReply>
# detail::ReadReply<XProto::QueryColorsReply>(const uint8_t* buffer) {
#   ReadBuffer buf{buffer, 0UL};
#   auto reply = std::make_unique<XProto::QueryColorsReply>();
#
#   auto& colors_len = (*reply).colors_len;
#   auto& colors = (*reply).colors;
#
#   // response_type
#   uint8_t response_type;
#   Read(&response_type, &buf);
#
#   // pad0
#   Pad(&buf, 1);
#
#   // sequence
#   uint16_t sequence;
#   Read(&sequence, &buf);
#
#   // length
#   uint32_t length;
#   Read(&length, &buf);
#
#   // colors_len
#   Read(&colors_len, &buf);
#
#   // pad1
#   Pad(&buf, 22);
#
#   // colors
#   colors.resize(colors_len);
#   for (auto& colors_elem : colors) {
#     auto& red = colors_elem.red;
#     auto& green = colors_elem.green;
#     auto& blue = colors_elem.blue;
#
#     // red
#     Read(&red, &buf);
#
#     // green
#     Read(&green, &buf);
#
#     // blue
#     Read(&blue, &buf);
#
#     // pad0
#     Pad(&buf, 2);
#
#   }
#
#   Align(&buf, 4);
#   DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);
#
#   return reply;
# }
#
# }  // namespace x11

from __future__ import print_function

import argparse
import collections
import functools
import itertools
import os
import re
import sys
import types

# __main__.output must be defined before importing xcbgen,
# so this global is unavoidable.
output = collections.defaultdict(int)

RENAME = {
    'ANIMCURSORELT': 'AnimationCursorElement',
    'CA': 'ChangeAlarmAttribute',
    'CHAR2B': 'Char16',
    'CHARINFO': 'CharInfo',
    'COLORITEM': 'ColorItem',
    'COLORMAP': 'ColorMap',
    'Connection': 'RandRConnection',
    'CP': 'CreatePictureAttribute',
    'CS': 'ClientSpec',
    'CW': 'CreateWindowAttribute',
    'DAMAGE': 'DamageId',
    'DIRECTFORMAT': 'DirectFormat',
    'DOTCLOCK': 'DotClock',
    'FBCONFIG': 'FbConfig',
    'FLOAT32': 'float',
    'FLOAT64': 'double',
    'FONTPROP': 'FontProperty',
    'GC': 'GraphicsContextAttribute',
    'GCONTEXT': 'GraphicsContext',
    'GLYPHINFO': 'GlyphInfo',
    'GLYPHSET': 'GlyphSet',
    'INDEXVALUE': 'IndexValue',
    'KB': 'Keyboard',
    'KEYCODE': 'KeyCode',
    'KEYCODE32': 'KeyCode32',
    'KEYSYM': 'KeySym',
    'LINEFIX': 'LineFix',
    'OP': 'Operation',
    'PBUFFER': 'PBuffer',
    'PCONTEXT': 'PContext',
    'PICTDEPTH': 'PictDepth',
    'PICTFORMAT': 'PictFormat',
    'PICTFORMINFO': 'PictFormInfo',
    'PICTSCREEN': 'PictScreen',
    'PICTVISUAL': 'PictVisual',
    'POINTFIX': 'PointFix',
    'SPANFIX': 'SpanFix',
    'SUBPICTURE': 'SubPicture',
    'SYSTEMCOUNTER': 'SystemCounter',
    'TIMECOORD': 'TimeCoord',
    'TIMESTAMP': 'Time',
    'VISUALID': 'VisualId',
    'VISUALTYPE': 'VisualType',
    'WAITCONDITION': 'WaitCondition',
}

READ_SPECIAL = set([
    ('xcb', 'Setup'),
])

WRITE_SPECIAL = set([
    ('xcb', 'ClientMessage'),
    ('xcb', 'Expose'),
    ('xcb', 'UnmapNotify'),
    ('xcb', 'SelectionNotify'),
    ('xcb', 'MotionNotify'),
    ('xcb', 'Key'),
    ('xcb', 'Button'),
    ('xcb', 'PropertyNotify'),
])


def adjust_type_name(name):
    if name in RENAME:
        return RENAME[name]
    # If there's an underscore, then this is either snake case or upper case.
    if '_' in name:
        return ''.join([
            token[0].upper() + token[1:].lower() for token in name.split('_')
        ])
    if name.isupper():
        name = name.lower()
    # Now the only possibilities are caml case and pascal case.  It could also
    # be snake case with a single word, but that would be same as caml case.
    # To convert all of these, just capitalize the first letter.
    return name[0].upper() + name[1:]


# Given a list of event names like ["KeyPress", "KeyRelease"], returns a name
# suitable for use as a base event like "Key".
def event_base_name(names):
    # If there's only one event in this group, the "common name" is just
    # the event name.
    if len(names) == 1:
        return names[0]

    # Handle a few special cases where the longest common prefix is empty: eg.
    # EnterNotify/LeaveNotify/FocusIn/FocusOut -> Crossing.
    EVENT_NAMES = [
        ('TouchBegin', 'Device'),
        ('RawTouchBegin', 'RawDevice'),
        ('Enter', 'Crossing'),
        ('EnterNotify', 'Crossing'),
        ('DeviceButtonPress', 'LegacyDevice'),
    ]
    for name, rename in EVENT_NAMES:
        if name in names:
            return rename

    # Use the longest common prefix of the event names as the base name.
    name = ''.join(
        chars[0]
        for chars in itertools.takewhile(lambda chars: len(set(chars)) == 1,
                                         zip(*names)))
    assert name
    return name


def list_size(name, list_type):
    separator = '->' if list_type.is_ref_counted_memory else '.'
    return '%s%ssize()' % (name, separator)


# Left-pad with 2 spaces while this class is alive.
class Indent:
    def __init__(self, xproto, opening_line, closing_line):
        self.xproto = xproto
        self.opening_line = opening_line
        self.closing_line = closing_line

    def __enter__(self):
        self.xproto.write(self.opening_line)
        self.xproto.indent += 1

    def __exit__(self, exc_type, exc_value, exc_traceback):
        self.xproto.indent -= 1
        self.xproto.write(self.closing_line)


# Make all members of |obj|, given by |fields|, visible in
# the local scope while this class is alive.
class ScopedFields:
    def __init__(self, xproto, obj, fields):
        self.xproto = xproto
        self.obj = obj
        self.fields = fields
        self.n_pushed = 0

    def __enter__(self):
        for field in self.fields:
            self.n_pushed += self.xproto.add_field_to_scope(field, self.obj)

        if self.n_pushed:
            self.xproto.write()

    def __exit__(self, exc_type, exc_value, exc_traceback):
        for _ in range(self.n_pushed):
            self.xproto.scope.pop()


# Ensures |name| is usable as a C++ field by avoiding keywords and
# symbols that start with numbers.
def safe_name(name):
    RESERVED = [
        'and',
        'xor',
        'or',
        'class',
        'explicit',
        'new',
        'delete',
        'default',
        'private',
    ]
    if name[0].isdigit() or name in RESERVED:
        return 'c_' + name
    return name


class FileWriter:
    def __init__(self):
        self.indent = 0

    # Write a line to the current file.
    def write(self, line=''):
        indent = self.indent if line and not line.startswith('#') else 0
        print(('  ' * indent) + line, file=self.file)


class GenXproto(FileWriter):
    def __init__(self, proto, proto_dir, gen_dir, xcbgen, all_types):
        FileWriter.__init__(self)

        # Command line arguments
        self.proto = proto
        self.xml_filename = os.path.join(proto_dir, '%s.xml' % proto)
        self.header_file = open(os.path.join(gen_dir, '%s.h' % proto), 'w')
        self.source_file = open(os.path.join(gen_dir, '%s.cc' % proto), 'w')

        # Top-level xcbgen python module
        self.xcbgen = xcbgen

        # Types for every module including this one
        self.all_types = all_types

        # The last used UID for making unique names
        self.prev_id = -1

        # Current file to write to
        self.file = None

        # Flag to indicate if we're generating code to serialize or
        # deserialize data.
        self.is_read = False

        # List of the fields in scope
        self.scope = []

        # Current place in C++ namespace hierarchy (including classes)
        self.namespace = []

        # Map from type names to a set of types.  Certain types
        # like enums and simple types can alias each other.
        self.types = collections.defaultdict(list)

        # Set of names of simple types to be replaced with enums
        self.replace_with_enum = set()

        # Map of enums to their underlying types
        self.enum_types = collections.defaultdict(set)

        # Map from (XML tag, XML name) to XML element
        self.module_names = {}

        # Enums that represent bit masks.
        self.bitenums = []

    # Geenerate an ID suitable for use in temporary variable names.
    def new_uid(self, ):
        self.prev_id += 1
        return self.prev_id

    def type_suffix(self, t):
        if isinstance(t, self.xcbgen.xtypes.Error):
            return 'Error'
        elif isinstance(t, self.xcbgen.xtypes.Request):
            return 'Request'
        elif t.is_reply:
            return 'Reply'
        elif t.is_event:
            return 'Event'
        return ''

    def rename_type(self, t, name):
        # Work around a bug in xcbgen: ('int') should have been ('int',)
        if name == 'int':
            name = ('int', )

        name = list(name)

        if name[0] == 'xcb':
            # Use namespace x11 instead of xcb.
            name[0] = 'x11'

        for i in range(1, len(name)):
            name[i] = adjust_type_name(name[i])
        name[-1] += self.type_suffix(t)
        return name

    # Given an unqualified |name| like ('Window') and a namespace like ['x11'],
    # returns a fully qualified name like ('x11', 'Window').
    def qualify_type(self, name, namespace):
        if tuple(namespace + name) in self.all_types:
            return namespace + name
        return self.qualify_type(name, namespace[:-1])

    # Given an xcbgen.xtypes.Type, returns a C++-namespace-qualified
    # string that looks like Input::InputClass::Key.
    def qualtype(self, t, name):
        name = self.rename_type(t, name)

        # Try to avoid adding namespace qualifiers if they're not necessary.
        chop = 0
        for t1, t2 in zip(name, self.namespace):
            if t1 != t2:
                break
            if self.qualify_type(name[chop + 1:], self.namespace) != name:
                break
            chop += 1
        return '::'.join(name[chop:])

    def fieldtype(self, field):
        if field.isfd:
            return 'base::ScopedFD'
        return self.qualtype(field.type,
                             field.enum if field.enum else field.field_type)

    def switch_fields(self, switch):
        fields = []
        for case in switch.bitcases:
            if case.field_name:
                fields.append(case)
            else:
                fields.extend(case.type.fields)
        return fields

    def add_field_to_scope(self, field, obj):
        if not field.visible or (not field.wire and not field.isfd):
            return 0

        field_name = safe_name(field.field_name)

        if field.type.is_switch:
            self.write('auto& %s = %s;' % (field_name, obj))
            return 0

        self.scope.append(field)

        if field.for_list or field.for_switch:
            self.write('%s %s{};' % (self.fieldtype(field), field_name))
        else:
            self.write('auto& %s = %s.%s;' % (field_name, obj, field_name))

        if field.type.is_list:
            len_name = field_name + '_len'
            if not self.field_from_scope(len_name):
                len_expr = list_size(field_name, field.type)
                if field.type.is_ref_counted_memory:
                    len_expr = '%s ? %s : 0' % (field_name, len_expr)
                self.write('size_t %s = %s;' % (len_name, len_expr))

        return 1

    # Lookup |name| in the current scope.  Returns the deepest
    # (most local) occurrence of |name|.
    def field_from_scope(self, name):
        for field in reversed(self.scope):
            if field.field_name == name:
                return field
        return None

    def expr(self, expr):
        if expr.op == 'popcount':
            return 'PopCount(%s)' % self.expr(expr.rhs)
        if expr.op == '~':
            return 'BitNot(%s)' % self.expr(expr.rhs)
        if expr.op == '&':
            return 'BitAnd(%s, %s)' % (self.expr(expr.lhs), self.expr(
                expr.rhs))
        if expr.op in ('+', '-', '*', '/', '|'):
            return ('(%s) %s (%s)' %
                    (self.expr(expr.lhs), expr.op, self.expr(expr.rhs)))
        if expr.op == 'calculate_len':
            return expr.lenfield_name
        if expr.op == 'sumof':
            tmp_id = self.new_uid()
            lenfield = self.field_from_scope(expr.lenfield_name)
            elem_type = lenfield.type.member
            fields = elem_type.fields if elem_type.is_container else []
            header = 'auto sum%d_ = SumOf([](%sauto& listelem_ref) {' % (
                tmp_id, '' if self.is_read else 'const ')
            footer = '}, %s);' % expr.lenfield_name
            with Indent(self, header,
                        footer), ScopedFields(self, 'listelem_ref', fields):
                body = self.expr(expr.rhs) if expr.rhs else 'listelem_ref'
                self.write('return %s;' % body)
            return 'sum%d_' % tmp_id
        if expr.op == 'listelement-ref':
            return 'listelem_ref'
        if expr.op == 'enumref':
            return '%s::%s' % (self.qualtype(
                expr.lenfield_type,
                expr.lenfield_type.name), safe_name(expr.lenfield_name))

        assert expr.op == None
        if expr.nmemb:
            return str(expr.nmemb)

        assert expr.lenfield_name
        return expr.lenfield_name

    def get_xidunion_element(self, name):
        key = ('xidunion', name[-1])
        return self.module_names.get(key, None)

    def declare_xidunion(self, xidunion, xidname):
        names = [type_element.text for type_element in xidunion]
        types = list(set([self.module.get_type(name) for name in names]))
        assert len(types) == 1
        value_type = types[0]
        value_typename = self.qualtype(value_type, value_type.name)
        with Indent(self, 'struct %s {' % xidname, '};'):
            self.write('%s() : value{} {}' % xidname)
            self.write()
            for name in names:
                cpp_name = self.module.get_type_name(name)
                typename = self.qualtype(value_type, cpp_name)
                self.write('%s(%s value) : value{static_cast<%s>(value)} {}' %
                           (xidname, typename, value_typename))
                self.write(
                    'operator %s() const { return static_cast<%s>(value); }' %
                    (typename, typename))
                self.write()
            self.write('%s value{};' % value_typename)

    def declare_simple(self, item, name):
        # The underlying type of an enum must be integral, so avoid defining
        # FLOAT32 or FLOAT64.  Usages are renamed to float and double instead.
        renamed = tuple(self.rename_type(item, name))
        if (name[-1] in ('FLOAT32', 'FLOAT64')
                or renamed in self.replace_with_enum):
            return

        xidunion = self.get_xidunion_element(name)
        if xidunion:
            self.declare_xidunion(xidunion, renamed[-1])
        else:
            self.write('enum class %s : %s {};' %
                       (renamed[-1], self.qualtype(item, item.name)))
        self.write()

    def copy_primitive(self, name):
        if self.is_read:
            self.write('Read(&%s, &buf);' % name)
        else:
            self.write('buf.Write(&%s);' % name)

    def copy_fd(self, field, name):
        if self.is_read:
            self.write('%s = base::ScopedFD(buf.TakeFd());' % name)
        else:
            # We take the request struct as const&, so dup() the fd to preserve
            # const-correctness because XCB close()s it after writing it.
            self.write('buf.fds().push_back(HANDLE_EINTR(dup(%s.get())));' %
                       name)

    def copy_special_field(self, field):
        type_name = self.fieldtype(field)
        name = safe_name(field.field_name)

        def copy_basic():
            self.write('%s %s;' % (type_name, name))
            self.copy_primitive(name)

        if name in ('major_opcode', 'minor_opcode'):
            assert not self.is_read
            is_ext = self.module.namespace.is_ext
            self.write(
                '%s %s = %s;' %
                (type_name, name, 'info_.major_opcode' if is_ext
                 and name == 'major_opcode' else field.parent[0].opcode))
            self.copy_primitive(name)
        elif name == 'response_type':
            if self.is_read:
                copy_basic()
            else:
                container_type, container_name = field.parent
                assert container_type.is_event
                # Extension events require offsetting the opcode, so make
                # sure this path is only hit for non-extension events for now.
                assert not self.module.namespace.is_ext
                opcode = container_type.opcodes.get(container_name,
                                                    'obj.opcode')
                self.write('%s %s = %s;' % (type_name, name, opcode))
                self.copy_primitive(name)
        elif name in ('extension', 'error_code', 'event_type'):
            assert self.is_read
            copy_basic()
        elif name == 'length':
            if not self.is_read:
                self.write('// Caller fills in length for writes.')
                self.write('Pad(&buf, sizeof(%s));' % type_name)
            else:
                copy_basic()
        else:
            assert field.type.is_expr
            assert (not isinstance(field.type, self.xcbgen.xtypes.Enum))
            self.write('%s %s = %s;' %
                       (type_name, name, self.expr(field.type.expr)))
            self.copy_primitive(name)

    def declare_case(self, case):
        assert case.type.is_case != case.type.is_bitcase

        fields = [
            field for case_field in case.type.fields
            for field in self.declare_field(case_field)
        ]
        if not case.field_name:
            return fields
        name = safe_name(case.field_name)
        typename = adjust_type_name(name)
        with Indent(self, 'struct %s {' % typename, '};'):
            for field in fields:
                self.write('%s %s{};' % field)
        return [(typename, name)]

    def copy_case(self, case, switch_name):
        op = 'CaseEq' if case.type.is_case else 'CaseAnd'
        condition = ' || '.join([
            '%s(%s_expr, %s)' % (op, switch_name, self.expr(expr))
            for expr in case.type.expr
        ])

        with Indent(self, 'if (%s) {' % condition, '}'):
            if case.field_name:
                fields = [case]
                obj = '(*%s.%s)' % (switch_name, safe_name(case.field_name))
            else:
                fields = case.type.fields
                obj = '*' + switch_name
            for case_field in fields:
                name = safe_name(case_field.field_name)
                if case_field.visible and self.is_read:
                    self.write('%s.%s.emplace();' % (switch_name, name))
            with ScopedFields(self, obj, case.type.fields):
                for case_field in case.type.fields:
                    self.copy_field(case_field)

    def declare_switch(self, field):
        return [('base::Optional<%s>' % field_type, field_name)
                for case in field.type.bitcases
                for field_type, field_name in self.declare_case(case)]

    def copy_switch(self, field):
        t = field.type
        name = safe_name(field.field_name)

        self.write('auto %s_expr = %s;' % (name, self.expr(t.expr)))
        for case in t.bitcases:
            self.copy_case(case, name)

    def declare_list(self, field):
        t = field.type
        type_name = self.fieldtype(field)
        name = safe_name(field.field_name)

        assert (t.nmemb not in (0, 1))
        if t.is_ref_counted_memory:
            type_name = 'scoped_refptr<base::RefCountedMemory>'
        elif t.nmemb:
            type_name = 'std::array<%s, %d>' % (type_name, t.nmemb)
        elif type_name == 'char':
            type_name = 'std::string'
        else:
            type_name = 'std::vector<%s>' % type_name
        return [(type_name, name)]

    def copy_list(self, field):
        t = field.type
        name = safe_name(field.field_name)
        size = self.expr(t.expr)

        if t.is_ref_counted_memory:
            if self.is_read:
                self.write('%s = buffer->ReadAndAdvance(%s);' % (name, size))
            else:
                self.write('buf.AppendBuffer(%s, %s);' % (name, size))
            return

        if not t.nmemb:
            if self.is_read:
                self.write('%s.resize(%s);' % (name, size))
            else:
                left = 'static_cast<size_t>(%s)' % size
                self.write('DCHECK_EQ(%s, %s.size());' % (left, name))
        with Indent(self, 'for (auto& %s_elem : %s) {' % (name, name), '}'):
            elem_name = name + '_elem'
            elem_type = t.member
            elem_field = self.xcbgen.expr.Field(elem_type, field.field_type,
                                                elem_name, field.visible,
                                                field.wire, field.auto,
                                                field.enum, field.isfd)
            elem_field.for_list = None
            elem_field.for_switch = None
            self.copy_field(elem_field)

    def generate_switch_var(self, field):
        name = safe_name(field.field_name)
        for case in field.for_switch.type.bitcases:
            case_field = case if case.field_name else case.type.fields[0]
            self.write('SwitchVar(%s, %s.%s.has_value(), %s, &%s);' %
                       (self.expr(case.type.expr[0]),
                        safe_name(field.for_switch.field_name),
                        safe_name(case_field.field_name),
                        'true' if case.type.is_bitcase else 'false', name))

    def declare_field(self, field):
        t = field.type
        name = safe_name(field.field_name)

        if not field.visible or field.for_list or field.for_switch:
            return []

        if t.is_switch:
            return self.declare_switch(field)
        if t.is_list:
            return self.declare_list(field)
        return [(self.fieldtype(field), name)]

    def copy_field(self, field):
        if not field.wire and not field.isfd:
            return

        t = field.type
        renamed = tuple(self.rename_type(field.type, field.field_type))
        if t.is_list:
            t.member = self.all_types.get(renamed, t.member)
        else:
            t = self.all_types.get(renamed, t)
        name = safe_name(field.field_name)

        self.write('// ' + name)

        # If this is a generated field, initialize the value of the field
        # variable from the given context.
        if not self.is_read:
            if field.for_list:
                size = list_size(safe_name(field.for_list.field_name),
                                 field.for_list.type)
                self.write('%s = %s;' % (name, size))
            if field.for_switch:
                self.generate_switch_var(field)

        if t.is_pad:
            if t.align > 1:
                assert t.nmemb == 1
                assert t.align in (2, 4)
                self.write('Align(&buf, %d);' % t.align)
            else:
                self.write('Pad(&buf, %d);' % t.nmemb)
        elif not field.visible:
            self.copy_special_field(field)
        elif t.is_switch:
            self.copy_switch(field)
        elif t.is_list:
            self.copy_list(field)
        elif t.is_union:
            self.copy_primitive(name)
        elif t.is_container:
            with Indent(self, '{', '}'):
                self.copy_container(t, name)
        else:
            assert t.is_simple
            if field.isfd:
                self.copy_fd(field, name)
            elif field.enum:
                self.copy_enum(field)
            else:
                self.copy_primitive(name)

        self.write()

    def declare_enum(self, enum):
        def declare_enum_entry(name, value):
            name = safe_name(name)
            self.write('%s = %s,' % (name, value))

        with Indent(
                self, 'enum class %s : %s {' %
            (adjust_type_name(enum.name[-1]), self.enum_types[enum.name][0]
             if enum.name in self.enum_types else 'int'), '};'):
            bitnames = set([name for name, _ in enum.bits])
            for name, value in enum.values:
                if name not in bitnames:
                    declare_enum_entry(name, value)
            for name, value in enum.bits:
                declare_enum_entry(name, '1 << ' + value)
        self.write()

    def copy_enum(self, field):
        # The size of enum types may be different depending on the
        # context, so they should always be casted to the contextual
        # underlying type before calling Read() or Write().
        underlying_type = self.qualtype(field.type, field.type.name)
        tmp_name = 'tmp%d' % self.new_uid()
        real_name = safe_name(field.field_name)
        self.write('%s %s;' % (underlying_type, tmp_name))
        if not self.is_read:
            self.write('%s = static_cast<%s>(%s);' %
                       (tmp_name, underlying_type, real_name))
        self.copy_primitive(tmp_name)
        if self.is_read:
            enum_type = self.qualtype(field.type, field.enum)
            self.write('%s = static_cast<%s>(%s);' %
                       (real_name, enum_type, tmp_name))

    def declare_fields(self, fields):
        for field in fields:
            for field_type_name in self.declare_field(field):
                self.write('%s %s{};' % field_type_name)

    # This tries to match XEvent.xany.window, except the window will be
    # x11::Window::None for events that don't have a window, unlike the XEvent
    # union which will get whatever data happened to be at the offset of
    # xany.window.
    def get_window_field(self, event):
        # The window field is not stored at any particular offset in the event,
        # so get a list of all the window fields.
        WINDOW_TYPES = set([
            ('xcb', 'WINDOW'),
            ('xcb', 'DRAWABLE'),
            ('xcb', 'Glx', 'DRAWABLE'),
        ])
        # The window we want may not be the first in the list if there are
        # multiple windows. This is a list of all possible window names,
        # ordered from highest to lowest priority.
        WINDOW_NAMES = [
            'event',
            'window',
            'request_window',
            'owner',
        ]
        windows = set([
            field.field_name for field in event.fields
            if field.field_type in WINDOW_TYPES
        ])
        if len(windows) == 0:
            return ''
        if len(windows) == 1:
            return list(windows)[0]
        for name in WINDOW_NAMES:
            if name in windows:
                return name
        assert False

    def declare_event(self, event, name):
        event_name = name[-1] + 'Event'
        with Indent(self, 'struct %s {' % adjust_type_name(event_name), '};'):
            self.write('static constexpr int type_id = %d;' % event.type_id)
            if len(event.opcodes) == 1:
                self.write('static constexpr uint8_t opcode = %s;' %
                           event.opcodes[name])
            else:
                with Indent(self, 'enum Opcode {', '} opcode{};'):
                    items = [(int(x), y)
                             for (y, x) in event.enum_opcodes.items()]
                    for opcode, opname in sorted(items):
                        self.write('%s = %s,' % (opname, opcode))
            self.write('bool send_event{};')
            self.declare_fields(event.fields)
            self.write()
            window_field = self.get_window_field(event)
            ret = ('reinterpret_cast<x11::Window*>(&%s)' %
                   window_field if window_field else 'nullptr')
            self.write('x11::Window* GetWindow() { return %s; }' % ret)
        self.write()

    def declare_container(self, struct, struct_name):
        name = struct_name[-1] + self.type_suffix(struct)
        with Indent(self, 'struct %s {' % adjust_type_name(name), '};'):
            self.declare_fields(struct.fields)
        self.write()

    def copy_container(self, struct, name):
        assert not struct.is_union
        with ScopedFields(self, name, struct.fields):
            for field in struct.fields:
                self.copy_field(field)

    def read_special_container(self, struct, name):
        self.namespace = ['x11']
        name = self.qualtype(struct, name)
        self.write('template <> COMPONENT_EXPORT(X11)')
        self.write('%s Read<%s>(' % (name, name))
        with Indent(self, '    ReadBuffer* buffer) {', '}'):
            self.write('auto& buf = *buffer;')
            self.write('%s obj;' % name)
            self.write()
            self.is_read = True
            self.copy_container(struct, 'obj')
            self.write('return obj;')
        self.write()

    def write_special_container(self, struct, name):
        self.namespace = ['x11']
        name = self.qualtype(struct, name)
        self.write('template <> COMPONENT_EXPORT(X11)')
        self.write('WriteBuffer Write<%s>(' % name)
        with Indent(self, '    const %s& obj) {' % name, '}'):
            self.write('WriteBuffer buf;')
            self.write()
            self.is_read = False
            self.copy_container(struct, 'obj')
            self.write('return buf;')
        self.write()

    def declare_union(self, union):
        name = union.name[-1]
        if union.elt.tag == 'eventstruct':
            # There's only one of these in all of the protocol descriptions.
            # It's just used to represent any 32-byte event for XInput.
            self.write('using %s = std::array<uint8_t, 32>;' % name)
            return
        with Indent(self, 'union %s {' % name, '};'):
            self.write('%s() { memset(this, 0, sizeof(*this)); }' % name)
            self.write()
            for field in union.fields:
                field_type_names = self.declare_field(field)
                assert len(field_type_names) == 1
                self.write('%s %s;' % field_type_names[0])
        self.write(
            'static_assert(std::is_trivially_copyable<%s>::value, "");' % name)
        self.write()

    def declare_request(self, request):
        method_name = request.name[-1]
        request_name = method_name + 'Request'
        reply_name = method_name + 'Reply' if request.reply else 'void'

        in_class = self.namespace == ['x11', self.class_name]

        if not in_class or self.module.namespace.is_ext:
            self.declare_container(request, request.name)
            if request.reply:
                self.declare_container(request.reply, request.reply.name)

            self.write('using %sResponse = Response<%s>;' %
                       (method_name, reply_name))
            self.write()

        if in_class:
            self.write('Future<%s> %s(' % (reply_name, method_name))
            self.write('    const %s& request);' % request_name)
            self.write()

    def define_request(self, request):
        method_name = '%s::%s' % (self.class_name, request.name[-1])
        prefix = (method_name
                  if self.module.namespace.is_ext else request.name[-1])
        request_name = prefix + 'Request'
        reply_name = prefix + 'Reply'

        reply = request.reply
        if not reply:
            reply_name = 'void'

        self.write('Future<%s>' % reply_name)
        self.write('%s(' % method_name)
        with Indent(self, '    const %s& request) {' % request_name, '}'):
            cond = '!connection_->Ready()'
            if self.module.namespace.is_ext:
                cond += ' || !present()'
            self.write('if (%s)' % cond)
            self.write('  return {};')
            self.write()
            self.namespace = ['x11', self.class_name]
            self.write('WriteBuffer buf;')
            self.write()
            self.is_read = False
            self.copy_container(request, 'request')
            self.write('Align(&buf, 4);')
            self.write()
            reply_has_fds = reply and any(field.isfd for field in reply.fields)
            self.write('return x11::SendRequest<%s>(connection_, &buf, %s);' %
                       (reply_name, 'true' if reply_has_fds else 'false'))
        self.write()

        if not reply:
            return

        self.write('template<> COMPONENT_EXPORT(X11)')
        self.write('std::unique_ptr<%s>' % reply_name)
        sig = 'detail::ReadReply<%s>(ReadBuffer* buffer) {' % reply_name
        with Indent(self, sig, '}'):
            self.namespace = ['x11']
            self.write('auto& buf = *buffer;')
            self.write('auto reply = std::make_unique<%s>();' % reply_name)
            self.write()
            self.is_read = True
            self.copy_container(reply, '(*reply)')
            self.write('Align(&buf, 4);')
            offset = 'buf.offset < 32 ? 0 : buf.offset - 32'
            self.write('DCHECK_EQ(%s, 4 * length);' % offset)
            self.write()
            self.write('return reply;')
        self.write()

    def define_event(self, event, name):
        self.namespace = ['x11']
        name = self.qualtype(event, name)
        self.write('template <> COMPONENT_EXPORT(X11)')
        self.write('void ReadEvent<%s>(' % name)
        with Indent(self, '    %s* event_, ReadBuffer* buffer) {' % name, '}'):
            self.write('auto& buf = *buffer;')
            self.write()
            self.is_read = True
            self.copy_container(event, '(*event_)')
            if event.is_ge_event:
                self.write('Align(&buf, 4);')
                self.write('DCHECK_EQ(buf.offset, 32 + 4 * length);')
            else:
                self.write('DCHECK_LE(buf.offset, 32ul);')
        self.write()

    def define_type(self, item, name):
        if name in READ_SPECIAL:
            self.read_special_container(item, name)
        if name in WRITE_SPECIAL:
            self.write_special_container(item, name)
        if isinstance(item, self.xcbgen.xtypes.Request):
            self.define_request(item)
        elif item.is_event:
            self.define_event(item, name)

    def declare_type(self, item, name):
        if item.is_union:
            self.declare_union(item)
        elif isinstance(item, self.xcbgen.xtypes.Request):
            self.declare_request(item)
        elif item.is_event:
            self.declare_event(item, name)
        elif item.is_container:
            self.declare_container(item, name)
        elif isinstance(item, self.xcbgen.xtypes.Enum):
            self.declare_enum(item)
        else:
            assert item.is_simple
            self.declare_simple(item, name)

    # Additional type information identifying the enum/mask is present in the
    # XML data, but xcbgen doesn't make use of it: it only uses the underlying
    # type, as it appears on the wire.  We want additional type safety, so
    # extract this information the from XML directly.
    def resolve_element(self, xml_element, fields):
        for child in xml_element:
            if 'name' not in child.attrib:
                if child.tag == 'case' or child.tag == 'bitcase':
                    self.resolve_element(child, fields)
                continue
            name = child.attrib['name']
            field = fields[name]
            field.elt = child
            enums = [
                child.attrib[attr] for attr in ['enum', 'mask']
                if attr in child.attrib
            ]
            if enums:
                assert len(enums) == 1
                enum = enums[0]
                field.enum = self.module.get_type(enum).name
                self.enum_types[enum].add(field.type.name)
            else:
                field.enum = None

    def resolve_type(self, t, name):
        renamed = tuple(self.rename_type(t, name))
        assert renamed[0] == 'x11'
        assert t not in self.types[renamed]
        self.types[renamed].append(t)
        self.all_types[renamed] = t

        if isinstance(t, self.xcbgen.xtypes.Enum):
            self.bitenums.append((t, name))

        if not t.is_container:
            return

        fields = {
            field.field_name: field
            for field in (self.switch_fields(t) if t.is_switch else t.fields)
        }

        self.resolve_element(t.elt, fields)

        for field in fields.values():
            if field.field_name == 'sequence':
                field.visible = True
            field.parent = (t, name)

            if field.type.is_list:
                # xcb uses void* in some places to represent arbitrary data.
                field.type.is_ref_counted_memory = (
                    not field.type.nmemb and field.field_type[0] == 'void')

            # |for_list| and |for_switch| may have already been set when
            # processing other fields in this structure.
            field.for_list = getattr(field, 'for_list', None)
            field.for_switch = getattr(field, 'for_switch', None)

            for is_type, for_type in ((field.type.is_list, 'for_list'),
                                      (field.type.is_switch, 'for_switch')):
                if not is_type:
                    continue
                expr = field.type.expr
                field_name = expr.lenfield_name
                if (expr.op in (None, 'calculate_len')
                        and field_name in fields):
                    setattr(fields[field_name], for_type, field)

            if field.type.is_switch or field.type.is_case_or_bitcase:
                self.resolve_type(field.type, field.field_type)

        if isinstance(t, self.xcbgen.xtypes.Request) and t.reply:
            self.resolve_type(t.reply, t.reply.name)

    # Multiple event names may map to the same underlying event.  For these
    # cases, we want to avoid duplicating the event structure.  Instead, put
    # all of these events under one structure with an additional opcode field
    # to indicate the type of event.
    def uniquify_events(self):
        # Manually merge some events in XInput.  These groups of 8 events have
        # idential structure, and are merged as XIDeviceEvent in Xlib.  To avoid
        # duplication, and to ease the transition from Xlib to XProto, we merge
        # the events here too.
        # TODO(thomasanderson): We should avoid adding workarounds for xcbproto.
        # Instead, the protocol files should be modified directly.  However,
        # some of the changes we want to make change the API, so the changes
        # should be made in a fork in //third_party rather than upstreamed.
        MERGE = [
            ([
                'KeyPress', 'KeyRelease', 'ButtonPress', 'ButtonRelease',
                'Motion', 'TouchBegin', 'TouchUpdate', 'TouchEnd'
            ], []),
            ([
                'RawKeyPress', 'RawKeyRelease', 'RawButtonPress',
                'RawButtonRelease', 'RawMotion', 'RawTouchBegin',
                'RawTouchUpdate', 'RawTouchEnd'
            ], []),
        ]
        for i, (name, t) in enumerate(self.module.all):
            if t.is_event and name[1] == 'Input':
                for names, event in MERGE:
                    if name[-1] in names:
                        if event:
                            event[0].opcodes.update(t.opcodes)
                            self.module.all[i] = name, event[0]
                        else:
                            event.append(t)

        types = []
        events = set()
        for name, t in self.module.all:
            if not t.is_event or len(t.opcodes) == 1:
                types.append((name, t))
                continue

            renamed = tuple(self.rename_type(t, name))
            self.all_types[renamed] = t
            if t in events:
                continue
            events.add(t)

            names = [name[-1] for name in t.opcodes.keys()]
            name = name[:-1] + (event_base_name(names), )
            types.append((name, t))

            t.enum_opcodes = {}
            for opname in t.opcodes:
                opcode = t.opcodes[opname]
                opname = opname[-1]
                if opname.startswith(name[-1]):
                    opname = opname[len(name[-1]):]
                t.enum_opcodes[opname] = opcode
        self.module.all = types

    # Perform preprocessing like renaming, reordering, and adding additional
    # data fields.
    def resolve(self):
        self.class_name = (adjust_type_name(self.module.namespace.ext_name)
                           if self.module.namespace.is_ext else 'XProto')

        self.uniquify_events()

        for i, (name, t) in enumerate(self.module.all):
            # Work around a name conflict: the type ScreenSaver has the same
            # name as the extension, so rename the type.
            if name == ('xcb', 'ScreenSaver'):
                name = ('xcb', 'ScreenSaverMode')
                t.name = name
                self.module.all[i] = (name, t)
            self.resolve_type(t, name)

        for enum, types in list(self.enum_types.items()):
            if len(types) == 1:
                self.enum_types[enum] = list(types)[0]
            else:
                del self.enum_types[enum]

        for t in self.types:
            l = self.types[t]
            if len(l) == 1:
                continue

            # Allow simple types and enums to alias each other after renaming.
            # This is done because we want strong typing even for simple types.
            # If the types were not merged together, then a cast would be
            # necessary to convert from eg. AtomEnum to AtomSimple.
            assert len(l) == 2
            if isinstance(l[0], self.xcbgen.xtypes.Enum):
                enum = l[0]
                simple = l[1]
            elif isinstance(l[1], self.xcbgen.xtypes.Enum):
                enum = l[1]
                simple = l[0]
            assert simple.is_simple
            assert enum and simple
            self.replace_with_enum.add(t)
            self.enum_types[enum.name] = simple.name

        for node in self.module.namespace.root:
            if 'name' in node.attrib:
                key = (node.tag, node.attrib['name'])
                assert key not in self.module_names
                self.module_names[key] = node

        # The order of types in xcbproto's xml files are inconsistent, so sort
        # them in the order {type aliases, enums, xidunions, structs,
        # requests/replies}.
        def type_order_priority(module_type):
            name, item = module_type
            if item.is_simple:
                return 2 if self.get_xidunion_element(name) else 0
            if isinstance(item, self.xcbgen.xtypes.Enum):
                return 1
            if isinstance(item, self.xcbgen.xtypes.Request):
                return 4
            return 3

        def cmp(type1, type2):
            return type_order_priority(type1) - type_order_priority(type2)

        # sort() is guaranteed to be stable.
        self.module.all.sort(key=functools.cmp_to_key(cmp))

    def gen_header(self):
        self.file = self.header_file
        include_guard = self.header_file.name.replace('/', '_').replace(
            '.', '_').upper() + '_'
        self.write('#ifndef ' + include_guard)
        self.write('#define ' + include_guard)
        self.write()
        self.write('#include <array>')
        self.write('#include <cstddef>')
        self.write('#include <cstdint>')
        self.write('#include <cstring>')
        self.write('#include <vector>')
        self.write()
        self.write('#include "base/component_export.h"')
        self.write('#include "base/memory/ref_counted_memory.h"')
        self.write('#include "base/memory/scoped_refptr.h"')
        self.write('#include "base/optional.h"')
        self.write('#include "base/files/scoped_file.h"')
        self.write('#include "ui/gfx/x/xproto_types.h"')
        imports = set(self.module.direct_imports)
        if self.module.namespace.is_ext:
            imports.add(('xproto', 'xproto'))
        for direct_import in sorted(list(imports)):
            self.write('#include "%s.h"' % direct_import[-1])
        self.write()
        self.write('namespace x11 {')
        self.write()
        self.write('class Connection;')
        self.write()

        self.namespace = ['x11']
        if not self.module.namespace.is_ext:
            for (name, item) in self.module.all:
                self.declare_type(item, name)

        name = self.class_name
        with Indent(self, 'class COMPONENT_EXPORT(X11) %s {' % name, '};'):
            self.namespace = ['x11', self.class_name]
            self.write('public:')
            if self.module.namespace.is_ext:
                self.write('static constexpr unsigned major_version = %s;' %
                           self.module.namespace.major_version)
                self.write('static constexpr unsigned minor_version = %s;' %
                           self.module.namespace.minor_version)
                self.write()
                self.write(name + '(Connection* connection,')
                self.write('    const x11::QueryExtensionReply& info);')
                self.write()
                with Indent(self, 'uint8_t present() const {', '}'):
                    self.write('return info_.present;')
                with Indent(self, 'uint8_t major_opcode() const {', '}'):
                    self.write('return info_.major_opcode;')
                with Indent(self, 'uint8_t first_event() const {', '}'):
                    self.write('return info_.first_event;')
                with Indent(self, 'uint8_t first_error() const {', '}'):
                    self.write('return info_.first_error;')
            else:
                self.write('explicit %s(Connection* connection);' % name)
            self.write()
            self.write(
                'Connection* connection() const { return connection_; }')
            self.write()
            for (name, item) in self.module.all:
                if self.module.namespace.is_ext:
                    self.declare_type(item, name)
                elif isinstance(item, self.xcbgen.xtypes.Request):
                    self.declare_request(item)
            self.write('private:')
            self.write('x11::Connection* const connection_;')
            if self.module.namespace.is_ext:
                self.write('x11::QueryExtensionReply info_{};')

        self.write()
        self.write('}  // namespace x11')
        self.write()
        self.namespace = []

        def binop(op, name):
            self.write('inline constexpr %s operator%s(' % (name, op))
            with Indent(self, '    {0} l, {0} r)'.format(name) + ' {', '}'):
                self.write('using T = std::underlying_type_t<%s>;' % name)
                self.write('return static_cast<%s>(' % name)
                self.write('    static_cast<T>(l) %s static_cast<T>(r));' % op)
            self.write()

        for enum, name in self.bitenums:
            name = self.qualtype(enum, name)
            binop('|', name)
            binop('&', name)

        self.write()
        self.write('#endif  // ' + include_guard)

    def gen_source(self):
        self.file = self.source_file
        self.write('#include "%s.h"' % self.module.namespace.header)
        self.write()
        self.write('#include <xcb/xcb.h>')
        self.write('#include <xcb/xcbext.h>')
        self.write()
        self.write('#include "base/logging.h"')
        self.write('#include "base/posix/eintr_wrapper.h"')
        self.write('#include "ui/gfx/x/xproto_internal.h"')
        self.write()
        self.write('namespace x11 {')
        self.write()
        ctor = '%s::%s' % (self.class_name, self.class_name)
        if self.module.namespace.is_ext:
            self.write(ctor + '(x11::Connection* connection,')
            self.write('    const x11::QueryExtensionReply& info)')
            self.write('    : connection_(connection), info_(info) {}')
        else:
            self.write(ctor +
                       '(Connection* connection) : connection_(connection) {}')
        self.write()
        for (name, item) in self.module.all:
            self.define_type(item, name)
        self.write('}  // namespace x11')

    def parse(self):
        self.module = self.xcbgen.state.Module(self.xml_filename, None)
        self.module.register()
        self.module.resolve()

    def generate(self):
        self.gen_header()
        self.gen_source()


class GenExtensionManager(FileWriter):
    def __init__(self, gen_dir, genprotos):
        FileWriter.__init__(self)

        self.gen_dir = gen_dir
        self.genprotos = genprotos
        self.extensions = [
            proto for proto in genprotos if proto.module.namespace.is_ext
        ]

    def gen_header(self):
        self.file = open(os.path.join(self.gen_dir, 'extension_manager.h'),
                         'w')
        self.write('#ifndef UI_GFX_X_EXTENSION_MANAGER_H_')
        self.write('#define UI_GFX_X_EXTENSION_MANAGER_H_')
        self.write()
        self.write('#include <memory>')
        self.write()
        self.write('#include "base/component_export.h"')
        self.write()
        self.write('namespace x11 {')
        self.write()
        self.write('class Connection;')
        self.write()
        for genproto in self.genprotos:
            self.write('class %s;' % genproto.class_name)
        self.write()
        with Indent(self, 'class COMPONENT_EXPORT(X11) ExtensionManager {',
                    '};'):
            self.write('public:')
            self.write('ExtensionManager();')
            self.write('~ExtensionManager();')
            self.write()
            for extension in self.extensions:
                name = extension.proto
                self.write('%s& %s() { return *%s_; }' %
                           (extension.class_name, name, name))
            self.write()
            self.write('protected:')
            self.write('void Init(Connection* conn);')
            self.write()
            self.write('private:')
            for extension in self.extensions:
                self.write('std::unique_ptr<%s> %s_;' %
                           (extension.class_name, extension.proto))
        self.write()
        self.write('}  // namespace x11')
        self.write()
        self.write('#endif  // UI_GFX_X_EXTENSION_MANAGER_H_')

    def gen_source(self):
        self.file = open(os.path.join(self.gen_dir, 'extension_manager.cc'),
                         'w')
        self.write('#include "ui/gfx/x/extension_manager.h"')
        self.write()
        self.write('#include "ui/gfx/x/connection.h"')
        self.write('#include "ui/gfx/x/xproto_internal.h"')
        for genproto in self.genprotos:
            self.write('#include "ui/gfx/x/%s.h"' % genproto.proto)
        self.write()
        self.write('namespace x11 {')
        self.write()
        init = 'void ExtensionManager::Init'
        with Indent(self, init + '(Connection* conn) {', '}'):
            for extension in self.extensions:
                self.write(
                    'auto %s_future = conn->QueryExtension({"%s"});' %
                    (extension.proto, extension.module.namespace.ext_xname))
            self.write()
            for extension in self.extensions:
                name = extension.proto
                self.write(
                    '%s_ = MakeExtension<%s>(conn, std::move(%s_future));' %
                    (name, extension.class_name, name))
        self.write()
        self.write('ExtensionManager::ExtensionManager() = default;')
        self.write('ExtensionManager::~ExtensionManager() = default;')
        self.write()
        self.write('}  // namespace x11')


class GenReadEvent(FileWriter):
    def __init__(self, gen_dir, genprotos):
        FileWriter.__init__(self)

        self.gen_dir = gen_dir
        self.genprotos = genprotos

        self.events = []
        for proto in self.genprotos:
            for name, item in proto.module.all:
                if item.is_event:
                    self.events.append((name, item, proto))

    def event_condition(self, event, typename, proto):
        ext = 'conn->%s()' % proto.proto

        conds = []
        if not proto.module.namespace.is_ext:
            # Core protocol event
            opcode = 'evtype'
        elif event.is_ge_event:
            # GenericEvent extension event
            conds.extend([
                'evtype == GeGenericEvent::opcode',
                '%s.present()' % ext,
                'ge->extension == %s.major_opcode()' % ext,
            ])
            opcode = 'ge->event_type'
        else:
            # Extension event
            opcode = 'evtype - %s.first_event()' % ext
            conds.append('%s.present()' % ext)

        if len(event.opcodes) == 1:
            conds.append('%s == %s::opcode' % (opcode, typename))
        else:
            conds.append('(%s)' % ' || '.join([
                '%s == %s::%s' % (opcode, typename, opname)
                for opname in event.enum_opcodes.keys()
            ]))

        return ' && '.join(conds), opcode

    def gen_event(self, name, event, proto):
        # We can't ever have a plain generic event.  It must be a concrete
        # event provided by an extension.
        if name == ('xcb', 'GeGeneric'):
            return

        name = [adjust_type_name(part) for part in name[1:]]
        typename = '::'.join(name) + 'Event'

        cond, opcode = self.event_condition(event, typename, proto)
        with Indent(self, 'if (%s) {' % cond, '}'):
            self.write('event->type_id_ = %d;' % event.type_id)
            with Indent(self, 'event->deleter_ = [](void* event) {', '};'):
                self.write('delete reinterpret_cast<%s*>(event);' % typename)
            self.write('auto* event_ = new %s;' % typename)
            self.write('ReadEvent(event_, buffer);')
            if len(event.opcodes) > 1:
                self.write('{0} = static_cast<decltype({0})>({1});'.format(
                    'event_->opcode', opcode))
            self.write('event_->send_event = send_event;')
            self.write('event->event_ = event_;')
            self.write('event->window_ = event_->GetWindow();')
            self.write('return;')
        self.write()

    def gen_source(self):
        self.file = open(os.path.join(self.gen_dir, 'read_event.cc'), 'w')
        self.write('#include "ui/gfx/x/event.h"')
        self.write()
        self.write('#include "ui/gfx/x/connection.h"')
        for genproto in self.genprotos:
            self.write('#include "ui/gfx/x/%s.h"' % genproto.proto)
        self.write()
        self.write('namespace x11 {')
        self.write()
        self.write('void ReadEvent(')
        args = 'Event* event, Connection* conn, ReadBuffer* buffer'
        with Indent(self, '    %s) {' % args, '}'):
            self.write('auto* buf = buffer->data->data();')
            cast = 'auto* %s = reinterpret_cast<const %s*>(buf);'
            self.write(cast % ('ev', 'xcb_generic_event_t'))
            self.write(cast % ('ge', 'xcb_ge_generic_event_t'))
            self.write('auto evtype = ev->response_type & ~kSendEventMask;')
            self.write('bool send_event = ev->response_type & kSendEventMask;')
            self.write()
            for name, event, proto in self.events:
                self.gen_event(name, event, proto)
            self.write('NOTREACHED();')
        self.write()
        self.write('}  // namespace x11')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('xcbproto_dir', type=str)
    parser.add_argument('gen_dir', type=str)
    parser.add_argument('protos', type=str, nargs='*')
    args = parser.parse_args()

    sys.path.insert(1, args.xcbproto_dir)
    import xcbgen.xtypes
    import xcbgen.state

    all_types = {}
    proto_src_dir = os.path.join(args.xcbproto_dir, 'src')
    genprotos = [
        GenXproto(proto, proto_src_dir, args.gen_dir, xcbgen, all_types)
        for proto in args.protos
    ]
    for genproto in genprotos:
        genproto.parse()
    for genproto in genprotos:
        genproto.resolve()

    # Give each event a unique type ID.  This is used by x11::Event to
    # implement downcasting for events.
    type_id = 1
    for proto in genprotos:
        for _, item in proto.module.all:
            if item.is_event:
                item.type_id = type_id
                type_id += 1

    for genproto in genprotos:
        genproto.generate()

    gen_extension_manager = GenExtensionManager(args.gen_dir, genprotos)
    gen_extension_manager.gen_header()
    gen_extension_manager.gen_source()

    gen_read_event = GenReadEvent(args.gen_dir, genprotos)
    gen_read_event.gen_source()

    return 0


if __name__ == '__main__':
    sys.exit(main())
