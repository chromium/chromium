import os
import unittest

from mako import compat
from mako import exceptions
from mako import lookup
from mako import runtime
from mako.template import Template
from mako.util import FastEncodingBuffer
from test import assert_raises_message
from test import eq_
from test import template_base
from test.util import result_lines

tl = lookup.TemplateLookup(directories=[template_base])


class LookupTest(unittest.TestCase):
    def test_basic(self):
        t = tl.get_template("index.html")
        assert result_lines(t.render()) == ["this is index"]

    def test_subdir(self):
        t = tl.get_template("/subdir/index.html")
        assert result_lines(t.render()) == [
            "this is sub index",
            "this is include 2",
        ]

        assert (
            tl.get_template("/subdir/index.html").module_id
            == "_subdir_index_html"
        )

    def test_updir(self):
        t = tl.get_template("/subdir/foo/../bar/../index.html")
        assert result_lines(t.render()) == [
            "this is sub index",
            "this is include 2",
        ]

    def test_directory_lookup(self):
        """test that hitting an existent directory still raises
        LookupError."""

        self.assertRaises(
            exceptions.TopLevelLookupException, tl.get_template, "/subdir"
        )

    def test_no_lookup(self):
        t = Template("hi <%include file='foo.html'/>")
        try:
            t.render()
            assert False
        except exceptions.TemplateLookupException:
            eq_(
                str(compat.exception_as()),
                "Template 'memory:%s' has no TemplateLookup associated"
                % hex(id(t)),
            )

    def test_uri_adjust(self):
        tl = lookup.TemplateLookup(directories=["/foo/bar"])
        assert (
            tl.filename_to_uri("/foo/bar/etc/lala/index.html")
            == "/etc/lala/index.html"
        )

        tl = lookup.TemplateLookup(directories=["./foo/bar"])
        assert (
            tl.filename_to_uri("./foo/bar/etc/index.html") == "/etc/index.html"
        )

    def test_uri_cache(self):
        """test that the _uri_cache dictionary is available"""
        tl._uri_cache[("foo", "bar")] = "/some/path"
        assert tl._uri_cache[("foo", "bar")] == "/some/path"

    def test_check_not_found(self):
        tl = lookup.TemplateLookup()
        tl.put_string("foo", "this is a template")
        f = tl.get_template("foo")
        assert f.uri in tl._collection
        f.filename = "nonexistent"
        self.assertRaises(
            exceptions.TemplateLookupException, tl.get_template, "foo"
        )
        assert f.uri not in tl._collection

    def test_dont_accept_relative_outside_of_root(self):
        """test the mechanics of an include where
        the include goes outside of the path"""
        tl = lookup.TemplateLookup(
            directories=[os.path.join(template_base, "subdir")]
        )
        index = tl.get_template("index.html")

        ctx = runtime.Context(FastEncodingBuffer())
        ctx._with_template = index

        assert_raises_message(
            exceptions.TemplateLookupException,
            'Template uri "../index.html" is invalid - it '
            "cannot be relative outside of the root path",
            runtime._lookup_template,
            ctx,
            "../index.html",
            index.uri,
        )

        assert_raises_message(
            exceptions.TemplateLookupException,
            'Template uri "../othersubdir/foo.html" is invalid - it '
            "cannot be relative outside of the root path",
            runtime._lookup_template,
            ctx,
            "../othersubdir/foo.html",
            index.uri,
        )

        # this is OK since the .. cancels out
        runtime._lookup_template(ctx, "foo/../index.html", index.uri)
