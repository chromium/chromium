import sys

from mako import exceptions
from mako.lookup import TemplateLookup
from mako.template import Template
from mako.testing.exclusions import requires_no_pygments_exceptions
from mako.testing.exclusions import requires_pygments_14
from mako.testing.fixtures import TemplateTest
from mako.testing.helpers import result_lines


class ExceptionsTest(TemplateTest):
    def test_html_error_template(self):
        """test the html_error_template"""
        code = """
% i = 0
"""
        try:
            template = Template(code)
            template.render_unicode()
            assert False
        except exceptions.CompileException:
            html_error = exceptions.html_error_template().render_unicode()
            assert (
                "CompileException: Fragment &#39;i = 0&#39; is not "
                "a partial control statement at line: 2 char: 1"
            ) in html_error
            assert "<style>" in html_error
            html_error_stripped = html_error.strip()
            assert html_error_stripped.startswith("<html>")
            assert html_error_stripped.endswith("</html>")

            not_full = exceptions.html_error_template().render_unicode(
                full=False
            )
            assert "<html>" not in not_full
            assert "<style>" in not_full

            no_css = exceptions.html_error_template().render_unicode(css=False)
            assert "<style>" not in no_css
        else:
            assert False, (
                "This function should trigger a CompileException, "
                "but didn't"
            )

    def test_text_error_template(self):
        code = """
% i = 0
"""
        try:
            template = Template(code)
            template.render_unicode()
            assert False
        except exceptions.CompileException:
            text_error = exceptions.text_error_template().render_unicode()
            assert "Traceback (most recent call last):" in text_error
            assert (
                "CompileException: Fragment 'i = 0' is not a partial "
                "control statement"
            ) in text_error

    @requires_pygments_14
    def test_utf8_html_error_template_pygments(self):
        """test the html_error_template with a Template containing UTF-8
        chars"""

        code = """# -*- coding: utf-8 -*-
% if 2 == 2: /an error
${'привет'}
% endif
"""
        try:
            template = Template(code)
            template.render_unicode()
        except exceptions.CompileException:
            html_error = exceptions.html_error_template().render()
            assert (
                "CompileException: Fragment &#39;if 2 == 2: /an "
                "error&#39; is not a partial control statement "
                "at line: 2 char: 1"
            ).encode(
                sys.getdefaultencoding(), "htmlentityreplace"
            ) in html_error

            assert (
                "".encode(sys.getdefaultencoding(), "htmlentityreplace")
                in html_error
            )
        else:
            assert False, (
                "This function should trigger a CompileException, "
                "but didn't"
            )

    @requires_no_pygments_exceptions
    def test_utf8_html_error_template_no_pygments(self):
        """test the html_error_template with a Template containing UTF-8
        chars"""

        code = """# -*- coding: utf-8 -*-
% if 2 == 2: /an error
${'привет'}
% endif
"""
        try:
            template = Template(code)
            template.render_unicode()
        except exceptions.CompileException:
            html_error = exceptions.html_error_template().render()
            assert (
                "CompileException: Fragment &#39;if 2 == 2: /an "
                "error&#39; is not a partial control statement "
                "at line: 2 char: 1"
            ).encode(
                sys.getdefaultencoding(), "htmlentityreplace"
            ) in html_error
            assert (
                "${&#39;привет&#39;}".encode(
                    sys.getdefaultencoding(), "htmlentityreplace"
                )
                in html_error
            )
        else:
            assert False, (
                "This function should trigger a CompileException, "
                "but didn't"
            )

    def test_format_closures(self):
        try:
            exec("def foo():" "    raise RuntimeError('test')", locals())
            foo()  # noqa
        except:
            html_error = exceptions.html_error_template().render()
            assert "RuntimeError: test" in str(html_error)

    def test_py_utf8_html_error_template(self):
        try:
            foo = "日本"  # noqa
            raise RuntimeError("test")
        except:
            html_error = exceptions.html_error_template().render()
            assert "RuntimeError: test" in html_error.decode("utf-8")
            assert "foo = &quot;日本&quot;" in html_error.decode(
                "utf-8"
            ) or "foo = &#34;日本&#34;" in html_error.decode("utf-8")

    def test_py_unicode_error_html_error_template(self):
        try:
            raise RuntimeError("日本")
        except:
            html_error = exceptions.html_error_template().render()
            assert "RuntimeError: 日本".encode("ascii", "ignore") in html_error

    @requires_pygments_14
    def test_format_exceptions_pygments(self):
        l = TemplateLookup(format_exceptions=True)

        l.put_string(
            "foo.html",
            """
<%inherit file="base.html"/>
${foobar}
        """,
        )

        l.put_string(
            "base.html",
            """
        ${self.body()}
        """,
        )

        assert (
            '<table class="syntax-highlightedtable">'
            in l.get_template("foo.html").render_unicode()
        )

    @requires_no_pygments_exceptions
    def test_format_exceptions_no_pygments(self):
        l = TemplateLookup(format_exceptions=True)

        l.put_string(
            "foo.html",
            """
<%inherit file="base.html"/>
${foobar}
        """,
        )

        l.put_string(
            "base.html",
            """
        ${self.body()}
        """,
        )

        assert '<div class="sourceline">${foobar}</div>' in result_lines(
            l.get_template("foo.html").render_unicode()
        )

    @requires_pygments_14
    def test_utf8_format_exceptions_pygments(self):
        """test that htmlentityreplace formatting is applied to
        exceptions reported with format_exceptions=True"""

        l = TemplateLookup(format_exceptions=True)
        l.put_string(
            "foo.html", """# -*- coding: utf-8 -*-\n${'привет' + foobar}"""
        )

        assert "&#39;привет&#39;</span>" in l.get_template(
            "foo.html"
        ).render().decode("utf-8")

    @requires_no_pygments_exceptions
    def test_utf8_format_exceptions_no_pygments(self):
        """test that htmlentityreplace formatting is applied to
        exceptions reported with format_exceptions=True"""

        l = TemplateLookup(format_exceptions=True)
        l.put_string(
            "foo.html", """# -*- coding: utf-8 -*-\n${'привет' + foobar}"""
        )

        assert (
            '<div class="sourceline">${&#39;привет&#39; + foobar}</div>'
            in result_lines(
                l.get_template("foo.html").render().decode("utf-8")
            )
        )

    def test_mod_no_encoding(self):

        mod = __import__("test.foo.mod_no_encoding").foo.mod_no_encoding
        try:
            mod.run()
        except:
            t, v, tback = sys.exc_info()
            exceptions.html_error_template().render_unicode(
                error=v, traceback=tback
            )

    def test_custom_tback(self):
        try:
            raise RuntimeError("error 1")
            foo("bar")  # noqa
        except:
            t, v, tback = sys.exc_info()

        try:
            raise RuntimeError("error 2")
        except:
            html_error = exceptions.html_error_template().render_unicode(
                error=v, traceback=tback
            )

        # obfuscate the text so that this text
        # isn't in the 'wrong' exception
        assert (
            "".join(reversed(");touq&rab;touq&(oof")) in html_error
            or "".join(reversed(");43#&rab;43#&(oof")) in html_error
        )

    def test_tback_no_trace_from_py_file(self):
        try:
            t = self._file_template("runtimeerr.html")
            t.render()
        except:
            t, v, tback = sys.exc_info()

        # and don't even send what we have.
        html_error = exceptions.html_error_template().render_unicode(
            error=v, traceback=None
        )

        assert self.indicates_unbound_local_error(html_error, "y")

    def test_tback_trace_from_py_file(self):
        t = self._file_template("runtimeerr.html")
        try:
            t.render()
            assert False
        except:
            html_error = exceptions.html_error_template().render_unicode()

        assert self.indicates_unbound_local_error(html_error, "y")

    def test_code_block_line_number(self):
        l = TemplateLookup()
        l.put_string(
            "foo.html",
            """
<%
msg = "Something went wrong."
raise RuntimeError(msg)  # This is the line.
%>
            """,
        )
        t = l.get_template("foo.html")
        try:
            t.render()
        except:
            text_error = exceptions.text_error_template().render_unicode()
            assert 'File "foo.html", line 4, in render_body' in text_error
            assert "raise RuntimeError(msg)  # This is the line." in text_error
        else:
            assert False

    def test_module_block_line_number(self):
        l = TemplateLookup()
        l.put_string(
            "foo.html",
            """
<%!
def foo():
    msg = "Something went wrong."
    raise RuntimeError(msg)  # This is the line.
%>
${foo()}
            """,
        )
        t = l.get_template("foo.html")
        try:
            t.render()
        except:
            text_error = exceptions.text_error_template().render_unicode()
            assert 'File "foo.html", line 7, in render_body' in text_error
            assert 'File "foo.html", line 5, in foo' in text_error
            assert "raise RuntimeError(msg)  # This is the line." in text_error
        else:
            assert False

    def test_alternating_file_names(self):
        l = TemplateLookup()
        l.put_string(
            "base.html",
            """
<%!
def broken():
    raise RuntimeError("Something went wrong.")
%> body starts here
<%block name="foo">
    ${broken()}
</%block>
            """,
        )
        l.put_string(
            "foo.html",
            """
<%inherit file="base.html"/>
<%block name="foo">
    ${parent.foo()}
</%block>
            """,
        )
        t = l.get_template("foo.html")
        try:
            t.render()
        except:
            text_error = exceptions.text_error_template().render_unicode()
            assert (
                """
  File "base.html", line 5, in render_body
    %> body starts here
  File "foo.html", line 4, in render_foo
    ${parent.foo()}
  File "base.html", line 7, in render_foo
    ${broken()}
  File "base.html", line 4, in broken
    raise RuntimeError("Something went wrong.")
"""
                in text_error
            )
        else:
            assert False
