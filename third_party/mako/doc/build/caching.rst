.. _caching_toplevel:

=======
Caching
=======

Any template or component can be cached using the ``cache``
argument to the ``<%page>``, ``<%def>`` or ``<%block>`` directives:

.. sourcecode:: mako

    <%page cached="True"/>

    template text

The above template, after being executed the first time, will
store its content within a cache that by default is scoped
within memory. Subsequent calls to the template's :meth:`~.Template.render`
method will return content directly from the cache. When the
:class:`.Template` object itself falls out of scope, its corresponding
cache is garbage collected along with the template.

The caching system requires that a cache backend be installed; this
includes either the `Beaker <http://beaker.readthedocs.org/>`_ package
or the `dogpile.cache <http://dogpilecache.readthedocs.org>`_, as well as
any other third-party caching libraries that feature Mako integration.

By default, caching will attempt to make use of Beaker.
To use dogpile.cache, the
``cache_impl`` argument must be set; see this argument in the
section :ref:`cache_arguments`.

In addition to being available on the ``<%page>`` tag, the caching flag and all
its options can be used with the ``<%def>`` tag as well:

.. sourcecode:: mako

    <%def name="mycomp" cached="True" cache_timeout="60">
        other text
    </%def>

... and equivalently with the ``<%block>`` tag, anonymous or named:

.. sourcecode:: mako

    <%block cached="True" cache_timeout="60">
        other text
    </%block>


.. _cache_arguments:

Cache Arguments
===============

Mako has two cache arguments available on tags that are
available in all cases.   The rest of the arguments
available are specific to a backend.

The two generic tags arguments are:

* ``cached="True"`` - enable caching for this ``<%page>``,
  ``<%def>``, or ``<%block>``.
* ``cache_key`` - the "key" used to uniquely identify this content
  in the cache.   Usually, this key is chosen automatically
  based on the name of the rendering callable (i.e. ``body``
  when used in ``<%page>``, the name of the def when using ``<%def>``,
  the explicit or internally-generated name when using ``<%block>``).
  Using the ``cache_key`` parameter, the key can be overridden
  using a fixed or programmatically generated value.

  For example, here's a page
  that caches any page which inherits from it, based on the
  filename of the calling template:

  .. sourcecode:: mako

     <%page cached="True" cache_key="${self.filename}"/>

     ${next.body()}

     ## rest of template

On a :class:`.Template` or :class:`.TemplateLookup`, the
caching can be configured using these arguments:

* ``cache_enabled`` - Setting this
  to ``False`` will disable all caching functionality
  when the template renders.  Defaults to ``True``.
  e.g.:

  .. sourcecode:: python

      lookup = TemplateLookup(
                      directories='/path/to/templates',
                      cache_enabled = False
                      )

* ``cache_impl`` - The string name of the cache backend
  to use.   This defaults to ``'beaker'``, indicating
  that the 'beaker' backend will be used.

* ``cache_args`` - A dictionary of cache parameters that
  will be consumed by the cache backend.   See
  :ref:`beaker_backend` and :ref:`dogpile.cache_backend` for examples.


Backend-Specific Cache Arguments
--------------------------------

The ``<%page>``, ``<%def>``, and ``<%block>`` tags
accept any named argument that starts with the prefix ``"cache_"``.
Those arguments are then packaged up and passed along to the
underlying caching implementation, minus the ``"cache_"`` prefix.

The actual arguments understood are determined by the backend.

* :ref:`beaker_backend` - Includes arguments understood by
  Beaker.
* :ref:`dogpile.cache_backend` - Includes arguments understood by
  dogpile.cache.

.. _beaker_backend:

Using the Beaker Cache Backend
------------------------------

When using Beaker, new implementations will want to make usage
of **cache regions** so that cache configurations can be maintained
externally to templates.  These configurations live under
named "regions" that can be referred to within templates themselves.

.. versionadded:: 0.6.0
   Support for Beaker cache regions.

For example, suppose we would like two regions.  One is a "short term"
region that will store content in a memory-based dictionary,
expiring after 60 seconds.   The other is a Memcached region,
where values should expire in five minutes.   To configure
our :class:`.TemplateLookup`, first we get a handle to a
:class:`beaker.cache.CacheManager`:

.. sourcecode:: python

    from beaker.cache import CacheManager

    manager = CacheManager(cache_regions={
        'short_term':{
            'type': 'memory',
            'expire': 60
        },
        'long_term':{
            'type': 'ext:memcached',
            'url': '127.0.0.1:11211',
            'expire': 300
        }
    })

    lookup = TemplateLookup(
                    directories=['/path/to/templates'],
                    module_directory='/path/to/modules',
                    cache_impl='beaker',
                    cache_args={
                        'manager':manager
                    }
            )

Our templates can then opt to cache data in one of either region,
using the ``cache_region`` argument.   Such as using ``short_term``
at the ``<%page>`` level:

.. sourcecode:: mako

    <%page cached="True" cache_region="short_term">

    ## ...

Or, ``long_term`` at the ``<%block>`` level:

.. sourcecode:: mako

    <%block name="header" cached="True" cache_region="long_term">
        other text
    </%block>

The Beaker backend also works without regions.   There are a
variety of arguments that can be passed to the ``cache_args``
dictionary, which are also allowable in templates via the
``<%page>``, ``<%block>``,
and ``<%def>`` tags specific to those sections.   The values
given override those specified at the  :class:`.TemplateLookup`
or :class:`.Template` level.

With the possible exception
of ``cache_timeout``, these arguments are probably better off
staying at the template configuration level.  Each argument
specified as ``cache_XYZ`` in a template tag is specified
without the ``cache_`` prefix in the ``cache_args`` dictionary:

* ``cache_timeout`` - number of seconds in which to invalidate the
  cached data.  After this timeout, the content is re-generated
  on the next call.  Available as ``timeout`` in the ``cache_args``
  dictionary.
* ``cache_type`` - type of caching. ``'memory'``, ``'file'``, ``'dbm'``, or
  ``'ext:memcached'`` (note that  the string ``memcached`` is
  also accepted by the dogpile.cache Mako plugin, though not by Beaker itself).
  Available as ``type`` in the ``cache_args`` dictionary.
* ``cache_url`` - (only used for ``memcached`` but required) a single
  IP address or a semi-colon separated list of IP address of
  memcache servers to use.  Available as ``url`` in the ``cache_args``
  dictionary.
* ``cache_dir`` - in the case of the ``'file'`` and ``'dbm'`` cache types,
  this is the filesystem directory with which to store data
  files. If this option is not present, the value of
  ``module_directory`` is used (i.e. the directory where compiled
  template modules are stored). If neither option is available
  an exception is thrown.  Available as ``dir`` in the
  ``cache_args`` dictionary.

.. _dogpile.cache_backend:

Using the dogpile.cache Backend
-------------------------------

`dogpile.cache`_ is a new replacement for Beaker.   It provides
a modernized, slimmed down interface and is generally easier to use
than Beaker.   As of this writing it has not yet been released.  dogpile.cache
includes its own Mako cache plugin -- see :mod:`dogpile.cache.plugins.mako_cache` in the
dogpile.cache documentation.

Programmatic Cache Access
=========================

The :class:`.Template`, as well as any template-derived :class:`.Namespace`, has
an accessor called ``cache`` which returns the :class:`.Cache` object
for that template. This object is a facade on top of the underlying
:class:`.CacheImpl` object, and provides some very rudimental
capabilities, such as the ability to get and put arbitrary
values:

.. sourcecode:: mako

    <%
        local.cache.set("somekey", type="memory", "somevalue")
    %>

Above, the cache associated with the ``local`` namespace is
accessed and a key is placed within a memory cache.

More commonly, the ``cache`` object is used to invalidate cached
sections programmatically:

.. sourcecode:: python

    template = lookup.get_template('/sometemplate.html')

    # invalidate the "body" of the template
    template.cache.invalidate_body()

    # invalidate an individual def
    template.cache.invalidate_def('somedef')

    # invalidate an arbitrary key
    template.cache.invalidate('somekey')

You can access any special method or attribute of the :class:`.CacheImpl`
itself using the :attr:`impl <.Cache.impl>` attribute:

.. sourcecode:: python

    template.cache.impl.do_something_special()

Note that using implementation-specific methods will mean you can't
swap in a different kind of :class:`.CacheImpl` implementation at a
later time.

.. _cache_plugins:

Cache Plugins
=============

The mechanism used by caching can be plugged in
using a :class:`.CacheImpl` subclass.    This class implements
the rudimental methods Mako needs to implement the caching
API.   Mako includes the :class:`.BeakerCacheImpl` class to
provide the default implementation.  A :class:`.CacheImpl` class
is acquired by Mako using a ``pkg_resources`` entrypoint, using
the name given as the ``cache_impl`` argument to :class:`.Template`
or :class:`.TemplateLookup`.    This entry point can be
installed via the standard `setuptools`/``setup()`` procedure, underneath
the `EntryPoint` group named ``"mako.cache"``.  It can also be
installed at runtime via a convenience installer :func:`.register_plugin`
which accomplishes essentially the same task.

An example plugin that implements a local dictionary cache:

.. sourcecode:: python

    from mako.cache import Cacheimpl, register_plugin

    class SimpleCacheImpl(CacheImpl):
        def __init__(self, cache):
            super(SimpleCacheImpl, self).__init__(cache)
            self._cache = {}

        def get_or_create(self, key, creation_function, **kw):
            if key in self._cache:
                return self._cache[key]
            else:
                self._cache[key] = value = creation_function()
                return value

        def set(self, key, value, **kwargs):
            self._cache[key] = value

        def get(self, key, **kwargs):
            return self._cache.get(key)

        def invalidate(self, key, **kwargs):
            self._cache.pop(key, None)

    # optional - register the class locally
    register_plugin("simple", __name__, "SimpleCacheImpl")

Enabling the above plugin in a template would look like:

.. sourcecode:: python

    t = Template("mytemplate",
                 file="mytemplate.html",
                 cache_impl='simple')

Guidelines for Writing Cache Plugins
------------------------------------

* The :class:`.CacheImpl` is created on a per-:class:`.Template` basis.  The
  class should ensure that only data for the parent :class:`.Template` is
  persisted or returned by the cache methods.    The actual :class:`.Template`
  is available via the ``self.cache.template`` attribute.   The ``self.cache.id``
  attribute, which is essentially the unique modulename of the template, is
  a good value to use in order to represent a unique namespace of keys specific
  to the template.
* Templates only use the :meth:`.CacheImpl.get_or_create()` method
  in an implicit fashion.  The :meth:`.CacheImpl.set`,
  :meth:`.CacheImpl.get`, and :meth:`.CacheImpl.invalidate` methods are
  only used in response to direct programmatic access to the corresponding
  methods on the :class:`.Cache` object.
* :class:`.CacheImpl` will be accessed in a multithreaded fashion if the
  :class:`.Template` itself is used multithreaded.  Care should be taken
  to ensure caching implementations are threadsafe.
* A library like `Dogpile <http://pypi.python.org/pypi/dogpile.core>`_, which
  is a minimal locking system derived from Beaker, can be used to help
  implement the :meth:`.CacheImpl.get_or_create` method in a threadsafe
  way that can maximize effectiveness across multiple threads as well
  as processes. :meth:`.CacheImpl.get_or_create` is the
  key method used by templates.
* All arguments passed to ``**kw`` come directly from the parameters
  inside the ``<%def>``, ``<%block>``, or ``<%page>`` tags directly,
  minus the ``"cache_"`` prefix, as strings, with the exception of
  the argument ``cache_timeout``, which is passed to the plugin
  as the name ``timeout`` with the value converted to an integer.
  Arguments present in ``cache_args`` on :class:`.Template` or
  :class:`.TemplateLookup` are passed directly, but are superseded
  by those present in the most specific template tag.
* The directory where :class:`.Template` places module files can
  be acquired using the accessor ``self.cache.template.module_directory``.
  This directory can be a good place to throw cache-related work
  files, underneath a prefix like ``_my_cache_work`` so that name
  conflicts with generated modules don't occur.

API Reference
=============

.. autoclass:: mako.cache.Cache
    :members:
    :show-inheritance:

.. autoclass:: mako.cache.CacheImpl
    :members:
    :show-inheritance:

.. autofunction:: mako.cache.register_plugin

.. autoclass:: mako.ext.beaker_cache.BeakerCacheImpl
    :members:
    :show-inheritance:

