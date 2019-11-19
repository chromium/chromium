.. _namespaces_toplevel:

==========
Namespaces
==========

Namespaces are used to organize groups of defs into
categories, and also to "import" defs from other files.

If the file ``components.html`` defines these two defs:

.. sourcecode:: mako

    ## components.html
    <%def name="comp1()">
        this is comp1
    </%def>

    <%def name="comp2(x)">
        this is comp2, x is ${x}
    </%def>

you can make another file, for example ``index.html``, that
pulls those two defs into a namespace called ``comp``:

.. sourcecode:: mako

    ## index.html
    <%namespace name="comp" file="components.html"/>

    Here's comp1:  ${comp.comp1()}
    Here's comp2:  ${comp.comp2(x=5)}

The ``comp`` variable above is an instance of
:class:`.Namespace`, a **proxy object** which delivers
method calls to the underlying template callable using the
current context.

``<%namespace>`` also provides an ``import`` attribute which can
be used to pull the names into the local namespace, removing the
need to call it via the "``.``" operator. When ``import`` is used, the
``name`` attribute is optional.

.. sourcecode:: mako

    <%namespace file="components.html" import="comp1, comp2"/>

    Heres comp1:  ${comp1()}
    Heres comp2:  ${comp2(x=5)}

``import`` also supports the "``*``" operator:

.. sourcecode:: mako

    <%namespace file="components.html" import="*"/>

    Heres comp1:  ${comp1()}
    Heres comp2:  ${comp2(x=5)}

The names imported by the ``import`` attribute take precedence
over any names that exist within the current context.

.. note:: In current versions of Mako, usage of ``import='*'`` is
   known to decrease performance of the template. This will be
   fixed in a future release.

The ``file`` argument allows expressions -- if looking for
context variables, the ``context`` must be named explicitly:

.. sourcecode:: mako

    <%namespace name="dyn" file="${context['namespace_name']}"/>

Ways to Call Namespaces
=======================

There are essentially four ways to call a function from a
namespace.

The "expression" format, as described previously. Namespaces are
just Python objects with functions on them, and can be used in
expressions like any other function:

.. sourcecode:: mako

    ${mynamespace.somefunction('some arg1', 'some arg2', arg3='some arg3', arg4='some arg4')}

Synonymous with the "expression" format is the "custom tag"
format, when a "closed" tag is used. This format, introduced in
Mako 0.2.3, allows the usage of a "custom" Mako tag, with the
function arguments passed in using named attributes:

.. sourcecode:: mako

    <%mynamespace:somefunction arg1="some arg1" arg2="some arg2" arg3="some arg3" arg4="some arg4"/>

When using tags, the values of the arguments are taken as
literal strings by default. To embed Python expressions as
arguments, use the embedded expression format:

.. sourcecode:: mako

    <%mynamespace:somefunction arg1="${someobject.format()}" arg2="${somedef(5, 12)}"/>

The "custom tag" format is intended mainly for namespace
functions which recognize body content, which in Mako is known
as a "def with embedded content":

.. sourcecode:: mako

    <%mynamespace:somefunction arg1="some argument" args="x, y">
        Some record: ${x}, ${y}
    </%mynamespace:somefunction>

The "classic" way to call defs with embedded content is the ``<%call>`` tag:

.. sourcecode:: mako

    <%call expr="mynamespace.somefunction(arg1='some argument')" args="x, y">
        Some record: ${x}, ${y}
    </%call>

For information on how to construct defs that embed content from
the caller, see :ref:`defs_with_content`.

.. _namespaces_python_modules:

Namespaces from Regular Python Modules
======================================

Namespaces can also import regular Python functions from
modules. These callables need to take at least one argument,
``context``, an instance of :class:`.Context`. A module file
``some/module.py`` might contain the callable:

.. sourcecode:: python

    def my_tag(context):
        context.write("hello world")
        return ''

A template can use this module via:

.. sourcecode:: mako

    <%namespace name="hw" module="some.module"/>

    ${hw.my_tag()}

Note that the ``context`` argument is not needed in the call;
the :class:`.Namespace` tag creates a locally-scoped callable which
takes care of it. The ``return ''`` is so that the def does not
dump a ``None`` into the output stream -- the return value of any
def is rendered after the def completes, in addition to whatever
was passed to :meth:`.Context.write` within its body.

If your def is to be called in an "embedded content" context,
that is as described in :ref:`defs_with_content`, you should use
the :func:`.supports_caller` decorator, which will ensure that Mako
will ensure the correct "caller" variable is available when your
def is called, supporting embedded content:

.. sourcecode:: python

    from mako.runtime import supports_caller

    @supports_caller
    def my_tag(context):
        context.write("<div>")
        context['caller'].body()
        context.write("</div>")
        return ''

Capturing of output is available as well, using the
outside-of-templates version of the :func:`.capture` function,
which accepts the "context" as its first argument:

.. sourcecode:: python

    from mako.runtime import supports_caller, capture

    @supports_caller
    def my_tag(context):
        return "<div>%s</div>" % \
                capture(context, context['caller'].body, x="foo", y="bar")

Declaring Defs in Namespaces
============================

The ``<%namespace>`` tag supports the definition of ``<%def>``\ s
directly inside the tag. These defs become part of the namespace
like any other function, and will override the definitions
pulled in from a remote template or module:

.. sourcecode:: mako

    ## define a namespace
    <%namespace name="stuff">
        <%def name="comp1()">
            comp1
        </%def>
    </%namespace>

    ## then call it
    ${stuff.comp1()}

.. _namespaces_body:

The ``body()`` Method
=====================

Every namespace that is generated from a template contains a
method called ``body()``. This method corresponds to the main
body of the template, and plays its most important roles when
using inheritance relationships as well as
def-calls-with-content.

Since the ``body()`` method is available from a namespace just
like all the other defs defined in a template, what happens if
you send arguments to it? By default, the ``body()`` method
accepts no positional arguments, and for usefulness in
inheritance scenarios will by default dump all keyword arguments
into a dictionary called ``pageargs``. But if you actually want
to get at the keyword arguments, Mako recommends you define your
own argument signature explicitly. You do this via using the
``<%page>`` tag:

.. sourcecode:: mako

    <%page args="x, y, someval=8, scope='foo', **kwargs"/>

A template which defines the above signature requires that the
variables ``x`` and ``y`` are defined, defines default values
for ``someval`` and ``scope``, and sets up ``**kwargs`` to
receive all other keyword arguments. If ``**kwargs`` or similar
is not present, the argument ``**pageargs`` gets tacked on by
Mako. When the template is called as a top-level template (i.e.
via :meth:`~.Template.render`) or via the ``<%include>`` tag, the
values for these arguments will be pulled from the ``Context``.
In all other cases, i.e. via calling the ``body()`` method, the
arguments are taken as ordinary arguments from the method call.
So above, the body might be called as:

.. sourcecode:: mako

    ${self.body(5, y=10, someval=15, delta=7)}

The :class:`.Context` object also supplies a :attr:`~.Context.kwargs`
accessor, for cases when you'd like to pass along the top level context
arguments to a ``body()`` callable:

.. sourcecode:: mako

    ${next.body(**context.kwargs)}

The usefulness of calls like the above become more apparent when
one works with inheriting templates. For more information on
this, as well as the meanings of the names ``self`` and
``next``, see :ref:`inheritance_toplevel`.

.. _namespaces_builtin:

Built-in Namespaces
===================

The namespace is so great that Mako gives your template one (or
two) for free. The names of these namespaces are ``local`` and
``self``. Other built-in namespaces include ``parent`` and
``next``, which are optional and are described in
:ref:`inheritance_toplevel`.

.. _namespace_local:

``local``
---------

The ``local`` namespace is basically the namespace for the
currently executing template. This means that all of the top
level defs defined in your template, as well as your template's
``body()`` function, are also available off of the ``local``
namespace.

The ``local`` namespace is also where properties like ``uri``,
``filename``, and ``module`` and the ``get_namespace`` method
can be particularly useful.

.. _namespace_self:

``self``
--------

The ``self`` namespace, in the case of a template that does not
use inheritance, is synonymous with ``local``. If inheritance is
used, then ``self`` references the topmost template in the
inheritance chain, where it is most useful for providing the
ultimate form of various "method" calls which may have been
overridden at various points in an inheritance chain. See
:ref:`inheritance_toplevel`.

Inheritable Namespaces
======================

The ``<%namespace>`` tag includes an optional attribute
``inheritable="True"``, which will cause the namespace to be
attached to the ``self`` namespace. Since ``self`` is globally
available throughout an inheritance chain (described in the next
section), all the templates in an inheritance chain can get at
the namespace imported in a super-template via ``self``.

.. sourcecode:: mako

    ## base.html
    <%namespace name="foo" file="foo.html" inheritable="True"/>

    ${next.body()}

    ## somefile.html
    <%inherit file="base.html"/>

    ${self.foo.bar()}

This allows a super-template to load a whole bunch of namespaces
that its inheriting templates can get to, without them having to
explicitly load those namespaces themselves.

The ``import="*"`` part of the ``<%namespace>`` tag doesn't yet
interact with the ``inheritable`` flag, so currently you have to
use the explicit namespace name off of ``self``, followed by the
desired function name. But more on this in a future release.

Namespace API Usage Example - Static Dependencies
==================================================

The ``<%namespace>`` tag at runtime produces an instance of
:class:`.Namespace`.   Programmatic access of :class:`.Namespace` can be used
to build various kinds of scaffolding in templates and between templates.

A common request is the ability for a particular template to declare
"static includes" - meaning, the usage of a particular set of defs requires
that certain Javascript/CSS files are present.   Using :class:`.Namespace` as the
object that holds together the various templates present, we can build a variety
of such schemes.   In particular, the :class:`.Context` has a ``namespaces``
attribute, which is a dictionary of all :class:`.Namespace` objects declared.
Iterating the values of this dictionary will provide a :class:`.Namespace`
object for each time the ``<%namespace>`` tag was used, anywhere within the
inheritance chain.


.. _namespace_attr_for_includes:

Version One - Use :attr:`.Namespace.attr`
-----------------------------------------

The :attr:`.Namespace.attr` attribute allows us to locate any variables declared
in the ``<%! %>`` of a template.

.. sourcecode:: mako

    ## base.mako
    ## base-most template, renders layout etc.
    <html>
    <head>
    ## traverse through all namespaces present,
    ## look for an attribute named 'includes'
    % for ns in context.namespaces.values():
        % for incl in getattr(ns.attr, 'includes', []):
            ${incl}
        % endfor
    % endfor
    </head>
    <body>
    ${next.body()}
    </body
    </html>

    ## library.mako
    ## library functions.
    <%!
        includes = [
            '<link rel="stylesheet" type="text/css" href="mystyle.css"/>',
            '<script type="text/javascript" src="functions.js"></script>'
        ]
    %>

    <%def name="mytag()">
        <form>
            ${caller.body()}
        </form>
    </%def>

    ## index.mako
    ## calling template.
    <%inherit file="base.mako"/>
    <%namespace name="foo" file="library.mako"/>

    <%foo:mytag>
        a form
    </%foo:mytag>


Above, the file ``library.mako`` declares an attribute ``includes`` inside its global ``<%! %>`` section.
``index.mako`` includes this template using the ``<%namespace>`` tag.  The base template ``base.mako``, which is the inherited parent of ``index.mako`` and is responsible for layout, then locates this attribute and iterates through its contents to produce the includes that are specific to ``library.mako``.

Version Two - Use a specific named def
-----------------------------------------

In this version, we put the includes into a ``<%def>`` that
follows a naming convention.

.. sourcecode:: mako

    ## base.mako
    ## base-most template, renders layout etc.
    <html>
    <head>
    ## traverse through all namespaces present,
    ## look for a %def named 'includes'
    % for ns in context.namespaces.values():
        % if hasattr(ns, 'includes'):
            ${ns.includes()}
        % endif
    % endfor
    </head>
    <body>
    ${next.body()}
    </body
    </html>

    ## library.mako
    ## library functions.

    <%def name="includes()">
        <link rel="stylesheet" type="text/css" href="mystyle.css"/>
        <script type="text/javascript" src="functions.js"></script>
    </%def>

    <%def name="mytag()">
        <form>
            ${caller.body()}
        </form>
    </%def>


    ## index.mako
    ## calling template.
    <%inherit file="base.mako"/>
    <%namespace name="foo" file="library.mako"/>

    <%foo:mytag>
        a form
    </%foo:mytag>

In this version, ``library.mako`` declares a ``<%def>`` named ``includes``.   The example works
identically to the previous one, except that ``base.mako`` looks for defs named ``include``
on each namespace it examines.

API Reference
=============

.. autoclass:: mako.runtime.Namespace
    :show-inheritance:
    :members:

.. autoclass:: mako.runtime.TemplateNamespace
    :show-inheritance:
    :members:

.. autoclass:: mako.runtime.ModuleNamespace
    :show-inheritance:
    :members:

.. autofunction:: mako.runtime.supports_caller

.. autofunction:: mako.runtime.capture

