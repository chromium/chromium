import os

from .. import skip_if
from .. import template_base
from .. import TemplateTest

try:
    import lingua
except:
    lingua = None

if lingua is not None:
    from mako.ext.linguaplugin import LinguaMakoExtractor
    from lingua.extractors import register_extractors


class MockOptions:
    keywords = []
    domain = None


def skip():
    return skip_if(
        lambda: not lingua, "lingua not installed: skipping linguaplugin test"
    )


class ExtractMakoTestCase(TemplateTest):
    @skip()
    def test_extract(self):
        register_extractors()
        plugin = LinguaMakoExtractor({"comment-tags": "TRANSLATOR"})
        messages = list(
            plugin(os.path.join(template_base, "gettext.mako"), MockOptions())
        )
        msgids = [(m.msgid, m.msgid_plural) for m in messages]
        self.assertEqual(
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
                (u"foo", None),
                ("hoho", None),
                (u"bar", None),
                ("Inside a p tag", None),
                ("Later in a p tag", None),
                ("No action at a distance.", None),
            ],
        )
