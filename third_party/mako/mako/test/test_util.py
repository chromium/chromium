import os
import sys

import pytest

from mako import compat
from mako import exceptions
from mako import util
from mako.testing.assertions import assert_raises_message
from mako.testing.assertions import eq_
from mako.testing.assertions import in_
from mako.testing.assertions import ne_
from mako.testing.assertions import not_in


class UtilTest:
    def test_fast_buffer_write(self):
        buf = util.FastEncodingBuffer()
        buf.write("string a ")
        buf.write("string b")
        eq_(buf.getvalue(), "string a string b")

    def test_fast_buffer_truncate(self):
        buf = util.FastEncodingBuffer()
        buf.write("string a ")
        buf.write("string b")
        buf.truncate()
        buf.write("string c ")
        buf.write("string d")
        eq_(buf.getvalue(), "string c string d")

    def test_fast_buffer_encoded(self):
        s = "drôl m’a rée « S’il"
        buf = util.FastEncodingBuffer(encoding="utf-8")
        buf.write(s[0:10])
        buf.write(s[10:])
        eq_(buf.getvalue(), s.encode("utf-8"))

    def test_read_file(self):
        fn = os.path.join(os.path.dirname(__file__), "test_util.py")
        data = util.read_file(fn, "rb")
        assert b"test_util" in data

    @pytest.mark.skipif(compat.pypy, reason="Pypy does this differently")
    def test_load_module(self):
        path = os.path.join(os.path.dirname(__file__), "module_to_import.py")
        some_module = compat.load_module("test.module_to_import", path)

        not_in("test.module_to_import", sys.modules)
        in_("some_function", dir(some_module))
        import test.module_to_import

        ne_(some_module, test.module_to_import)

    def test_load_plugin_failure(self):
        loader = util.PluginLoader("fakegroup")
        assert_raises_message(
            exceptions.RuntimeException,
            "Can't load plugin fakegroup fake",
            loader.load,
            "fake",
        )
