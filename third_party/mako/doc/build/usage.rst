.. _usage_toplevel:

=====
Usage
=====

Basic Usage
===========

This section describes the Python API for Mako templates. If you
are using Mako within a web framework such as Pylons, the work
of integrating Mako's API is already done for you, in which case
you can skip to the next section, :ref:`syntax_toplevel`.

The most basic way to create a template and render it is through
the :class:`.Template` class:

.. sourcecode:: python

    from mako.template import Template

    mytemplate = Template("hello world!")
    print(mytemplate.render())

Above, the text argument to :class:`.Template` is **compiled** into a
Python module representation. This module contains a function
called ``render_body()``, which produces the output of the
template. When ``mytemplate.render()`` is called, Mako sets up a
runtime environment for the template and calls the
``render_body()`` function, capturing the output into a buffer and
returning its string contents.


The code inside the ``render_body()`` function has access to a
namespace of variables. You can specify these variables by
sending them as additional keyword arguments to the :meth:`~.Template.render`
method:

.. sourcecode:: python

    from mako.template import Template

    mytemplate = Template("hello, ${name}!")
    print(mytemplate.render(name="jack"))

The :meth:`~.Template.render` method calls upon Mako to create a
:class:`.Context` object, which stores all the variable names accessible
to the template and also stores a buffer used to capture output.
You can create this :class:`.Context` yourself and have the template
render with it, using the :meth:`~.Template.render_context` method:

.. sourcecode:: python

    from mako.template import Template
    from mako.runtime import Context
    from StringIO import StringIO

    mytemplate = Template("hello, ${name}!")
    buf = StringIO()
    ctx = Context(buf, name="jack")
    mytemplate.render_context(ctx)
    print(buf.getvalue())

Using File-Based Templates
==========================

A :class:`.Template` can also load its template source code from a file,
using the ``filename`` keyword argument:

.. sourcecode:: python

    from mako.template import Template

    mytemplate = Template(filename='/docs/mytmpl.txt')
    print(mytemplate.render())

For improved performance, a :class:`.Template` which is loaded from a
file can also cache the source code to its generated module on
the filesystem as a regular Python module file (i.e. a ``.py``
file). To do this, just add the ``module_directory`` argument to
the template:

.. sourcecode:: python

    from mako.template import Template

    mytemplate = Template(filename='/docs/mytmpl.txt', module_directory='/tmp/mako_modules')
    print(mytemplate.render())

When the above code is rendered, a file
``/tmp/mako_modules/docs/mytmpl.txt.py`` is created containing the
source code for the module. The next time a :class:`.Template` with the
same arguments is created, this module file will be
automatically re-used.

.. _usage_templatelookup:

Using ``TemplateLookup``
========================

All of the examples thus far have dealt with the usage of a
single :class:`.Template` object. If the code within those templates
tries to locate another template resource, it will need some way
to find them, using simple URI strings. For this need, the
resolution of other templates from within a template is
accomplished by the :class:`.TemplateLookup` class. This class is
constructed given a list of directories in which to search for
templates, as well as keyword arguments that will be passed to
the :class:`.Template` objects it creates:

.. sourcecode:: python

    from mako.template import Template
    from mako.lookup import TemplateLookup

    mylookup = TemplateLookup(directories=['/docs'])
    mytemplate = Template("""<%include file="header.txt"/> hello world!""", lookup=mylookup)

Above, we created a textual template which includes the file
``"header.txt"``. In order for it to have somewhere to look for
``"header.txt"``, we passed a :class:`.TemplateLookup` object to it, which
will search in the directory ``/docs`` for the file ``"header.txt"``.

Usually, an application will store most or all of its templates
as text files on the filesystem. So far, all of our examples
have been a little bit contrived in order to illustrate the
basic concepts. But a real application would get most or all of
its templates directly from the :class:`.TemplateLookup`, using the
aptly named :meth:`~.TemplateLookup.get_template` method, which accepts the URI of the
desired template:

.. sourcecode:: python

    from mako.template import Template
    from mako.lookup import TemplateLookup

    mylookup = TemplateLookup(directories=['/docs'], module_directory='/tmp/mako_modules')

    def serve_template(templatename, **kwargs):
        mytemplate = mylookup.get_template(templatename)
        print(mytemplate.render(**kwargs))

In the example above, we create a :class:`.TemplateLookup` which will
look for templates in the ``/docs`` directory, and will store
generated module files in the ``/tmp/mako_modules`` directory. The
lookup locates templates by appending the given URI to each of
its search directories; so if you gave it a URI of
``/etc/beans/info.txt``, it would search for the file
``/docs/etc/beans/info.txt``, else raise a :class:`.TopLevelNotFound`
exception, which is a custom Mako exception.

When the lookup locates templates, it will also assign a ``uri``
property to the :class:`.Template` which is the URI passed to the
:meth:`~.TemplateLookup.get_template()` call. :class:`.Template` uses this URI to calculate the
name of its module file. So in the above example, a
``templatename`` argument of ``/etc/beans/info.txt`` will create a
module file ``/tmp/mako_modules/etc/beans/info.txt.py``.

Setting the Collection Size
---------------------------

The :class:`.TemplateLookup` also serves the important need of caching a
fixed set of templates in memory at a given time, so that
successive URI lookups do not result in full template
compilations and/or module reloads on each request. By default,
the :class:`.TemplateLookup` size is unbounded. You can specify a fixed
size using the ``collection_size`` argument:

.. sourcecode:: python

    mylookup = TemplateLookup(directories=['/docs'],
                    module_directory='/tmp/mako_modules', collection_size=500)

The above lookup will continue to load templates into memory
until it reaches a count of around 500. At that point, it will
clean out a certain percentage of templates using a least
recently used scheme.

Setting Filesystem Checks
-------------------------

Another important flag on :class:`.TemplateLookup` is
``filesystem_checks``. This defaults to ``True``, and says that each
time a template is returned by the :meth:`~.TemplateLookup.get_template()` method, the
revision time of the original template file is checked against
the last time the template was loaded, and if the file is newer
will reload its contents and recompile the template. On a
production system, setting ``filesystem_checks`` to ``False`` can
afford a small to moderate performance increase (depending on
the type of filesystem used).

.. _usage_unicode:

Using Unicode and Encoding
==========================

Both :class:`.Template` and :class:`.TemplateLookup` accept ``output_encoding``
and ``encoding_errors`` parameters which can be used to encode the
output in any Python supported codec:

.. sourcecode:: python

    from mako.template import Template
    from mako.lookup import TemplateLookup

    mylookup = TemplateLookup(directories=['/docs'], output_encoding='utf-8', encoding_errors='replace')

    mytemplate = mylookup.get_template("foo.txt")
    print(mytemplate.render())

When using Python 3, the :meth:`~.Template.render` method will return a ``bytes``
object, **if** ``output_encoding`` is set. Otherwise it returns a
``string``.

Additionally, the :meth:`~.Template.render_unicode()` method exists which will
return the template output as a Python ``unicode`` object, or in
Python 3 a ``string``:

.. sourcecode:: python

    print(mytemplate.render_unicode())

The above method disregards the output encoding keyword
argument; you can encode yourself by saying:

.. sourcecode:: python

    print(mytemplate.render_unicode().encode('utf-8', 'replace'))

Note that Mako's ability to return data in any encoding and/or
``unicode`` implies that the underlying output stream of the
template is a Python unicode object. This behavior is described
fully in :ref:`unicode_toplevel`.

.. _handling_exceptions:

Handling Exceptions
===================

Template exceptions can occur in two distinct places. One is
when you **lookup, parse and compile** the template, the other
is when you **run** the template. Within the running of a
template, exceptions are thrown normally from whatever Python
code originated the issue. Mako has its own set of exception
classes which mostly apply to the lookup and lexer/compiler
stages of template construction. Mako provides some library
routines that can be used to help provide Mako-specific
information about any exception's stack trace, as well as
formatting the exception within textual or HTML format. In all
cases, the main value of these handlers is that of converting
Python filenames, line numbers, and code samples into Mako
template filenames, line numbers, and code samples. All lines
within a stack trace which correspond to a Mako template module
will be converted to be against the originating template file.

To format exception traces, the :func:`.text_error_template` and
:func:`.html_error_template` functions are provided. They make usage of
``sys.exc_info()`` to get at the most recently thrown exception.
Usage of these handlers usually looks like:

.. sourcecode:: python

    from mako import exceptions

    try:
        template = lookup.get_template(uri)
        print(template.render())
    except:
        print(exceptions.text_error_template().render())

Or for the HTML render function:

.. sourcecode:: python

    from mako import exceptions

    try:
        template = lookup.get_template(uri)
        print(template.render())
    except:
        print(exceptions.html_error_template().render())

The :func:`.html_error_template` template accepts two options:
specifying ``full=False`` causes only a section of an HTML
document to be rendered. Specifying ``css=False`` will disable the
default stylesheet from being rendered.

E.g.:

.. sourcecode:: python

    print(exceptions.html_error_template().render(full=False))

The HTML render function is also available built-in to
:class:`.Template` using the ``format_exceptions`` flag. In this case, any
exceptions raised within the **render** stage of the template
will result in the output being substituted with the output of
:func:`.html_error_template`:

.. sourcecode:: python

    template = Template(filename="/foo/bar", format_exceptions=True)
    print(template.render())

Note that the compile stage of the above template occurs when
you construct the :class:`.Template` itself, and no output stream is
defined. Therefore exceptions which occur within the
lookup/parse/compile stage will not be handled and will
propagate normally. While the pre-render traceback usually will
not include any Mako-specific lines anyway, it will mean that
exceptions which occur previous to rendering and those which
occur within rendering will be handled differently... so the
``try``/``except`` patterns described previously are probably of more
general use.

The underlying object used by the error template functions is
the :class:`.RichTraceback` object. This object can also be used
directly to provide custom error views. Here's an example usage
which describes its general API:

.. sourcecode:: python

    from mako.exceptions import RichTraceback

    try:
        template = lookup.get_template(uri)
        print(template.render())
    except:
        traceback = RichTraceback()
        for (filename, lineno, function, line) in traceback.traceback:
            print("File %s, line %s, in %s" % (filename, lineno, function))
            print(line, "\n")
        print("%s: %s" % (str(traceback.error.__class__.__name__), traceback.error))

Common Framework Integrations
=============================

The Mako distribution includes a little bit of helper code for
the purpose of using Mako in some popular web framework
scenarios. This is a brief description of what's included.

WSGI
----

A sample WSGI application is included in the distribution in the
file ``examples/wsgi/run_wsgi.py``. This runner is set up to pull
files from a `templates` as well as an `htdocs` directory and
includes a rudimental two-file layout. The WSGI runner acts as a
fully functional standalone web server, using ``wsgiutils`` to run
itself, and propagates GET and POST arguments from the request
into the :class:`.Context`, can serve images, CSS files and other kinds
of files, and also displays errors using Mako's included
exception-handling utilities.

Pygments
--------

A `Pygments <https://pygments.org/>`_-compatible syntax
highlighting module is included under :mod:`mako.ext.pygmentplugin`.
This module is used in the generation of Mako documentation and
also contains various `setuptools` entry points under the heading
``pygments.lexers``, including ``mako``, ``html+mako``, ``xml+mako``
(see the ``setup.py`` file for all the entry points).

Babel
-----

Mako provides support for extracting `gettext` messages from
templates via a `Babel`_ extractor
entry point under ``mako.ext.babelplugin``.

`Gettext` messages are extracted from all Python code sections,
including those of control lines and expressions embedded
in tags.

`Translator
comments <http://babel.edgewall.org/wiki/Documentation/messages.html#comments-tags-and-translator-comments-explanation>`_
may also be extracted from Mako templates when a comment tag is
specified to `Babel`_ (such as with
the ``-c`` option).

For example, a project ``"myproj"`` contains the following Mako
template at ``myproj/myproj/templates/name.html``:

.. sourcecode:: mako

    <div id="name">
      Name:
      ## TRANSLATORS: This is a proper name. See the gettext
      ## manual, section Names.
      ${_('Francois Pinard')}
    </div>

To extract gettext messages from this template the project needs
a Mako section in its `Babel Extraction Method Mapping
file <http://babel.edgewall.org/wiki/Documentation/messages.html#extraction-method-mapping-and-configuration>`_
(typically located at ``myproj/babel.cfg``):

.. sourcecode:: cfg

    # Extraction from Python source files

    [python: myproj/**.py]

    # Extraction from Mako templates

    [mako: myproj/templates/**.html]
    input_encoding = utf-8

The Mako extractor supports an optional ``input_encoding``
parameter specifying the encoding of the templates (identical to
:class:`.Template`/:class:`.TemplateLookup`'s ``input_encoding`` parameter).

Invoking `Babel`_'s extractor at the
command line in the project's root directory:

.. sourcecode:: sh

    myproj$ pybabel extract -F babel.cfg -c "TRANSLATORS:" .

will output a `gettext` catalog to `stdout` including the following:

.. sourcecode:: pot

    #. TRANSLATORS: This is a proper name. See the gettext
    #. manual, section Names.
    #: myproj/templates/name.html:5
    msgid "Francois Pinard"
    msgstr ""

This is only a basic example:
`Babel`_ can be invoked from ``setup.py``
and its command line options specified in the accompanying
``setup.cfg`` via `Babel Distutils/Setuptools
Integration <http://babel.edgewall.org/wiki/Documentation/setup.html>`_.

Comments must immediately precede a `gettext` message to be
extracted. In the following case the ``TRANSLATORS:`` comment would
not have been extracted:

.. sourcecode:: mako

    <div id="name">
      ## TRANSLATORS: This is a proper name. See the gettext
      ## manual, section Names.
      Name: ${_('Francois Pinard')}
    </div>

See the `Babel User
Guide <http://babel.edgewall.org/wiki/Documentation/index.html>`_
for more information.

.. _babel: http://babel.edgewall.org/


API Reference
=============

.. autoclass:: mako.template.Template
    :show-inheritance:
    :members:

.. autoclass:: mako.template.DefTemplate
    :show-inheritance:
    :members:

.. autoclass:: mako.lookup.TemplateCollection
    :show-inheritance:
    :members:

.. autoclass:: mako.lookup.TemplateLookup
    :show-inheritance:
    :members:

.. autoclass:: mako.exceptions.RichTraceback
    :show-inheritance:

    .. py:attribute:: error

       the exception instance.

    .. py:attribute:: message

       the exception error message as unicode.

    .. py:attribute:: source

       source code of the file where the error occurred.
       If the error occurred within a compiled template,
       this is the template source.

    .. py:attribute:: lineno

       line number where the error occurred.  If the error
       occurred within a compiled template, the line number
       is adjusted to that of the template source.

    .. py:attribute:: records

       a list of 8-tuples containing the original
       python traceback elements, plus the
       filename, line number, source line, and full template source
       for the traceline mapped back to its originating source
       template, if any for that traceline (else the fields are ``None``).

    .. py:attribute:: reverse_records

       the list of records in reverse
       traceback -- a list of 4-tuples, in the same format as a regular
       python traceback, with template-corresponding
       traceback records replacing the originals.

    .. py:attribute:: reverse_traceback

       the traceback list in reverse.

.. autofunction:: mako.exceptions.html_error_template

.. autofunction:: mako.exceptions.text_error_template

