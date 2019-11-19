import time

from mako import lookup
from mako.cache import CacheImpl
from mako.cache import register_plugin
from mako.compat import py27
from mako.ext import beaker_cache
from mako.lookup import TemplateLookup
from mako.template import Template
from test import eq_
from test import module_base
from test import SkipTest
from test import TemplateTest
from test.util import result_lines

if beaker_cache.has_beaker:
    import beaker


class SimpleBackend(object):
    def __init__(self):
        self.cache = {}

    def get(self, key, **kw):
        return self.cache[key]

    def invalidate(self, key, **kw):
        self.cache.pop(key, None)

    def put(self, key, value, **kw):
        self.cache[key] = value

    def get_or_create(self, key, creation_function, **kw):
        if key in self.cache:
            return self.cache[key]
        else:
            self.cache[key] = value = creation_function()
            return value


class MockCacheImpl(CacheImpl):
    realcacheimpl = None

    def __init__(self, cache):
        self.cache = cache

    def set_backend(self, cache, backend):
        if backend == "simple":
            self.realcacheimpl = SimpleBackend()
        else:
            self.realcacheimpl = cache._load_impl(backend)

    def _setup_kwargs(self, kw):
        self.kwargs = kw.copy()
        self.kwargs.pop("regions", None)
        self.kwargs.pop("manager", None)
        if self.kwargs.get("region") != "myregion":
            self.kwargs.pop("region", None)

    def get_or_create(self, key, creation_function, **kw):
        self.key = key
        self._setup_kwargs(kw)
        return self.realcacheimpl.get_or_create(key, creation_function, **kw)

    def put(self, key, value, **kw):
        self.key = key
        self._setup_kwargs(kw)
        self.realcacheimpl.put(key, value, **kw)

    def get(self, key, **kw):
        self.key = key
        self._setup_kwargs(kw)
        return self.realcacheimpl.get(key, **kw)

    def invalidate(self, key, **kw):
        self.key = key
        self._setup_kwargs(kw)
        self.realcacheimpl.invalidate(key, **kw)


register_plugin("mock", __name__, "MockCacheImpl")


class CacheTest(TemplateTest):

    real_backend = "simple"

    def _install_mock_cache(self, template, implname=None):
        template.cache_impl = "mock"
        impl = template.cache.impl
        impl.set_backend(template.cache, implname or self.real_backend)
        return impl

    def test_def(self):
        t = Template(
            """
        <%!
            callcount = [0]
        %>
        <%def name="foo()" cached="True">
            this is foo
            <%
            callcount[0] += 1
            %>
        </%def>

        ${foo()}
        ${foo()}
        ${foo()}
        callcount: ${callcount}
"""
        )
        m = self._install_mock_cache(t)
        assert result_lines(t.render()) == [
            "this is foo",
            "this is foo",
            "this is foo",
            "callcount: [1]",
        ]
        assert m.kwargs == {}

    def test_cache_enable(self):
        t = Template(
            """
            <%!
                callcount = [0]
            %>
            <%def name="foo()" cached="True">
                <% callcount[0] += 1 %>
            </%def>
            ${foo()}
            ${foo()}
            callcount: ${callcount}
        """,
            cache_enabled=False,
        )
        self._install_mock_cache(t)

        eq_(t.render().strip(), "callcount: [2]")

    def test_nested_def(self):
        t = Template(
            """
        <%!
            callcount = [0]
        %>
        <%def name="foo()">
            <%def name="bar()" cached="True">
                this is foo
                <%
                callcount[0] += 1
                %>
            </%def>
            ${bar()}
        </%def>

        ${foo()}
        ${foo()}
        ${foo()}
        callcount: ${callcount}
"""
        )
        m = self._install_mock_cache(t)
        assert result_lines(t.render()) == [
            "this is foo",
            "this is foo",
            "this is foo",
            "callcount: [1]",
        ]
        assert m.kwargs == {}

    def test_page(self):
        t = Template(
            """
        <%!
            callcount = [0]
        %>
        <%page cached="True"/>
        this is foo
        <%
        callcount[0] += 1
        %>
        callcount: ${callcount}
"""
        )
        m = self._install_mock_cache(t)
        t.render()
        t.render()
        assert result_lines(t.render()) == ["this is foo", "callcount: [1]"]
        assert m.kwargs == {}

    def test_dynamic_key_with_context(self):
        t = Template(
            """
            <%block name="foo" cached="True" cache_key="${mykey}">
                some block
            </%block>
        """
        )
        m = self._install_mock_cache(t)
        t.render(mykey="thekey")
        t.render(mykey="thekey")
        eq_(result_lines(t.render(mykey="thekey")), ["some block"])
        eq_(m.key, "thekey")

        t = Template(
            """
            <%def name="foo()" cached="True" cache_key="${mykey}">
                some def
            </%def>
            ${foo()}
        """
        )
        m = self._install_mock_cache(t)
        t.render(mykey="thekey")
        t.render(mykey="thekey")
        eq_(result_lines(t.render(mykey="thekey")), ["some def"])
        eq_(m.key, "thekey")

    def test_dynamic_key_with_funcargs(self):
        t = Template(
            """
            <%def name="foo(num=5)" cached="True" cache_key="foo_${str(num)}">
             hi
            </%def>

            ${foo()}
        """
        )
        m = self._install_mock_cache(t)
        t.render()
        t.render()
        assert result_lines(t.render()) == ["hi"]
        assert m.key == "foo_5"

        t = Template(
            """
            <%def name="foo(*args, **kwargs)" cached="True"
             cache_key="foo_${kwargs['bar']}">
             hi
            </%def>

            ${foo(1, 2, bar='lala')}
        """
        )
        m = self._install_mock_cache(t)
        t.render()
        assert result_lines(t.render()) == ["hi"]
        assert m.key == "foo_lala"

        t = Template(
            """
        <%page args="bar='hi'" cache_key="foo_${bar}" cached="True"/>
         hi
        """
        )
        m = self._install_mock_cache(t)
        t.render()
        assert result_lines(t.render()) == ["hi"]
        assert m.key == "foo_hi"

    def test_dynamic_key_with_imports(self):
        lookup = TemplateLookup()
        lookup.put_string(
            "foo.html",
            """
        <%!
            callcount = [0]
        %>
        <%namespace file="ns.html" import="*"/>
        <%page cached="True" cache_key="${foo}"/>
        this is foo
        <%
        callcount[0] += 1
        %>
        callcount: ${callcount}
""",
        )
        lookup.put_string("ns.html", """""")
        t = lookup.get_template("foo.html")
        m = self._install_mock_cache(t)
        t.render(foo="somekey")
        t.render(foo="somekey")
        assert result_lines(t.render(foo="somekey")) == [
            "this is foo",
            "callcount: [1]",
        ]
        assert m.kwargs == {}

    def test_fileargs_implicit(self):
        l = lookup.TemplateLookup(module_directory=module_base)
        l.put_string(
            "test",
            """
                <%!
                    callcount = [0]
                %>
                <%def name="foo()" cached="True" cache_type='dbm'>
                    this is foo
                    <%
                    callcount[0] += 1
                    %>
                </%def>

                ${foo()}
                ${foo()}
                ${foo()}
                callcount: ${callcount}
        """,
        )

        m = self._install_mock_cache(l.get_template("test"))
        assert result_lines(l.get_template("test").render()) == [
            "this is foo",
            "this is foo",
            "this is foo",
            "callcount: [1]",
        ]
        eq_(m.kwargs, {"type": "dbm"})

    def test_fileargs_deftag(self):
        t = Template(
            """
        <%%!
            callcount = [0]
        %%>
        <%%def name="foo()" cached="True" cache_type='file' cache_dir='%s'>
            this is foo
            <%%
            callcount[0] += 1
            %%>
        </%%def>

        ${foo()}
        ${foo()}
        ${foo()}
        callcount: ${callcount}
"""
            % module_base
        )
        m = self._install_mock_cache(t)
        assert result_lines(t.render()) == [
            "this is foo",
            "this is foo",
            "this is foo",
            "callcount: [1]",
        ]
        assert m.kwargs == {"type": "file", "dir": module_base}

    def test_fileargs_pagetag(self):
        t = Template(
            """
        <%%page cache_dir='%s' cache_type='dbm'/>
        <%%!
            callcount = [0]
        %%>
        <%%def name="foo()" cached="True">
            this is foo
            <%%
            callcount[0] += 1
            %%>
        </%%def>

        ${foo()}
        ${foo()}
        ${foo()}
        callcount: ${callcount}
"""
            % module_base
        )
        m = self._install_mock_cache(t)
        assert result_lines(t.render()) == [
            "this is foo",
            "this is foo",
            "this is foo",
            "callcount: [1]",
        ]
        eq_(m.kwargs, {"dir": module_base, "type": "dbm"})

    def test_args_complete(self):
        t = Template(
            """
        <%%def name="foo()" cached="True" cache_timeout="30" cache_dir="%s"
         cache_type="file" cache_key='somekey'>
            this is foo
        </%%def>

        ${foo()}
"""
            % module_base
        )
        m = self._install_mock_cache(t)
        t.render()
        eq_(m.kwargs, {"dir": module_base, "type": "file", "timeout": 30})

        t2 = Template(
            """
        <%%page cached="True" cache_timeout="30" cache_dir="%s"
         cache_type="file" cache_key='somekey'/>
        hi
        """
            % module_base
        )
        m = self._install_mock_cache(t2)
        t2.render()
        eq_(m.kwargs, {"dir": module_base, "type": "file", "timeout": 30})

    def test_fileargs_lookup(self):
        l = lookup.TemplateLookup(cache_dir=module_base, cache_type="file")
        l.put_string(
            "test",
            """
                <%!
                    callcount = [0]
                %>
                <%def name="foo()" cached="True">
                    this is foo
                    <%
                    callcount[0] += 1
                    %>
                </%def>

                ${foo()}
                ${foo()}
                ${foo()}
                callcount: ${callcount}
        """,
        )

        t = l.get_template("test")
        m = self._install_mock_cache(t)
        assert result_lines(l.get_template("test").render()) == [
            "this is foo",
            "this is foo",
            "this is foo",
            "callcount: [1]",
        ]
        eq_(m.kwargs, {"dir": module_base, "type": "file"})

    def test_buffered(self):
        t = Template(
            """
        <%!
            def a(text):
                return "this is a " + text.strip()
        %>
        ${foo()}
        ${foo()}
        <%def name="foo()" cached="True" buffered="True">
            this is a test
        </%def>
        """,
            buffer_filters=["a"],
        )
        self._install_mock_cache(t)
        eq_(
            result_lines(t.render()),
            ["this is a this is a test", "this is a this is a test"],
        )

    def test_load_from_expired(self):
        """test that the cache callable can be called safely after the
        originating template has completed rendering.

        """
        t = Template(
            """
        ${foo()}
        <%def name="foo()" cached="True" cache_timeout="1">
            foo
        </%def>
        """
        )
        self._install_mock_cache(t)

        x1 = t.render()
        time.sleep(1.2)
        x2 = t.render()
        assert x1.strip() == x2.strip() == "foo"

    def test_namespace_access(self):
        t = Template(
            """
            <%def name="foo(x)" cached="True">
                foo: ${x}
            </%def>

            <%
                foo(1)
                foo(2)
                local.cache.invalidate_def('foo')
                foo(3)
                foo(4)
            %>
        """
        )
        self._install_mock_cache(t)
        eq_(result_lines(t.render()), ["foo: 1", "foo: 1", "foo: 3", "foo: 3"])

    def test_lookup(self):
        l = TemplateLookup(cache_impl="mock")
        l.put_string(
            "x",
            """
            <%page cached="True" />
            ${y}
        """,
        )
        t = l.get_template("x")
        self._install_mock_cache(t)
        assert result_lines(t.render(y=5)) == ["5"]
        assert result_lines(t.render(y=7)) == ["5"]
        assert isinstance(t.cache.impl, MockCacheImpl)

    def test_invalidate(self):
        t = Template(
            """
            <%%def name="foo()" cached="True">
                foo: ${x}
            </%%def>

            <%%def name="bar()" cached="True" cache_type='dbm' cache_dir='%s'>
                bar: ${x}
            </%%def>
            ${foo()} ${bar()}
        """
            % module_base
        )
        self._install_mock_cache(t)
        assert result_lines(t.render(x=1)) == ["foo: 1", "bar: 1"]
        assert result_lines(t.render(x=2)) == ["foo: 1", "bar: 1"]
        t.cache.invalidate_def("foo")
        assert result_lines(t.render(x=3)) == ["foo: 3", "bar: 1"]
        t.cache.invalidate_def("bar")
        assert result_lines(t.render(x=4)) == ["foo: 3", "bar: 4"]

        t = Template(
            """
            <%%page cached="True" cache_type="dbm" cache_dir="%s"/>

            page: ${x}
        """
            % module_base
        )
        self._install_mock_cache(t)
        assert result_lines(t.render(x=1)) == ["page: 1"]
        assert result_lines(t.render(x=2)) == ["page: 1"]
        t.cache.invalidate_body()
        assert result_lines(t.render(x=3)) == ["page: 3"]
        assert result_lines(t.render(x=4)) == ["page: 3"]

    def test_custom_args_def(self):
        t = Template(
            """
            <%def name="foo()" cached="True" cache_region="myregion"
                    cache_timeout="50" cache_foo="foob">
            </%def>
            ${foo()}
        """
        )
        m = self._install_mock_cache(t, "simple")
        t.render()
        eq_(m.kwargs, {"region": "myregion", "timeout": 50, "foo": "foob"})

    def test_custom_args_block(self):
        t = Template(
            """
            <%block name="foo" cached="True" cache_region="myregion"
                    cache_timeout="50" cache_foo="foob">
            </%block>
        """
        )
        m = self._install_mock_cache(t, "simple")
        t.render()
        eq_(m.kwargs, {"region": "myregion", "timeout": 50, "foo": "foob"})

    def test_custom_args_page(self):
        t = Template(
            """
            <%page cached="True" cache_region="myregion"
                    cache_timeout="50" cache_foo="foob"/>
        """
        )
        m = self._install_mock_cache(t, "simple")
        t.render()
        eq_(m.kwargs, {"region": "myregion", "timeout": 50, "foo": "foob"})

    def test_pass_context(self):
        t = Template(
            """
            <%page cached="True"/>
        """
        )
        m = self._install_mock_cache(t)
        t.render()
        assert "context" not in m.kwargs

        m.pass_context = True
        t.render(x="bar")
        assert "context" in m.kwargs
        assert m.kwargs["context"].get("x") == "bar"


class RealBackendTest(object):
    def test_cache_uses_current_context(self):
        t = Template(
            """
        ${foo()}
        <%def name="foo()" cached="True" cache_timeout="1">
            foo: ${x}
        </%def>
        """
        )
        self._install_mock_cache(t)

        x1 = t.render(x=1)
        time.sleep(1.2)
        x2 = t.render(x=2)
        eq_(x1.strip(), "foo: 1")
        eq_(x2.strip(), "foo: 2")

    def test_region(self):
        t = Template(
            """
            <%block name="foo" cached="True" cache_region="short">
                short term ${x}
            </%block>
            <%block name="bar" cached="True" cache_region="long">
                long term ${x}
            </%block>
            <%block name="lala">
                none ${x}
            </%block>
        """
        )

        self._install_mock_cache(t)
        r1 = result_lines(t.render(x=5))
        time.sleep(1.2)
        r2 = result_lines(t.render(x=6))
        r3 = result_lines(t.render(x=7))
        eq_(r1, ["short term 5", "long term 5", "none 5"])
        eq_(r2, ["short term 6", "long term 5", "none 6"])
        eq_(r3, ["short term 6", "long term 5", "none 7"])


class BeakerCacheTest(RealBackendTest, CacheTest):
    real_backend = "beaker"

    def setUp(self):
        if not beaker_cache.has_beaker:
            raise SkipTest("Beaker is required for these tests.")
        if not py27:
            raise SkipTest("newer beakers not working w/ py26")

    def _install_mock_cache(self, template, implname=None):
        template.cache_args["manager"] = self._regions()
        impl = super(BeakerCacheTest, self)._install_mock_cache(
            template, implname
        )
        return impl

    def _regions(self):
        return beaker.cache.CacheManager(
            cache_regions={
                "short": {"expire": 1, "type": "memory"},
                "long": {"expire": 60, "type": "memory"},
            }
        )


class DogpileCacheTest(RealBackendTest, CacheTest):
    real_backend = "dogpile.cache"

    def setUp(self):
        try:
            import dogpile.cache  # noqa
        except ImportError:
            raise SkipTest("dogpile.cache is required to run these tests")

    def _install_mock_cache(self, template, implname=None):
        template.cache_args["regions"] = self._regions()
        template.cache_args.setdefault("region", "short")
        impl = super(DogpileCacheTest, self)._install_mock_cache(
            template, implname
        )
        return impl

    def _regions(self):
        from dogpile.cache import make_region

        my_regions = {
            "short": make_region().configure(
                "dogpile.cache.memory", expiration_time=1
            ),
            "long": make_region().configure(
                "dogpile.cache.memory", expiration_time=60
            ),
            "myregion": make_region().configure(
                "dogpile.cache.memory", expiration_time=60
            ),
        }

        return my_regions
