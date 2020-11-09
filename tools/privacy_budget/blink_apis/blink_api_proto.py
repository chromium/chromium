# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

from v8_utilities import capitalize
from google.protobuf.text_format import MessageToString
import web_idl
import blink_apis_pb2 as pb


class BlinkApiProto(object):
    """BlinkApiProto converts a WebIdlDatabase to
    a identifiability.blink_apis.Snapshot proto defined in
    proto/blink_apis.proto"""

    def __init__(self, web_idl_pickle, proto_out_file, chromium_revision):
        self.web_idl_database = web_idl.Database.read_from_file(web_idl_pickle)
        self.proto_out_file = proto_out_file
        self.snapshot = pb.Snapshot()
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
                                      function)

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

    def _GetUseCounter(self, parent, measure, measure_as):
        if measure_as:
            return measure_as.value
        if measure:
            use_counter = capitalize(parent.identifier)
            if not isinstance(parent, web_idl.Interface):
                use_counter = (capitalize(parent.owner.identifier) + '_' +
                               use_counter)
            return use_counter
        return None

    def _ConvertIdlType(self, dest, idl_type):
        assert isinstance(idl_type, web_idl.IdlType)

        dest.idl_type_string = idl_type.type_name_without_extended_attributes
        self._ConvertExtendedAttributes(dest.extended_attributes, idl_type)

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

    def _ConvertExtendedAttributes(self, dest, parent):
        attr = parent.extended_attributes
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
                    e.member = parent.identifier

        setattr(dest, 'global', ('Global' in attr))
        dest.same_object = ('SameObject' in attr)
        dest.secure_context = ('SecureContext' in attr)
        dest.high_entropy = self._GetHighEntropyType(attr.get('HighEntropy'))
        if 'Measure' in attr or 'MeasureAs' in attr:
            dest.use_counter = self._GetUseCounter(parent, attr.get('Measure'),
                                                   attr.get('MeasureAs'))
        if 'RuntimeEnabled' in attr:
            dest.runtime_enabled = attr.value_of('RuntimeEnabled')
        if 'ImplementedAs' in attr:
            dest.implemented_as = attr.value_of('ImplementedAs')

    def _ConvertIdlAttribute(self, dest, attr):
        dest.name = attr.identifier
        dest.is_static = attr.is_static
        dest.is_readonly = attr.is_readonly
        self._ConvertExtendedAttributes(dest.extended_attributes, attr)
        self._ConvertIdlType(dest.idl_type, attr.idl_type)

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

    def _ConvertIdlOperation(self, dest, op):
        dest.name = op.identifier
        dest.static = op.is_static
        dest.special_op_type = self._GetSpecialOperationType(op)
        self._ConvertIdlType(dest.return_type, op.return_type)
        for arg in op.arguments:
            self._ConvertIdlType(dest.arguments.add(), arg.idl_type)

    def _ConvertIdlEnumeration(self, dest, enumer):
        dest.name = enumer.identifier
        dest.values[:] = enumer.values

    def _ConvertIdlConstant(self, dest, constant):
        dest.name = constant.identifier
        dest.value = constant.value.literal
        self._ConvertExtendedAttributes(dest.extended_attributes, constant)
        self._ConvertIdlType(dest.idl_type, constant.idl_type)

    def _ConvertIdlInterfaceLike(self, dest, interface):
        dest.name = interface.identifier
        if hasattr(interface, 'inherited') and interface.inherited:
            dest.inherits_from = interface.inherited.identifier
        self._ConvertExtendedAttributes(dest.extended_attributes, interface)
        for attr in interface.attributes:
            self._ConvertIdlAttribute(dest.attributes.add(), attr)
        for op in interface.operations:
            self._ConvertIdlOperation(dest.operations.add(), op)
        for constant in interface.constants:
            self._ConvertIdlConstant(dest.constants.add(), constant)

    def _ConvertDictionaryMember(self, dest, member):
        assert isinstance(member, web_idl.DictionaryMember)
        dest.name = member.identifier
        self._ConvertExtendedAttributes(dest.extended_attributes, member)
        self._ConvertIdlType(dest.idl_type, member.idl_type)

    def _ConvertIdlDictionary(self, dest, dictionary):
        assert isinstance(dictionary, web_idl.Dictionary)
        dest.name = dictionary.identifier
        if dictionary.inherited:
            dest.inherits_from = dictionary.inherited.identifier
        for member in dictionary.members:
            self._ConvertDictionaryMember(dest.members.add(), member)

    def _ConvertIdlTypedef(self, dest, typedef):
        assert isinstance(typedef, web_idl.Typedef)
        dest.name = typedef.identifier
        self._ConvertIdlType(dest.idl_type, typedef.idl_type)
