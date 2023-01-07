from mako.template import Template
from mako.testing.helpers import flatten_result


class DecoratorTest:
    def test_toplevel(self):
        template = Template(
            """
            <%!
                def bar(fn):
                    def decorate(context, *args, **kw):
                        return "BAR" + runtime.capture"""
            """(context, fn, *args, **kw) + "BAR"
                    return decorate
            %>

            <%def name="foo(y, x)" decorator="bar">
                this is foo ${y} ${x}
            </%def>

            ${foo(1, x=5)}
        """
        )

        assert flatten_result(template.render()) == "BAR this is foo 1 5 BAR"

    def test_toplevel_contextual(self):
        template = Template(
            """
            <%!
                def bar(fn):
                    def decorate(context):
                        context.write("BAR")
                        fn()
                        context.write("BAR")
                        return ''
                    return decorate
            %>

            <%def name="foo()" decorator="bar">
                this is foo
            </%def>

            ${foo()}
        """
        )

        assert flatten_result(template.render()) == "BAR this is foo BAR"

        assert (
            flatten_result(template.get_def("foo").render())
            == "BAR this is foo BAR"
        )

    def test_nested(self):
        template = Template(
            """
            <%!
                def bat(fn):
                    def decorate(context):
                        return "BAT" + runtime.capture(context, fn) + "BAT"
                    return decorate
            %>

            <%def name="foo()">

                <%def name="bar()" decorator="bat">
                    this is bar
                </%def>
                ${bar()}
            </%def>

            ${foo()}
        """
        )

        assert flatten_result(template.render()) == "BAT this is bar BAT"

    def test_toplevel_decorated_name(self):
        template = Template(
            """
            <%!
                def bar(fn):
                    def decorate(context, *args, **kw):
                        return "function " + fn.__name__ + """
            """" " + runtime.capture(context, fn, *args, **kw)
                    return decorate
            %>

            <%def name="foo(y, x)" decorator="bar">
                this is foo ${y} ${x}
            </%def>

            ${foo(1, x=5)}
        """
        )

        assert (
            flatten_result(template.render()) == "function foo this is foo 1 5"
        )

    def test_nested_decorated_name(self):
        template = Template(
            """
            <%!
                def bat(fn):
                    def decorate(context):
                        return "function " + fn.__name__ + " " + """
            """runtime.capture(context, fn)
                    return decorate
            %>

            <%def name="foo()">

                <%def name="bar()" decorator="bat">
                    this is bar
                </%def>
                ${bar()}
            </%def>

            ${foo()}
        """
        )

        assert flatten_result(template.render()) == "function bar this is bar"
