.. _syntax_toplevel:

======
Syntax
======

A Mako template is parsed from a text stream containing any kind
of content, XML, HTML, email text, etc. The template can further
contain Mako-specific directives which represent variable and/or
expression substitutions, control structures (i.e. conditionals
and loops), server-side comments, full blocks of Python code, as
well as various tags that offer additional functionality. All of
these constructs compile into real Python code. This means that
you can leverage the full power of Python in almost every aspect
of a Mako template.

Expression Substitution
=======================

The simplest expression is just a variable substitution. The
syntax for this is the ``${}`` construct, which is inspired by
Perl, Genshi, JSP EL, and others:

.. sourcecode:: mako

    this is x: ${x}

Above, the string representation of ``x`` is applied to the
template's output stream. If you're wondering where ``x`` comes
from, it's usually from the :class:`.Context` supplied to the
template's rendering function. If ``x`` was not supplied to the
template and was not otherwise assigned locally, it evaluates to
a special value ``UNDEFINED``. More on that later.

The contents within the ``${}`` tag are evaluated by Python
directly, so full expressions are OK:

.. sourcecode:: mako

    pythagorean theorem:  ${pow(x,2) + pow(y,2)}

The results of the expression are evaluated into a string result
in all cases before being rendered to the output stream, such as
the above example where the expression produces a numeric
result.

Expression Escaping
===================

Mako includes a number of built-in escaping mechanisms,
including HTML, URI and XML escaping, as well as a "trim"
function. These escapes can be added to an expression
substitution using the ``|`` operator:

.. sourcecode:: mako

    ${"this is some text" | u}

The above expression applies URL escaping to the expression, and
produces ``this+is+some+text``. The ``u`` name indicates URL
escaping, whereas ``h`` represents HTML escaping, ``x``
represents XML escaping, and ``trim`` applies a trim function.

Read more about built-in filtering functions, including how to
make your own filter functions, in :ref:`filtering_toplevel`.

Control Structures
==================

A control structure refers to all those things that control the
flow of a program -- conditionals (i.e. ``if``/``else``), loops (like
``while`` and ``for``), as well as things like ``try``/``except``. In Mako,
control structures are written using the ``%`` marker followed
by a regular Python control expression, and are "closed" by
using another ``%`` marker with the tag "``end<name>``", where
"``<name>``" is the keyword of the expression:

.. sourcecode:: mako

    % if x==5:
        this is some output
    % endif

The ``%`` can appear anywhere on the line as long as no text
precedes it; indentation is not significant. The full range of
Python "colon" expressions are allowed here, including
``if``/``elif``/``else``, ``while``, ``for``, ``with``, and even ``def``,
although Mako has a built-in tag for defs which is more full-featured.

.. sourcecode:: mako

    % for a in ['one', 'two', 'three', 'four', 'five']:
        % if a[0] == 't':
        its two or three
        % elif a[0] == 'f':
        four/five
        % else:
        one
        % endif
    % endfor

The ``%`` sign can also be "escaped", if you actually want to
emit a percent sign as the first non whitespace character on a
line, by escaping it as in ``%%``:

.. sourcecode:: mako

    %% some text

        %% some more text

The Loop Context
----------------

The **loop context** provides additional information about a loop
while inside of a ``% for`` structure:

.. sourcecode:: mako

    <ul>
    % for a in ("one", "two", "three"):
        <li>Item ${loop.index}: ${a}</li>
    % endfor
    </ul>

See :ref:`loop_context` for more information on this feature.

.. versionadded:: 0.7

Comments
========

Comments come in two varieties. The single line comment uses
``##`` as the first non-space characters on a line:

.. sourcecode:: mako

    ## this is a comment.
    ...text ...

A multiline version exists using ``<%doc> ...text... </%doc>``:

.. sourcecode:: mako

    <%doc>
        these are comments
        more comments
    </%doc>

Newline Filters
===============

The backslash ("``\``") character, placed at the end of any
line, will consume the newline character before continuing to
the next line:

.. sourcecode:: mako

    here is a line that goes onto \
    another line.

The above text evaluates to:

.. sourcecode:: text

    here is a line that goes onto another line.

Python Blocks
=============

Any arbitrary block of python can be dropped in using the ``<%
%>`` tags:

.. sourcecode:: mako

    this is a template
    <%
        x = db.get_resource('foo')
        y = [z.element for z in x if x.frobnizzle==5]
    %>
    % for elem in y:
        element: ${elem}
    % endfor

Within ``<% %>``, you're writing a regular block of Python code.
While the code can appear with an arbitrary level of preceding
whitespace, it has to be consistently formatted with itself.
Mako's compiler will adjust the block of Python to be consistent
with the surrounding generated Python code.

Module-level Blocks
===================

A variant on ``<% %>`` is the module-level code block, denoted
by ``<%! %>``. Code within these tags is executed at the module
level of the template, and not within the rendering function of
the template. Therefore, this code does not have access to the
template's context and is only executed when the template is
loaded into memory (which can be only once per application, or
more, depending on the runtime environment). Use the ``<%! %>``
tags to declare your template's imports, as well as any
pure-Python functions you might want to declare:

.. sourcecode:: mako

    <%!
        import mylib
        import re

        def filter(text):
            return re.sub(r'^@', '', text)
    %>

Any number of ``<%! %>`` blocks can be declared anywhere in a
template; they will be rendered in the resulting module
in a single contiguous block above all render callables,
in the order in which they appear in the source template.

Tags
====

The rest of what Mako offers takes place in the form of tags.
All tags use the same syntax, which is similar to an XML tag
except that the first character of the tag name is a ``%``
character. The tag is closed either by a contained slash
character, or an explicit closing tag:

.. sourcecode:: mako

    <%include file="foo.txt"/>

    <%def name="foo" buffered="True">
        this is a def
    </%def>

All tags have a set of attributes which are defined for each
tag. Some of these attributes are required. Also, many
attributes support **evaluation**, meaning you can embed an
expression (using ``${}``) inside the attribute text:

.. sourcecode:: mako

    <%include file="/foo/bar/${myfile}.txt"/>

Whether or not an attribute accepts runtime evaluation depends
on the type of tag and how that tag is compiled into the
template. The best way to find out if you can stick an
expression in is to try it! The lexer will tell you if it's not
valid.

Heres a quick summary of all the tags:

``<%page>``
-----------

This tag defines general characteristics of the template,
including caching arguments, and optional lists of arguments
which the template expects when invoked.

.. sourcecode:: mako

    <%page args="x, y, z='default'"/>

Or a page tag that defines caching characteristics:

.. sourcecode:: mako

    <%page cached="True" cache_type="memory"/>

Currently, only one ``<%page>`` tag gets used per template, the
rest get ignored. While this will be improved in a future
release, for now make sure you have only one ``<%page>`` tag
defined in your template, else you may not get the results you
want.  Further details on what ``<%page>`` is used for are described
in the following sections:

* :ref:`namespaces_body` - ``<%page>`` is used to define template-level
  arguments and defaults

* :ref:`expression_filtering` - expression filters can be applied to all
  expressions throughout a template using the ``<%page>`` tag

* :ref:`caching_toplevel` - options to control template-level caching
  may be applied in the ``<%page>`` tag.

``<%include>``
--------------

A tag that is familiar from other template languages, ``%include``
is a regular joe that just accepts a file argument and calls in
the rendered result of that file:

.. sourcecode:: mako

    <%include file="header.html"/>

        hello world

    <%include file="footer.html"/>

Include also accepts arguments which are available as ``<%page>`` arguments in the receiving template:

.. sourcecode:: mako

    <%include file="toolbar.html" args="current_section='members', username='ed'"/>

``<%def>``
----------

The ``%def`` tag defines a Python function which contains a set
of content, that can be called at some other point in the
template. The basic idea is simple:

.. sourcecode:: mako

    <%def name="myfunc(x)">
        this is myfunc, x is ${x}
    </%def>

    ${myfunc(7)}

The ``%def`` tag is a lot more powerful than a plain Python ``def``, as
the Mako compiler provides many extra services with ``%def`` that
you wouldn't normally have, such as the ability to export defs
as template "methods", automatic propagation of the current
:class:`.Context`, buffering/filtering/caching flags, and def calls
with content, which enable packages of defs to be sent as
arguments to other def calls (not as hard as it sounds). Get the
full deal on what ``%def`` can do in :ref:`defs_toplevel`.

``<%block>``
------------

``%block`` is a tag that is close to a ``%def``,
except executes itself immediately in its base-most scope,
and can also be anonymous (i.e. with no name):

.. sourcecode:: mako

    <%block filter="h">
        some <html> stuff.
    </%block>

Inspired by Jinja2 blocks, named blocks offer a syntactically pleasing way
to do inheritance:

.. sourcecode:: mako

    <html>
        <body>
        <%block name="header">
            <h2><%block name="title"/></h2>
        </%block>
        ${self.body()}
        </body>
    </html>

Blocks are introduced in :ref:`blocks` and further described in :ref:`inheritance_toplevel`.

.. versionadded:: 0.4.1

``<%namespace>``
----------------

``%namespace`` is Mako's equivalent of Python's ``import``
statement. It allows access to all the rendering functions and
metadata of other template files, plain Python modules, as well
as locally defined "packages" of functions.

.. sourcecode:: mako

    <%namespace file="functions.html" import="*"/>

The underlying object generated by ``%namespace``, an instance of
:class:`.mako.runtime.Namespace`, is a central construct used in
templates to reference template-specific information such as the
current URI, inheritance structures, and other things that are
not as hard as they sound right here. Namespaces are described
in :ref:`namespaces_toplevel`.

``<%inherit>``
--------------

Inherit allows templates to arrange themselves in **inheritance
chains**. This is a concept familiar in many other template
languages.

.. sourcecode:: mako

    <%inherit file="base.html"/>

When using the ``%inherit`` tag, control is passed to the topmost
inherited template first, which then decides how to handle
calling areas of content from its inheriting templates. Mako
offers a lot of flexibility in this area, including dynamic
inheritance, content wrapping, and polymorphic method calls.
Check it out in :ref:`inheritance_toplevel`.

``<%``\ nsname\ ``:``\ defname\ ``>``
-------------------------------------

Any user-defined "tag" can be created against
a namespace by using a tag with a name of the form
``<%<namespacename>:<defname>>``. The closed and open formats of such a
tag are equivalent to an inline expression and the ``<%call>``
tag, respectively.

.. sourcecode:: mako

    <%mynamespace:somedef param="some value">
        this is the body
    </%mynamespace:somedef>

To create custom tags which accept a body, see
:ref:`defs_with_content`.

.. versionadded:: 0.2.3

``<%call>``
-----------

The call tag is the "classic" form of a user-defined tag, and is
roughly equivalent to the ``<%namespacename:defname>`` syntax
described above. This tag is also described in :ref:`defs_with_content`.

``<%doc>``
----------

The ``%doc`` tag handles multiline comments:

.. sourcecode:: mako

    <%doc>
        these are comments
        more comments
    </%doc>

Also the ``##`` symbol as the first non-space characters on a line can be used for single line comments.

``<%text>``
-----------

This tag suspends the Mako lexer's normal parsing of Mako
template directives, and returns its entire body contents as
plain text. It is used pretty much to write documentation about
Mako:

.. sourcecode:: mako

    <%text filter="h">
        heres some fake mako ${syntax}
        <%def name="x()">${x}</%def>
    </%text>

.. _syntax_exiting_early:

Exiting Early from a Template
=============================

Sometimes you want to stop processing a template or ``<%def>``
method in the middle and just use the text you've accumulated so
far.  This is accomplished by using ``return`` statement inside
a Python block.   It's a good idea for the ``return`` statement
to return an empty string, which prevents the Python default return
value of ``None`` from being rendered by the template.  This
return value is for semantic purposes provided in templates via
the ``STOP_RENDERING`` symbol:

.. sourcecode:: mako

    % if not len(records):
        No records found.
        <% return STOP_RENDERING %>
    % endif

Or perhaps:

.. sourcecode:: mako

    <%
        if not len(records):
            return STOP_RENDERING
    %>

In older versions of Mako, an empty string can be substituted for
the ``STOP_RENDERING`` symbol:

.. sourcecode:: mako

    <% return '' %>

.. versionadded:: 1.0.2 - added the ``STOP_RENDERING`` symbol which serves
   as a semantic identifier for the empty string ``""`` used by a
   Python ``return`` statement.

