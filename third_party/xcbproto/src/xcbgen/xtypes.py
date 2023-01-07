'''
This module contains the classes which represent XCB data types.
'''
import sys
from xcbgen.expr import Field, Expression
from xcbgen.align import Alignment, AlignmentLog

if sys.version_info[:2] >= (3, 3):
    from xml.etree.ElementTree import SubElement
else:
    from xml.etree.cElementTree import SubElement

import __main__

verbose_align_log = False
true_values = ['true', '1', 'yes']

class Type(object):
    '''
    Abstract base class for all XCB data types.
    Contains default fields, and some abstract methods.
    '''
    def __init__(self, name):
        '''
        Default structure initializer.  Sets up default fields.

        Public fields:
        name is a tuple of strings specifying the full type name.
        size is the size of the datatype in bytes, or None if variable-sized.
        nmemb is 1 for non-list types, None for variable-sized lists, otherwise number of elts.
        booleans for identifying subclasses, because I can't figure out isinstance().
        '''
        self.name = name
        self.size = None
        self.nmemb = None
        self.resolved = False

        # Screw isinstance().
        self.is_simple = False
        self.is_list = False
        self.is_expr = False
        self.is_container = False
        self.is_reply = False
        self.is_union = False
        self.is_pad = False
        self.is_eventstruct = False
        self.is_event = False
        self.is_switch = False
        self.is_case_or_bitcase = False
        self.is_bitcase = False
        self.is_case = False
        self.is_fd = False
        self.required_start_align = Alignment()

        # the biggest align value of an align-pad contained in this type
        self.max_align_pad = 1

    def resolve(self, module):
        '''
        Abstract method for resolving a type.
        This should make sure any referenced types are already declared.
        '''
        raise Exception('abstract resolve method not overridden!')

    def out(self, name):
        '''
        Abstract method for outputting code.
        These are declared in the language-specific modules, and
        there must be a dictionary containing them declared when this module is imported!
        '''
        raise Exception('abstract out method not overridden!')

    def fixed_size(self):
        '''
        Abstract method for determining if the data type is fixed-size.
        '''
        raise Exception('abstract fixed_size method not overridden!')

    def make_member_of(self, module, complex_type, field_type, field_name, visible, wire, auto, enum=None, is_fd=False):
        '''
        Default method for making a data type a member of a structure.
        Extend this if the data type needs to add an additional length field or something.

        module is the global module object.
        complex_type is the structure object.
        see Field for the meaning of the other parameters.
        '''
        new_field = Field(self, field_type, field_name, visible, wire, auto, enum, is_fd)

        # We dump the _placeholder_byte if any fields are added.
        for (idx, field) in enumerate(complex_type.fields):
            if field == _placeholder_byte:
                complex_type.fields[idx] = new_field
                return

        complex_type.fields.append(new_field)
        new_field.parent = complex_type

    def make_fd_of(self, module, complex_type, fd_name):
        '''
        Method for making a fd member of a structure.
        '''
        new_fd = Field(self, module.get_type_name('INT32'), fd_name, True, False, False, None, True)
        # We dump the _placeholder_byte if any fields are added.
        for (idx, field) in enumerate(complex_type.fields):
            if field == _placeholder_byte:
                complex_type.fields[idx] = new_fd
                return

        complex_type.fields.append(new_fd)


    def get_total_size(self):
        '''
        get the total size of this type if it is fixed-size, otherwise None
        '''
        if self.fixed_size():
            if self.nmemb is None:
                return self.size
            else:
                return self.size * self.nmemb
        else:
            return None

    def get_align_offset(self):
        if self.required_start_align is None:
            return 0
        else:
            return self.required_start_align.offset

    def is_acceptable_start_align(self, start_align, callstack, log):
        return self.get_alignment_after(start_align, callstack, log) is not None

    def get_alignment_after(self, start_align, callstack, log):
        '''
        get the alignment after this type based on the given start_align.
        the start_align is checked for compatibility with the
        internal start align. If it is not compatible, then None is returned
        '''
        if self.required_start_align is None or self.required_start_align.is_guaranteed_at(start_align):
            return self.unchecked_get_alignment_after(start_align, callstack, log)
        else:
            if log is not None:
                log.fail(start_align, "", self, callstack + [self],
                    "start_align is incompatible with required_start_align %s"
                    % (str(self.required_start_align)))
            return None

    def unchecked_get_alignment_after(self, start_align, callstack, log):
        '''
        Abstract method for geting the alignment after this type
        when the alignment at the start is given, and when this type
        has variable size.
        '''
        raise Exception('abstract unchecked_get_alignment_after method not overridden!')


    @staticmethod
    def type_name_to_str(type_name):
        if isinstance(type_name, str):
            #already a string
            return type_name
        else:
            return ".".join(type_name)


    def __str__(self):
        return type(self).__name__ + " \"" + Type.type_name_to_str(self.name) + "\""

class PrimitiveType(Type):

    def __init__(self, name, size):
        Type.__init__(self, name)
        self.size = size
        self.nmemb = 1

        # compute the required start_alignment based on the size of the type
        self.required_start_align = Alignment.for_primitive_type(self.size)

    def unchecked_get_alignment_after(self, start_align, callstack, log):
        my_callstack = callstack + [self];
        after_align = start_align.align_after_fixed_size(self.size)

        if log is not None:
            if after_align is None:
                log.fail(start_align, "", self, my_callstack,
                "align after fixed size %d failed" % self.size)
            else:
                log.ok(start_align, "", self, my_callstack, after_align)

        return after_align

    def fixed_size(self):
        return True

class SimpleType(PrimitiveType):
    '''
    Derived class which represents a cardinal type like CARD32 or char.
    Any type which is typedef'ed to cardinal will be one of these.

    Public fields added:
    xml_type is the original string describing the type in the XML
    '''
    def __init__(self, name, size, xml_type=None):
        PrimitiveType.__init__(self, name, size)
        self.is_simple = True
        self.xml_type = xml_type

    def resolve(self, module):
        self.resolved = True

    out = __main__.output['simple']


# Cardinal datatype globals.  See module __init__ method.
tcard8 = SimpleType(('uint8_t',), 1, 'CARD8')
tcard16 = SimpleType(('uint16_t',), 2, 'CARD16')
tcard32 = SimpleType(('uint32_t',), 4, 'CARD32')
tcard64 = SimpleType(('uint64_t',), 8, 'CARD64')
tint8 =  SimpleType(('int8_t',), 1, 'INT8')
tint16 = SimpleType(('int16_t',), 2, 'INT16')
tint32 = SimpleType(('int32_t',), 4, 'INT32')
tint64 = SimpleType(('int64_t',), 8, 'INT64')
tchar =  SimpleType(('char',), 1, 'char')
tfloat = SimpleType(('float',), 4, 'float')
tdouble = SimpleType(('double',), 8, 'double')
tbyte = SimpleType(('uint8_t',), 1, 'BYTE')
tbool = SimpleType(('uint8_t',), 1, 'BOOL')
tvoid = SimpleType(('uint8_t',), 1, 'void')

class FileDescriptor(SimpleType):
    '''
    Derived class which represents a file descriptor.
    '''
    def __init__(self):
        SimpleType.__init__(self, ('int', ), 4, 'fd')
        self.is_fd = True

    def fixed_size(self):
        return True

    out = __main__.output['simple']

class Enum(SimpleType):
    '''
    Derived class which represents an enum.  Fixed-size.

    Public fields added:
    values contains a list of (name, value) tuples.  value is empty, or a number.
    bits contains a list of (name, bitnum) tuples.  items only appear if specified as a bit. bitnum is a number.
    '''
    def __init__(self, name, elt):
        SimpleType.__init__(self, name, 4, 'enum')
        self.values = []
        self.bits = []
        self.doc = None
        for item in list(elt):
            if item.tag == 'doc':
                self.doc = Doc(name, item)

            # First check if we're using a default value
            if len(list(item)) == 0:
                self.values.append((item.get('name'), ''))
                continue

            # An explicit value or bit was specified.
            value = list(item)[0]
            if value.tag == 'value':
                self.values.append((item.get('name'), value.text))
            elif value.tag == 'bit':
                self.values.append((item.get('name'), '%u' % (1 << int(value.text, 0))))
                self.bits.append((item.get('name'), value.text))

    def resolve(self, module):
        self.resolved = True

    def fixed_size(self):
        return True

    out = __main__.output['enum']


class ListType(Type):
    '''
    Derived class which represents a list of some other datatype.  Fixed- or variable-sized.

    Public fields added:
    member is the datatype of the list elements.
    parent is the structure type containing the list.
    expr is an Expression object containing the length information, for variable-sized lists.
    '''
    def __init__(self, elt, member, *parent):
        Type.__init__(self, member.name)
        self.is_list = True
        self.member = member
        self.parents = list(parent)
        lenfield_name = False

        if elt.tag == 'list':
            elts = list(elt)
            self.expr = Expression(elts[0] if len(elts) else elt, self)
            is_list_in_parent = self.parents[0].elt.tag in ('request', 'event', 'reply', 'error')
            if not len(elts) and is_list_in_parent:
                self.expr = Expression(elt,self)
                self.expr.op = 'calculate_len'
            else:
                self.expr = Expression(elts[0] if len(elts) else elt, self)

        self.size = member.size if member.fixed_size() else None
        self.nmemb = self.expr.nmemb if self.expr.fixed_size() else None

        self.required_start_align = self.member.required_start_align

    def make_member_of(self, module, complex_type, field_type, field_name, visible, wire, auto, enum=None):
        if not self.fixed_size():
            # We need a length field.
            # Ask our Expression object for it's name, type, and whether it's on the wire.
            lenfid = self.expr.lenfield_type
            lenfield_name = self.expr.lenfield_name
            lenwire = self.expr.lenwire
            needlen = True

            # See if the length field is already in the structure.
            for parent in self.parents:
                for field in parent.fields:
                    if field.field_name == lenfield_name:
                        needlen = False

            # It isn't, so we need to add it to the structure ourself.
            if needlen:
                type = module.get_type(lenfid)
                lenfield_type = module.get_type_name(lenfid)
                type.make_member_of(module, complex_type, lenfield_type, lenfield_name, True, lenwire, False, enum)

        # Add ourself to the structure by calling our original method.
        if self.member.is_fd:
            wire = False
        Type.make_member_of(self, module, complex_type, field_type, field_name, visible, wire, auto, enum, self.member.is_fd)

    def resolve(self, module):
        if self.resolved:
            return
        self.member.resolve(module)
        self.expr.resolve(module, self.parents)

        # resolve() could have changed the size (ComplexType starts with size 0)
        self.size = self.member.size if self.member.fixed_size() else None

        self.required_start_align = self.member.required_start_align

        # Find my length field again.  We need the actual Field object in the expr.
        # This is needed because we might have added it ourself above.
        if not self.fixed_size():
            for parent in self.parents:
                for field in parent.fields:
                    if field.field_name == self.expr.lenfield_name and field.wire:
                        self.expr.lenfield = field
                        break

        self.resolved = True

    def fixed_size(self):
        return self.member.fixed_size() and self.expr.fixed_size()

    def unchecked_get_alignment_after(self, start_align, callstack, log):
        my_callstack = callstack[:]
        my_callstack.append(self)
        if start_align is None:
            log.fail(start_align, "", self, my_callstack, "start_align is None")
            return None
        if self.expr.fixed_size():
            # fixed number of elements
            num_elements = self.nmemb
            prev_alignment = None
            alignment = start_align
            while num_elements > 0:
                if alignment is None:
                    if log is not None:
                        log.fail(start_align, "", self, my_callstack,
                            ("fixed size list with size %d after %d iterations"
                            + ", at transition from alignment \"%s\"")
                            % (self.nmemb,
                               (self.nmemb - num_elements),
                               str(prev_alignment)))
                    return None
                prev_alignment = alignment
                alignment = self.member.get_alignment_after(prev_alignment, my_callstack, log)
                num_elements -= 1
            if log is not None:
                log.ok(start_align, "", self, my_callstack, alignment)
            return alignment
        else:
            # variable number of elements
            # check whether the number of elements is a multiple
            multiple = self.expr.get_multiple()
            assert multiple > 0

            # iterate until the combined alignment does not change anymore
            alignment = start_align
            while True:
                prev_multiple_alignment = alignment
                # apply "multiple" amount of changes sequentially
                prev_alignment = alignment
                for multiple_count in range(0, multiple):

                    after_alignment = self.member.get_alignment_after(prev_alignment, my_callstack, log)
                    if after_alignment is None:
                        if log is not None:
                            log.fail(start_align, "", self, my_callstack,
                                ("variable size list "
                                + "at transition from alignment \"%s\"")
                                % (str(prev_alignment)))
                        return None

                    prev_alignment = after_alignment

                # combine with the cumulatively combined alignment
                # (to model the variable number of entries)
                alignment = prev_multiple_alignment.combine_with(after_alignment)

                if alignment == prev_multiple_alignment:
                    # does not change anymore by adding more potential elements
                    # -> finished
                    if log is not None:
                        log.ok(start_align, "", self, my_callstack, alignment)
                    return alignment

class ExprType(PrimitiveType):
    '''
    Derived class which represents an exprfield.  Fixed size.

    Public fields added:
    expr is an Expression object containing the value of the field.
    '''
    def __init__(self, elt, member, *parents):
        PrimitiveType.__init__(self, member.name, member.size)
        self.is_expr = True
        self.member = member
        self.parents = parents

        self.expr = Expression(list(elt)[0], self)

    def resolve(self, module):
        if self.resolved:
            return
        self.member.resolve(module)
        self.resolved = True


class PadType(Type):
    '''
    Derived class which represents a padding field.
    '''
    def __init__(self, elt):
        Type.__init__(self, tcard8.name)
        self.is_pad = True
        self.size = 1
        self.nmemb = 1
        self.align = 1
        if elt != None:
            self.nmemb = int(elt.get('bytes', "1"), 0)
            self.align = int(elt.get('align', "1"), 0)
            self.serialize = elt.get('serialize', "false").lower() in true_values

        # pads don't require any alignment at their start
        self.required_start_align = Alignment(1,0)

    def resolve(self, module):
        self.resolved = True

    def fixed_size(self):
        return self.align <= 1

    def unchecked_get_alignment_after(self, start_align, callstack, log):
        if self.align <= 1:
            # fixed size pad
            after_align = start_align.align_after_fixed_size(self.get_total_size())
            if log is not None:
                if after_align is None:
                    log.fail(start_align, "", self, callstack,
                    "align after fixed size pad of size %d failed" % self.size)
                else:
                    log.ok(start_align, "", self, callstack, after_align)

            return after_align

        # align-pad
        assert self.align > 1
        assert self.size == 1
        assert self.nmemb == 1
        if (start_align.offset == 0
           and self.align <= start_align.align
           and start_align.align % self.align == 0):
            # the alignment pad is size 0 because the start_align
            # is already sufficiently aligned -> return the start_align
            after_align = start_align
        else:
            # the alignment pad has nonzero size -> return the alignment
            # that is guaranteed by it, independently of the start_align
            after_align = Alignment(self.align, 0)

        if log is not None:
            log.ok(start_align, "", self, callstack, after_align)

        return after_align
    
class ComplexType(Type):
    '''
    Derived class which represents a structure.  Base type for all structure types.

    Public fields added:
    fields is an array of Field objects describing the structure fields.
    length_expr is an expression that defines the length of the structure.

    '''
    def __init__(self, name, elt):
        Type.__init__(self, name)
        self.is_container = True
        self.elt = elt
        self.fields = []
        self.nmemb = 1
        self.size = 0
        self.lenfield_parent = [self]
        self.length_expr = None

        # get required_start_alignment
        required_start_align_element = elt.find("required_start_align")
        if required_start_align_element is None:
            # unknown -> mark for autocompute
            self.required_start_align = None
        else:
            self.required_start_align = Alignment(
                int(required_start_align_element.get('align', "4"), 0),
                int(required_start_align_element.get('offset', "0"), 0))
            if verbose_align_log:
                print ("Explicit start-align for %s: %s\n" % (self, self.required_start_align))

    def resolve(self, module):
        if self.resolved:
            return

        # Resolve all of our field datatypes.
        for child in list(self.elt):
            enum = None
            if child.tag == 'pad':
                field_name = 'pad' + str(module.pads)
                fkey = 'CARD8'
                type = PadType(child)
                module.pads = module.pads + 1
                visible = False
            elif child.tag == 'field':
                field_name = child.get('name')
                enum = child.get('enum')
                fkey = child.get('type')
                type = module.get_type(fkey)
                visible = True
            elif child.tag == 'exprfield':
                field_name = child.get('name')
                fkey = child.get('type')
                type = ExprType(child, module.get_type(fkey), *self.lenfield_parent)
                visible = False
            elif child.tag == 'list':
                field_name = child.get('name')
                fkey = child.get('type')
                if fkey == 'fd':
                    ftype = FileDescriptor()
                    fkey = 'INT32'
                else:
                    ftype = module.get_type(fkey)
                type = ListType(child, ftype, *self.lenfield_parent)
                visible = True
            elif child.tag == 'switch':
                field_name = child.get('name')
                # construct the switch type name from the parent type and the field name
                field_type = self.name + (field_name,)
                type = SwitchType(field_type, child, *self.lenfield_parent)
                visible = True
                type.make_member_of(module, self, field_type, field_name, visible, True, False)
                type.resolve(module)
                continue
            elif child.tag == 'fd':
                fd_name = child.get('name')
                type = module.get_type('INT32')
                type.make_fd_of(module, self, fd_name)
                continue
            elif child.tag == 'length':
                self.length_expr = Expression(list(child)[0], self)
                continue
            else:
                # Hit this on Reply
                continue

            # Get the full type name for the field
            field_type = module.get_type_name(fkey)
            # Add the field to ourself
            type.make_member_of(module, self, field_type, field_name, visible, True, False, enum)
            # Recursively resolve the type (could be another structure, list)
            type.resolve(module)

            # Compute the size of the maximally contain align-pad
            if type.max_align_pad > self.max_align_pad:
                self.max_align_pad = type.max_align_pad

        self.check_implicit_fixed_size_part_aligns();

        self.calc_size() # Figure out how big we are
        self.calc_or_check_required_start_align()

        self.resolved = True

    def calc_size(self):
        self.size = 0
        for m in self.fields:
            if not m.wire:
                continue
            if m.type.fixed_size():
                self.size = self.size + m.type.get_total_size()
            else:
                self.size = None
                break

    def calc_or_check_required_start_align(self):
        if self.required_start_align is None:
            # no required-start-align configured -> calculate it
            log = AlignmentLog()
            callstack = []
            self.required_start_align = self.calc_minimally_required_start_align(callstack, log)
            if self.required_start_align is None:
                print ("ERROR: could not calc required_start_align of %s\nDetails:\n%s"
                    % (str(self), str(log)))
            else:
                if verbose_align_log:
                    print ("calc_required_start_align: %s has start-align %s"
                        % (str(self), str(self.required_start_align)))
                    print ("Details:\n" + str(log))
                if self.required_start_align.offset != 0:
                    print (("WARNING: %s\n\thas start-align with non-zero offset: %s"
                        + "\n\tsuggest to add explicit definition with:"
                        + "\n\t\t<required_start_align align=\"%d\" offset=\"%d\" />"
                        + "\n\tor to fix the xml so that zero offset is ok\n")
                        % (str(self), self.required_start_align,
                           self.required_start_align.align,
                           self.required_start_align.offset))
        else:
            # required-start-align configured -> check it
            log = AlignmentLog()
            callstack = []
            if not self.is_possible_start_align(self.required_start_align, callstack, log):
                print ("ERROR: required_start_align %s of %s causes problems\nDetails:\n%s"
                    % (str(self.required_start_align), str(self), str(log)))


    def calc_minimally_required_start_align(self, callstack, log):
        # calculate the minimally required start_align that causes no
        # align errors
        best_log = None
        best_failed_align = None
        for align in [1,2,4,8]:
            for offset in range(0,align):
                align_candidate = Alignment(align, offset)
                if verbose_align_log:
                    print ("trying %s for %s" % (str(align_candidate), str(self)))
                my_log = AlignmentLog()
                if self.is_possible_start_align(align_candidate, callstack, my_log):
                    log.append(my_log)
                    if verbose_align_log:
                        print ("found start-align %s for %s" % (str(align_candidate), str(self)))
                    return align_candidate
                else:
                    my_ok_count = my_log.ok_count()
                    if (best_log is None
                       or my_ok_count > best_log.ok_count()
                       or (my_ok_count == best_log.ok_count()
                          and align_candidate.align > best_failed_align.align)
                          and align_candidate.align != 8):
                        best_log = my_log
                        best_failed_align = align_candidate



        # none of the candidates applies
        # this type has illegal internal aligns for all possible start_aligns
        if verbose_align_log:
            print ("didn't find start-align for %s" % str(self))
        log.append(best_log)
        return None

    def is_possible_start_align(self, align, callstack, log):
        if align is None:
            return False
        if (self.max_align_pad > align.align
           or align.align % self.max_align_pad != 0):
            # our align pad implementation depends on known alignment
            # at the start of our type
            return False

        return self.get_alignment_after(align, callstack, log) is not None

    def fixed_size(self):
        for m in self.fields:
            if not m.type.fixed_size():
                return False
        return True


    # default impls of polymorphic methods which assume sequential layout of fields
    # (like Struct or CaseOrBitcaseType)
    def check_implicit_fixed_size_part_aligns(self):
        # find places where the implementation of the C-binding would
        # create code that makes the compiler add implicit alignment.
        # make these places explicit, so we have
        # consistent behaviour for all bindings
        size = 0
        for field in self.fields:
            if not field.wire:
                continue
            if not field.type.fixed_size():
                # end of fixed-size part
                break
            required_field_align = field.type.required_start_align
            if required_field_align is None:
                raise Exception(
                    "field \"%s\" in \"%s\" has not required_start_align"
                    % (field.field_name, self.name)
                )
            mis_align = (size + required_field_align.offset) % required_field_align.align
            if mis_align != 0:
                # implicit align pad is required
                padsize = required_field_align.align - mis_align
                raise Exception(
                    "C-compiler would insert implicit alignpad of size %d before field \"%s\" in \"%s\""
                    % (padsize, field.field_name, self.name)
                )

    def unchecked_get_alignment_after(self, start_align, callstack, log):
        # default impl assumes sequential layout of fields
        # (like Struct or CaseOrBitcaseType)
        my_align = start_align
        if my_align is None:
            return None

        for field in self.fields:
            if not field.wire:
                continue
            my_callstack = callstack[:]
            my_callstack.extend([self, field])

            prev_align = my_align
            my_align = field.type.get_alignment_after(my_align, my_callstack, log)
            if my_align is None:
                if log is not None:
                    log.fail(prev_align, field.field_name, self, my_callstack,
                        "alignment is incompatible with this field")
                return None
            else:
                if log is not None:
                    log.ok(prev_align, field.field_name, self, my_callstack, my_align)

        if log is not None:
            my_callstack = callstack[:]
            my_callstack.append(self)
            log.ok(start_align, "", self, my_callstack, my_align)
        return my_align


class SwitchType(ComplexType):
    '''
    Derived class which represents a List of Items.  

    Public fields added:
    bitcases is an array of Bitcase objects describing the list items
    '''

    def __init__(self, name, elt, *parents):
        ComplexType.__init__(self, name, elt)
        self.parents = parents
        # FIXME: switch cannot store lenfields, so it should just delegate the parents
        self.lenfield_parent = list(parents) + [self]
        # self.fields contains all possible fields collected from the Bitcase objects, 
        # whereas self.items contains the Bitcase objects themselves
        self.bitcases = []

        self.is_switch = True
        elts = list(elt)
        self.expr = Expression(elts[0] if len(elts) else elt, self)

    def resolve(self, module):
        if self.resolved:
            return

        parents = list(self.parents) + [self]

        # Resolve all of our field datatypes.
        for index, child in enumerate(list(self.elt)):
            if child.tag == 'bitcase' or child.tag == 'case':
                field_name = child.get('name')
                if field_name is None:
                    field_type = self.name + ('%s%d' % ( child.tag, index ),)
                else:
                    field_type = self.name + (field_name,)

                # use self.parent to indicate anchestor, 
                # as switch does not contain named fields itself
                if child.tag == 'bitcase':
                    type = BitcaseType(index, field_type, child, *parents)
                else:
                    type = CaseType(index, field_type, child, *parents)

                # construct the switch type name from the parent type and the field name
                if field_name is None:
                    type.has_name = False
                    # Get the full type name for the field
                    field_type = type.name               
                visible = True

                # add the field to ourself
                type.make_member_of(module, self, field_type, field_name, visible, True, False)

                # recursively resolve the type (could be another structure, list)
                type.resolve(module)
                inserted = False
                for new_field in type.fields:
                    # We dump the _placeholder_byte if any fields are added.
                    for (idx, field) in enumerate(self.fields):
                        if field == _placeholder_byte:
                            self.fields[idx] = new_field
                            inserted = True
                            break
                    if False == inserted:
                        self.fields.append(new_field)

        self.calc_size() # Figure out how big we are
        self.calc_or_check_required_start_align()
        self.resolved = True

    def make_member_of(self, module, complex_type, field_type, field_name, visible, wire, auto, enum=None):
        if not self.fixed_size():
            # We need a length field.
            # Ask our Expression object for it's name, type, and whether it's on the wire.
            lenfid = self.expr.lenfield_type
            lenfield_name = self.expr.lenfield_name
            lenwire = self.expr.lenwire
            needlen = True

            # See if the length field is already in the structure.
            for parent in self.parents:
                for field in parent.fields:
                    if field.field_name == lenfield_name:
                        needlen = False

            # It isn't, so we need to add it to the structure ourself.
            if needlen:
                type = module.get_type(lenfid)
                lenfield_type = module.get_type_name(lenfid)
                type.make_member_of(module, complex_type, lenfield_type, lenfield_name, True, lenwire, False, enum)

        # Add ourself to the structure by calling our original method.
        Type.make_member_of(self, module, complex_type, field_type, field_name, visible, wire, auto, enum)

    # size for switch can only be calculated at runtime
    def calc_size(self):
        pass

    # note: switch is _always_ of variable size, but we indicate here wether 
    # it contains elements that are variable-sized themselves
    def fixed_size(self):
        return False
#        for m in self.fields:
#            if not m.type.fixed_size():
#                return False
#        return True



    def check_implicit_fixed_size_part_aligns(self):
        # this is done for the CaseType or BitCaseType
        return

    def unchecked_get_alignment_after(self, start_align, callstack, log):
        # we assume that BitCases can appear in any combination,
        # and that at most one Case can appear
        # (assuming that Cases are mutually exclusive)

        # get all Cases (we assume that at least one case is selected if there are cases)
        case_fields = []
        for field in self.bitcases:
            if field.type.is_case:
                case_fields.append(field)

        if not case_fields:
            # there are no case-fields -> check without case-fields
            case_fields = [None]

        my_callstack = callstack[:]
        my_callstack.append(self)
        #
        total_align = None
        first = True
        for case_field in case_fields:
            my2_callstack = my_callstack[:]
            if case_field is not None:
                my2_callstack.append(case_field)

            case_align = self.get_align_for_selected_case_field(
                             case_field, start_align, my2_callstack, log)


            if case_align is None:
                if log is not None:
                    if case_field is None:
                        log.fail(start_align, "", self, my2_callstack,
                            "alignment without cases (only bitcases) failed")
                    else:
                        log.fail(start_align, "", self, my2_callstack + [case_field],
                            "alignment for selected case %s failed"
                            % case_field.field_name)
                return None
            if first:
                total_align = case_align
            else:
                total_align = total_align.combine_with(case_align)

            if log is not None:
                if case_field is None:
                    log.ok(
                        start_align,
                        "without cases (only arbitrary bitcases)",
                        self, my2_callstack, case_align)
                else:
                    log.ok(
                        start_align,
                        "case %s and arbitrary bitcases" % case_field.field_name,
                        self, my2_callstack, case_align)


        if log is not None:
            log.ok(start_align, "", self, my_callstack, total_align)
        return total_align

    # aux function for unchecked_get_alignment_after
    def get_align_for_selected_case_field(self, case_field, start_align, callstack, log):
        if verbose_align_log:
            print ("get_align_for_selected_case_field: %s, case_field = %s" % (str(self), str(case_field)))
        total_align = start_align
        for field in self.bitcases:
            my_callstack = callstack[:]
            my_callstack.append(field)

            if not field.wire:
                continue
            if field is case_field:
                # assume that this field is active -> no combine_with to emulate optional
                after_field_align = field.type.get_alignment_after(total_align, my_callstack, log)

                if log is not None:
                    if after_field_align is None:
                        log.fail(total_align, field.field_name, field.type, my_callstack,
                            "invalid aligment for this case branch")
                    else:
                        log.ok(total_align, field.field_name, field.type, my_callstack,
                            after_field_align)

                total_align = after_field_align
            elif field.type.is_bitcase:
                after_field_align = field.type.get_alignment_after(total_align, my_callstack, log)
                # we assume that this field is optional, therefore combine
                # alignment after the field with the alignment before the field.
                if after_field_align is None:
                    if log is not None:
                        log.fail(total_align, field.field_name, field.type, my_callstack,
                            "invalid aligment for this bitcase branch")
                    total_align = None
                else:
                    if log is not None:
                        log.ok(total_align, field.field_name, field.type, my_callstack,
                            after_field_align)

                    # combine with the align before the field because
                    # the field is optional
                    total_align = total_align.combine_with(after_field_align)
            else:
                # ignore other fields as they are irrelevant for alignment
                continue

            if total_align is None:
                break

        return total_align


class Struct(ComplexType):
    '''
    Derived class representing a struct data type.
    '''
    out = __main__.output['struct']


class Union(ComplexType):
    '''
    Derived class representing a union data type.
    '''
    def __init__(self, name, elt):
        ComplexType.__init__(self, name, elt)
        self.is_union = True

    out = __main__.output['union']


    def calc_size(self):
        self.size = 0
        for m in self.fields:
            if not m.wire:
                continue
            if m.type.fixed_size():
                self.size = max(self.size, m.type.get_total_size())
            else:
                self.size = None
                break


    def check_implicit_fixed_size_part_aligns(self):
        # a union does not have implicit aligns because all fields start
        # at the start of the union
        return


    def unchecked_get_alignment_after(self, start_align, callstack, log):
        my_callstack = callstack[:]
        my_callstack.append(self)

        after_align = None
        if self.fixed_size():

            #check proper alignment for all members
            start_align_ok = all(
                [field.type.is_acceptable_start_align(start_align, my_callstack + [field], log)
                for field in self.fields])

            if start_align_ok:
                #compute the after align from the start_align
                after_align = start_align.align_after_fixed_size(self.get_total_size())
            else:
                after_align = None

            if log is not None and after_align is not None:
                log.ok(start_align, "fixed sized union", self, my_callstack, after_align)

        else:
            if start_align is None:
                if log is not None:
                    log.fail(start_align, "", self, my_callstack,
                        "missing start_align for union")
                return None

            after_align = reduce(
                lambda x, y: None if x is None or y is None else x.combine_with(y),
                [field.type.get_alignment_after(start_align, my_callstack + [field], log)
                 for field in self.fields])

            if log is not None and after_align is not None:
                log.ok(start_align, "var sized union", self, my_callstack, after_align)


        if after_align is None and log is not None:
            log.fail(start_align, "", self, my_callstack, "start_align is not ok for all members")

        return after_align

class CaseOrBitcaseType(ComplexType):
    '''
    Derived class representing a case or bitcase.
    '''
    def __init__(self, index, name, elt, *parent):
        elts = list(elt)
        self.expr = []
        for sub_elt in elts:
            if sub_elt.tag == 'enumref':
                self.expr.append(Expression(sub_elt, self))
                elt.remove(sub_elt)
        ComplexType.__init__(self, name, elt)
        self.has_name = True
        self.index = 1
        self.lenfield_parent = list(parent) + [self]
        self.parents = list(parent)
        self.is_case_or_bitcase = True

    def make_member_of(self, module, switch_type, field_type, field_name, visible, wire, auto, enum=None):
        '''
        register BitcaseType with the corresponding SwitchType

        module is the global module object.
        complex_type is the structure object.
        see Field for the meaning of the other parameters.
        '''
        new_field = Field(self, field_type, field_name, visible, wire, auto, enum)

        # We dump the _placeholder_byte if any bitcases are added.
        for (idx, field) in enumerate(switch_type.bitcases):
            if field == _placeholder_byte:
                switch_type.bitcases[idx] = new_field
                return

        switch_type.bitcases.append(new_field)

    def resolve(self, module):
        if self.resolved:
            return

        for e in self.expr:
            e.resolve(module, self.parents+[self])

        # Resolve the bitcase expression
        ComplexType.resolve(self, module)

        #calculate alignment
        self.calc_or_check_required_start_align()


class BitcaseType(CaseOrBitcaseType):
    '''
    Derived class representing a bitcase.
    '''
    def __init__(self, index, name, elt, *parent):
        CaseOrBitcaseType.__init__(self, index, name, elt, *parent)
        self.is_bitcase = True

class CaseType(CaseOrBitcaseType):
    '''
    Derived class representing a case.
    '''
    def __init__(self, index, name, elt, *parent):
        CaseOrBitcaseType.__init__(self, index, name, elt, *parent)
        self.is_case = True


class Reply(ComplexType):
    '''
    Derived class representing a reply.  Only found as a field of Request.
    '''
    def __init__(self, name, elt):
        ComplexType.__init__(self, name, elt)
        self.is_reply = True
        self.doc = None
        if self.required_start_align is None:
            self.required_start_align = Alignment(4,0)

        for child in list(elt):
            if child.tag == 'doc':
                self.doc = Doc(name, child)

    def resolve(self, module):
        if self.resolved:
            return
        # Reset pads count
        module.pads = 0
        # Add the automatic protocol fields
        self.fields.append(Field(tcard8, tcard8.name, 'response_type', False, True, True))
        self.fields.append(_placeholder_byte)
        self.fields.append(Field(tcard16, tcard16.name, 'sequence', False, True, True))
        self.fields.append(Field(tcard32, tcard32.name, 'length', False, True, True))
        ComplexType.resolve(self, module)
        

class Request(ComplexType):
    '''
    Derived class representing a request.

    Public fields added:
    reply contains the reply datatype or None for void requests.
    opcode contains the request number.
    '''
    def __init__(self, name, elt):
        ComplexType.__init__(self, name, elt)
        self.reply = None
        self.doc = None
        self.opcode = elt.get('opcode')
        if self.required_start_align is None:
            self.required_start_align = Alignment(4,0)

        for child in list(elt):
            if child.tag == 'reply':
                self.reply = Reply(name, child)
            if child.tag == 'doc':
                self.doc = Doc(name, child)

    def resolve(self, module):
        if self.resolved:
            return
        # Add the automatic protocol fields
        if module.namespace.is_ext:
            self.fields.append(Field(tcard8, tcard8.name, 'major_opcode', False, True, True))
            self.fields.append(Field(tcard8, tcard8.name, 'minor_opcode', False, True, True))
            self.fields.append(Field(tcard16, tcard16.name, 'length', False, True, True))
            ComplexType.resolve(self, module)
        else:
            self.fields.append(Field(tcard8, tcard8.name, 'major_opcode', False, True, True))
            self.fields.append(_placeholder_byte)
            self.fields.append(Field(tcard16, tcard16.name, 'length', False, True, True))
            ComplexType.resolve(self, module)

        if self.reply:
            self.reply.resolve(module)

    out = __main__.output['request']


class EventStructAllowedRule:

    def __init__(self, parent, elt):
        self.elt = elt
        self.extension = elt.get('extension')
        self.ge_events = elt.get('xge') == "true"
        self.min_opcode = int( elt.get('opcode-min') )
        self.max_opcode = int( elt.get('opcode-max') )

    def resolve(self, parent, module):
        # get the namespace of the specified extension
        extension_namespace = module.get_namespace( self.extension )
        if extension_namespace is None:
            raise Exception( "EventStructAllowedRule.resolve: cannot find extension \"" + self.extension + "\"" )
            return

        # find and add the selected events
        for opcode in range(self.min_opcode, self.max_opcode):
            name_and_event = extension_namespace.get_event_by_opcode( opcode, self.ge_events )
            if name_and_event is None:
                # could not find event -> error handling
                if self.ge_events:
                    raise Exception("EventStructAllowedRule.resolve: cannot find xge-event with opcode " + str(opcode) + " in extension " + self.extension )
                else:
                    raise Exception("EventStructAllowedRule.resolve: cannot find oldstyle-event with opcode " + str(opcode) + " in extension " + self.extension )
                return

            ( name, event ) = name_and_event
            # add event to EventStruct
            parent.add_event( module, self.extension, opcode, name, event )


class EventStruct(Union):
    '''
    Derived class representing an event-use-as-struct data type.
    '''

    def __init__(self, name, elt):
        Union.__init__(self, name, elt)
        self.is_eventstruct = True
        self.events = []
        self.allowedRules = []
        self.contains_ge_events = False
        for item in list(elt):
            if item.tag == 'allowed':
                allowedRule = EventStructAllowedRule(self, item)
                self.allowedRules.append( allowedRule )
                if allowedRule.ge_events:
                    self.contains_ge_events = True

    out = __main__.output['eventstruct']

    def resolve(self, module):
        if self.resolved:
            return
        for allowedRule in self.allowedRules:
            allowedRule.resolve(self, module)
        Union.resolve(self,module)
        self.resolved = True

    # add event. called by resolve
    def add_event(self, module, extension, opcode, name, event_type ):
        self.events.append( (extension, opcode, name, event_type) )
        # Add the field to ourself
        event_type.make_member_of(module, self, name, name[-1], True, True, False)
        # Recursively resolve the event (could be another structure, list)
        event_type.resolve(module)

    def fixed_size(self):
        is_fixed_size = True
        for extension, opcode, name, event in self.events:
            if not event.fixed_size():
                is_fixed_size = False
        return is_fixed_size


class Event(ComplexType):
    '''
    Derived class representing an event data type.

    Public fields added:
    opcodes is a dictionary of name -> opcode number, for eventcopies.
    '''
    def __init__(self, name, elt):
        ComplexType.__init__(self, name, elt)

        if self.required_start_align is None:
            self.required_start_align = Alignment(4,0)

        self.opcodes = {}

        self.has_seq = not bool(elt.get('no-sequence-number'))

        self.is_ge_event = bool(elt.get('xge'))

        self.is_event = True

        self.doc = None
        for item in list(elt):
            if item.tag == 'doc':
                self.doc = Doc(name, item)

    def add_opcode(self, opcode, name, main):
        self.opcodes[name] = opcode
        if main:
            self.name = name

    def get_name_for_opcode(self, opcode):
        for name, my_opcode in self.opcodes.items():
            if int(my_opcode) == opcode:
                return name
        else:
            return None

    def resolve(self, module):
        def add_event_header():
            self.fields.append(Field(tcard8, tcard8.name, 'response_type', False, True, True))
            if self.has_seq:
                self.fields.append(_placeholder_byte)
                self.fields.append(Field(tcard16, tcard16.name, 'sequence', False, True, True))

        def add_ge_event_header():
            self.fields.append(Field(tcard8,  tcard8.name,  'response_type', False, True, True))
            self.fields.append(Field(tcard8,  tcard8.name,  'extension', False, True, True))
            self.fields.append(Field(tcard16, tcard16.name, 'sequence', False, True, True))
            self.fields.append(Field(tcard32, tcard32.name, 'length', False, True, True))
            self.fields.append(Field(tcard16, tcard16.name, 'event_type', False, True, True))

        if self.resolved:
            return

        # Add the automatic protocol fields
        if self.is_ge_event:
            add_ge_event_header()
        else:
            add_event_header()

        ComplexType.resolve(self, module)

    out = __main__.output['event']


class Error(ComplexType):
    '''
    Derived class representing an error data type.

    Public fields added:
    opcodes is a dictionary of name -> opcode number, for errorcopies.
    '''
    def __init__(self, name, elt):
        ComplexType.__init__(self, name, elt)
        self.opcodes = {}
        if self.required_start_align is None:
            self.required_start_align = Alignment(4,0)

        # All errors are basically the same, but they still got different XML
        # for historic reasons. This 'invents' the missing parts.
        if len(self.elt) < 1:
            SubElement(self.elt, "field", type="CARD32", name="bad_value")
        if len(self.elt) < 2:
            SubElement(self.elt, "field", type="CARD16", name="minor_opcode")
        if len(self.elt) < 3:
            SubElement(self.elt, "field", type="CARD8", name="major_opcode")

    def add_opcode(self, opcode, name, main):
        self.opcodes[name] = opcode
        if main:
            self.name = name

    def resolve(self, module):
        if self.resolved:
            return

        # Add the automatic protocol fields
        self.fields.append(Field(tcard8, tcard8.name, 'response_type', False, True, True))
        self.fields.append(Field(tcard8, tcard8.name, 'error_code', False, True, True))
        self.fields.append(Field(tcard16, tcard16.name, 'sequence', False, True, True))
        ComplexType.resolve(self, module)

    out = __main__.output['error']


class Doc(object):
    '''
    Class representing a <doc> tag.
    '''
    def __init__(self, name, elt):
        self.name = name
        self.description = None
        self.brief = 'BRIEF DESCRIPTION MISSING'
        self.fields = {}
        self.errors = {}
        self.see = {}
        self.example = None

        for child in list(elt):
            text = child.text if child.text else ''
            if child.tag == 'description':
                self.description = text.strip()
            if child.tag == 'brief':
                self.brief = text.strip()
            if child.tag == 'field':
                self.fields[child.get('name')] = text.strip()
            if child.tag == 'error':
                self.errors[child.get('type')] = text.strip()
            if child.tag == 'see':
                self.see[child.get('name')] = child.get('type')
            if child.tag == 'example':
                self.example = text.strip()



_placeholder_byte = Field(PadType(None), tcard8.name, 'pad0', False, True, False)
