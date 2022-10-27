# Copyright 2015 Google Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import io
import json
import math
import os
import sys
import unittest
from collections import OrderedDict
from string import printable

import json5

try:
    # Make the `hypothesis` library optional, so that the other tests will
    # still run if it isn't installed.
    import hypothesis.strategies as some
    from hypothesis import given

    some_json = some.recursive(
        some.none() |
        some.booleans() |
        some.floats(allow_nan=False) |
        some.text(printable),
        lambda children: some.lists(children, min_size=1)
        | some.dictionaries(some.text(printable), children, min_size=1),
    )
except ImportError as e:
    def given(x):
        def func(y):
            pass
        return func
    some_json = {}

class TestLoads(unittest.TestCase):
    maxDiff = None

    def check(self, s, obj):
        self.assertEqual(json5.loads(s), obj)

    def check_fail(self, s, err=None):
        try:
            json5.loads(s)
            self.fail()  # pragma: no cover
        except ValueError as e:
            if err:
                self.assertEqual(err, str(e))

    def test_arrays(self):
        self.check('[]', [])
        self.check('[0]', [0])
        self.check('[0,1]', [0, 1])
        self.check('[ 0 , 1 ]', [0, 1])

        try:
            json5.loads('[ ,]')
            self.fail()
        except ValueError as e:
            self.assertIn('Unexpected "," at column 3', str(e))

    def test_bools(self):
        self.check('true', True)
        self.check('false', False)

    def test_cls_is_not_supported(self):
        self.assertRaises(AssertionError, json5.loads, '1', cls=lambda x: x)

    def test_duplicate_keys_should_be_allowed(self):
        self.assertEqual(json5.loads('{foo: 1, foo: 2}',
                                     allow_duplicate_keys=True),
                         {"foo": 2})

    def test_duplicate_keys_should_be_allowed_by_default(self):
        self.check('{foo: 1, foo: 2}', {"foo": 2})

    def test_duplicate_keys_should_not_be_allowed(self):
        self.assertRaises(ValueError, json5.loads, '{foo: 1, foo: 2}',
                          allow_duplicate_keys=False)

    def test_empty_strings_are_errors(self):
        self.check_fail('', 'Empty strings are not legal JSON5')

    def test_encoding(self):
        if sys.version_info[0] < 3:
            s = '"\xf6"'
        else:
            s = b'"\xf6"'
        self.assertEqual(json5.loads(s, encoding='iso-8859-1'),
                         u'\xf6')

    def test_numbers(self):
        # decimal literals
        self.check('1', 1)
        self.check('-1', -1)
        self.check('+1', 1)

        # hex literals
        self.check('0xf', 15)
        self.check('0xfe', 254)
        self.check('0xfff', 4095)
        self.check('0XABCD', 43981)
        self.check('0x123456', 1193046)

        # floats
        self.check('1.5', 1.5)
        self.check('1.5e3', 1500.0)
        self.check('-0.5e-2', -0.005)

        # names
        self.check('Infinity', float('inf'))
        self.check('-Infinity', float('-inf'))
        self.assertTrue(math.isnan(json5.loads('NaN')))
        self.assertTrue(math.isnan(json5.loads('-NaN')))

        # syntax errors
        self.check_fail('14d', '<string>:1 Unexpected "d" at column 3')

    def test_identifiers(self):
        self.check('{a: 1}', {'a': 1})
        self.check('{$: 1}', {'$': 1})
        self.check('{_: 1}', {'_': 1})
        self.check('{a_b: 1}', {'a_b': 1})
        self.check('{a$: 1}', {'a$': 1})

        # This valid JavaScript but not valid JSON5; keys must be identifiers
        # or strings.
        self.check_fail('{1: 1}')

    def test_identifiers_unicode(self):
        self.check(u'{\xc3: 1}', {u'\xc3': 1})

    def test_null(self):
        self.check('null', None)

    def test_object_hook(self):
        hook = lambda d: [d]
        self.assertEqual(json5.loads('{foo: 1}', object_hook=hook),
                         [{"foo": 1}])

    def test_object_pairs_hook(self):
        hook = lambda pairs: pairs
        self.assertEqual(json5.loads('{foo: 1, bar: 2}',
                                     object_pairs_hook=hook),
                         [('foo', 1), ('bar', 2)])

    def test_objects(self):
        self.check('{}', {})
        self.check('{"foo": 0}', {"foo": 0})
        self.check('{"foo":0,"bar":1}', {"foo": 0, "bar": 1})
        self.check('{ "foo" : 0 , "bar" : 1 }', {"foo": 0, "bar": 1})

    def test_parse_constant(self):
        hook = lambda x: x
        self.assertEqual(json5.loads('-Infinity', parse_constant=hook),
                         '-Infinity')
        self.assertEqual(json5.loads('NaN', parse_constant=hook),
                         'NaN')

    def test_parse_float(self):
        hook = lambda x: x
        self.assertEqual(json5.loads('1.0', parse_float=hook), '1.0')

    def test_parse_int(self):
        hook = lambda x, base=10: x
        self.assertEqual(json5.loads('1', parse_int=hook), '1')

    def test_sample_file(self):
        path = os.path.join(os.path.dirname(__file__), '..', 'sample.json5')
        with open(path) as fp:
            obj = json5.load(fp)
        self.assertEqual({
            u'oh': [
                u"we shouldn't forget",
                u"arrays can have",
                u"trailing commas too",
            ],
            u"this": u"is a multi-line string",
            u"delta": 10,
            u"hex": 3735928559,
            u"finally": "a trailing comma",
            u"here": "is another",
            u"to": float("inf"),
            u"while": True,
            u"half": 0.5,
            u"foo": u"bar"
            }, obj)

    def test_strings(self):
        self.check('"foo"', 'foo')
        self.check("'foo'", 'foo')

        # escape chars
        self.check("'\\b\\t\\f\\n\\r\\v\\\\'", '\b\t\f\n\r\v\\')
        self.check("'\\''", "'")
        self.check('"\\""', '"')

        # hex literals
        self.check('"\\x66oo"', 'foo')

        # unicode literals
        self.check('"\\u0066oo"', 'foo')

        # string literals w/ continuation markers at the end of the line.
        # These should not have spaces is the result.
        self.check('"foo\\\nbar"', 'foobar')
        self.check("'foo\\\nbar'", 'foobar')

        # unterminated string literals.
        self.check_fail('"\n')
        self.check_fail("'\n")

        # bad hex literals
        self.check_fail("'\\x0'")
        self.check_fail("'\\xj'")
        self.check_fail("'\\x0j'")

        # bad unicode literals
        self.check_fail("'\\u0'")
        self.check_fail("'\\u00'")
        self.check_fail("'\\u000'")
        self.check_fail("'\\u000j'")
        self.check_fail("'\\u00j0'")
        self.check_fail("'\\u0j00'")
        self.check_fail("'\\uj000'")

    def test_unrecognized_escape_char(self):
        self.check(r'"\/"', '/')

    def test_nul(self):
        self.check(r'"\0"', '\x00')

    def test_whitespace(self):
        self.check('\n1', 1)
        self.check('\r1', 1)
        self.check('\r\n1', 1)
        self.check('\t1', 1)
        self.check('\v1', 1)
        self.check(u'\uFEFF 1', 1)
        self.check(u'\u00A0 1', 1)
        self.check(u'\u2028 1', 1)
        self.check(u'\u2029 1', 1)

    def test_error_reporting(self):
        self.check_fail('[ ,]',
            err='<string>:1 Unexpected "," at column 3')

        self.check_fail(
            '{\n'
            '    version: "1.0",\n'
            '    author: "John Smith",\n'
            '    people : [\n'
            '        "Monty",\n'
            '        "Python"foo\n'
            '    ]\n'
            '}\n',
            err='<string>:6 Unexpected "f" at column 17')


class TestDump(unittest.TestCase):
    def test_basic(self):
        sio = io.StringIO()
        json5.dump(True, sio)
        self.assertEqual('true', sio.getvalue())


class TestDumps(unittest.TestCase):
    maxDiff = None

    def check(self, obj, s):
        self.assertEqual(json5.dumps(obj), s)

    def test_allow_duplicate_keys(self):
        self.assertIn(json5.dumps({1: "foo", "1": "bar"}),
                      {'{"1": "foo", "1": "bar"}',
                       '{"1": "bar", "1": "foo"}'})

        self.assertRaises(ValueError, json5.dumps,
                          {1: "foo", "1": "bar"},
                           allow_duplicate_keys=False)

    def test_arrays(self):
        self.check([], '[]')
        self.check([1, 2, 3], '[1, 2, 3]')
        self.check([{'foo': 'bar'}, {'baz': 'quux'}],
                    '[{foo: "bar"}, {baz: "quux"}]')

    def test_bools(self):
        self.check(True, 'true')
        self.check(False, 'false')

    def test_check_circular(self):
        # This tests a trivial cycle.
        l = [1, 2, 3]
        l[2] = l
        self.assertRaises(ValueError, json5.dumps, l)

        # This checks that json5 doesn't raise an error. However,
        # the underlying Python implementation likely will.
        try:
            json5.dumps(l, check_circular=False)
            self.fail()  # pragma: no cover
        except Exception as e:
            self.assertNotIn(str(e), 'Circular reference detected')

        # This checks that repeated but non-circular references
        # are okay.
        x = [1, 2]
        y = {"foo": x, "bar": x}
        self.check(y,
                   '{foo: [1, 2], bar: [1, 2]}')

        # This tests a more complicated cycle.
        x = {}
        y = {}
        z = {}
        z['x'] = x
        z['y'] = y
        z['x']['y'] = y
        z['y']['x'] = x
        self.assertRaises(ValueError, json5.dumps, z)

    def test_custom_arrays(self):
        class MyArray(object):
            def __iter__(self):
                yield 0
                yield 1
                yield 1

            def __getitem__(self, i):
                return 0 if i == 0 else 1

            def __len__(self):
                return 3

        self.assertEqual(json5.dumps(MyArray()), '[0, 1, 1]')

    def test_custom_numbers(self):
        # See https://github.com/dpranke/pyjson5/issues/57: we
        # need to ensure that we use the bare int.__repr__ and
        # float.__repr__ in order to get legal JSON values when
        # people have custom subclasses with customer __repr__ methods.
        # (This is what JSON does and we want to match it).
        class MyInt(int):
            def __repr__(self):
                return 'fail'

        self.assertEqual(json5.dumps(MyInt(5)), '5')

        class MyFloat(float):
            def __repr__(self):
                return 'fail'

        self.assertEqual(json5.dumps(MyFloat(0.5)), '0.5')

    def test_custom_objects(self):
        class MyDict(object):
            def __iter__(self):
                yield ('a', 1)
                yield ('b', 2)

            def keys(self):
                return ['a', 'b']

            def __getitem__(self, k):
                return {'a': 1, 'b': 2}[k]

            def __len__(self):
                return 2

        self.assertEqual(json5.dumps(MyDict()), '{a: 1, b: 2}')

    def test_custom_strings(self):
        class MyStr(str):
            pass

        self.assertEqual(json5.dumps({'foo': MyStr('bar')}), '{foo: "bar"}')

    def test_default(self):

        def _custom_serializer(obj):
            del obj
            return 'something'

        self.assertRaises(TypeError, json5.dumps, set())
        self.assertEqual(json5.dumps(set(), default=_custom_serializer),
                         '"something"')

    def test_ensure_ascii(self):
        self.check(u'\u00fc', '"\\u00fc"')
        self.assertEqual(json5.dumps(u'\u00fc', ensure_ascii=False),
                         u'"\u00fc"')

    def test_indent(self):
        self.assertEqual(json5.dumps([1, 2, 3], indent=None),
                         u'[1, 2, 3]')
        self.assertEqual(json5.dumps([1, 2, 3], indent=-1),
                         u'[\n1,\n2,\n3,\n]')
        self.assertEqual(json5.dumps([1, 2, 3], indent=0),
                         u'[\n1,\n2,\n3,\n]')
        self.assertEqual(json5.dumps([], indent=2),
                         u'[]')
        self.assertEqual(json5.dumps([1, 2, 3], indent=2),
                         u'[\n  1,\n  2,\n  3,\n]')
        self.assertEqual(json5.dumps([1, 2, 3], indent=' '),
                         u'[\n 1,\n 2,\n 3,\n]')
        self.assertEqual(json5.dumps([1, 2, 3], indent='++'),
                         u'[\n++1,\n++2,\n++3,\n]')
        self.assertEqual(json5.dumps([[1, 2, 3]], indent=2),
                         u'[\n  [\n    1,\n    2,\n    3,\n  ],\n]')

        self.assertEqual(json5.dumps({}, indent=2),
                         u'{}')
        self.assertEqual(json5.dumps({'foo': 'bar', 'baz': 'quux'}, indent=2),
                         u'{\n  foo: "bar",\n  baz: "quux",\n}')

    def test_numbers(self):
        self.check(15, '15')
        self.check(1.0, '1.0')
        self.check(float('inf'), 'Infinity')
        self.check(float('-inf'), '-Infinity')
        self.check(float('nan'), 'NaN')

        self.assertRaises(ValueError, json5.dumps,
                          float('inf'), allow_nan=False)
        self.assertRaises(ValueError, json5.dumps,
                          float('-inf'), allow_nan=False)
        self.assertRaises(ValueError, json5.dumps,
                          float('nan'), allow_nan=False)

    def test_null(self):
        self.check(None, 'null')

    def test_objects(self):
        self.check({'foo': 1}, '{foo: 1}')
        self.check({'foo bar': 1}, '{"foo bar": 1}')
        self.check({'1': 1}, '{"1": 1}')

    def test_reserved_words_in_object_keys_are_quoted(self):
        self.check({'new': 1}, '{"new": 1}')

    def test_identifiers_only_starting_with_reserved_words_are_not_quoted(self):
        self.check({'newbie': 1}, '{newbie: 1}')

    def test_non_string_keys(self):
        self.assertEqual(json5.dumps({False: 'a', 1: 'b', 2.0: 'c', None: 'd'}),
                         '{"false": "a", "1": "b", "2.0": "c", "null": "d"}')

    def test_quote_keys(self):
        self.assertEqual(json5.dumps({"foo": 1}, quote_keys=True),
                         '{"foo": 1}')

    def test_strings(self):
        self.check("'single'", '"\'single\'"')
        self.check('"double"', '"\\"double\\""')
        self.check("'single \\' and double \"'",
                   '"\'single \\\\\' and double \\"\'"')

    def test_string_escape_sequences(self):
        self.check(u'\u2028\u2029\b\t\f\n\r\v\\\0',
                   '"\\u2028\\u2029\\b\\t\\f\\n\\r\\v\\\\\\0"')

    def test_skip_keys(self):
        od = OrderedDict()
        od[(1, 2)] = 2
        self.assertRaises(TypeError, json5.dumps, od)
        self.assertEqual(json5.dumps(od, skipkeys=True), '{}')

        od['foo'] = 1
        self.assertEqual(json5.dumps(od, skipkeys=True), '{foo: 1}')

        # Also test that having an invalid key as the last element
        # doesn't incorrectly add a trailing comma (see
        # https://github.com/dpranke/pyjson5/issues/33).
        od = OrderedDict()
        od['foo'] = 1
        od[(1, 2)] = 2
        self.assertEqual(json5.dumps(od, skipkeys=True), '{foo: 1}')

    def test_sort_keys(self):
        od = OrderedDict()
        od['foo'] = 1
        od['bar'] = 2
        self.assertEqual(json5.dumps(od, sort_keys=True),
                         '{bar: 2, foo: 1}')

    def test_trailing_commas(self):
        # By default, multi-line dicts and lists should have trailing
        # commas after their last items.
        self.assertEqual(json5.dumps({"foo": 1}, indent=2),
                         '{\n  foo: 1,\n}')
        self.assertEqual(json5.dumps([1], indent=2),
                         '[\n  1,\n]')

        self.assertEqual(json5.dumps({"foo": 1}, indent=2,
                                     trailing_commas=False),
                         '{\n  foo: 1\n}')
        self.assertEqual(json5.dumps([1], indent=2, trailing_commas=False),
                         '[\n  1\n]')

    def test_supplemental_unicode(self):
        try:
            s = chr(0x10000)
            self.check(s, '"\\ud800\\udc00"')
        except ValueError:
            # Python2 doesn't support supplemental unicode planes, so
            # we can't test this there.
            pass

    def test_empty_key(self):
        self.assertEqual(json5.dumps({'': 'value'}), '{"": "value"}')

    @given(some_json)
    def test_object_roundtrip(self, input_object):
        dumped_string_json = json.dumps(input_object)
        dumped_string_json5 = json5.dumps(input_object)

        parsed_object_json = json5.loads(dumped_string_json)
        parsed_object_json5 = json5.loads(dumped_string_json5)

        assert parsed_object_json == input_object
        assert parsed_object_json5 == input_object


if __name__ == '__main__':  # pragma: no cover
    unittest.main()
