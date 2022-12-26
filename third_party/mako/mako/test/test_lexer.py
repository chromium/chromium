import re

import pytest

from mako import compat
from mako import exceptions
from mako import parsetree
from mako import util
from mako.lexer import Lexer
from mako.template import Template
from mako.testing.assertions import assert_raises
from mako.testing.assertions import assert_raises_message
from mako.testing.assertions import eq_
from mako.testing.fixtures import TemplateTest
from mako.testing.helpers import flatten_result

# create fake parsetree classes which are constructed
# exactly as the repr() of a real parsetree object.
# this allows us to use a Python construct as the source
# of a comparable repr(), which is also hit by the 2to3 tool.


def repr_arg(x):
    if isinstance(x, dict):
        return util.sorted_dict_repr(x)
    else:
        return repr(x)


def _as_unicode(arg):
    if isinstance(arg, dict):
        return {k: _as_unicode(v) for k, v in arg.items()}
    else:
        return arg


Node = None
TemplateNode = None
ControlLine = None
Text = None
Code = None
Comment = None
Expression = None
_TagMeta = None
Tag = None
IncludeTag = None
NamespaceTag = None
TextTag = None
DefTag = None
BlockTag = None
CallTag = None
CallNamespaceTag = None
InheritTag = None
PageTag = None

# go through all the elements in parsetree and build out
# mocks of them
for cls in list(parsetree.__dict__.values()):
    if isinstance(cls, type) and issubclass(cls, parsetree.Node):
        clsname = cls.__name__
        exec(
            (
                """
class %s:
    def __init__(self, *args):
        self.args = [_as_unicode(arg) for arg in args]
    def __repr__(self):
        return "%%s(%%s)" %% (
            self.__class__.__name__,
            ", ".join(repr_arg(x) for x in self.args)
            )
"""
                % clsname
            ),
            locals(),
        )

# NOTE: most assertion expressions were generated, then formatted
# by PyTidy, hence the dense formatting.


class LexerTest(TemplateTest):
    def _compare(self, node, expected):
        eq_(repr(node), repr(expected))

    def test_text_and_tag(self):
        template = """
<b>Hello world</b>
        <%def name="foo()">
                this is a def.
        </%def>

        and some more text.
"""
        node = Lexer(template).parse()
        self._compare(
            node,
            TemplateNode(
                {},
                [
                    Text("""\n<b>Hello world</b>\n        """, (1, 1)),
                    DefTag(
                        "def",
                        {"name": "foo()"},
                        (3, 9),
                        [
                            Text(
                                "\n                this is a def.\n        ",
                                (3, 28),
                            )
                        ],
                    ),
                    Text("""\n\n        and some more text.\n""", (5, 16)),
                ],
            ),
        )

    def test_unclosed_tag(self):
        template = """

            <%def name="foo()">
             other text
        """
        try:
            Lexer(template).parse()
            assert False
        except exceptions.SyntaxException:
            eq_(
                str(compat.exception_as()),
                "Unclosed tag: <%def> at line: 5 char: 9",
            )

    def test_onlyclosed_tag(self):
        template = """
            <%def name="foo()">
                foo
            </%def>

            </%namespace>

            hi.
        """
        assert_raises(exceptions.SyntaxException, Lexer(template).parse)

    def test_noexpr_allowed(self):
        template = """
            <%namespace name="${foo}"/>
        """
        assert_raises(exceptions.CompileException, Lexer(template).parse)

    def test_closing_tag_many_spaces(self):
        """test #367"""
        template = '<%def name="foo()"> this is a def. </%' + " " * 10000
        assert_raises(exceptions.SyntaxException, Lexer(template).parse)

    def test_opening_tag_many_quotes(self):
        """test #366"""
        template = "<%0" + '"' * 3000
        assert_raises(exceptions.SyntaxException, Lexer(template).parse)

    def test_unmatched_tag(self):
        template = """
        <%namespace name="bar">
        <%def name="foo()">
            foo
            </%namespace>
        </%def>


        hi.
"""
        assert_raises(exceptions.SyntaxException, Lexer(template).parse)

    def test_nonexistent_tag(self):
        template = """
            <%lala x="5"/>
        """
        assert_raises(exceptions.CompileException, Lexer(template).parse)

    def test_wrongcase_tag(self):
        template = """
            <%DEF name="foo()">
            </%def>

        """
        assert_raises(exceptions.CompileException, Lexer(template).parse)

    def test_percent_escape(self):
        template = """

%% some whatever.

    %% more some whatever
    % if foo:
    % endif
        """
        node = Lexer(template).parse()
        self._compare(
            node,
            TemplateNode(
                {},
                [
                    Text("""\n\n""", (1, 1)),
                    Text("""% some whatever.\n\n""", (3, 2)),
                    Text("   %% more some whatever\n", (5, 2)),
                    ControlLine("if", "if foo:", False, (6, 1)),
                    ControlLine("if", "endif", True, (7, 1)),
                    Text("        ", (8, 1)),
                ],
            ),
        )

    def test_old_multiline_comment(self):
        template = """#*"""
        node = Lexer(template).parse()
        self._compare(node, TemplateNode({}, [Text("""#*""", (1, 1))]))

    def test_text_tag(self):
        template = """
        ## comment
        % if foo:
            hi
        % endif
        <%text>
            # more code

            % more code
            <%illegal compionent>/></>
            <%def name="laal()">def</%def>


        </%text>

        <%def name="foo()">this is foo</%def>

        % if bar:
            code
        % endif
        """
        node = Lexer(template).parse()
        self._compare(
            node,
            TemplateNode(
                {},
                [
                    Text("\n", (1, 1)),
                    Comment("comment", (2, 1)),
                    ControlLine("if", "if foo:", False, (3, 1)),
                    Text("            hi\n", (4, 1)),
                    ControlLine("if", "endif", True, (5, 1)),
                    Text("        ", (6, 1)),
                    TextTag(
                        "text",
                        {},
                        (6, 9),
                        [
                            Text(
                                "\n            # more code\n\n           "
                                " % more code\n            "
                                "<%illegal compionent>/></>\n"
                                '            <%def name="laal()">def</%def>'
                                "\n\n\n        ",
                                (6, 16),
                            )
                        ],
                    ),
                    Text("\n\n        ", (14, 17)),
                    DefTag(
                        "def",
                        {"name": "foo()"},
                        (16, 9),
                        [Text("this is foo", (16, 28))],
                    ),
                    Text("\n\n", (16, 46)),
                    ControlLine("if", "if bar:", False, (18, 1)),
                    Text("            code\n", (19, 1)),
                    ControlLine("if", "endif", True, (20, 1)),
                    Text("        ", (21, 1)),
                ],
            ),
        )

    def test_def_syntax(self):
        template = """
        <%def lala>
            hi
        </%def>
"""
        assert_raises(exceptions.CompileException, Lexer(template).parse)

    def test_def_syntax_2(self):
        template = """
        <%def name="lala">
            hi
        </%def>
    """
        assert_raises(exceptions.CompileException, Lexer(template).parse)

    def test_whitespace_equals(self):
        template = """
            <%def name = "adef()" >
              adef
            </%def>
        """
        node = Lexer(template).parse()
        self._compare(
            node,
            TemplateNode(
                {},
                [
                    Text("\n            ", (1, 1)),
                    DefTag(
                        "def",
                        {"name": "adef()"},
                        (2, 13),
                        [
                            Text(
                                """\n              adef\n            """,
                                (2, 36),
                            )
                        ],
                    ),
                    Text("\n        ", (4, 20)),
                ],
            ),
        )

    def test_ns_tag_closed(self):
        template = """

            <%self:go x="1" y="2" z="${'hi' + ' ' + 'there'}"/>
        """
        nodes = Lexer(template).parse()
        self._compare(
            nodes,
            TemplateNode(
                {},
                [
                    Text(
                        """

            """,
                        (1, 1),
                    ),
                    CallNamespaceTag(
                        "self:go",
                        {"x": "1", "y": "2", "z": "${'hi' + ' ' + 'there'}"},
                        (3, 13),
                        [],
                    ),
                    Text("\n        ", (3, 64)),
                ],
            ),
        )

    def test_ns_tag_empty(self):
        template = """
            <%form:option value=""></%form:option>
        """
        nodes = Lexer(template).parse()
        self._compare(
            nodes,
            TemplateNode(
                {},
                [
                    Text("\n            ", (1, 1)),
                    CallNamespaceTag(
                        "form:option", {"value": ""}, (2, 13), []
                    ),
                    Text("\n        ", (2, 51)),
                ],
            ),
        )

    def test_ns_tag_open(self):
        template = """

            <%self:go x="1" y="${process()}">
                this is the body
            </%self:go>
        """
        nodes = Lexer(template).parse()
        self._compare(
            nodes,
            TemplateNode(
                {},
                [
                    Text(
                        """

            """,
                        (1, 1),
                    ),
                    CallNamespaceTag(
                        "self:go",
                        {"x": "1", "y": "${process()}"},
                        (3, 13),
                        [
                            Text(
                                """
                this is the body
            """,
                                (3, 46),
                            )
                        ],
                    ),
                    Text("\n        ", (5, 24)),
                ],
            ),
        )

    def test_expr_in_attribute(self):
        """test some slightly trickier expressions.

        you can still trip up the expression parsing, though, unless we
        integrated really deeply somehow with AST."""

        template = """
            <%call expr="foo>bar and 'lala' or 'hoho'"/>
            <%call expr='foo<bar and hoho>lala and "x" + "y"'/>
        """
        nodes = Lexer(template).parse()
        self._compare(
            nodes,
            TemplateNode(
                {},
                [
                    Text("\n            ", (1, 1)),
                    CallTag(
                        "call",
                        {"expr": "foo>bar and 'lala' or 'hoho'"},
                        (2, 13),
                        [],
                    ),
                    Text("\n            ", (2, 57)),
                    CallTag(
                        "call",
                        {"expr": 'foo<bar and hoho>lala and "x" + "y"'},
                        (3, 13),
                        [],
                    ),
                    Text("\n        ", (3, 64)),
                ],
            ),
        )

    @pytest.mark.parametrize("comma,numchars", [(",", 48), ("", 47)])
    def test_pagetag(self, comma, numchars):
        # note that the comma here looks like:
        # <%page cached="True", args="a, b"/>
        # that's what this test has looked like for decades, however, the
        # comma there is not actually the right syntax.  When issue #366
        # was fixed, the reg was altered to accommodate for this comma to allow
        # backwards compat
        template = f"""
            <%page cached="True"{comma} args="a, b"/>

            some template
        """
        nodes = Lexer(template).parse()
        self._compare(
            nodes,
            TemplateNode(
                {},
                [
                    Text("\n            ", (1, 1)),
                    PageTag(
                        "page", {"args": "a, b", "cached": "True"}, (2, 13), []
                    ),
                    Text(
                        """

            some template
        """,
                        (2, numchars),
                    ),
                ],
            ),
        )

    def test_nesting(self):
        template = """

        <%namespace name="ns">
            <%def name="lala(hi, there)">
                <%call expr="something()"/>
            </%def>
        </%namespace>

        """
        nodes = Lexer(template).parse()
        self._compare(
            nodes,
            TemplateNode(
                {},
                [
                    Text(
                        """

        """,
                        (1, 1),
                    ),
                    NamespaceTag(
                        "namespace",
                        {"name": "ns"},
                        (3, 9),
                        [
                            Text("\n            ", (3, 31)),
                            DefTag(
                                "def",
                                {"name": "lala(hi, there)"},
                                (4, 13),
                                [
                                    Text("\n                ", (4, 42)),
                                    CallTag(
                                        "call",
                                        {"expr": "something()"},
                                        (5, 17),
                                        [],
                                    ),
                                    Text("\n            ", (5, 44)),
                                ],
                            ),
                            Text("\n        ", (6, 20)),
                        ],
                    ),
                    Text(
                        """

        """,
                        (7, 22),
                    ),
                ],
            ),
        )

    def test_code(self):
        template = """text
    <%
        print("hi")
        for x in range(1,5):
            print(x)
    %>
more text
    <%!
        import foo
    %>
"""
        nodes = Lexer(template).parse()
        self._compare(
            nodes,
            TemplateNode(
                {},
                [
                    Text("text\n    ", (1, 1)),
                    Code(
                        '\nprint("hi")\nfor x in range(1,5):\n    '
                        "print(x)\n    \n",
                        False,
                        (2, 5),
                    ),
                    Text("\nmore text\n    ", (6, 7)),
                    Code("\nimport foo\n    \n", True, (8, 5)),
                    Text("\n", (10, 7)),
                ],
            ),
        )

    def test_code_and_tags(self):
        template = """
<%namespace name="foo">
    <%def name="x()">
        this is x
    </%def>
    <%def name="y()">
        this is y
    </%def>
</%namespace>

<%
    result = []
    data = get_data()
    for x in data:
        result.append(x+7)
%>

    result: <%call expr="foo.x(result)"/>
"""
        nodes = Lexer(template).parse()
        self._compare(
            nodes,
            TemplateNode(
                {},
                [
                    Text("\n", (1, 1)),
                    NamespaceTag(
                        "namespace",
                        {"name": "foo"},
                        (2, 1),
                        [
                            Text("\n    ", (2, 24)),
                            DefTag(
                                "def",
                                {"name": "x()"},
                                (3, 5),
                                [
                                    Text(
                                        """\n        this is x\n    """,
                                        (3, 22),
                                    )
                                ],
                            ),
                            Text("\n    ", (5, 12)),
                            DefTag(
                                "def",
                                {"name": "y()"},
                                (6, 5),
                                [
                                    Text(
                                        """\n        this is y\n    """,
                                        (6, 22),
                                    )
                                ],
                            ),
                            Text("\n", (8, 12)),
                        ],
                    ),
                    Text("""\n\n""", (9, 14)),
                    Code(
                        """\nresult = []\ndata = get_data()\n"""
                        """for x in data:\n    result.append(x+7)\n\n""",
                        False,
                        (11, 1),
                    ),
                    Text("""\n\n    result: """, (16, 3)),
                    CallTag("call", {"expr": "foo.x(result)"}, (18, 13), []),
                    Text("\n", (18, 42)),
                ],
            ),
        )

    def test_expression(self):
        template = """
        this is some ${text} and this is ${textwith | escapes, moreescapes}
        <%def name="hi()">
            give me ${foo()} and ${bar()}
        </%def>
        ${hi()}
"""
        nodes = Lexer(template).parse()
        self._compare(
            nodes,
            TemplateNode(
                {},
                [
                    Text("\n        this is some ", (1, 1)),
                    Expression("text", [], (2, 22)),
                    Text(" and this is ", (2, 29)),
                    Expression(
                        "textwith ", ["escapes", "moreescapes"], (2, 42)
                    ),
                    Text("\n        ", (2, 76)),
                    DefTag(
                        "def",
                        {"name": "hi()"},
                        (3, 9),
                        [
                            Text("\n            give me ", (3, 27)),
                            Expression("foo()", [], (4, 21)),
                            Text(" and ", (4, 29)),
                            Expression("bar()", [], (4, 34)),
                            Text("\n        ", (4, 42)),
                        ],
                    ),
                    Text("\n        ", (5, 16)),
                    Expression("hi()", [], (6, 9)),
                    Text("\n", (6, 16)),
                ],
            ),
        )

    def test_tricky_expression(self):
        template = """

            ${x and "|" or "hi"}
        """
        nodes = Lexer(template).parse()
        self._compare(
            nodes,
            TemplateNode(
                {},
                [
                    Text("\n\n            ", (1, 1)),
                    Expression('x and "|" or "hi"', [], (3, 13)),
                    Text("\n        ", (3, 33)),
                ],
            ),
        )

        template = r"""

            ${hello + '''heres '{|}' text | | }''' | escape1}
            ${'Tricky string: ' + '\\\"\\\'|\\'}
        """
        nodes = Lexer(template).parse()
        self._compare(
            nodes,
            TemplateNode(
                {},
                [
                    Text("\n\n            ", (1, 1)),
                    Expression(
                        "hello + '''heres '{|}' text | | }''' ",
                        ["escape1"],
                        (3, 13),
                    ),
                    Text("\n            ", (3, 62)),
                    Expression(
                        r"""'Tricky string: ' + '\\\"\\\'|\\'""", [], (4, 13)
                    ),
                    Text("\n        ", (4, 49)),
                ],
            ),
        )

    def test_tricky_code(self):
        template = """<% print('hi %>') %>"""
        nodes = Lexer(template).parse()
        self._compare(
            nodes, TemplateNode({}, [Code("print('hi %>') \n", False, (1, 1))])
        )

    def test_tricky_code_2(self):
        template = """<%
        # someone's comment
%>
        """
        nodes = Lexer(template).parse()
        self._compare(
            nodes,
            TemplateNode(
                {},
                [
                    Code(
                        """
        # someone's comment

""",
                        False,
                        (1, 1),
                    ),
                    Text("\n        ", (3, 3)),
                ],
            ),
        )

    def test_tricky_code_3(self):
        template = """<%
        print('hi')
        # this is a comment
        # another comment
        x = 7 # someone's '''comment
        print('''
    there
    ''')
        # someone else's comment
%> '''and now some text '''"""
        nodes = Lexer(template).parse()
        self._compare(
            nodes,
            TemplateNode(
                {},
                [
                    Code(
                        """
print('hi')
# this is a comment
# another comment
x = 7 # someone's '''comment
print('''
    there
    ''')
# someone else's comment

""",
                        False,
                        (1, 1),
                    ),
                    Text(" '''and now some text '''", (10, 3)),
                ],
            ),
        )

    def test_tricky_code_4(self):
        template = """<% foo = "\\"\\\\" %>"""
        nodes = Lexer(template).parse()
        self._compare(
            nodes,
            TemplateNode({}, [Code("""foo = "\\"\\\\" \n""", False, (1, 1))]),
        )

    def test_tricky_code_5(self):
        template = """before ${ {'key': 'value'} } after"""
        nodes = Lexer(template).parse()
        self._compare(
            nodes,
            TemplateNode(
                {},
                [
                    Text("before ", (1, 1)),
                    Expression(" {'key': 'value'} ", [], (1, 8)),
                    Text(" after", (1, 29)),
                ],
            ),
        )

    def test_tricky_code_6(self):
        template = """before ${ (0x5302 | 0x0400) } after"""
        nodes = Lexer(template).parse()
        self._compare(
            nodes,
            TemplateNode(
                {},
                [
                    Text("before ", (1, 1)),
                    Expression(" (0x5302 | 0x0400) ", [], (1, 8)),
                    Text(" after", (1, 30)),
                ],
            ),
        )

    def test_control_lines(self):
        template = """
text text la la
% if foo():
 mroe text la la blah blah
% endif

        and osme more stuff
        % for l in range(1,5):
    tex tesl asdl l is ${l} kfmas d
      % endfor
    tetx text

"""
        nodes = Lexer(template).parse()
        self._compare(
            nodes,
            TemplateNode(
                {},
                [
                    Text("""\ntext text la la\n""", (1, 1)),
                    ControlLine("if", "if foo():", False, (3, 1)),
                    Text(" mroe text la la blah blah\n", (4, 1)),
                    ControlLine("if", "endif", True, (5, 1)),
                    Text("""\n        and osme more stuff\n""", (6, 1)),
                    ControlLine("for", "for l in range(1,5):", False, (8, 1)),
                    Text("    tex tesl asdl l is ", (9, 1)),
                    Expression("l", [], (9, 24)),
                    Text(" kfmas d\n", (9, 28)),
                    ControlLine("for", "endfor", True, (10, 1)),
                    Text("""    tetx text\n\n""", (11, 1)),
                ],
            ),
        )

    def test_control_lines_2(self):
        template = """% for file in requestattr['toc'].filenames:
    x
% endfor
"""
        nodes = Lexer(template).parse()
        self._compare(
            nodes,
            TemplateNode(
                {},
                [
                    ControlLine(
                        "for",
                        "for file in requestattr['toc'].filenames:",
                        False,
                        (1, 1),
                    ),
                    Text("    x\n", (2, 1)),
                    ControlLine("for", "endfor", True, (3, 1)),
                ],
            ),
        )

    def test_long_control_lines(self):
        template = """
    % for file in \\
        requestattr['toc'].filenames:
        x
    % endfor
        """
        nodes = Lexer(template).parse()
        self._compare(
            nodes,
            TemplateNode(
                {},
                [
                    Text("\n", (1, 1)),
                    ControlLine(
                        "for",
                        "for file in \\\n        "
                        "requestattr['toc'].filenames:",
                        False,
                        (2, 1),
                    ),
                    Text("        x\n", (4, 1)),
                    ControlLine("for", "endfor", True, (5, 1)),
                    Text("        ", (6, 1)),
                ],
            ),
        )

    def test_unmatched_control(self):
        template = """

        % if foo:
            % for x in range(1,5):
        % endif
"""
        assert_raises_message(
            exceptions.SyntaxException,
            "Keyword 'endif' doesn't match keyword 'for' at line: 5 char: 1",
            Lexer(template).parse,
        )

    def test_unmatched_control_2(self):
        template = """

        % if foo:
            % for x in range(1,5):
            % endfor
"""

        assert_raises_message(
            exceptions.SyntaxException,
            "Unterminated control keyword: 'if' at line: 3 char: 1",
            Lexer(template).parse,
        )

    def test_unmatched_control_3(self):
        template = """

        % if foo:
            % for x in range(1,5):
            % endlala
        % endif
"""
        assert_raises_message(
            exceptions.SyntaxException,
            "Keyword 'endlala' doesn't match keyword 'for' at line: 5 char: 1",
            Lexer(template).parse,
        )

    def test_ternary_control(self):
        template = """
        % if x:
            hi
        % elif y+7==10:
            there
        % elif lala:
            lala
        % else:
            hi
        % endif
"""
        nodes = Lexer(template).parse()
        self._compare(
            nodes,
            TemplateNode(
                {},
                [
                    Text("\n", (1, 1)),
                    ControlLine("if", "if x:", False, (2, 1)),
                    Text("            hi\n", (3, 1)),
                    ControlLine("elif", "elif y+7==10:", False, (4, 1)),
                    Text("            there\n", (5, 1)),
                    ControlLine("elif", "elif lala:", False, (6, 1)),
                    Text("            lala\n", (7, 1)),
                    ControlLine("else", "else:", False, (8, 1)),
                    Text("            hi\n", (9, 1)),
                    ControlLine("if", "endif", True, (10, 1)),
                ],
            ),
        )

    def test_integration(self):
        template = """<%namespace name="foo" file="somefile.html"/>
 ## inherit from foobar.html
<%inherit file="foobar.html"/>

<%def name="header()">
     <div>header</div>
</%def>
<%def name="footer()">
    <div> footer</div>
</%def>

<table>
    % for j in data():
    <tr>
        % for x in j:
            <td>Hello ${x| h}</td>
        % endfor
    </tr>
    % endfor
</table>
"""
        nodes = Lexer(template).parse()
        self._compare(
            nodes,
            TemplateNode(
                {},
                [
                    NamespaceTag(
                        "namespace",
                        {"file": "somefile.html", "name": "foo"},
                        (1, 1),
                        [],
                    ),
                    Text("\n", (1, 46)),
                    Comment("inherit from foobar.html", (2, 1)),
                    InheritTag("inherit", {"file": "foobar.html"}, (3, 1), []),
                    Text("""\n\n""", (3, 31)),
                    DefTag(
                        "def",
                        {"name": "header()"},
                        (5, 1),
                        [Text("""\n     <div>header</div>\n""", (5, 23))],
                    ),
                    Text("\n", (7, 8)),
                    DefTag(
                        "def",
                        {"name": "footer()"},
                        (8, 1),
                        [Text("""\n    <div> footer</div>\n""", (8, 23))],
                    ),
                    Text("""\n\n<table>\n""", (10, 8)),
                    ControlLine("for", "for j in data():", False, (13, 1)),
                    Text("    <tr>\n", (14, 1)),
                    ControlLine("for", "for x in j:", False, (15, 1)),
                    Text("            <td>Hello ", (16, 1)),
                    Expression("x", ["h"], (16, 23)),
                    Text("</td>\n", (16, 30)),
                    ControlLine("for", "endfor", True, (17, 1)),
                    Text("    </tr>\n", (18, 1)),
                    ControlLine("for", "endfor", True, (19, 1)),
                    Text("</table>\n", (20, 1)),
                ],
            ),
        )

    def test_comment_after_statement(self):
        template = """
        % if x: #comment
            hi
        % else: #next
            hi
        % endif #end
"""
        nodes = Lexer(template).parse()
        self._compare(
            nodes,
            TemplateNode(
                {},
                [
                    Text("\n", (1, 1)),
                    ControlLine("if", "if x: #comment", False, (2, 1)),
                    Text("            hi\n", (3, 1)),
                    ControlLine("else", "else: #next", False, (4, 1)),
                    Text("            hi\n", (5, 1)),
                    ControlLine("if", "endif #end", True, (6, 1)),
                ],
            ),
        )

    def test_crlf(self):
        template = util.read_file(self._file_path("crlf.html"))
        nodes = Lexer(template).parse()
        self._compare(
            nodes,
            TemplateNode(
                {},
                [
                    Text("<html>\r\n\r\n", (1, 1)),
                    PageTag(
                        "page",
                        {"args": "a=['foo',\n                'bar']"},
                        (3, 1),
                        [],
                    ),
                    Text("\r\n\r\nlike the name says.\r\n\r\n", (4, 26)),
                    ControlLine("for", "for x in [1,2,3]:", False, (8, 1)),
                    Text("        ", (9, 1)),
                    Expression("x", [], (9, 9)),
                    ControlLine("for", "endfor", True, (10, 1)),
                    Text("\r\n", (11, 1)),
                    Expression(
                        "trumpeter == 'Miles' and "
                        "trumpeter or \\\n      'Dizzy'",
                        [],
                        (12, 1),
                    ),
                    Text("\r\n\r\n", (13, 15)),
                    DefTag(
                        "def",
                        {"name": "hi()"},
                        (15, 1),
                        [Text("\r\n    hi!\r\n", (15, 19))],
                    ),
                    Text("\r\n\r\n</html>\r\n", (17, 8)),
                ],
            ),
        )
        assert (
            flatten_result(Template(template).render())
            == """<html> like the name says. 1 2 3 Dizzy </html>"""
        )

    def test_comments(self):
        template = """
<style>
 #someselector
 # other non comment stuff
</style>
## a comment

# also not a comment

   ## this is a comment

this is ## not a comment

<%doc> multiline
comment
</%doc>

hi
"""
        nodes = Lexer(template).parse()
        self._compare(
            nodes,
            TemplateNode(
                {},
                [
                    Text(
                        """\n<style>\n #someselector\n # """
                        """other non comment stuff\n</style>\n""",
                        (1, 1),
                    ),
                    Comment("a comment", (6, 1)),
                    Text("""\n# also not a comment\n\n""", (7, 1)),
                    Comment("this is a comment", (10, 1)),
                    Text("""\nthis is ## not a comment\n\n""", (11, 1)),
                    Comment(""" multiline\ncomment\n""", (14, 1)),
                    Text(
                        """

hi
""",
                        (16, 8),
                    ),
                ],
            ),
        )

    def test_docs(self):
        template = """
        <%doc>
            this is a comment
        </%doc>
        <%def name="foo()">
            <%doc>
                this is the foo func
            </%doc>
        </%def>
        """
        nodes = Lexer(template).parse()
        self._compare(
            nodes,
            TemplateNode(
                {},
                [
                    Text("\n        ", (1, 1)),
                    Comment(
                        """\n            this is a comment\n        """, (2, 9)
                    ),
                    Text("\n        ", (4, 16)),
                    DefTag(
                        "def",
                        {"name": "foo()"},
                        (5, 9),
                        [
                            Text("\n            ", (5, 28)),
                            Comment(
                                """\n                this is the foo func\n"""
                                """            """,
                                (6, 13),
                            ),
                            Text("\n        ", (8, 20)),
                        ],
                    ),
                    Text("\n        ", (9, 16)),
                ],
            ),
        )

    def test_preprocess(self):
        def preproc(text):
            return re.sub(r"(?<=\n)\s*#[^#]", "##", text)

        template = """
    hi
    # old style comment
# another comment
"""
        nodes = Lexer(template, preprocessor=preproc).parse()
        self._compare(
            nodes,
            TemplateNode(
                {},
                [
                    Text("""\n    hi\n""", (1, 1)),
                    Comment("old style comment", (3, 1)),
                    Comment("another comment", (4, 1)),
                ],
            ),
        )
