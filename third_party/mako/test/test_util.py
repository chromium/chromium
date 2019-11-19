# -*- coding: utf-8 -*-

import os
import unittest

from mako import compat
from mako import exceptions
from mako import util
from mako.compat import u
from test import assert_raises_message
from test import eq_
from test import skip_if


class UtilTest(unittest.TestCase):
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
        s = u("drôl m’a rée « S’il")
        buf = util.FastEncodingBuffer(encoding="utf-8")
        buf.write(s[0:10])
        buf.write(s[10:])
        eq_(buf.getvalue(), s.encode("utf-8"))

    def test_read_file(self):
        fn = os.path.join(os.path.dirname(__file__), "test_util.py")
        data = util.read_file(fn, "rb")
        assert "test_util" in str(data)  # str() for py3k

    @skip_if(lambda: compat.pypy, "Pypy does this differently")
    def test_load_module(self):
        fn = os.path.join(os.path.dirname(__file__), "test_util.py")
        module = compat.load_module("mako.template", fn)
        import mako.template

        self.assertEqual(module, mako.template)

    def test_load_plugin_failure(self):
        loader = util.PluginLoader("fakegroup")
        assert_raises_message(
            exceptions.RuntimeException,
            "Can't load plugin fakegroup fake",
            loader.load,
            "fake",
        )
