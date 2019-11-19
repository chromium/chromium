from mako import lookup
from mako.template import Template
from test import eq_
from test import TemplateTest
from test.util import flatten_result
from test.util import result_lines


class NamespaceTest(TemplateTest):
    def test_inline_crossreference(self):
        self._do_memory_test(
            """
            <%namespace name="x">
                <%def name="a()">
                    this is x a
                </%def>
                <%def name="b()">
                    this is x b, and heres ${a()}
                </%def>
            </%namespace>

            ${x.a()}

            ${x.b()}
    """,
            "this is x a this is x b, and heres this is x a",
            filters=flatten_result,
        )

    def test_inline_assignment(self):
        self._do_memory_test(
            """
            <%namespace name="x">
                <%def name="a()">
                    <%
                        x = 5
                    %>
                    this is x: ${x}
                </%def>
            </%namespace>

            ${x.a()}

    """,
            "this is x: 5",
            filters=flatten_result,
        )

    def test_inline_arguments(self):
        self._do_memory_test(
            """
            <%namespace name="x">
                <%def name="a(x, y)">
                    <%
                        result = x * y
                    %>
                    result: ${result}
                </%def>
            </%namespace>

            ${x.a(5, 10)}

    """,
            "result: 50",
            filters=flatten_result,
        )

    def test_inline_not_duped(self):
        self._do_memory_test(
            """
            <%namespace name="x">
                <%def name="a()">
                    foo
                </%def>
            </%namespace>

            <%
                assert x.a is not UNDEFINED, "namespace x.a wasn't defined"
                assert a is UNDEFINED, "name 'a' is in the body locals"
            %>

    """,
            "",
            filters=flatten_result,
        )

    def test_dynamic(self):
        collection = lookup.TemplateLookup()

        collection.put_string(
            "a",
            """
        <%namespace name="b" file="${context['b_def']}"/>

        a.  b: ${b.body()}
""",
        )

        collection.put_string(
            "b",
            """
        b.
""",
        )

        eq_(
            flatten_result(collection.get_template("a").render(b_def="b")),
            "a. b: b.",
        )

    def test_template(self):
        collection = lookup.TemplateLookup()

        collection.put_string(
            "main.html",
            """
        <%namespace name="comp" file="defs.html"/>

        this is main.  ${comp.def1("hi")}
        ${comp.def2("there")}
""",
        )

        collection.put_string(
            "defs.html",
            """
        <%def name="def1(s)">
            def1: ${s}
        </%def>

        <%def name="def2(x)">
            def2: ${x}
        </%def>
""",
        )

        assert (
            flatten_result(collection.get_template("main.html").render())
            == "this is main. def1: hi def2: there"
        )

    def test_module(self):
        collection = lookup.TemplateLookup()

        collection.put_string(
            "main.html",
            """
        <%namespace name="comp" module="test.sample_module_namespace"/>

        this is main.  ${comp.foo1()}
        ${comp.foo2("hi")}
""",
        )

        assert (
            flatten_result(collection.get_template("main.html").render())
            == "this is main. this is foo1. this is foo2, x is hi"
        )

    def test_module_2(self):
        collection = lookup.TemplateLookup()

        collection.put_string(
            "main.html",
            """
        <%namespace name="comp" module="test.foo.test_ns"/>

        this is main.  ${comp.foo1()}
        ${comp.foo2("hi")}
""",
        )

        assert (
            flatten_result(collection.get_template("main.html").render())
            == "this is main. this is foo1. this is foo2, x is hi"
        )

    def test_module_imports(self):
        collection = lookup.TemplateLookup()

        collection.put_string(
            "main.html",
            """
        <%namespace import="*" module="test.foo.test_ns"/>

        this is main.  ${foo1()}
        ${foo2("hi")}
""",
        )

        assert (
            flatten_result(collection.get_template("main.html").render())
            == "this is main. this is foo1. this is foo2, x is hi"
        )

    def test_module_imports_2(self):
        collection = lookup.TemplateLookup()

        collection.put_string(
            "main.html",
            """
        <%namespace import="foo1, foo2" module="test.foo.test_ns"/>

        this is main.  ${foo1()}
        ${foo2("hi")}
""",
        )

        assert (
            flatten_result(collection.get_template("main.html").render())
            == "this is main. this is foo1. this is foo2, x is hi"
        )

    def test_context(self):
        """test that namespace callables get access to the current context"""
        collection = lookup.TemplateLookup()

        collection.put_string(
            "main.html",
            """
        <%namespace name="comp" file="defs.html"/>

        this is main.  ${comp.def1()}
        ${comp.def2("there")}
""",
        )

        collection.put_string(
            "defs.html",
            """
        <%def name="def1()">
            def1: x is ${x}
        </%def>

        <%def name="def2(x)">
            def2: x is ${x}
        </%def>
""",
        )

        assert (
            flatten_result(
                collection.get_template("main.html").render(x="context x")
            )
            == "this is main. def1: x is context x def2: x is there"
        )

    def test_overload(self):
        collection = lookup.TemplateLookup()

        collection.put_string(
            "main.html",
            """
        <%namespace name="comp" file="defs.html">
            <%def name="def1(x, y)">
                overridden def1 ${x}, ${y}
            </%def>
        </%namespace>

        this is main.  ${comp.def1("hi", "there")}
        ${comp.def2("there")}
    """,
        )

        collection.put_string(
            "defs.html",
            """
        <%def name="def1(s)">
            def1: ${s}
        </%def>

        <%def name="def2(x)">
            def2: ${x}
        </%def>
    """,
        )

        assert (
            flatten_result(collection.get_template("main.html").render())
            == "this is main. overridden def1 hi, there def2: there"
        )

    def test_getattr(self):
        collection = lookup.TemplateLookup()
        collection.put_string(
            "main.html",
            """
            <%namespace name="foo" file="ns.html"/>
            <%
                 if hasattr(foo, 'lala'):
                     foo.lala()
                 if not hasattr(foo, 'hoho'):
                     context.write('foo has no hoho.')
            %>
         """,
        )
        collection.put_string(
            "ns.html",
            """
          <%def name="lala()">this is lala.</%def>
        """,
        )
        assert (
            flatten_result(collection.get_template("main.html").render())
            == "this is lala.foo has no hoho."
        )

    def test_in_def(self):
        collection = lookup.TemplateLookup()
        collection.put_string(
            "main.html",
            """
            <%namespace name="foo" file="ns.html"/>

            this is main.  ${bar()}
            <%def name="bar()">
                this is bar, foo is ${foo.bar()}
            </%def>
        """,
        )

        collection.put_string(
            "ns.html",
            """
            <%def name="bar()">
                this is ns.html->bar
            </%def>
        """,
        )

        assert result_lines(collection.get_template("main.html").render()) == [
            "this is main.",
            "this is bar, foo is",
            "this is ns.html->bar",
        ]

    def test_in_remote_def(self):
        collection = lookup.TemplateLookup()
        collection.put_string(
            "main.html",
            """
            <%namespace name="foo" file="ns.html"/>

            this is main.  ${bar()}
            <%def name="bar()">
                this is bar, foo is ${foo.bar()}
            </%def>
        """,
        )

        collection.put_string(
            "ns.html",
            """
            <%def name="bar()">
                this is ns.html->bar
            </%def>
        """,
        )

        collection.put_string(
            "index.html",
            """
            <%namespace name="main" file="main.html"/>

            this is index
            ${main.bar()}
        """,
        )

        assert result_lines(
            collection.get_template("index.html").render()
        ) == ["this is index", "this is bar, foo is", "this is ns.html->bar"]

    def test_dont_pollute_self(self):
        # test that get_namespace() doesn't modify the original context
        # incompatibly

        collection = lookup.TemplateLookup()
        collection.put_string(
            "base.html",
            """

        <%def name="foo()">
        <%
            foo = local.get_namespace("foo.html")
        %>
        </%def>

        name: ${self.name}
        name via bar: ${bar()}

        ${next.body()}

        name: ${self.name}
        name via bar: ${bar()}
        <%def name="bar()">
            ${self.name}
        </%def>


        """,
        )

        collection.put_string(
            "page.html",
            """
        <%inherit file="base.html"/>

        ${self.foo()}

        hello world

        """,
        )

        collection.put_string("foo.html", """<%inherit file="base.html"/>""")
        assert result_lines(collection.get_template("page.html").render()) == [
            "name: self:page.html",
            "name via bar:",
            "self:page.html",
            "hello world",
            "name: self:page.html",
            "name via bar:",
            "self:page.html",
        ]

    def test_inheritance(self):
        """test namespace initialization in a base inherited template that
        doesnt otherwise access the namespace"""
        collection = lookup.TemplateLookup()
        collection.put_string(
            "base.html",
            """
            <%namespace name="foo" file="ns.html" inheritable="True"/>

            ${next.body()}
""",
        )
        collection.put_string(
            "ns.html",
            """
            <%def name="bar()">
                this is ns.html->bar
            </%def>
        """,
        )

        collection.put_string(
            "index.html",
            """
            <%inherit file="base.html"/>

            this is index
            ${self.foo.bar()}
        """,
        )

        assert result_lines(
            collection.get_template("index.html").render()
        ) == ["this is index", "this is ns.html->bar"]

    def test_inheritance_two(self):
        collection = lookup.TemplateLookup()
        collection.put_string(
            "base.html",
            """
            <%def name="foo()">
                base.foo
            </%def>

            <%def name="bat()">
                base.bat
            </%def>
""",
        )
        collection.put_string(
            "lib.html",
            """
            <%inherit file="base.html"/>
            <%def name="bar()">
                lib.bar
                ${parent.foo()}
                ${self.foo()}
                ${parent.bat()}
                ${self.bat()}
            </%def>

            <%def name="foo()">
                lib.foo
            </%def>

        """,
        )

        collection.put_string(
            "front.html",
            """
            <%namespace name="lib" file="lib.html"/>
            ${lib.bar()}
        """,
        )

        assert result_lines(
            collection.get_template("front.html").render()
        ) == ["lib.bar", "base.foo", "lib.foo", "base.bat", "base.bat"]

    def test_attr(self):
        l = lookup.TemplateLookup()

        l.put_string(
            "foo.html",
            """
        <%!
            foofoo = "foo foo"
            onlyfoo = "only foo"
        %>
        <%inherit file="base.html"/>
        <%def name="setup()">
            <%
            self.attr.foolala = "foo lala"
            %>
        </%def>
        ${self.attr.basefoo}
        ${self.attr.foofoo}
        ${self.attr.onlyfoo}
        ${self.attr.lala}
        ${self.attr.foolala}
        """,
        )

        l.put_string(
            "base.html",
            """
        <%!
            basefoo = "base foo 1"
            foofoo = "base foo 2"
        %>
        <%
            self.attr.lala = "base lala"
        %>

        ${self.attr.basefoo}
        ${self.attr.foofoo}
        ${self.attr.onlyfoo}
        ${self.attr.lala}
        ${self.setup()}
        ${self.attr.foolala}
        body
        ${self.body()}
        """,
        )

        assert result_lines(l.get_template("foo.html").render()) == [
            "base foo 1",
            "foo foo",
            "only foo",
            "base lala",
            "foo lala",
            "body",
            "base foo 1",
            "foo foo",
            "only foo",
            "base lala",
            "foo lala",
        ]

    def test_attr_raise(self):
        l = lookup.TemplateLookup()

        l.put_string(
            "foo.html",
            """
            <%def name="foo()">
            </%def>
        """,
        )

        l.put_string(
            "bar.html",
            """
        <%namespace name="foo" file="foo.html"/>

        ${foo.notfoo()}
        """,
        )

        self.assertRaises(AttributeError, l.get_template("bar.html").render)

    def test_custom_tag_1(self):
        template = Template(
            """

            <%def name="foo(x, y)">
                foo: ${x} ${y}
            </%def>

            <%self:foo x="5" y="${7+8}"/>
        """
        )
        assert result_lines(template.render()) == ["foo: 5 15"]

    def test_custom_tag_2(self):
        collection = lookup.TemplateLookup()
        collection.put_string(
            "base.html",
            """
            <%def name="foo(x, y)">
                foo: ${x} ${y}
            </%def>

            <%def name="bat(g)"><%
                return "the bat! %s" % g
            %></%def>

            <%def name="bar(x)">
                ${caller.body(z=x)}
            </%def>
        """,
        )

        collection.put_string(
            "index.html",
            """
            <%namespace name="myns" file="base.html"/>

            <%myns:foo x="${'some x'}" y="some y"/>

            <%myns:bar x="${myns.bat(10)}" args="z">
                record: ${z}
            </%myns:bar>

        """,
        )

        assert result_lines(
            collection.get_template("index.html").render()
        ) == ["foo: some x some y", "record: the bat! 10"]

    def test_custom_tag_3(self):
        collection = lookup.TemplateLookup()
        collection.put_string(
            "base.html",
            """
            <%namespace name="foo" file="ns.html" inheritable="True"/>

            ${next.body()}
    """,
        )
        collection.put_string(
            "ns.html",
            """
            <%def name="bar()">
                this is ns.html->bar
                caller body: ${caller.body()}
            </%def>
        """,
        )

        collection.put_string(
            "index.html",
            """
            <%inherit file="base.html"/>

            this is index
            <%self.foo:bar>
                call body
            </%self.foo:bar>
        """,
        )

        assert result_lines(
            collection.get_template("index.html").render()
        ) == [
            "this is index",
            "this is ns.html->bar",
            "caller body:",
            "call body",
        ]

    def test_custom_tag_case_sensitive(self):
        t = Template(
            """
        <%def name="renderPanel()">
            panel ${caller.body()}
        </%def>

        <%def name="renderTablePanel()">
            <%self:renderPanel>
                hi
            </%self:renderPanel>
        </%def>

        <%self:renderTablePanel/>
        """
        )
        assert result_lines(t.render()) == ["panel", "hi"]

    def test_expr_grouping(self):
        """test that parenthesis are placed around string-embedded
        expressions."""

        template = Template(
            """
            <%def name="bar(x, y)">
                ${x}
                ${y}
            </%def>

            <%self:bar x=" ${foo} " y="x${g and '1' or '2'}y"/>
        """,
            input_encoding="utf-8",
        )

        # the concat has to come out as "x + (g and '1' or '2') + y"
        assert result_lines(template.render(foo="this is foo", g=False)) == [
            "this is foo",
            "x2y",
        ]

    def test_ccall(self):
        collection = lookup.TemplateLookup()
        collection.put_string(
            "base.html",
            """
            <%namespace name="foo" file="ns.html" inheritable="True"/>

            ${next.body()}
    """,
        )
        collection.put_string(
            "ns.html",
            """
            <%def name="bar()">
                this is ns.html->bar
                caller body: ${caller.body()}
            </%def>
        """,
        )

        collection.put_string(
            "index.html",
            """
            <%inherit file="base.html"/>

            this is index
            <%call expr="self.foo.bar()">
                call body
            </%call>
        """,
        )

        assert result_lines(
            collection.get_template("index.html").render()
        ) == [
            "this is index",
            "this is ns.html->bar",
            "caller body:",
            "call body",
        ]

    def test_ccall_2(self):
        collection = lookup.TemplateLookup()
        collection.put_string(
            "base.html",
            """
            <%namespace name="foo" file="ns1.html" inheritable="True"/>

            ${next.body()}
    """,
        )
        collection.put_string(
            "ns1.html",
            """
            <%namespace name="foo2" file="ns2.html"/>
            <%def name="bar()">
                <%call expr="foo2.ns2_bar()">
                this is ns1.html->bar
                caller body: ${caller.body()}
                </%call>
            </%def>
        """,
        )

        collection.put_string(
            "ns2.html",
            """
            <%def name="ns2_bar()">
                this is ns2.html->bar
                caller body: ${caller.body()}
            </%def>
        """,
        )

        collection.put_string(
            "index.html",
            """
            <%inherit file="base.html"/>

            this is index
            <%call expr="self.foo.bar()">
                call body
            </%call>
        """,
        )

        assert result_lines(
            collection.get_template("index.html").render()
        ) == [
            "this is index",
            "this is ns2.html->bar",
            "caller body:",
            "this is ns1.html->bar",
            "caller body:",
            "call body",
        ]

    def test_import(self):
        collection = lookup.TemplateLookup()
        collection.put_string(
            "functions.html",
            """
            <%def name="foo()">
                this is foo
            </%def>

            <%def name="bar()">
                this is bar
            </%def>

            <%def name="lala()">
                this is lala
            </%def>
        """,
        )

        collection.put_string(
            "func2.html",
            """
            <%def name="a()">
                this is a
            </%def>
            <%def name="b()">
                this is b
            </%def>
        """,
        )
        collection.put_string(
            "index.html",
            """
            <%namespace file="functions.html" import="*"/>
            <%namespace file="func2.html" import="a, b"/>
            ${foo()}
            ${bar()}
            ${lala()}
            ${a()}
            ${b()}
            ${x}
        """,
        )

        assert result_lines(
            collection.get_template("index.html").render(
                bar="this is bar", x="this is x"
            )
        ) == [
            "this is foo",
            "this is bar",
            "this is lala",
            "this is a",
            "this is b",
            "this is x",
        ]

    def test_import_calledfromdef(self):
        l = lookup.TemplateLookup()
        l.put_string(
            "a",
            """
        <%def name="table()">
            im table
        </%def>
        """,
        )

        l.put_string(
            "b",
            """
        <%namespace file="a" import="table"/>

        <%
            def table2():
                table()
                return ""
        %>

        ${table2()}
        """,
        )

        t = l.get_template("b")
        assert flatten_result(t.render()) == "im table"

    def test_closure_import(self):
        collection = lookup.TemplateLookup()
        collection.put_string(
            "functions.html",
            """
            <%def name="foo()">
                this is foo
            </%def>

            <%def name="bar()">
                this is bar
            </%def>
        """,
        )

        collection.put_string(
            "index.html",
            """
            <%namespace file="functions.html" import="*"/>
            <%def name="cl1()">
                ${foo()}
            </%def>

            <%def name="cl2()">
                ${bar()}
            </%def>

            ${cl1()}
            ${cl2()}
        """,
        )
        assert result_lines(
            collection.get_template("index.html").render(
                bar="this is bar", x="this is x"
            )
        ) == ["this is foo", "this is bar"]

    def test_import_local(self):
        t = Template(
            """
            <%namespace import="*">
                <%def name="foo()">
                    this is foo
                </%def>
            </%namespace>

            ${foo()}

        """
        )
        assert flatten_result(t.render()) == "this is foo"

    def test_ccall_import(self):
        collection = lookup.TemplateLookup()
        collection.put_string(
            "functions.html",
            """
            <%def name="foo()">
                this is foo
            </%def>

            <%def name="bar()">
                this is bar.
                ${caller.body()}
                ${caller.lala()}
            </%def>
        """,
        )

        collection.put_string(
            "index.html",
            """
            <%namespace name="func" file="functions.html" import="*"/>
            <%call expr="bar()">
                this is index embedded
                foo is ${foo()}
                <%def name="lala()">
                     this is lala ${foo()}
                </%def>
            </%call>
        """,
        )
        # print collection.get_template("index.html").code
        # print collection.get_template("functions.html").code
        assert result_lines(
            collection.get_template("index.html").render()
        ) == [
            "this is bar.",
            "this is index embedded",
            "foo is",
            "this is foo",
            "this is lala",
            "this is foo",
        ]
