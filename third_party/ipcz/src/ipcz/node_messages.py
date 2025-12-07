# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Creates messages.cc from node_messages_generator.h for ipcz"""

import argparse
import glob
import os
import re
import sys

sys.path.append(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', '..',
                 'third_party'))
from jinja2 import Environment, FileSystemLoader

### Representations


class Field(object):
    """ Represents a field in an ipcz wire message, which will be mapped to
    a name in a Foo_Params.Vn struct. Some complex fields are really offsets
    with further processing. See *.tmpl for a full discussion.
    """

    def __init__(self, type, name):
        """
        Args:
            type: The C++ type used to overlay the wire data.
            name: The field name used when accessing the field in C++.
        """
        self.type = type
        self.name = name
        # Headers that are needed for `type`, set by subclasses.
        self.headers = set()

    def is_offset(self):
        """
        Returns True if the wire representation of the field uses an offset to
        point to further data (e.g. Arrays). False if the field is inline.
        """
        return False

    def param_type(self):
        """
        Returns the ipcz parameter type, which determines the serialization
        format of the field:
          kData - inline data with no validation (e.g. uint64_t) (Default).
          kDataArray - offset to an ArrayHeader containing sized elements.
          kDriverObject - offset to in the message's driver object array.
          kDriverObjectArray - offset to an ArrayHeader of driver objects.
          kEnum - inline data with range validation.
        """
        return 'kData'

    def get_headers(self):
        """
        Returns a set of headers that must be included when a field of this type
        is present in a message. May be empty.
        """
        return self.headers


class FieldU8(Field):

    def __init__(self, name):
        super().__init__('uint8_t', name)


class FieldU16(Field):

    def __init__(self, name):
        super().__init__('uint16_t', name)


class FieldU32(Field):

    def __init__(self, name):
        super().__init__('uint32_t', name)


class FieldU64(Field):

    def __init__(self, name):
        super().__init__('uint64_t', name)


class NodeName(Field):

    def __init__(self, name):
        super().__init__('NodeName', name)
        self.headers.add('ipcz/node_name.h')


class Enum(Field):

    def __init__(self, type, name):
        super().__init__(type, name)

    def param_type(self):
        return 'kEnum'


class LinkSide(Enum):

    def __init__(self, name):
        super().__init__('LinkSide', name)
        self.headers.add('ipcz/link_side.h')


class NodeType(Enum):

    def __init__(self, name):
        super().__init__('NodeType', name)
        self.headers.add('ipcz/node_type.h')


class HandleType(Enum):

    def __init__(self, name):
        super().__init__('HandleType', name)
        self.headers.add('ipcz/handle_type.h')


class TestEnum8(Enum):

    def __init__(self, name):
        super().__init__('TestEnum8', name)
        self.headers.add('ipcz/message_test_types.h')


class TestEnum32(Enum):

    def __init__(self, name):
        super().__init__('TestEnum32', name)
        self.headers.add('ipcz/message_test_types.h')


class Id(Field):

    def __init__(self, type, name):
        super().__init__(type, name)


class BufferId(Id):

    def __init__(self, name):
        super().__init__('BufferId', name)
        self.headers.add('ipcz/buffer_id.h')


class SublinkId(Id):

    def __init__(self, name):
        super().__init__('SublinkId', name)
        self.headers.add('ipcz/sublink_id.h')


class SequenceNumber(Field):

    def __init__(self, name):
        super().__init__('SequenceNumber', name)
        self.headers.add('ipcz/sequence_number.h')


class DriverObject(Field):

    def __init__(self, name):
        super().__init__('DriverObject', name)
        self.headers.add('ipcz/driver_object.h')

    def is_offset(self):
        return True

    def param_type(self):
        return 'kDriverObject'


class DriverObjectArrayData(Field):

    def __init__(self, name):
        super().__init__('internal::DriverObjectArrayData', name)
        self.headers.add('ipcz/driver_object.h')

    def param_type(self):
        return 'kDriverObjectArray'


class FeaturesBitfield(Field):

    def __init__(self, name):
        super().__init__('Features::Bitfield', name)
        self.headers.add('ipcz/features.h')


### Aggregates


class FragmentDescriptor(Field):

    def __init__(self, name):
        super().__init__('FragmentDescriptor', name)
        self.headers.add('ipcz/fragment_descriptor.h')


class RouterDescriptor(Field):

    def __init__(self, name):
        super().__init__('RouterDescriptor', name)
        self.headers.add('ipcz/router_descriptor.h')


class Array(Field):

    def __init__(self, name, contains):
        super().__init__(f'Array[{contains.type}]', name)
        self.contains = contains

    def is_offset(self):
        return True

    def param_type(self):
        return 'kDataArray'

    def get_headers(self):
        return self.contains.get_headers()


def make_field(type, name) -> Field:
    if type == 'NodeName':
        return NodeName(name)
    elif type == 'BufferId':
        return BufferId(name)
    elif type == 'SublinkId':
        return SublinkId(name)
    elif type == 'SequenceNumber':
        return SequenceNumber(name)
    elif type == 'FragmentDescriptor':
        return FragmentDescriptor(name)
    elif type == 'RouterDescriptor':
        return RouterDescriptor(name)
    elif type == 'uint8_t':
        return FieldU8(name)
    elif type == 'uint16_t':
        return FieldU16(name)
    elif type == 'uint32_t':
        return FieldU32(name)
    elif type == 'uint64_t':
        return FieldU64(name)
    elif type == 'Features::Bitfield':
        return FeaturesBitfield(name)
    elif type == 'HandleType':
        # Note: Enum used in arrays, will be supported as enum eventually.
        return HandleType(name)
    raise Exception(f'type {type} not supported as Field')


def make_enum(type, name) -> Enum:
    if type == 'LinkSide':
        return LinkSide(name)
    elif type == 'NodeType':
        return NodeType(name)
    elif type == 'TestEnum8':
        return TestEnum8(name)
    elif type == 'TestEnum32':
        return TestEnum32(name)
    raise Exception(f'type {type} not supported as Enum')


def make_array(type, name) -> Array:
    contained = make_field(type, name)
    return Array(name, contained)


class Message(object):

    def __init__(self, name, msgid):
        self.msgid = msgid
        self.message = name
        # List of fields
        self.fields = []
        # List of versions [0,1,...]
        self.versions = []
        # Offset of last field in version[idx].
        self.offsets = []

    def v_fields(self, version):
        version = int(version)
        # Fields for version.
        if version < 0 or version > len(self.versions):
            raise Exception(f'Invalid version: {version}')
        prev_offset = 0
        if version > 0:
            prev_offset = self.offsets[version - 1]
        return self.fields[prev_offset:self.offsets[version]]

    def begin_v(self, version):
        if len(self.versions) != int(version):
            raise Exception(
                f'Version {version} already started for {self.message}')

    def end_v(self, version):
        if len(self.versions) != int(version):
            raise Exception(f'Version {version} not active for {self.message}')
        self.versions.append(version)
        self.offsets.append(len(self.fields))

    def add_field(self, type, name):
        self.fields.append(make_field(type, name))

    def add_enum(self, type, name):
        self.fields.append(make_enum(type, name))

    def add_driver_object(self, name):
        self.fields.append(DriverObject(name))

    def add_array(self, type, name):
        self.fields.append(make_array(type, name))

    def add_driver_object_array(self, name):
        self.fields.append(DriverObjectArrayData(name))


### Processing & Template Filling
def gather_includes(messages):
    includes = set()
    for m in messages:
        for f in m.fields:
            for include in f.get_headers():
                includes.add(include)
    return sorted(includes)


def read_messages_file(filename):
    with open(filename) as f:
        return f.readlines()


def process_messages_file(lines):
    lineno = 0
    messages = []
    cur_message = None
    for line in lines:
        lineno = lineno + 1
        # IPCZ_MSG_BEGIN(ConnectFromBrokerToNonBroker, IPCZ_MSG_ID(0))
        m = re.match(
            r'IPCZ_MSG_BEGIN\((?P<message>\w+),\s+IPCZ_MSG_ID\((?P<msgid>\d+)\)\)',
            line)
        if m:
            if cur_message:
                raise Exception(
                    f'Unexpected new message on line {lineno}: {line}')
            cur_message = Message(m.group('message'), m.group('msgid'))
            continue
        # IPCZ_MSG_BEGIN_VERSION(0)
        m = re.match(r'\s+IPCZ_MSG_BEGIN_VERSION\((?P<version>\d+)\)', line)
        if m:
            cur_message.begin_v(m.group('version'))
            continue

        # IPCZ_MSG_PARAM(uint32_t, num_initial_portals)
        m = re.match(r'\s+IPCZ_MSG_PARAM\((?P<type>\w+),\s*(?P<name>\w+)\)',
                     line)
        if m:
            cur_message.add_field(m.group('type'), m.group('name'))
            continue

        # IPCZ_MSG_PARAM(uint32_t, num_initial_portals)
        m = re.match(r'\s+IPCZ_MSG_PARAM_ENUM\((?P<type>\w+),\s*(?P<name>\w+)\)',
                     line)
        if m:
            cur_message.add_enum(m.group('type'), m.group('name'))
            continue

        # IPCZ_MSG_PARAM_DRIVER_OBJECT(driver_object)
        m = re.match(r'\s+IPCZ_MSG_PARAM_DRIVER_OBJECT\((?P<name>\w+)\)', line)
        if m:
            cur_message.add_driver_object(m.group('name'))
            continue

        # IPCZ_MSG_PARAM_ARRAY(uint8_t, parcel_data)
        m = re.match(
            r'\s+IPCZ_MSG_PARAM_ARRAY\((?P<type>\w+(::\w+)?),\s*(?P<name>\w+)\)',
            line)
        if m:
            cur_message.add_array(m.group('type'), m.group('name'))
            continue

        # IPCZ_MSG_PARAM_DRIVER_OBJECT_ARRAY(driver_objects)
        m = re.match(r'\s+IPCZ_MSG_PARAM_DRIVER_OBJECT_ARRAY\((?P<name>\w+)\)',
                     line)
        if m:
            cur_message.add_driver_object_array(m.group('name'))
            continue

        # IPCZ_MSG_END_VERSION(0)
        m = re.match(r'\s+IPCZ_MSG_END_VERSION\((?P<version>\d+)\)', line)
        if m:
            cur_message.end_v(m.group('version'))
            continue
        # IPCZ_MSG_END()
        if 'IPCZ_MSG_END()' in line:
            messages.append(cur_message)
            cur_message = None
            continue

        m = re.match(r'^IPCZ_MSG_BEGIN_INTERFACE\((?P<interface>\w+)\)', line)
        if m:
            interface = m.group('interface')
            continue

        # Skip comments and blank lines & some other metadata we don't need.
        m = re.match(r'\s*(//.*)?$', line)
        if m:
            continue
        if 'IPCZ_MSG_END_INTERFACE' in line:
            continue

        # Explode for everything else.
        raise Exception(f'Unexpected Line {lineno}: {line}')
    return messages, interface


def header_guard(prefix):
    prefix = prefix.upper()
    return f'IPCZ_SRC_IPCZ_{prefix}_H_'


def namespace(interface):
    if interface == 'Node':
        return 'ipcz::msg'
    elif interface == 'Test':
        return 'ipcz::test::msg'
    raise Exception(f'{interface} not supported')


def compare_lines(file, content, exceptions=False):
    # Returns True if the file and content are the same, or False otherwise.
    # If exceptions=True then raises an exception when a difference is found.
    content_lines = content.splitlines()
    with open(file, 'r', encoding='utf-8') as f:
        file_lines = [l.rstrip('\n') for l in f.readlines()]
        if len(file_lines) != len(content_lines):
            if exceptions:
                raise Exception(f'Different lengths file:{len(file_lines)} '
                                f'generated:{len(content_lines)}')
            return False
        for i in range(len(file_lines)):
            if content_lines[i] != file_lines[i]:
                if exceptions:
                    raise Exception(f'{file}:{i} `{file_lines[i]}`\n'
                                    'does not match generated content\n'
                                    f'`{content_lines[i]}`')
                return False
    return True


def process(messages_file, template_prefix, output, check):
    (m, interface) = process_messages_file(read_messages_file(messages_file))

    if not interface in ['Node', 'Test']:
        raise Exception(f'{interface} not a supported interface type')

    template_dir = os.path.dirname(__file__)
    env = Environment(loader=FileSystemLoader(template_dir),
                      lstrip_blocks=False,
                      trim_blocks=True,
                      newline_sequence='\n')
    # These mainly key off 'interface'
    env.filters['header_guard'] = header_guard
    env.filters['namespace'] = namespace

    args = {
        'messages': m,
        'interface': interface,
        'prefix': output,
        'messages_file': os.path.basename(messages_file),
        'includes': gather_includes(m),
    }

    t = env.get_template(f'{template_prefix}.h.tmpl')
    header_content = t.render(args)
    t = env.get_template(f'{template_prefix}.cc.tmpl')
    cc_content = t.render(args)

    output_dir = os.path.dirname(messages_file)
    header_file = os.path.join(output_dir, f'{output}.h')
    cc_file = os.path.join(output_dir, f'{output}.cc')

    if check:
        compare_lines(header_file, header_content, exceptions=True)
        compare_lines(cc_file, cc_content, exceptions=True)
    else:
        if not compare_lines(header_file, header_content):
            print(f'Writing {header_file}')
            with open(header_file, 'w', encoding="utf-8", newline='') as f:
                f.write(header_content)
        if not compare_lines(cc_file, cc_content):
            print(f'Writing {cc_file}')
            with open(cc_file, 'w', encoding="utf-8", newline='') as f:
                f.write(cc_content)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=(
        "Parses ipcz message definition files (*_messages_generator.h) and\n"
        "generates .cc and .h files for use by the IPCZ build. Determines the\n"
        "interface type from the provided IPCZ_MSG_BEGIN_INTERFACE(name) and\n"
        "supports both Node and Test.\n"
        "\n"
        r"  node_messages.py --dir=.\third_party\ipcz\src\ipcz"
        "\n\n"
        "Writes node_messages.h and node_messages.cc in the same directory\n"
        "`messages`. Files are only touched if the contents have changed.\n"
        "\n"
        "Can be run in a check-only mode which does not write the output\n"
        "files, but instead validates that the checked-in file matches the\n"
        "expected output.\n"
        "\n"
        r"  node_messages.py --dir=.\third_party\ipcz\src\ipcz --check"),
                                     formatter_class=argparse.
                                     RawDescriptionHelpFormatter)
    parser.add_argument('--template',
                        default='node_messages',
                        help='template prefix')
    parser.add_argument(
        '--dir',
        default='.',
        help="directory containing *_messages_generator.h files.")
    parser.add_argument(
        '--check',
        action='store_true',
        help='do not write files, instead compare with expected output')
    args = parser.parse_args()

    if not args.dir:
        print("Supply a directory with --dir=. arg.")
        exit(1)
    generators = glob.glob(os.path.join(args.dir, '*_messages_generator.h'))

    for generator in generators:
        m = re.match(r'^(?P<prefix>\w+_messages)_generator.h',
                     os.path.basename(generator))
        if m:
            process(generator, args.template, m.group('prefix'), args.check)
        else:
            raise Exception("Cannot find prefix in {generator}")
