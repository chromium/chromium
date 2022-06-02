# -*- coding: utf-8 -*-

import unittest

from mako import compat
from mako.compat import u
from mako.template import Template
from test import eq_
from test import requires_python_2
from test import TemplateTest
from test.util import flatten_result
from test.util import result_lines


class FilterTest(TemplateTest):
    def test_basic(self):
        t = Template(
            """
        ${x | myfilter}
"""
        )
        assert (
            flatten_result(
                t.render(
                    x="this is x",
                    myfilter=lambda t: "MYFILTER->%s<-MYFILTER" % t,
                )
            )
            == "MYFILTER->this is x<-MYFILTER"
        )

    def test_expr(self):
        """test filters that are themselves expressions"""
        t = Template(
            """
        ${x | myfilter(y)}
"""
        )

        def myfilter(y):
            return lambda x: "MYFILTER->%s<-%s" % (x, y)

        assert (
            flatten_result(
                t.render(x="this is x", myfilter=myfilter, y="this is y")
            )
            == "MYFILTER->this is x<-this is y"
        )

    def test_convert_str(self):
        """test that string conversion happens in expressions before
        sending to filters"""
        t = Template(
            """
            ${x | trim}
        """
        )
        assert flatten_result(t.render(x=5)) == "5"

    def test_quoting(self):
        t = Template(
            """
            foo ${bar | h}
        """
        )

        eq_(
            flatten_result(t.render(bar="<'some bar'>")),
            "foo &lt;&#39;some bar&#39;&gt;",
        )

    def test_url_escaping(self):
        t = Template(
            """
            http://example.com/?bar=${bar | u}&v=1
        """
        )

        eq_(
            flatten_result(t.render(bar=u"酒吧bar")),
            "http://example.com/?bar=%E9%85%92%E5%90%A7bar&v=1",
        )

    def test_entity(self):
        t = Template("foo ${bar | entity}")
        eq_(
            flatten_result(t.render(bar="<'some bar'>")),
            "foo &lt;'some bar'&gt;",
        )

    @requires_python_2
    def test_quoting_non_unicode(self):
        t = Template(
            """
            foo ${bar | h}
        """,
            disable_unicode=True,
            output_encoding=None,
        )

        eq_(
            flatten_result(t.render(bar="<'привет'>")),
            "foo &lt;&#39;привет&#39;&gt;",
        )

    @requires_python_2
    def test_url_escaping_non_unicode(self):
        t = Template(
            """
            http://example.com/?bar=${bar | u}&v=1
        """,
            disable_unicode=True,
            output_encoding=None,
        )

        eq_(
            flatten_result(t.render(bar="酒吧bar")),
            "http://example.com/?bar=%E9%85%92%E5%90%A7bar&v=1",
        )

    def test_def(self):
        t = Template(
            """
            <%def name="foo()" filter="myfilter">
                this is foo
            </%def>
            ${foo()}
"""
        )

        eq_(
            flatten_result(
                t.render(
                    x="this is x",
                    myfilter=lambda t: "MYFILTER->%s<-MYFILTER" % t,
                )
            ),
            "MYFILTER-> this is foo <-MYFILTER",
        )

    def test_import(self):
        t = Template(
            """
        <%!
            from mako import filters
        %>\
        trim this string: """
            """${"  some string to trim   " | filters.trim} continue\
        """
        )

        assert (
            t.render().strip()
            == "trim this string: some string to trim continue"
        )

    def test_import_2(self):
        t = Template(
            """
        trim this string: """
            """${"  some string to trim   " | filters.trim} continue\
        """,
            imports=["from mako import filters"],
        )
        # print t.code
        assert (
            t.render().strip()
            == "trim this string: some string to trim continue"
        )

    def test_encode_filter(self):
        t = Template(
            """# coding: utf-8
            some stuff.... ${x}
        """,
            default_filters=["decode.utf8"],
        )
        eq_(
            t.render_unicode(x=u("voix m’a réveillé")).strip(),
            u("some stuff.... voix m’a réveillé"),
        )

    def test_encode_filter_non_str(self):
        t = Template(
            """# coding: utf-8
            some stuff.... ${x}
        """,
            default_filters=["decode.utf8"],
        )
        eq_(t.render_unicode(x=3).strip(), u("some stuff.... 3"))

    @requires_python_2
    def test_encode_filter_non_str_we_return_bytes(self):
        class Foo(object):
            def __str__(self):
                return compat.b("å")

        t = Template(
            """# coding: utf-8
            some stuff.... ${x}
        """,
            default_filters=["decode.utf8"],
        )
        eq_(t.render_unicode(x=Foo()).strip(), u("some stuff.... å"))

    def test_custom_default(self):
        t = Template(
            """
        <%!
            def myfilter(x):
                return "->" + x + "<-"
        %>

            hi ${'there'}
        """,
            default_filters=["myfilter"],
        )
        assert t.render().strip() == "hi ->there<-"

    def test_global(self):
        t = Template(
            """
            <%page expression_filter="h"/>
            ${"<tag>this is html</tag>"}
        """
        )
        assert t.render().strip() == "&lt;tag&gt;this is html&lt;/tag&gt;"

    def test_block_via_context(self):
        t = Template(
            """
            <%block name="foo" filter="myfilter">
                some text
            </%block>
        """
        )

        def myfilter(text):
            return "MYTEXT" + text

        eq_(result_lines(t.render(myfilter=myfilter)), ["MYTEXT", "some text"])

    def test_def_via_context(self):
        t = Template(
            """
            <%def name="foo()" filter="myfilter">
                some text
            </%def>
            ${foo()}
        """
        )

        def myfilter(text):
            return "MYTEXT" + text

        eq_(result_lines(t.render(myfilter=myfilter)), ["MYTEXT", "some text"])

    def test_text_via_context(self):
        t = Template(
            """
            <%text filter="myfilter">
                some text
            </%text>
        """
        )

        def myfilter(text):
            return "MYTEXT" + text

        eq_(result_lines(t.render(myfilter=myfilter)), ["MYTEXT", "some text"])

    def test_nflag(self):
        t = Template(
            """
            ${"<tag>this is html</tag>" | n}
        """,
            default_filters=["h", "unicode"],
        )
        assert t.render().strip() == "<tag>this is html</tag>"

        t = Template(
            """
            <%page expression_filter="h"/>
            ${"<tag>this is html</tag>" | n}
        """
        )
        assert t.render().strip() == "<tag>this is html</tag>"

        t = Template(
            """
            <%page expression_filter="h"/>
            ${"<tag>this is html</tag>" | n, h}
        """
        )
        assert t.render().strip() == "&lt;tag&gt;this is html&lt;/tag&gt;"

    def test_global_json(self):
        t = Template(
            """
<%!
import json
%><%page expression_filter="n, json.dumps"/>
data = {a: ${123}, b: ${"123"}};
        """
        )
        assert t.render().strip() == """data = {a: 123, b: "123"};"""

    def test_non_expression(self):
        t = Template(
            """
        <%!
            def a(text):
                return "this is a"
            def b(text):
                return "this is b"
        %>

        ${foo()}
        <%def name="foo()" buffered="True">
            this is text
        </%def>
        """,
            buffer_filters=["a"],
        )
        assert t.render().strip() == "this is a"

        t = Template(
            """
        <%!
            def a(text):
                return "this is a"
            def b(text):
                return "this is b"
        %>

        ${'hi'}
        ${foo()}
        <%def name="foo()" buffered="True">
            this is text
        </%def>
        """,
            buffer_filters=["a"],
            default_filters=["b"],
        )
        assert flatten_result(t.render()) == "this is b this is b"

        t = Template(
            """
        <%!
            class Foo(object):
                foo = True
                def __str__(self):
                    return "this is a"
            def a(text):
                return Foo()
            def b(text):
                if hasattr(text, 'foo'):
                    return str(text)
                else:
                    return "this is b"
        %>

        ${'hi'}
        ${foo()}
        <%def name="foo()" buffered="True">
            this is text
        </%def>
        """,
            buffer_filters=["a"],
            default_filters=["b"],
        )
        assert flatten_result(t.render()) == "this is b this is a"

        t = Template(
            """
        <%!
            def a(text):
                return "this is a"
            def b(text):
                return "this is b"
        %>

        ${foo()}
        ${bar()}
        <%def name="foo()" filter="b">
            this is text
        </%def>
        <%def name="bar()" filter="b" buffered="True">
            this is text
        </%def>
        """,
            buffer_filters=["a"],
        )
        assert flatten_result(t.render()) == "this is b this is a"

    def test_builtins(self):
        t = Template(
            """
            ${"this is <text>" | h}
"""
        )
        assert flatten_result(t.render()) == "this is &lt;text&gt;"

        t = Template(
            """
            http://foo.com/arg1=${"hi! this is a string." | u}
"""
        )
        assert (
            flatten_result(t.render())
            == "http://foo.com/arg1=hi%21+this+is+a+string."
        )


class BufferTest(unittest.TestCase):
    def test_buffered_def(self):
        t = Template(
            """
            <%def name="foo()" buffered="True">
                this is foo
            </%def>
            ${"hi->" + foo() + "<-hi"}
"""
        )
        assert flatten_result(t.render()) == "hi-> this is foo <-hi"

    def test_unbuffered_def(self):
        t = Template(
            """
            <%def name="foo()" buffered="False">
                this is foo
            </%def>
            ${"hi->" + foo() + "<-hi"}
"""
        )
        assert flatten_result(t.render()) == "this is foo hi-><-hi"

    def test_capture(self):
        t = Template(
            """
            <%def name="foo()" buffered="False">
                this is foo
            </%def>
            ${"hi->" + capture(foo) + "<-hi"}
"""
        )
        assert flatten_result(t.render()) == "hi-> this is foo <-hi"

    def test_capture_exception(self):
        template = Template(
            """
            <%def name="a()">
                this is a
                <%
                    raise TypeError("hi")
                %>
            </%def>
            <%
                c = capture(a)
            %>
            a->${c}<-a
        """
        )
        try:
            template.render()
            assert False
        except TypeError:
            assert True

    def test_buffered_exception(self):
        template = Template(
            """
            <%def name="a()" buffered="True">
                <%
                    raise TypeError("hi")
                %>
            </%def>

            ${a()}

"""
        )
        try:
            print(template.render())
            assert False
        except TypeError:
            assert True

    def test_capture_ccall(self):
        t = Template(
            """
            <%def name="foo()">
                <%
                    x = capture(caller.body)
                %>
                this is foo.  body: ${x}
            </%def>

            <%call expr="foo()">
                ccall body
            </%call>
"""
        )

        # print t.render()
        assert flatten_result(t.render()) == "this is foo. body: ccall body"
