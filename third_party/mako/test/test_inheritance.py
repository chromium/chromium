import unittest

from mako import compat
from mako import lookup
from test.util import result_lines


class InheritanceTest(unittest.TestCase):
    def test_basic(self):
        collection = lookup.TemplateLookup()

        collection.put_string(
            "main",
            """
<%inherit file="base"/>

<%def name="header()">
    main header.
</%def>

this is the content.
""",
        )

        collection.put_string(
            "base",
            """
This is base.

header: ${self.header()}

body: ${self.body()}

footer: ${self.footer()}

<%def name="footer()">
    this is the footer. header again ${next.header()}
</%def>
""",
        )

        assert result_lines(collection.get_template("main").render()) == [
            "This is base.",
            "header:",
            "main header.",
            "body:",
            "this is the content.",
            "footer:",
            "this is the footer. header again",
            "main header.",
        ]

    def test_multilevel_nesting(self):
        collection = lookup.TemplateLookup()

        collection.put_string(
            "main",
            """
<%inherit file="layout"/>
<%def name="d()">main_d</%def>
main_body ${parent.d()}
full stack from the top:
    ${self.name} ${parent.name} ${parent.context['parent'].name} """
            """${parent.context['parent'].context['parent'].name}
""",
        )

        collection.put_string(
            "layout",
            """
<%inherit file="general"/>
<%def name="d()">layout_d</%def>
layout_body
parent name: ${parent.name}
${parent.d()}
${parent.context['parent'].d()}
${next.body()}
""",
        )

        collection.put_string(
            "general",
            """
<%inherit file="base"/>
<%def name="d()">general_d</%def>
general_body
${next.d()}
${next.context['next'].d()}
${next.body()}
""",
        )
        collection.put_string(
            "base",
            """
base_body
full stack from the base:
    ${self.name} ${self.context['parent'].name} """
            """${self.context['parent'].context['parent'].name} """
            """${self.context['parent'].context['parent'].context['parent'].name}
${next.body()}
<%def name="d()">base_d</%def>
""",
        )

        assert result_lines(collection.get_template("main").render()) == [
            "base_body",
            "full stack from the base:",
            "self:main self:layout self:general self:base",
            "general_body",
            "layout_d",
            "main_d",
            "layout_body",
            "parent name: self:general",
            "general_d",
            "base_d",
            "main_body layout_d",
            "full stack from the top:",
            "self:main self:layout self:general self:base",
        ]

    def test_includes(self):
        """test that an included template also has its full hierarchy
        invoked."""
        collection = lookup.TemplateLookup()

        collection.put_string(
            "base",
            """
        <%def name="a()">base_a</%def>
        This is the base.
        ${next.body()}
        End base.
""",
        )

        collection.put_string(
            "index",
            """
        <%inherit file="base"/>
        this is index.
        a is: ${self.a()}
        <%include file="secondary"/>
""",
        )

        collection.put_string(
            "secondary",
            """
        <%inherit file="base"/>
        this is secondary.
        a is: ${self.a()}
""",
        )

        assert result_lines(collection.get_template("index").render()) == [
            "This is the base.",
            "this is index.",
            "a is: base_a",
            "This is the base.",
            "this is secondary.",
            "a is: base_a",
            "End base.",
            "End base.",
        ]

    def test_namespaces(self):
        """test that templates used via <%namespace> have access to an
        inheriting 'self', and that the full 'self' is also exported."""
        collection = lookup.TemplateLookup()

        collection.put_string(
            "base",
            """
        <%def name="a()">base_a</%def>
        <%def name="b()">base_b</%def>
        This is the base.
        ${next.body()}
""",
        )

        collection.put_string(
            "layout",
            """
        <%inherit file="base"/>
        <%def name="a()">layout_a</%def>
        This is the layout..
        ${next.body()}
""",
        )

        collection.put_string(
            "index",
            """
        <%inherit file="base"/>
        <%namespace name="sc" file="secondary"/>
        this is index.
        a is: ${self.a()}
        sc.a is: ${sc.a()}
        sc.b is: ${sc.b()}
        sc.c is: ${sc.c()}
        sc.body is: ${sc.body()}
""",
        )

        collection.put_string(
            "secondary",
            """
        <%inherit file="layout"/>
        <%def name="c()">secondary_c.  a is ${self.a()} b is ${self.b()} """
            """d is ${self.d()}</%def>
        <%def name="d()">secondary_d.</%def>
        this is secondary.
        a is: ${self.a()}
        c is: ${self.c()}
""",
        )

        assert result_lines(collection.get_template("index").render()) == [
            "This is the base.",
            "this is index.",
            "a is: base_a",
            "sc.a is: layout_a",
            "sc.b is: base_b",
            "sc.c is: secondary_c. a is layout_a b is base_b d is "
            "secondary_d.",
            "sc.body is:",
            "this is secondary.",
            "a is: layout_a",
            "c is: secondary_c. a is layout_a b is base_b d is secondary_d.",
        ]

    def test_pageargs(self):
        collection = lookup.TemplateLookup()
        collection.put_string(
            "base",
            """
            this is the base.

            <%
            sorted_ = pageargs.items()
            sorted_ = sorted(sorted_)
            %>
            pageargs: (type: ${type(pageargs)}) ${sorted_}
            <%def name="foo()">
                ${next.body(**context.kwargs)}
            </%def>

            ${foo()}
        """,
        )
        collection.put_string(
            "index",
            """
            <%inherit file="base"/>
            <%page args="x, y, z=7"/>
            print ${x}, ${y}, ${z}
        """,
        )

        if compat.py3k:
            assert result_lines(
                collection.get_template("index").render_unicode(x=5, y=10)
            ) == [
                "this is the base.",
                "pageargs: (type: <class 'dict'>) [('x', 5), ('y', 10)]",
                "print 5, 10, 7",
            ]
        else:
            assert result_lines(
                collection.get_template("index").render_unicode(x=5, y=10)
            ) == [
                "this is the base.",
                "pageargs: (type: <type 'dict'>) [('x', 5), ('y', 10)]",
                "print 5, 10, 7",
            ]

    def test_pageargs_2(self):
        collection = lookup.TemplateLookup()
        collection.put_string(
            "base",
            """
            this is the base.

            ${next.body(**context.kwargs)}

            <%def name="foo(**kwargs)">
                ${next.body(**kwargs)}
            </%def>

            <%def name="bar(**otherargs)">
                ${next.body(z=16, **context.kwargs)}
            </%def>

            ${foo(x=12, y=15, z=8)}
            ${bar(x=19, y=17)}
        """,
        )
        collection.put_string(
            "index",
            """
            <%inherit file="base"/>
            <%page args="x, y, z=7"/>
            pageargs: ${x}, ${y}, ${z}
        """,
        )
        assert result_lines(
            collection.get_template("index").render(x=5, y=10)
        ) == [
            "this is the base.",
            "pageargs: 5, 10, 7",
            "pageargs: 12, 15, 8",
            "pageargs: 5, 10, 16",
        ]

    def test_pageargs_err(self):
        collection = lookup.TemplateLookup()
        collection.put_string(
            "base",
            """
            this is the base.
            ${next.body()}
        """,
        )
        collection.put_string(
            "index",
            """
            <%inherit file="base"/>
            <%page args="x, y, z=7"/>
            print ${x}, ${y}, ${z}
        """,
        )
        try:
            print(collection.get_template("index").render(x=5, y=10))
            assert False
        except TypeError:
            assert True

    def test_toplevel(self):
        collection = lookup.TemplateLookup()
        collection.put_string(
            "base",
            """
            this is the base.
            ${next.body()}
        """,
        )
        collection.put_string(
            "index",
            """
            <%inherit file="base"/>
            this is the body
        """,
        )
        assert result_lines(collection.get_template("index").render()) == [
            "this is the base.",
            "this is the body",
        ]
        assert result_lines(
            collection.get_template("index").get_def("body").render()
        ) == ["this is the body"]

    def test_dynamic(self):
        collection = lookup.TemplateLookup()
        collection.put_string(
            "base",
            """
            this is the base.
            ${next.body()}
        """,
        )
        collection.put_string(
            "index",
            """
            <%!
                def dyn(context):
                    if context.get('base', None) is not None:
                        return 'base'
                    else:
                        return None
            %>
            <%inherit file="${dyn(context)}"/>
            this is index.
        """,
        )
        assert result_lines(collection.get_template("index").render()) == [
            "this is index."
        ]
        assert result_lines(
            collection.get_template("index").render(base=True)
        ) == ["this is the base.", "this is index."]

    def test_in_call(self):
        collection = lookup.TemplateLookup()
        collection.put_string(
            "/layout.html",
            """
        Super layout!
        <%call expr="self.grid()">
            ${next.body()}
        </%call>
        Oh yea!

        <%def name="grid()">
            Parent grid
                ${caller.body()}
            End Parent
        </%def>
        """,
        )

        collection.put_string(
            "/subdir/layout.html",
            """
        ${next.body()}
        <%def name="grid()">
           Subdir grid
               ${caller.body()}
           End subdir
        </%def>
        <%inherit file="/layout.html"/>
        """,
        )

        collection.put_string(
            "/subdir/renderedtemplate.html",
            """
        Holy smokes!
        <%inherit file="/subdir/layout.html"/>
        """,
        )

        assert result_lines(
            collection.get_template("/subdir/renderedtemplate.html").render()
        ) == [
            "Super layout!",
            "Subdir grid",
            "Holy smokes!",
            "End subdir",
            "Oh yea!",
        ]
