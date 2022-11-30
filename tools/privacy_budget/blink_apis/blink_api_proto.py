# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

from blinkbuild import name_style_converter
from google.protobuf.text_format import MessageToString
import web_idl
import blink_apis_pb2 as pb


class BlinkApiProto(object):
    """BlinkApiProto converts a WebIdlDatabase to
    a identifiability.blink_apis.Snapshot proto defined in
    proto/blink_apis.proto"""

    def __init__(self, web_idl_pickle, proto_out_file, chromium_revision,
                 web_features):
        self.web_idl_database = web_idl.Database.read_from_file(web_idl_pickle)
        self.proto_out_file = proto_out_file
        self.snapshot = pb.Snapshot()
        self.web_features = web_features
        if chromium_revision:
            self.snapshot.chromium_revision = chromium_revision

    def Parse(self):
        """The main entry point, which synchronously does all the work."""
        for interface in self.web_idl_database.interfaces:
            self._ConvertIdlInterfaceLike(self.snapshot.interfaces.add(),
                                          interface)

        for dictionary in self.web_idl_database.dictionaries:
            self._ConvertIdlDictionary(self.snapshot.dictionaries.add(),
                                       dictionary)

        for enumeration in self.web_idl_database.enumerations:
            self._ConvertIdlEnumeration(self.snapshot.enumerations.add(),
                                        enumeration)

        for namespace in self.web_idl_database.namespaces:
            self._ConvertIdlInterfaceLike(self.snapshot.namespaces.add(),
                                          namespace)

        for function in self.web_idl_database.callback_functions:
            self._ConvertIdlOperation(self.snapshot.callback_functions.add(),
                                      function, None)

        for interface in self.web_idl_database.callback_interfaces:
            self._ConvertIdlInterfaceLike(
                self.snapshot.callback_interfaces.add(), interface)

        for typedef in self.web_idl_database.typedefs:
            self._ConvertIdlTypedef(self.snapshot.typedefs.add(), typedef)

    def WriteTo(self, where):
        with open(where, 'w') as f:
            f.write(MessageToString(self.snapshot, as_utf8=True))

    def _GetHighEntropyType(self, e):
        if e is None:
            return pb.HIGH_ENTROPY_BENIGN
        val = e.values
        if not val:
            return pb.HIGH_ENTROPY_UNCLASSIFIED
        if val[0] == 'Direct':
            return pb.HIGH_ENTROPY_DIRECT
        assert False, "Unknown HighEntropy value {}".format(val)

    def _GetUseCounter(self, member, parent, ext_attrs):
        assert isinstance(ext_attrs, web_idl.ExtendedAttributes)
        assert parent is None or hasattr(parent, 'identifier')

        if 'MeasureAs' in ext_attrs:
            return ext_attrs.value_of('MeasureAs')

        if 'Measure' not in ext_attrs:
            return None

        if parent is not None:
            prefix = '{}_{}'.format(
                name_style_converter.NameStyleConverter(
                    parent.identifier).to_upper_camel_case(),
                name_style_converter.NameStyleConverter(
                    member.identifier).to_upper_camel_case())
        else:
            prefix = name_style_converter.NameStyleConverter(
                member.identifier).to_upper_camel_case()

        suffix = ""
        if isinstance(member, web_idl.FunctionLike):
            if len(member.arguments) == 0 and member.is_getter:
                suffix = "AttributeGetter"
            elif len(member.arguments) == 1 and member.is_setter:
                suffix = "AttributeSetter"
            else:
                suffix = "Method"
        elif isinstance(member, web_idl.Attribute):
            suffix = "AttributeGetter"
        else:
            assert False, repr(member)

        return "V8" + prefix + "_" + suffix

    def _ConvertIdlType(self, dest, idl_type):
        assert isinstance(idl_type, web_idl.IdlType)

        dest.idl_type_string = idl_type.type_name_without_extended_attributes
        self._ConvertExtendedAttributes(dest.extended_attributes, idl_type,
                                        None)

        # Only look at named definitions. Simple, primitive types don't define
        # named identifiers.
        depends_on = set()
        depends_on.add(idl_type.type_name_without_extended_attributes)

        def capture_inner_type(elem):
            # All DefinitionType substrates have an 'identifier' field. This
            # excludes primitive types like Bool and simple types like Promise.
            if hasattr(elem, 'identifier'):
                depends_on.add(elem.type_name_without_extended_attributes)

        idl_type.apply_to_all_composing_elements(capture_inner_type)
        depends_on.remove(idl_type.type_name_without_extended_attributes)
        dest.depends_on[:] = list(depends_on)

    def _ConvertExtendedAttributes(self, dest, member, interface):
        attr = member.extended_attributes
        assert isinstance(attr, web_idl.ExtendedAttributes)
        dest.cross_origin_isolated = ('CrossOriginIsolated' in attr)
        if 'Exposed' in attr:
            exposed = attr.get('Exposed')
            if exposed.has_arguments:
                for (i, m) in exposed.arguments:
                    e = dest.exposed.add()
                    e.interface = i
                    e.member = m
            elif exposed.has_values:
                for v in exposed.values:
                    e = dest.exposed.add()
                    e.interface = v
                    e.member = member.identifier

        setattr(dest, 'global', ('Global' in attr))
        dest.same_object = ('SameObject' in attr)
        dest.secure_context = ('SecureContext' in attr)
        dest.high_entropy = self._GetHighEntropyType(attr.get('HighEntropy'))
        if 'Measure' in attr or 'MeasureAs' in attr:
            dest.use_counter = self._GetUseCounter(member, interface, attr)
            dest.use_counter_feature_value = self.web_features[
                dest.use_counter]
        if 'RuntimeEnabled' in attr:
            dest.runtime_enabled = attr.value_of('RuntimeEnabled')
        if 'ImplementedAs' in attr:
            dest.implemented_as = attr.value_of('ImplementedAs')

    def _ConvertIdlAttribute(self, dest, attr, interface):
        dest.name = attr.identifier
        dest.is_static = attr.is_static
        dest.is_readonly = attr.is_readonly
        self._ConvertExtendedAttributes(dest.extended_attributes, attr,
                                        interface)
        self._ConvertIdlType(dest.idl_type, attr.idl_type)
        self._ConvertSourceLocation(dest.source_location, attr.debug_info)

    def _GetSpecialOperationType(self, op):
        if not isinstance(op, web_idl.Operation):
            return pb.SPECIAL_OP_UNSPECIFIED
        if op.is_getter:
            return pb.SPECIAL_OP_GETTER
        if op.is_setter:
            return pb.SPECIAL_OP_SETTER
        if op.is_stringifier:
            return pb.SPECIAL_OP_STRINGIFIER
        return pb.SPECIAL_OP_UNSPECIFIED

    def _ConvertIdlOperation(self, dest, op, parent):
        dest.name = op.identifier
        dest.static = op.is_static
        dest.special_op_type = self._GetSpecialOperationType(op)
        self._ConvertIdlType(dest.return_type, op.return_type)
        self._ConvertSourceLocation(dest.source_location, op.debug_info)
        self._ConvertExtendedAttributes(dest.extended_attributes, op, parent)
        for arg in op.arguments:
            self._ConvertIdlType(dest.arguments.add(), arg.idl_type)

    def _ConvertIdlEnumeration(self, dest, enumer):
        dest.name = enumer.identifier
        dest.values[:] = enumer.values
        self._ConvertSourceLocation(dest.source_location, enumer.debug_info)

    def _ConvertIdlConstant(self, dest, constant, parent):
        dest.name = constant.identifier
        dest.value = constant.value.literal
        self._ConvertExtendedAttributes(dest.extended_attributes, constant,
                                        parent)
        self._ConvertIdlType(dest.idl_type, constant.idl_type)
        self._ConvertSourceLocation(dest.source_location, constant.debug_info)

    def _ConvertIdlInterfaceLike(self, dest, parent):
        dest.name = parent.identifier
        if hasattr(parent, 'inherited') and parent.inherited:
            dest.inherits_from = parent.inherited.identifier
        self._ConvertExtendedAttributes(dest.extended_attributes, parent, None)
        self._ConvertSourceLocation(dest.source_location, parent.debug_info)
        for attr in parent.attributes:
            self._ConvertIdlAttribute(dest.attributes.add(), attr, parent)
        for op in parent.operations:
            self._ConvertIdlOperation(dest.operations.add(), op, parent)
        for constant in parent.constants:
            self._ConvertIdlConstant(dest.constants.add(), constant, parent)

    def _ConvertDictionaryMember(self, dest, member, interface):
        assert isinstance(member, web_idl.DictionaryMember)
        dest.name = member.identifier
        self._ConvertExtendedAttributes(dest.extended_attributes, member,
                                        interface)
        self._ConvertIdlType(dest.idl_type, member.idl_type)
        self._ConvertSourceLocation(dest.source_location, member.debug_info)

    def _ConvertIdlDictionary(self, dest, dictionary):
        assert isinstance(dictionary, web_idl.Dictionary)
        dest.name = dictionary.identifier
        self._ConvertSourceLocation(dest.source_location,
                                    dictionary.debug_info)
        if dictionary.inherited:
            dest.inherits_from = dictionary.inherited.identifier
        for member in dictionary.members:
            self._ConvertDictionaryMember(dest.members.add(), member,
                                          dictionary)

    def _ConvertIdlTypedef(self, dest, typedef):
        assert isinstance(typedef, web_idl.Typedef)
        dest.name = typedef.identifier
        self._ConvertIdlType(dest.idl_type, typedef.idl_type)

    def _ConvertSourceLocation(self, dest, debug_info):
        source_file = None
        line_no = 0

        if not debug_info or not hasattr(debug_info, 'all_locations'):
            return

        for loc in list(debug_info.all_locations):
            if loc.filepath and loc.line_number:
                source_file = loc.filepath
                line_no = loc.line_number
                break
            if loc.filepath:
                source_file = loc.filepath

        if source_file:
            dest.filename = source_file

            dest.line = line_no
