import os

import pytest

from mako.testing.assertions import eq_
from mako.testing.config import config
from mako.testing.exclusions import requires_lingua
from mako.testing.fixtures import TemplateTest


class MockOptions:
    keywords = []
    domain = None
    comment_tag = True


@requires_lingua
class MakoExtractTest(TemplateTest):
    @pytest.fixture(autouse=True)
    def register_lingua_extractors(self):

        from lingua.extractors import register_extractors

        register_extractors()

    def test_extract(self):
        from mako.ext.linguaplugin import LinguaMakoExtractor

        plugin = LinguaMakoExtractor({"comment-tags": "TRANSLATOR"})
        messages = list(
            plugin(
                os.path.join(config.template_base, "gettext.mako"),
                MockOptions(),
            )
        )
        msgids = [(m.msgid, m.msgid_plural) for m in messages]
        eq_(
            msgids,
            [
                ("Page arg 1", None),
                ("Page arg 2", None),
                ("Begin", None),
                ("Hi there!", None),
                ("Hello", None),
                ("Welcome", None),
                ("Yo", None),
                ("The", None),
                ("bunny", "bunnies"),
                ("Goodbye", None),
                ("Babel", None),
                ("hella", "hellas"),
                ("The", None),
                ("bunny", "bunnies"),
                ("Goodbye, really!", None),
                ("P.S. byebye", None),
                ("Top", None),
                ("foo", None),
                ("hoho", None),
                ("bar", None),
                ("Inside a p tag", None),
                ("Later in a p tag", None),
                ("No action at a distance.", None),
            ],
        )
