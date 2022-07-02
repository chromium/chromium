from mako.ext.turbogears import TGPlugin
from mako.testing.config import config
from mako.testing.fixtures import TemplateTest
from mako.testing.helpers import result_lines

tl = TGPlugin(
    options=dict(directories=[config.template_base]), extension="html"
)


class TestTGPlugin(TemplateTest):
    def test_basic(self):
        t = tl.load_template("/index.html")
        assert result_lines(t.render()) == ["this is index"]

    def test_subdir(self):
        t = tl.load_template("/subdir/index.html")
        assert result_lines(t.render()) == [
            "this is sub index",
            "this is include 2",
        ]

        assert (
            tl.load_template("/subdir/index.html").module_id
            == "_subdir_index_html"
        )

    def test_basic_dot(self):
        t = tl.load_template("index")
        assert result_lines(t.render()) == ["this is index"]

    def test_subdir_dot(self):
        t = tl.load_template("subdir.index")
        assert result_lines(t.render()) == [
            "this is sub index",
            "this is include 2",
        ]

        assert (
            tl.load_template("subdir.index").module_id == "_subdir_index_html"
        )

    def test_string(self):
        t = tl.load_template("foo", "hello world")
        assert t.render() == "hello world"

    def test_render(self):
        assert result_lines(tl.render({}, template="/index.html")) == [
            "this is index"
        ]
        assert result_lines(tl.render({}, template=("/index.html"))) == [
            "this is index"
        ]
