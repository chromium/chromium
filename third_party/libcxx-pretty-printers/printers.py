# Pretty-printers for libc++.

# Copyright (C) 2008-2013 Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import re
import gdb
import sys

if sys.version_info[0] > 2:
    # Python 3 stuff
    Iterator = object
    # Python 3 folds these into the normal functions.
    imap = map
    izip = zip
    # Also, int subsumes long
    long = int
else:
    # Python 2 stuff
    class Iterator:
        """Compatibility mixin for iterators

        Instead of writing next() methods for iterators, write
        __next__() methods and use this mixin to make them work in
        Python 2 as well as Python 3.

        Idea stolen from the "six" documentation:
        <http://pythonhosted.org/six/#six.Iterator>
        """

        def next(self):
            return self.__next__()

    # In Python 2, we still need these from itertools
    from itertools import imap, izip

# Try to use the new-style pretty-printing if available.
_use_gdb_pp = True
try:
    import gdb.printing
except ImportError:
    _use_gdb_pp = False

# Try to install type-printers.
_use_type_printing = False
try:
    import gdb.types
    if hasattr(gdb.types, 'TypePrinter'):
        _use_type_printing = True
except ImportError:
    pass


def make_type_re(typename):
    return re.compile('^std::__[a-zA-Z0-9]+::' + typename + '<.*>$')


def get_node_value(it, ptr):
    t = gdb.lookup_type(it.type.name + '::__node_pointer')
    return ptr.cast(t)['__value_']


def get_node_value_from_pointer(ptr):
    return get_node_value(ptr.dereference(), ptr)


def get_node_value_from_iterator(it):
    return get_node_value(it, it['__ptr_'])


# Starting with the type ORIG, search for the member type NAME.  This
# handles searching upward through superclasses.  This is needed to
# work around http://sourceware.org/bugzilla/show_bug.cgi?id=13615.
def find_type(orig, name):
    typ = orig.strip_typedefs()
    while True:
        search = str(typ) + '::' + name
        try:
            return gdb.lookup_type(search)
        except RuntimeError:
            pass
        # The type was not found, so try the superclass.  We only need
        # to check the first superclass, so we don't bother with
        # anything fancier here.
        field = typ.fields()[0]
        if not field.is_base_class:
            raise ValueError("Cannot find type %s::%s" % (orig, name))
        typ = field.type


def pair_to_tuple(val):
    if make_type_re('__compressed_pair').match(val.type.name):
        t1 = val.type.template_argument(0)
        t2 = val.type.template_argument(1)

        base1 = val.type.fields()[0].type
        base2 = val.type.fields()[1].type

        return ((val if base1.template_argument(2) else
                 val.cast(base1)["__value_"]).cast(t1),
                (val if base2.template_argument(2) else
                 val.cast(base2)["__value_"]).cast(t2))
    return (val['first'], val['second'])


void_type = gdb.lookup_type('void')


def ptr_to_void_ptr(val):
    if gdb.types.get_basic_type(val.type).code == gdb.TYPE_CODE_PTR:
        return val.cast(void_type.pointer())
    else:
        return val


class StringPrinter:
    "Print a std::basic_string of some kind"

    def __init__(self, typename, val):
        self.val = val
        self.typename = typename

    def to_string(self):
        # Make sure &string works, too.
        type = self.val.type
        if type.code == gdb.TYPE_CODE_REF:
            type = type.target()

        ss = pair_to_tuple(self.val['__r_'])[0]['__s']
        __short_mask = int(self.val['__short_mask'])
        if (ss['__size_'] & __short_mask) == 0:
            len = ss['__size_'] >> 1 if __short_mask == 1 else ss['__size_']
            ptr = ss['__data_']
        else:
            sl = pair_to_tuple(self.val['__r_'])[0]['__l']
            len = sl['__size_']
            ptr = sl['__data_']

        return u''.join(unichr(ptr[i]) for i in range(len))

    def display_hint(self):
        return 'string'


class SharedPointerPrinter:
    "Print a shared_ptr or weak_ptr"

    def __init__(self, typename, val):
        self.typename = typename
        self.val = val

    def to_string(self):
        state = 'empty'
        refcounts = self.val['__cntrl_']
        if refcounts != 0:
            usecount = refcounts['__shared_owners_'] + 1
            weakcount = refcounts['__shared_weak_owners_']
            if usecount == 0:
                state = 'expired, weak %d' % weakcount
            else:
                state = 'count %d, weak %d' % (usecount, weakcount)

        if self.val['__ptr_'] == 0:
            return '%s (%s) = %s <nullptr>' % (self.typename, state,
                                               self.val['__ptr_'])
        return '%s (%s) = %s => %s' % (self.typename, state,
                                       self.val['__ptr_'],
                                       self.val['__ptr_'].dereference())


class UniquePointerPrinter:
    "Print a unique_ptr"

    def __init__(self, typename, val):
        self.typename = typename
        self.val = val

    def to_string(self):
        v = pair_to_tuple(self.val['__ptr_'])[0]
        if v == 0:
            return '%s<%s> = %s <nullptr>' % (str(self.typename),
                                              str(v.type.target()), str(v))
        return '%s<%s> = %s => %s' % (str(self.typename), str(v.type.target()),
                                      str(v), v.dereference())


class PairPrinter:
    "Print a std::pair"

    def __init__(self, typename, val):
        self.typename = typename
        self.val = val

    def children(self):
        vals = pair_to_tuple(self.val)
        return [('first', vals[0]), ('second', vals[1])]

    def to_string(self):
        return 'pair'


#    def display_hint(self):
#        return 'array'


class TuplePrinter:
    "Print a std::tuple"

    class _iterator(Iterator):
        def __init__(self, head):
            self.head = head['__base_']
            self.fields = self.head.type.fields()
            self.count = 0

        def __iter__(self):
            return self

        def __next__(self):
            if self.count >= len(self.fields):
                raise StopIteration
            field = self.head.cast(self.fields[self.count].type)['__value_']
            self.count += 1
            return ('[%d]' % (self.count - 1), field)

    def __init__(self, typename, val):
        self.typename = typename
        self.val = val

    def children(self):
        return self._iterator(self.val)

    def to_string(self):
        if len(self.val.type.fields()) == 0:
            return 'empty %s' % (self.typename)
        return 'tuple'


#    def display_hint(self):
#        return 'array'


class ListPrinter:
    "Print a std::list"

    class _iterator(Iterator):
        def __init__(self, nodetype, head):
            self.nodetype = nodetype
            self.base = head['__next_']
            self.head = head.address
            self.count = 0

        def __iter__(self):
            return self

        def __next__(self):
            if self.base == self.head:
                raise StopIteration
            elt = self.base.cast(self.nodetype).dereference()
            self.base = elt['__next_']
            count = self.count
            self.count = self.count + 1
            return ('[%d]' % count, elt['__value_'])

    def __init__(self, typename, val):
        self.typename = typename
        self.val = val

    def children(self):
        nodebase = find_type(self.val.type, '__node_base')
        nodetype = find_type(nodebase, '__node_pointer')
        nodetype = nodetype.strip_typedefs()
        return self._iterator(nodetype, self.val['__end_'])

    def to_string(self):
        if self.val['__end_']['__next_'] == self.val['__end_'].address:
            return 'empty %s' % (self.typename)
        return '%s' % (self.typename)


#    def display_hint(self):
#        return 'array'


class ListIteratorPrinter:
    "Print std::list::iterator"

    def __init__(self, typename, val):
        self.val = val
        self.typename = typename

    def to_string(self):
        return get_node_value_from_pointer(self.val['__ptr_'])


class ForwardListPrinter:
    "Print a std::forward_list"

    class _iterator(Iterator):
        def __init__(self, head):
            self.node = head
            self.count = 0

        def __iter__(self):
            return self

        def __next__(self):
            if self.node == 0:
                raise StopIteration

            result = ('[%d]' % self.count, self.node['__value_'])
            self.count += 1
            self.node = self.node['__next_']
            return result

    def __init__(self, typename, val):
        self.val = val
        self.typename = typename
        self.head = pair_to_tuple(val['__before_begin_'])[0]['__next_']

    def children(self):
        return self._iterator(self.head)

    def to_string(self):
        if self.head == 0:
            return 'empty %s' % (self.typename)
        return '%s' % (self.typename)


class VectorPrinter:
    "Print a std::vector"

    class _iterator(Iterator):
        def __init__(self, start, finish_or_size, bits_per_word, bitvec):
            self.bitvec = bitvec
            if bitvec:
                self.item = start
                self.so = 0
                self.size = finish_or_size
                self.bits_per_word = bits_per_word
            else:
                self.item = start
                self.finish = finish_or_size
            self.count = 0

        def __iter__(self):
            return self

        def __next__(self):
            count = self.count
            self.count = self.count + 1
            if self.bitvec:
                if count == self.size:
                    raise StopIteration
                elt = self.item.dereference()
                if elt & (1 << self.so):
                    obit = 1
                else:
                    obit = 0
                self.so = self.so + 1
                if self.so >= self.bits_per_word:
                    self.item = self.item + 1
                    self.so = 0
                return ('[%d]' % count, obit)
            else:
                if self.item == self.finish:
                    raise StopIteration
                elt = self.item.dereference()
                self.item = self.item + 1
                return ('[%d]' % count, elt)

    def __init__(self, typename, val):
        self.typename = typename
        self.val = val
        self.is_bool = 0
        for f in val.type.fields():
            if f.name == '__bits_per_word':
                self.is_bool = 1

    def children(self):
        if self.is_bool:
            return self._iterator(self.val['__begin_'], self.val['__size_'],
                                  self.val['__bits_per_word'], self.is_bool)
        else:
            return self._iterator(self.val['__begin_'], self.val['__end_'], 0,
                                  self.is_bool)

    def to_string(self):
        if self.is_bool:
            length = self.val['__size_']
            capacity = pair_to_tuple(
                self.val['__cap_alloc_'])[0] * self.val['__bits_per_word']
            if length == 0:
                return 'empty %s<bool> (capacity=%d)' % (self.typename,
                                                         int(capacity))
            else:
                return '%s<bool> (length=%d, capacity=%d)' % (
                    self.typename, int(length), int(capacity))
        else:
            start = ptr_to_void_ptr(self.val['__begin_'])
            finish = ptr_to_void_ptr(self.val['__end_'])
            end = ptr_to_void_ptr(pair_to_tuple(self.val['__end_cap_'])[0])
            length = finish - start
            capacity = end - start
            if length == 0:
                return 'empty %s (capacity=%d)' % (self.typename,
                                                   int(capacity))
            else:
                return '%s (length=%d, capacity=%d)' % (
                    self.typename, int(length), int(capacity))

    def display_hint(self):
        return 'array'


class VectorIteratorPrinter:
    "Print std::vector::iterator"

    def __init__(self, typename, val):
        self.val = val

    def to_string(self):
        return self.val['__i'].dereference()


class VectorBoolIteratorPrinter:
    "Print std::vector<bool>::iterator"

    def __init__(self, typename, val):
        self.segment = val['__seg_'].dereference()
        self.ctz = val['__ctz_']

    def to_string(self):
        if self.segment & (1 << self.ctz):
            return 1
        else:
            return 0


class DequePrinter:
    "Print a std::deque"

    class _iterator(Iterator):
        def __init__(self, size, block_size, start, map_begin, map_end):
            self.block_size = block_size
            self.count = 0
            self.end_p = size + start
            self.end_mp = map_begin + self.end_p / block_size
            self.p = 0
            self.mp = map_begin + start / block_size
            if map_begin == map_end:
                self.p = 0
                self.end_p = 0
            else:
                self.p = self.mp.dereference() + start % block_size
                self.end_p = self.end_mp.dereference(
                ) + self.end_p % block_size

        def __iter__(self):
            return self

        def __next__(self):
            old_p = self.p

            self.count += 1
            self.p += 1
            if (self.p - self.mp.dereference()) == self.block_size:
                self.mp += 1
                self.p = self.mp.dereference()

            if (self.mp > self.end_mp) or ((self.p > self.end_p) and
                                           (self.mp == self.end_mp)):
                raise StopIteration

            return ('[%d]' % int(self.count - 1), old_p.dereference())

    def __init__(self, typename, val):
        self.typename = typename
        self.val = val
        self.size = pair_to_tuple(val['__size_'])[0]

    def to_string(self):
        if self.size == 0:
            return 'empty %s' % self.typename
        else:
            return '%s (size=%d)' % (self.typename, long(self.size))

    def children(self):
        if self.size == 0:
            return []
        block_map = self.val['__map_']
        size_of_value_type = self.val.type.template_argument(0).sizeof
        block_size = self.val['__block_size']
        if block_size.is_optimized_out:
            # Warning, this is pretty flaky
            block_size = 4096 / size_of_value_type if size_of_value_type < 256 else 16
        return self._iterator(self.size, block_size, self.val['__start_'],
                              block_map['__begin_'], block_map['__end_'])


#    def display_hint (self):
#        return 'array'


class DequeIteratorPrinter:
    "Print std::deque::iterator"

    def __init__(self, typename, val):
        self.val = val

    def to_string(self):
        return self.val['__ptr_'].dereference()


class StackOrQueuePrinter:
    "Print a std::stack or std::queue"

    def __init__(self, typename, val):
        self.typename = typename
        self.visualizer = gdb.default_visualizer(val['c'])

    def children(self):
        return self.visualizer.children()

    def to_string(self):
        return '%s = %s' % (self.typename, self.visualizer.to_string())

    def display_hint(self):
        if hasattr(self.visualizer, 'display_hint'):
            return self.visualizer.display_hint()
        return None


class BitsetPrinter:
    "Print a std::bitset"

    def __init__(self, typename, val):
        self.typename = typename
        self.val = val
        self.bit_count = val.type.template_argument(0)

    def to_string(self):
        return '%s (length=%d)' % (self.typename, self.bit_count)

    def children(self):
        words = self.val['__first_']
        words_count = self.val['__n_words']
        bits_per_word = self.val['__bits_per_word']
        word_index = 0
        result = []

        while word_index < words_count:
            bit_index = 0
            word = words[word_index]
            while word != 0:
                if (word & 0x1) != 0:
                    result.append(
                        ('[%d]' % (word_index * bits_per_word + bit_index), 1))
                word >>= 1
                bit_index += 1
            word_index += 1

        return result


class SetPrinter:
    "Print a std::set or std::multiset"

    # Turn an RbtreeIterator into a pretty-print iterator.
    class _iterator(Iterator):
        def __init__(self, rbiter):
            self.rbiter = rbiter
            self.count = 0

        def __iter__(self):
            return self

        def __len__(self):
            return len(self.rbiter)

        def __next__(self):
            item = self.rbiter.__next__()
            item = item.dereference()['__value_']
            result = (('[%d]' % self.count), item)
            self.count += 1
            return result

    def __init__(self, typename, val):
        self.typename = typename
        self.val = val
        self.rbiter = RbtreeIterator(self.val['__tree_'])

    def to_string(self):
        length = len(self.rbiter)
        if length == 0:
            return 'empty %s' % self.typename
        else:
            return '%s (count=%d)' % (self.typename, int(length))

    def children(self):
        return self._iterator(self.rbiter)


#    def display_hint (self):
#        return 'set'


class RbtreeIterator(Iterator):
    def __init__(self, rbtree):
        self.node = rbtree['__begin_node_']
        self.size = pair_to_tuple(rbtree['__pair3_'])[0]
        self.node_pointer_type = gdb.lookup_type(
            rbtree.type.strip_typedefs().name + '::__node_pointer')
        self.count = 0

    def __iter__(self):
        return self

    def __len__(self):
        return int(self.size)

    def __next__(self):
        if self.count == self.size:
            raise StopIteration

        node = self.node.cast(self.node_pointer_type)
        result = node
        self.count += 1
        if self.count < self.size:
            # Compute the next node.
            if node.dereference()['__right_']:
                node = node.dereference()['__right_']
                while node.dereference()['__left_']:
                    node = node.dereference()['__left_']
            else:
                parent_node = node.dereference()['__parent_']
                while node != parent_node.dereference()['__left_']:
                    node = parent_node
                    parent_node = parent_node.dereference()['__parent_']
                node = parent_node

            self.node = node
        return result


class RbtreeIteratorPrinter:
    "Print std::set::iterator"

    def __init__(self, typename, val):
        self.val = val

    def to_string(self):
        return get_node_value_from_iterator(self.val)


class MapPrinter:
    "Print a std::map or std::multimap"

    # Turn an RbtreeIterator into a pretty-print iterator.
    class _iterator(Iterator):
        def __init__(self, rbiter):
            self.rbiter = rbiter
            self.count = 0

        def __iter__(self):
            return self

        def __len__(self):
            return len(self.rbiter)

        def __next__(self):
            item = self.rbiter.__next__()
            item = item.dereference()['__value_']
            result = ('[%d] %s' % (self.count, str(item['__cc']['first'])),
                      item['__cc']['second'])
            self.count += 1
            return result

    def __init__(self, typename, val):
        self.typename = typename
        self.val = val
        self.rbiter = RbtreeIterator(val['__tree_'])

    def to_string(self):
        length = len(self.rbiter)
        if length == 0:
            return 'empty %s' % self.typename
        else:
            return '%s (count=%d)' % (self.typename, int(length))

    def children(self):
        return self._iterator(self.rbiter)


#    def display_hint (self):
#        return 'map'


class MapIteratorPrinter:
    "Print std::map::iterator"

    def __init__(self, typename, val):
        self.val = val

    def to_string(self):
        value = get_node_value_from_iterator(self.val['__i_'])
        return '[%s] %s' % pair_to_tuple(value['__cc'])


class HashtableIterator(Iterator):
    def __init__(self, hashtable):
        self.node = pair_to_tuple(hashtable['__p1_'])[0]['__next_']
        self.size = pair_to_tuple(hashtable['__p2_'])[0]

    def __iter__(self):
        return self

    def __len__(self):
        return self.size

    def __next__(self):
        if self.node == 0:
            raise StopIteration
        hash_node_type = gdb.lookup_type(self.node.dereference().type.name +
                                         '::__node_pointer')
        node = self.node.cast(hash_node_type).dereference()
        self.node = node['__next_']
        value = node['__value_']
        try:
            # unordered_map's value is a union of __nc and __cc.
            value = value['__nc']
        except gdb.error:
            pass
        return value


class HashtableIteratorPrinter:
    "Print std::unordered_set::iterator"

    def __init__(self, typename, val):
        self.val = val

    def to_string(self):
        return get_node_value_from_pointer(self.val['__node_'])


class UnorderedMapIteratorPrinter:
    "Print std::unordered_map::iterator"

    def __init__(self, typename, val):
        self.val = val

    def to_string(self):
        value = get_node_value_from_pointer(self.val['__i_']['__node_'])
        return '[%s] %s' % pair_to_tuple(value['__cc'])


class UnorderedSetPrinter:
    "Print a std::unordered_set"

    def __init__(self, typename, val):
        self.typename = typename
        self.val = val
        self.hashtable = val['__table_']
        self.size = pair_to_tuple(self.hashtable['__p2_'])[0]
        self.hashtableiter = HashtableIterator(self.hashtable)

    def hashtable(self):
        return self.hashtable

    def to_string(self):
        if self.size == 0:
            return 'empty %s' % self.typename
        else:
            return '%s (count=%d)' % (self.typename, self.size)

    def children(self):
        result = []
        count = 0
        for elt in self.hashtableiter:
            result.append(('[%d]' % count, str(elt)))
            count += 1
        return result


class UnorderedMapPrinter:
    "Print a std::unordered_map"

    def __init__(self, typename, val):
        self.typename = typename
        self.val = val
        self.hashtable = val['__table_']
        self.size = pair_to_tuple(self.hashtable['__p2_'])[0]
        self.hashtableiter = HashtableIterator(self.hashtable)

    def hashtable(self):
        return self.hashtable

    def to_string(self):
        if self.size == 0:
            return 'empty %s' % self.typename
        else:
            return '%s (count=%d)' % (self.typename, self.size)

    def children(self):
        result = []
        count = 0
        for elt in self.hashtableiter:
            result.append(('[%d] %s' % (count, str(elt['__cc']['first'])),
                           elt['__cc']['second']))
            count += 1
        return result


#    def display_hint (self):
#        return 'map'

# A "regular expression" printer which conforms to the
# "SubPrettyPrinter" protocol from gdb.printing.


class RxPrinter(object):
    def __init__(self, name, function):
        super(RxPrinter, self).__init__()
        self.name = name
        self.function = function
        self.enabled = True

    def invoke(self, value):
        if not self.enabled:
            return None
        return self.function(self.name, value)


# A pretty-printer that conforms to the "PrettyPrinter" protocol from
# gdb.printing.  It can also be used directly as an old-style printer.


class Printer(object):
    def __init__(self, name):
        super(Printer, self).__init__()
        self.name = name
        self.subprinters = []
        self.lookup = []
        self.enabled = True

    def add(self, name, function):
        printer = RxPrinter('std::' + name, function)
        self.subprinters.append(printer)
        self.lookup.append((make_type_re(name), printer))

    @staticmethod
    def get_basic_type(type):
        # If it points to a reference, get the reference.
        if type.code == gdb.TYPE_CODE_REF:
            type = type.target()

        # Get the unqualified type, stripped of typedefs.
        type = type.unqualified().strip_typedefs()

        return type.tag

    def __call__(self, val):
        typename = self.get_basic_type(val.type)
        if not typename:
            return None

        for (regexp, printer) in self.lookup:
            if regexp.match(typename):
                return printer.invoke(val)

        # Cannot find a pretty printer.  Return None.
        return None


printer = None


class FilteringTypePrinter(object):
    def __init__(self, match, name):
        self.match = match
        self.name = name
        self.enabled = True

    class _recognizer(object):
        def __init__(self, match, name):
            self.match = match
            self.name = name
            self.type_obj = None

        def recognize(self, type_obj):
            if type_obj.tag is None:
                return None

            if self.type_obj is None:
                if not self.match in type_obj.tag:
                    # Filter didn't match.
                    return None
                try:
                    self.type_obj = gdb.lookup_type(self.name).strip_typedefs()
                except:
                    pass
            if self.type_obj == type_obj:
                return self.name
            return None

    def instantiate(self):
        return self._recognizer(self.match, self.name)


def add_one_type_printer(obj, match, name):
    printer = FilteringTypePrinter(match, 'std::' + name)
    gdb.types.register_type_printer(obj, printer)


def register_type_printers(obj):
    global _use_type_printing

    if not _use_type_printing:
        return

    for pfx in ('', 'w'):
        add_one_type_printer(obj, 'basic_string', pfx + 'string')
        add_one_type_printer(obj, 'basic_ios', pfx + 'ios')
        add_one_type_printer(obj, 'basic_streambuf', pfx + 'streambuf')
        add_one_type_printer(obj, 'basic_istream', pfx + 'istream')
        add_one_type_printer(obj, 'basic_ostream', pfx + 'ostream')
        add_one_type_printer(obj, 'basic_iostream', pfx + 'iostream')
        add_one_type_printer(obj, 'basic_stringbuf', pfx + 'stringbuf')
        add_one_type_printer(obj, 'basic_istringstream', pfx + 'istringstream')
        add_one_type_printer(obj, 'basic_ostringstream', pfx + 'ostringstream')
        add_one_type_printer(obj, 'basic_stringstream', pfx + 'stringstream')
        add_one_type_printer(obj, 'basic_filebuf', pfx + 'filebuf')
        add_one_type_printer(obj, 'basic_ifstream', pfx + 'ifstream')
        add_one_type_printer(obj, 'basic_ofstream', pfx + 'ofstream')
        add_one_type_printer(obj, 'basic_fstream', pfx + 'fstream')
        add_one_type_printer(obj, 'basic_regex', pfx + 'regex')
        add_one_type_printer(obj, 'sub_match', pfx + 'csub_match')
        add_one_type_printer(obj, 'sub_match', pfx + 'ssub_match')
        add_one_type_printer(obj, 'match_results', pfx + 'cmatch')
        add_one_type_printer(obj, 'match_results', pfx + 'smatch')
        add_one_type_printer(obj, 'regex_iterator', pfx + 'cregex_iterator')
        add_one_type_printer(obj, 'regex_iterator', pfx + 'sregex_iterator')
        add_one_type_printer(obj, 'regex_token_iterator',
                             pfx + 'cregex_token_iterator')
        add_one_type_printer(obj, 'regex_token_iterator',
                             pfx + 'sregex_token_iterator')

    # Note that we can't have a printer for std::wstreampos, because
    # it shares the same underlying type as std::streampos.
    add_one_type_printer(obj, 'fpos', 'streampos')
    add_one_type_printer(obj, 'basic_string', 'u16string')
    add_one_type_printer(obj, 'basic_string', 'u32string')

    for dur in ('nanoseconds', 'microseconds', 'milliseconds', 'seconds',
                'minutes', 'hours'):
        add_one_type_printer(obj, 'duration', dur)

    add_one_type_printer(obj, 'linear_congruential_engine', 'minstd_rand0')
    add_one_type_printer(obj, 'linear_congruential_engine', 'minstd_rand')
    add_one_type_printer(obj, 'mersenne_twister_engine', 'mt19937')
    add_one_type_printer(obj, 'mersenne_twister_engine', 'mt19937_64')
    add_one_type_printer(obj, 'subtract_with_carry_engine', 'ranlux24_base')
    add_one_type_printer(obj, 'subtract_with_carry_engine', 'ranlux48_base')
    add_one_type_printer(obj, 'discard_block_engine', 'ranlux24')
    add_one_type_printer(obj, 'discard_block_engine', 'ranlux48')
    add_one_type_printer(obj, 'shuffle_order_engine', 'knuth_b')


def register_libcxx_printers(obj):
    "Register libc++ pretty-printers with objfile Obj."

    global _use_gdb_pp
    global printer

    if _use_gdb_pp:
        gdb.printing.register_pretty_printer(obj, printer)
    else:
        if obj is None:
            obj = gdb
        obj.pretty_printers.append(printer)

    register_type_printers(obj)


def build_libcxx_dictionary():
    global printer

    printer = Printer("libc++")

    # libc++ objects requiring pretty-printing.
    printer.add('basic_string', StringPrinter)
    printer.add('bitset', BitsetPrinter)
    printer.add('deque', DequePrinter)
    printer.add('list', ListPrinter)
    printer.add('map', MapPrinter)
    printer.add('multimap', MapPrinter)
    printer.add('multiset', SetPrinter)
    printer.add('priority_queue', StackOrQueuePrinter)
    printer.add('queue', StackOrQueuePrinter)
    printer.add('tuple', TuplePrinter)
    printer.add('pair', PairPrinter)
    printer.add('set', SetPrinter)
    printer.add('stack', StackOrQueuePrinter)
    printer.add('unique_ptr', UniquePointerPrinter)
    printer.add('vector', VectorPrinter)  # Includes vector<bool>.
    printer.add('shared_ptr', SharedPointerPrinter)
    printer.add('weak_ptr', SharedPointerPrinter)
    printer.add('unordered_map', UnorderedMapPrinter)
    printer.add('unordered_set', UnorderedSetPrinter)
    printer.add('unordered_multimap', UnorderedMapPrinter)
    printer.add('unordered_multiset', UnorderedSetPrinter)
    printer.add('forward_list', ForwardListPrinter)
    # For std::array the default GDB pretty-printer seems reasonable.

    printer.add('__list_iterator', ListIteratorPrinter)
    printer.add('__list_const_iterator', ListIteratorPrinter)
    printer.add('__tree_iterator', RbtreeIteratorPrinter)
    printer.add('__tree_const_iterator', RbtreeIteratorPrinter)
    printer.add('__hash_iterator', HashtableIteratorPrinter)
    printer.add('__hash_const_iterator', HashtableIteratorPrinter)
    printer.add('__hash_map_iterator', UnorderedMapIteratorPrinter)
    printer.add('__hash_map_const_iterator', UnorderedMapIteratorPrinter)
    printer.add('__map_iterator', MapIteratorPrinter)
    printer.add('__map_const_iterator', MapIteratorPrinter)
    printer.add('__deque_iterator', DequeIteratorPrinter)
    printer.add('__wrap_iter', VectorIteratorPrinter)
    printer.add('__bit_iterator', VectorBoolIteratorPrinter)


build_libcxx_dictionary()
