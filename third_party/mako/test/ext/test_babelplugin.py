import io
import os
import unittest

from mako import compat
from .. import skip_if
from .. import template_base
from .. import TemplateTest

try:
    import babel.messages.extract as babel
    from mako.ext.babelplugin import extract

except ImportError:
    babel = None


def skip():
    return skip_if(
        lambda: not babel, "babel not installed: skipping babelplugin test"
    )


class Test_extract(unittest.TestCase):
    @skip()
    def test_parse_python_expression(self):
        input_ = io.BytesIO(compat.b('<p>${_("Message")}</p>'))
        messages = list(extract(input_, ["_"], [], {}))
        self.assertEqual(messages, [(1, "_", compat.u("Message"), [])])

    @skip()
    def test_python_gettext_call(self):
        input_ = io.BytesIO(compat.b('<p>${_("Message")}</p>'))
        messages = list(extract(input_, ["_"], [], {}))
        self.assertEqual(messages, [(1, "_", compat.u("Message"), [])])

    @skip()
    def test_translator_comment(self):
        input_ = io.BytesIO(
            compat.b(
                """
        <p>
          ## TRANSLATORS: This is a comment.
          ${_("Message")}
        </p>"""
            )
        )
        messages = list(extract(input_, ["_"], ["TRANSLATORS:"], {}))
        self.assertEqual(
            messages,
            [
                (
                    4,
                    "_",
                    compat.u("Message"),
                    [compat.u("TRANSLATORS: This is a comment.")],
                )
            ],
        )


class ExtractMakoTestCase(TemplateTest):
    @skip()
    def test_extract(self):
        mako_tmpl = open(os.path.join(template_base, "gettext.mako"))
        messages = list(
            extract(
                mako_tmpl,
                {"_": None, "gettext": None, "ungettext": (1, 2)},
                ["TRANSLATOR:"],
                {},
            )
        )
        expected = [
            (1, "_", "Page arg 1", []),
            (1, "_", "Page arg 2", []),
            (10, "gettext", "Begin", []),
            (14, "_", "Hi there!", ["TRANSLATOR: Hi there!"]),
            (19, "_", "Hello", []),
            (22, "_", "Welcome", []),
            (25, "_", "Yo", []),
            (36, "_", "The", ["TRANSLATOR: Ensure so and", "so, thanks"]),
            (36, "ungettext", ("bunny", "bunnies", None), []),
            (41, "_", "Goodbye", ["TRANSLATOR: Good bye"]),
            (44, "_", "Babel", []),
            (45, "ungettext", ("hella", "hellas", None), []),
            (62, "_", "The", ["TRANSLATOR: Ensure so and", "so, thanks"]),
            (62, "ungettext", ("bunny", "bunnies", None), []),
            (68, "_", "Goodbye, really!", ["TRANSLATOR: HTML comment"]),
            (71, "_", "P.S. byebye", []),
            (77, "_", "Top", []),
            (83, "_", "foo", []),
            (83, "_", "hoho", []),
            (85, "_", "bar", []),
            (92, "_", "Inside a p tag", ["TRANSLATOR: <p> tag is ok?"]),
            (95, "_", "Later in a p tag", ["TRANSLATOR: also this"]),
            (99, "_", "No action at a distance.", []),
        ]
        self.assertEqual(expected, messages)

    @skip()
    def test_extract_utf8(self):
        mako_tmpl = open(
            os.path.join(template_base, "gettext_utf8.mako"), "rb"
        )
        message = next(
            extract(mako_tmpl, set(["_", None]), [], {"encoding": "utf-8"})
        )
        assert message == (1, "_", u"K\xf6ln", [])

    @skip()
    def test_extract_cp1251(self):
        mako_tmpl = open(
            os.path.join(template_base, "gettext_cp1251.mako"), "rb"
        )
        message = next(
            extract(mako_tmpl, set(["_", None]), [], {"encoding": "cp1251"})
        )
        # "test" in Rusian. File encoding is cp1251 (aka "windows-1251")
        assert message == (1, "_", u"\u0442\u0435\u0441\u0442", [])
