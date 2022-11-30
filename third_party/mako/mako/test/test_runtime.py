"""Assorted runtime unit tests
"""
from mako import runtime
from mako.testing.assertions import eq_


class ContextTest:
    def test_locals_kwargs(self):
        c = runtime.Context(None, foo="bar")
        eq_(c.kwargs, {"foo": "bar"})

        d = c._locals({"zig": "zag"})

        # kwargs is the original args sent to the Context,
        # it's intentionally kept separate from _data
        eq_(c.kwargs, {"foo": "bar"})
        eq_(d.kwargs, {"foo": "bar"})

        eq_(d._data["zig"], "zag")
