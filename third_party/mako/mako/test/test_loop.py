import re
import unittest

from mako import exceptions
from mako.codegen import _FOR_LOOP
from mako.lookup import TemplateLookup
from mako.runtime import LoopContext
from mako.runtime import LoopStack
from mako.template import Template
from test import assert_raises_message
from test import TemplateTest
from test.util import flatten_result


class TestLoop(unittest.TestCase):
    def test__FOR_LOOP(self):
        for statement, target_list, expression_list in (
            ("for x in y:", "x", "y"),
            ("for x, y in z:", "x, y", "z"),
            ("for (x,y) in z:", "(x,y)", "z"),
            ("for ( x, y, z) in a:", "( x, y, z)", "a"),
            ("for x in [1, 2, 3]:", "x", "[1, 2, 3]"),
            ('for x in "spam":', "x", '"spam"'),
            (
                "for k,v in dict(a=1,b=2).items():",
                "k,v",
                "dict(a=1,b=2).items()",
            ),
            (
                "for x in [y+1 for y in [1, 2, 3]]:",
                "x",
                "[y+1 for y in [1, 2, 3]]",
            ),
        ):
            match = _FOR_LOOP.match(statement)
            assert match and match.groups() == (target_list, expression_list)

    def test_no_loop(self):
        template = Template(
            """% for x in 'spam':
${x}
% endfor"""
        )
        code = template.code
        assert not re.match(r"loop = __M_loop._enter\(:", code), (
            "No need to "
            "generate a loop context if the loop variable wasn't accessed"
        )
        print(template.render())

    def test_loop_demo(self):
        template = Template(
            """x|index|reverse_index|first|last|cycle|even|odd
% for x in 'ham':
${x}|${loop.index}|${loop.reverse_index}|${loop.first}|"""
            """${loop.last}|${loop.cycle('even', 'odd')}|"""
            """${loop.even}|${loop.odd}
% endfor"""
        )
        expected = [
            "x|index|reverse_index|first|last|cycle|even|odd",
            "h|0|2|True|False|even|True|False",
            "a|1|1|False|False|odd|False|True",
            "m|2|0|False|True|even|True|False",
        ]
        code = template.code
        assert "loop = __M_loop._enter(" in code, (
            "Generated a loop context since " "the loop variable was accessed"
        )
        rendered = template.render()
        print(rendered)
        for line in expected:
            assert line in rendered, (
                "Loop variables give information about "
                "the progress of the loop"
            )

    def test_nested_loops(self):
        template = Template(
            """% for x in 'ab':
${x} ${loop.index} <- start in outer loop
% for y in [0, 1]:
${y} ${loop.index} <- go to inner loop
% endfor
${x} ${loop.index} <- back to outer loop
% endfor"""
        )
        rendered = template.render()
        expected = [
            "a 0 <- start in outer loop",
            "0 0 <- go to inner loop",
            "1 1 <- go to inner loop",
            "a 0 <- back to outer loop",
            "b 1 <- start in outer loop",
            "0 0 <- go to inner loop",
            "1 1 <- go to inner loop",
            "b 1 <- back to outer loop",
        ]
        for line in expected:
            assert line in rendered, (
                "The LoopStack allows you to take "
                "advantage of the loop variable even in embedded loops"
            )

    def test_parent_loops(self):
        template = Template(
            """% for x in 'ab':
${x} ${loop.index} <- outer loop
% for y in [0, 1]:
${y} ${loop.index} <- inner loop
${x} ${loop.parent.index} <- parent loop
% endfor
${x} ${loop.index} <- outer loop
% endfor"""
        )
        code = template.code
        rendered = template.render()
        expected = [
            "a 0 <- outer loop",
            "a 0 <- parent loop",
            "b 1 <- outer loop",
            "b 1 <- parent loop",
        ]
        for line in expected:
            print(code)
            assert line in rendered, (
                "The parent attribute of a loop gives "
                "you the previous loop context in the stack"
            )

    def test_out_of_context_access(self):
        template = Template("""${loop.index}""")
        assert_raises_message(
            exceptions.RuntimeException,
            "No loop context is established",
            template.render,
        )


class TestLoopStack(unittest.TestCase):
    def setUp(self):
        self.stack = LoopStack()
        self.bottom = "spam"
        self.stack.stack = [self.bottom]

    def test_enter(self):
        iterable = "ham"
        s = self.stack._enter(iterable)
        assert s is self.stack.stack[-1], (
            "Calling the stack with an iterable returns " "the stack"
        )
        assert iterable == self.stack.stack[-1]._iterable, (
            "and pushes the " "iterable on the top of the stack"
        )

    def test__top(self):
        assert self.bottom == self.stack._top, (
            "_top returns the last item " "on the stack"
        )

    def test__pop(self):
        assert len(self.stack.stack) == 1
        top = self.stack._pop()
        assert top == self.bottom
        assert len(self.stack.stack) == 0

    def test__push(self):
        assert len(self.stack.stack) == 1
        iterable = "ham"
        self.stack._push(iterable)
        assert len(self.stack.stack) == 2
        assert iterable is self.stack._top._iterable

    def test_exit(self):
        iterable = "ham"
        self.stack._enter(iterable)
        before = len(self.stack.stack)
        self.stack._exit()
        after = len(self.stack.stack)
        assert before == (after + 1), "Exiting a context pops the stack"


class TestLoopContext(unittest.TestCase):
    def setUp(self):
        self.iterable = [1, 2, 3]
        self.ctx = LoopContext(self.iterable)

    def test___len__(self):
        assert len(self.iterable) == len(self.ctx), (
            "The LoopContext is the " "same length as the iterable"
        )

    def test_index(self):
        expected = tuple(range(len(self.iterable)))
        actual = tuple(self.ctx.index for i in self.ctx)
        assert expected == actual, (
            "The index is consistent with the current " "iteration count"
        )

    def test_reverse_index(self):
        length = len(self.iterable)
        expected = tuple([length - i - 1 for i in range(length)])
        actual = tuple(self.ctx.reverse_index for i in self.ctx)
        print(expected, actual)
        assert expected == actual, (
            "The reverse_index is the number of " "iterations until the end"
        )

    def test_first(self):
        expected = (True, False, False)
        actual = tuple(self.ctx.first for i in self.ctx)
        assert expected == actual, "first is only true on the first iteration"

    def test_last(self):
        expected = (False, False, True)
        actual = tuple(self.ctx.last for i in self.ctx)
        assert expected == actual, "last is only true on the last iteration"

    def test_even(self):
        expected = (True, False, True)
        actual = tuple(self.ctx.even for i in self.ctx)
        assert expected == actual, "even is true on even iterations"

    def test_odd(self):
        expected = (False, True, False)
        actual = tuple(self.ctx.odd for i in self.ctx)
        assert expected == actual, "odd is true on odd iterations"

    def test_cycle(self):
        expected = ("a", "b", "a")
        actual = tuple(self.ctx.cycle("a", "b") for i in self.ctx)
        assert expected == actual, "cycle endlessly cycles through the values"


class TestLoopFlags(TemplateTest):
    def test_loop_disabled_template(self):
        self._do_memory_test(
            """
            the loop: ${loop}
        """,
            "the loop: hi",
            template_args=dict(loop="hi"),
            filters=flatten_result,
            enable_loop=False,
        )

    def test_loop_disabled_lookup(self):
        l = TemplateLookup(enable_loop=False)
        l.put_string(
            "x",
            """
            the loop: ${loop}
        """,
        )

        self._do_test(
            l.get_template("x"),
            "the loop: hi",
            template_args=dict(loop="hi"),
            filters=flatten_result,
        )

    def test_loop_disabled_override_template(self):
        self._do_memory_test(
            """
            <%page enable_loop="True" />
            % for i in (1, 2, 3):
                ${i} ${loop.index}
            % endfor
        """,
            "1 0 2 1 3 2",
            template_args=dict(loop="hi"),
            filters=flatten_result,
            enable_loop=False,
        )

    def test_loop_disabled_override_lookup(self):
        l = TemplateLookup(enable_loop=False)
        l.put_string(
            "x",
            """
            <%page enable_loop="True" />
            % for i in (1, 2, 3):
                ${i} ${loop.index}
            % endfor
        """,
        )

        self._do_test(
            l.get_template("x"),
            "1 0 2 1 3 2",
            template_args=dict(loop="hi"),
            filters=flatten_result,
        )

    def test_loop_enabled_override_template(self):
        self._do_memory_test(
            """
            <%page enable_loop="True" />
            % for i in (1, 2, 3):
                ${i} ${loop.index}
            % endfor
        """,
            "1 0 2 1 3 2",
            template_args=dict(),
            filters=flatten_result,
        )

    def test_loop_enabled_override_lookup(self):
        l = TemplateLookup()
        l.put_string(
            "x",
            """
            <%page enable_loop="True" />
            % for i in (1, 2, 3):
                ${i} ${loop.index}
            % endfor
        """,
        )

        self._do_test(
            l.get_template("x"),
            "1 0 2 1 3 2",
            template_args=dict(),
            filters=flatten_result,
        )
