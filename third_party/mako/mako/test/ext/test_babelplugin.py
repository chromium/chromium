import io
import os

import pytest

from mako.testing.assertions import eq_
from mako.testing.config import config
from mako.testing.exclusions import requires_babel
from mako.testing.fixtures import TemplateTest


class UsesExtract:
    @pytest.fixture(scope="class")
    def extract(self):
        from mako.ext.babelplugin import extract

        return extract


@requires_babel
class PluginExtractTest(UsesExtract):
    def test_parse_python_expression(self, extract):
        input_ = io.BytesIO(b'<p>${_("Message")}</p>')
        messages = list(extract(input_, ["_"], [], {}))
        eq_(messages, [(1, "_", ("Message"), [])])

    def test_python_gettext_call(self, extract):
        input_ = io.BytesIO(b'<p>${_("Message")}</p>')
        messages = list(extract(input_, ["_"], [], {}))
        eq_(messages, [(1, "_", ("Message"), [])])

    def test_translator_comment(self, extract):
        input_ = io.BytesIO(
            b"""
        <p>
          ## TRANSLATORS: This is a comment.
          ${_("Message")}
        </p>"""
        )
        messages = list(extract(input_, ["_"], ["TRANSLATORS:"], {}))
        eq_(
            messages,
            [
                (
                    4,
                    "_",
                    ("Message"),
                    [("TRANSLATORS: This is a comment.")],
                )
            ],
        )


@requires_babel
class MakoExtractTest(UsesExtract, TemplateTest):
    def test_extract(self, extract):
        with open(
            os.path.join(config.template_base, "gettext.mako")
        ) as mako_tmpl:
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
        eq_(expected, messages)

    def test_extract_utf8(self, extract):
        with open(
            os.path.join(config.template_base, "gettext_utf8.mako"), "rb"
        ) as mako_tmpl:
            message = next(
                extract(mako_tmpl, {"_", None}, [], {"encoding": "utf-8"})
            )
            assert message == (1, "_", "K\xf6ln", [])

    def test_extract_cp1251(self, extract):
        with open(
            os.path.join(config.template_base, "gettext_cp1251.mako"), "rb"
        ) as mako_tmpl:
            message = next(
                extract(mako_tmpl, {"_", None}, [], {"encoding": "cp1251"})
            )
            # "test" in Rusian. File encoding is cp1251 (aka "windows-1251")
            assert message == (1, "_", "\u0442\u0435\u0441\u0442", [])
