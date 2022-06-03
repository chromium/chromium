from mako import exceptions
from mako.lookup import TemplateLookup
from mako.template import Template
from mako.testing.assertions import assert_raises_message
from mako.testing.fixtures import TemplateTest
from mako.testing.helpers import result_lines


class BlockTest(TemplateTest):
    def test_anonymous_block_namespace_raises(self):
        assert_raises_message(
            exceptions.CompileException,
            "Can't put anonymous blocks inside <%namespace>",
            Template,
            """
                <%namespace name="foo">
                    <%block>
                        block
                    </%block>
                </%namespace>
            """,
        )

    def test_anonymous_block_in_call(self):
        template = Template(
            """

            <%self:foo x="5">
                <%block>
                    this is the block x
                </%block>
            </%self:foo>

            <%def name="foo(x)">
                foo:
                ${caller.body()}
            </%def>
        """
        )
        self._do_test(
            template, ["foo:", "this is the block x"], filters=result_lines
        )

    def test_named_block_in_call(self):
        assert_raises_message(
            exceptions.CompileException,
            "Named block 'y' not allowed inside of <%call> tag",
            Template,
            """

            <%self:foo x="5">
                <%block name="y">
                    this is the block
                </%block>
            </%self:foo>

            <%def name="foo(x)">
                foo:
                ${caller.body()}
                ${caller.y()}
            </%def>
        """,
        )

    def test_name_collision_blocks_toplevel(self):
        assert_raises_message(
            exceptions.CompileException,
            "%def or %block named 'x' already exists in this template",
            Template,
            """
                <%block name="x">
                    block
                </%block>

                foob

                <%block name="x">
                    block
                </%block>
            """,
        )

    def test_name_collision_blocks_nested_block(self):
        assert_raises_message(
            exceptions.CompileException,
            "%def or %block named 'x' already exists in this template",
            Template,
            """
                <%block>
                <%block name="x">
                    block
                </%block>

                foob

                <%block name="x">
                    block
                </%block>
                </%block>
            """,
        )

    def test_name_collision_blocks_nested_def(self):
        assert_raises_message(
            exceptions.CompileException,
            "Named block 'x' not allowed inside of def 'foo'",
            Template,
            """
                <%def name="foo()">
                <%block name="x">
                    block
                </%block>

                foob

                <%block name="x">
                    block
                </%block>
                </%def>
            """,
        )

    def test_name_collision_block_def_toplevel(self):
        assert_raises_message(
            exceptions.CompileException,
            "%def or %block named 'x' already exists in this template",
            Template,
            """
                <%block name="x">
                    block
                </%block>

                foob

                <%def name="x()">
                    block
                </%def>
            """,
        )

    def test_name_collision_def_block_toplevel(self):
        assert_raises_message(
            exceptions.CompileException,
            "%def or %block named 'x' already exists in this template",
            Template,
            """
                <%def name="x()">
                    block
                </%def>

                foob

                <%block name="x">
                    block
                </%block>

            """,
        )

    def test_named_block_renders(self):
        template = Template(
            """
            above
            <%block name="header">
                the header
            </%block>
            below
        """
        )
        self._do_test(
            template, ["above", "the header", "below"], filters=result_lines
        )

    def test_inherited_block_no_render(self):
        l = TemplateLookup()
        l.put_string(
            "index",
            """
                <%inherit file="base"/>
                <%block name="header">
                    index header
                </%block>
            """,
        )
        l.put_string(
            "base",
            """
            above
            <%block name="header">
                the header
            </%block>

            ${next.body()}
            below
        """,
        )
        self._do_test(
            l.get_template("index"),
            ["above", "index header", "below"],
            filters=result_lines,
        )

    def test_no_named_in_def(self):
        assert_raises_message(
            exceptions.CompileException,
            "Named block 'y' not allowed inside of def 'q'",
            Template,
            """
            <%def name="q()">
                <%block name="y">
                </%block>
            </%def>
        """,
        )

    def test_inherited_block_nested_both(self):
        l = TemplateLookup()
        l.put_string(
            "index",
            """
                <%inherit file="base"/>
                <%block name="title">
                    index title
                </%block>

                <%block name="header">
                    index header
                    ${parent.header()}
                </%block>
            """,
        )
        l.put_string(
            "base",
            """
            above
            <%block name="header">
                base header
                <%block name="title">
                    the title
                </%block>
            </%block>

            ${next.body()}
            below
        """,
        )
        self._do_test(
            l.get_template("index"),
            ["above", "index header", "base header", "index title", "below"],
            filters=result_lines,
        )

    def test_inherited_block_nested_inner_only(self):
        l = TemplateLookup()
        l.put_string(
            "index",
            """
                <%inherit file="base"/>
                <%block name="title">
                    index title
                </%block>

            """,
        )
        l.put_string(
            "base",
            """
            above
            <%block name="header">
                base header
                <%block name="title">
                    the title
                </%block>
            </%block>

            ${next.body()}
            below
        """,
        )
        self._do_test(
            l.get_template("index"),
            ["above", "base header", "index title", "below"],
            filters=result_lines,
        )

    def test_noninherited_block_no_render(self):
        l = TemplateLookup()
        l.put_string(
            "index",
            """
                <%inherit file="base"/>
                <%block name="some_thing">
                    some thing
                </%block>
            """,
        )
        l.put_string(
            "base",
            """
            above
            <%block name="header">
                the header
            </%block>

            ${next.body()}
            below
        """,
        )
        self._do_test(
            l.get_template("index"),
            ["above", "the header", "some thing", "below"],
            filters=result_lines,
        )

    def test_no_conflict_nested_one(self):
        l = TemplateLookup()
        l.put_string(
            "index",
            """
                <%inherit file="base"/>
                <%block>
                    <%block name="header">
                        inner header
                    </%block>
                </%block>
            """,
        )
        l.put_string(
            "base",
            """
            above
            <%block name="header">
                the header
            </%block>

            ${next.body()}
            below
        """,
        )
        self._do_test(
            l.get_template("index"),
            ["above", "inner header", "below"],
            filters=result_lines,
        )

    def test_nested_dupe_names_raise(self):
        assert_raises_message(
            exceptions.CompileException,
            "%def or %block named 'header' already exists in this template.",
            Template,
            """
                <%inherit file="base"/>
                <%block name="header">
                    <%block name="header">
                        inner header
                    </%block>
                </%block>
            """,
        )

    def test_two_levels_one(self):
        l = TemplateLookup()
        l.put_string(
            "index",
            """
                <%inherit file="middle"/>
                <%block name="header">
                    index header
                </%block>
                <%block>
                    index anon
                </%block>
            """,
        )
        l.put_string(
            "middle",
            """
            <%inherit file="base"/>
            <%block>
                middle anon
            </%block>
            ${next.body()}
        """,
        )
        l.put_string(
            "base",
            """
            above
            <%block name="header">
                the header
            </%block>

            ${next.body()}
            below
        """,
        )
        self._do_test(
            l.get_template("index"),
            ["above", "index header", "middle anon", "index anon", "below"],
            filters=result_lines,
        )

    def test_filter(self):
        template = Template(
            """
            <%block filter="h">
                <html>
            </%block>
        """
        )
        self._do_test(template, ["&lt;html&gt;"], filters=result_lines)

    def test_anon_in_named(self):
        template = Template(
            """
            <%block name="x">
                outer above
                <%block>
                    inner
                </%block>
                outer below
            </%block>
        """
        )
        self._test_block_in_block(template)

    def test_named_in_anon(self):
        template = Template(
            """
            <%block>
                outer above
                <%block name="x">
                    inner
                </%block>
                outer below
            </%block>
        """
        )
        self._test_block_in_block(template)

    def test_anon_in_anon(self):
        template = Template(
            """
            <%block>
                outer above
                <%block>
                    inner
                </%block>
                outer below
            </%block>
        """
        )
        self._test_block_in_block(template)

    def test_named_in_named(self):
        template = Template(
            """
            <%block name="x">
                outer above
                <%block name="y">
                    inner
                </%block>
                outer below
            </%block>
        """
        )
        self._test_block_in_block(template)

    def _test_block_in_block(self, template):
        self._do_test(
            template,
            ["outer above", "inner", "outer below"],
            filters=result_lines,
        )

    def test_iteration(self):
        t = Template(
            """
            % for i in (1, 2, 3):
                <%block>${i}</%block>
            % endfor
        """
        )
        self._do_test(t, ["1", "2", "3"], filters=result_lines)

    def test_conditional(self):
        t = Template(
            """
            % if True:
                <%block>true</%block>
            % endif

            % if False:
                <%block>false</%block>
            % endif
        """
        )
        self._do_test(t, ["true"], filters=result_lines)

    def test_block_overridden_by_def(self):
        l = TemplateLookup()
        l.put_string(
            "index",
            """
                <%inherit file="base"/>
                <%def name="header()">
                    inner header
                </%def>
            """,
        )
        l.put_string(
            "base",
            """
            above
            <%block name="header">
                the header
            </%block>

            ${next.body()}
            below
        """,
        )
        self._do_test(
            l.get_template("index"),
            ["above", "inner header", "below"],
            filters=result_lines,
        )

    def test_def_overridden_by_block(self):
        l = TemplateLookup()
        l.put_string(
            "index",
            """
                <%inherit file="base"/>
                <%block name="header">
                    inner header
                </%block>
            """,
        )
        l.put_string(
            "base",
            """
            above
            ${self.header()}
            <%def name="header()">
                the header
            </%def>

            ${next.body()}
            below
        """,
        )
        self._do_test(
            l.get_template("index"),
            ["above", "inner header", "below"],
            filters=result_lines,
        )

    def test_block_args(self):
        l = TemplateLookup()
        l.put_string(
            "caller",
            """

            <%include file="callee" args="val1='3', val2='4'"/>

        """,
        )
        l.put_string(
            "callee",
            """
            <%page args="val1, val2"/>
            <%block name="foob" args="val1, val2">
                foob, ${val1}, ${val2}
            </%block>
        """,
        )
        self._do_test(
            l.get_template("caller"), ["foob, 3, 4"], filters=result_lines
        )

    def test_block_variables_contextual(self):
        t = Template(
            """
            <%block name="foob" >
                foob, ${val1}, ${val2}
            </%block>
        """
        )
        self._do_test(
            t,
            ["foob, 3, 4"],
            template_args={"val1": 3, "val2": 4},
            filters=result_lines,
        )

    def test_block_args_contextual(self):
        t = Template(
            """
            <%page args="val1"/>
            <%block name="foob" args="val1">
                foob, ${val1}, ${val2}
            </%block>
        """
        )
        self._do_test(
            t,
            ["foob, 3, 4"],
            template_args={"val1": 3, "val2": 4},
            filters=result_lines,
        )

    def test_block_pageargs_contextual(self):
        t = Template(
            """
            <%block name="foob">
                foob, ${pageargs['val1']}, ${pageargs['val2']}
            </%block>
        """
        )
        self._do_test(
            t,
            ["foob, 3, 4"],
            template_args={"val1": 3, "val2": 4},
            filters=result_lines,
        )

    def test_block_pageargs(self):
        l = TemplateLookup()
        l.put_string(
            "caller",
            """

            <%include file="callee" args="val1='3', val2='4'"/>

        """,
        )
        l.put_string(
            "callee",
            """
            <%block name="foob">
                foob, ${pageargs['val1']}, ${pageargs['val2']}
            </%block>
        """,
        )
        self._do_test(
            l.get_template("caller"), ["foob, 3, 4"], filters=result_lines
        )
