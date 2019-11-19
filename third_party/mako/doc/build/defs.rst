.. _defs_toplevel:

===============
Defs and Blocks
===============

``<%def>`` and ``<%block>`` are two tags that both demarcate any block of text
and/or code.   They both exist within generated Python as a callable function,
i.e., a Python ``def``.   They differ in their scope and calling semantics.
Whereas ``<%def>`` provides a construct that is very much like a named Python
``def``, the ``<%block>`` is more layout oriented.

Using Defs
==========

The ``<%def>`` tag requires a ``name`` attribute, where the ``name`` references
a Python function signature:

.. sourcecode:: mako

    <%def name="hello()">
        hello world
    </%def>

To invoke the ``<%def>``, it is normally called as an expression:

.. sourcecode:: mako

    the def:  ${hello()}

If the ``<%def>`` is not nested inside of another ``<%def>``,
it's known as a **top level def** and can be accessed anywhere in
the template, including above where it was defined.

All defs, top level or not, have access to the current
contextual namespace in exactly the same way their containing
template does. Suppose the template below is executed with the
variables ``username`` and ``accountdata`` inside the context:

.. sourcecode:: mako

    Hello there ${username}, how are ya.  Lets see what your account says:

    ${account()}

    <%def name="account()">
        Account for ${username}:<br/>

        % for row in accountdata:
            Value: ${row}<br/>
        % endfor
    </%def>

The ``username`` and ``accountdata`` variables are present
within the main template body as well as the body of the
``account()`` def.

Since defs are just Python functions, you can define and pass
arguments to them as well:

.. sourcecode:: mako

    ${account(accountname='john')}

    <%def name="account(accountname, type='regular')">
        account name: ${accountname}, type: ${type}
    </%def>

When you declare an argument signature for your def, they are
required to follow normal Python conventions (i.e., all
arguments are required except keyword arguments with a default
value). This is in contrast to using context-level variables,
which evaluate to ``UNDEFINED`` if you reference a name that
does not exist.

Calling Defs from Other Files
-----------------------------

Top level ``<%def>``\ s are **exported** by your template's
module, and can be called from the outside; including from other
templates, as well as normal Python code. Calling a ``<%def>``
from another template is something like using an ``<%include>``
-- except you are calling a specific function within the
template, not the whole template.

The remote ``<%def>`` call is also a little bit like calling
functions from other modules in Python. There is an "import"
step to pull the names from another template into your own
template; then the function or functions are available.

To import another template, use the ``<%namespace>`` tag:

.. sourcecode:: mako

    <%namespace name="mystuff" file="mystuff.html"/>

The above tag adds a local variable ``mystuff`` to the current
scope.

Then, just call the defs off of ``mystuff``:

.. sourcecode:: mako

    ${mystuff.somedef(x=5,y=7)}

The ``<%namespace>`` tag also supports some of the other
semantics of Python's ``import`` statement, including pulling
names into the local variable space, or using ``*`` to represent
all names, using the ``import`` attribute:

.. sourcecode:: mako

    <%namespace file="mystuff.html" import="foo, bar"/>

This is just a quick intro to the concept of a **namespace**,
which is a central Mako concept that has its own chapter in
these docs. For more detail and examples, see
:ref:`namespaces_toplevel`.

Calling Defs Programmatically
-----------------------------

You can call defs programmatically from any :class:`.Template` object
using the :meth:`~.Template.get_def()` method, which returns a :class:`.DefTemplate`
object. This is a :class:`.Template` subclass which the parent
:class:`.Template` creates, and is usable like any other template:

.. sourcecode:: python

    from mako.template import Template

    template = Template("""
        <%def name="hi(name)">
            hi ${name}!
        </%def>

        <%def name="bye(name)">
            bye ${name}!
        </%def>
    """)

    print(template.get_def("hi").render(name="ed"))
    print(template.get_def("bye").render(name="ed"))

Defs within Defs
----------------

The def model follows regular Python rules for closures.
Declaring ``<%def>`` inside another ``<%def>`` declares it
within the parent's **enclosing scope**:

.. sourcecode:: mako

    <%def name="mydef()">
        <%def name="subdef()">
            a sub def
        </%def>

        i'm the def, and the subcomponent is ${subdef()}
    </%def>

Just like Python, names that exist outside the inner ``<%def>``
exist inside it as well:

.. sourcecode:: mako

    <%
        x = 12
    %>
    <%def name="outer()">
        <%
            y = 15
        %>
        <%def name="inner()">
            inner, x is ${x}, y is ${y}
        </%def>

        outer, x is ${x}, y is ${y}
    </%def>

Assigning to a name inside of a def declares that name as local
to the scope of that def (again, like Python itself). This means
the following code will raise an error:

.. sourcecode:: mako

    <%
        x = 10
    %>
    <%def name="somedef()">
        ## error !
        somedef, x is ${x}
        <%
            x = 27
        %>
    </%def>

...because the assignment to ``x`` declares ``x`` as local to the
scope of ``somedef``, rendering the "outer" version unreachable
in the expression that tries to render it.

.. _defs_with_content:

Calling a Def with Embedded Content and/or Other Defs
-----------------------------------------------------

A flip-side to def within def is a def call with content. This
is where you call a def, and at the same time declare a block of
content (or multiple blocks) that can be used by the def being
called. The main point of such a call is to create custom,
nestable tags, just like any other template language's
custom-tag creation system -- where the external tag controls the
execution of the nested tags and can communicate state to them.
Only with Mako, you don't have to use any external Python
modules, you can define arbitrarily nestable tags right in your
templates.

To achieve this, the target def is invoked using the form
``<%namespacename:defname>`` instead of the normal ``${}``
syntax. This syntax, introduced in Mako 0.2.3, is functionally
equivalent to another tag known as ``%call``, which takes the form
``<%call expr='namespacename.defname(args)'>``. While ``%call``
is available in all versions of Mako, the newer style is
probably more familiar looking. The ``namespace`` portion of the
call is the name of the **namespace** in which the def is
defined -- in the most simple cases, this can be ``local`` or
``self`` to reference the current template's namespace (the
difference between ``local`` and ``self`` is one of inheritance
-- see :ref:`namespaces_builtin` for details).

When the target def is invoked, a variable ``caller`` is placed
in its context which contains another namespace containing the
body and other defs defined by the caller. The body itself is
referenced by the method ``body()``. Below, we build a ``%def``
that operates upon ``caller.body()`` to invoke the body of the
custom tag:

.. sourcecode:: mako

    <%def name="buildtable()">
        <table>
            <tr><td>
                ${caller.body()}
            </td></tr>
        </table>
    </%def>

    <%self:buildtable>
        I am the table body.
    </%self:buildtable>

This produces the output (whitespace formatted):

.. sourcecode:: html

    <table>
        <tr><td>
            I am the table body.
        </td></tr>
    </table>

Using the older ``%call`` syntax looks like:

.. sourcecode:: mako

    <%def name="buildtable()">
        <table>
            <tr><td>
                ${caller.body()}
            </td></tr>
        </table>
    </%def>

    <%call expr="buildtable()">
        I am the table body.
    </%call>

The ``body()`` can be executed multiple times or not at all.
This means you can use def-call-with-content to build iterators,
conditionals, etc:

.. sourcecode:: mako

    <%def name="lister(count)">
        % for x in range(count):
            ${caller.body()}
        % endfor
    </%def>

    <%self:lister count="${3}">
        hi
    </%self:lister>

Produces:

.. sourcecode:: html

    hi
    hi
    hi

Notice above we pass ``3`` as a Python expression, so that it
remains as an integer.

A custom "conditional" tag:

.. sourcecode:: mako

    <%def name="conditional(expression)">
        % if expression:
            ${caller.body()}
        % endif
    </%def>

    <%self:conditional expression="${4==4}">
        i'm the result
    </%self:conditional>

Produces:

.. sourcecode:: html

    i'm the result

But that's not all. The ``body()`` function also can handle
arguments, which will augment the local namespace of the body
callable. The caller must define the arguments which it expects
to receive from its target def using the ``args`` attribute,
which is a comma-separated list of argument names. Below, our
``<%def>`` calls the ``body()`` of its caller, passing in an
element of data from its argument:

.. sourcecode:: mako

    <%def name="layoutdata(somedata)">
        <table>
        % for item in somedata:
            <tr>
            % for col in item:
                <td>${caller.body(col=col)}</td>
            % endfor
            </tr>
        % endfor
        </table>
    </%def>

    <%self:layoutdata somedata="${[[1,2,3],[4,5,6],[7,8,9]]}" args="col">\
    Body data: ${col}\
    </%self:layoutdata>

Produces:

.. sourcecode:: html

    <table>
        <tr>
            <td>Body data: 1</td>
            <td>Body data: 2</td>
            <td>Body data: 3</td>
        </tr>
        <tr>
            <td>Body data: 4</td>
            <td>Body data: 5</td>
            <td>Body data: 6</td>
        </tr>
        <tr>
            <td>Body data: 7</td>
            <td>Body data: 8</td>
            <td>Body data: 9</td>
        </tr>
    </table>

You don't have to stick to calling just the ``body()`` function.
The caller can define any number of callables, allowing the
``<%call>`` tag to produce whole layouts:

.. sourcecode:: mako

    <%def name="layout()">
        ## a layout def
        <div class="mainlayout">
            <div class="header">
                ${caller.header()}
            </div>

            <div class="sidebar">
                ${caller.sidebar()}
            </div>

            <div class="content">
                ${caller.body()}
            </div>
        </div>
    </%def>

    ## calls the layout def
    <%self:layout>
        <%def name="header()">
            I am the header
        </%def>
        <%def name="sidebar()">
            <ul>
                <li>sidebar 1</li>
                <li>sidebar 2</li>
            </ul>
        </%def>

            this is the body
    </%self:layout>

The above layout would produce:

.. sourcecode:: html

    <div class="mainlayout">
        <div class="header">
        I am the header
        </div>

        <div class="sidebar">
        <ul>
            <li>sidebar 1</li>
            <li>sidebar 2</li>
        </ul>
        </div>

        <div class="content">
        this is the body
        </div>
    </div>

The number of things you can do with ``<%call>`` and/or the
``<%namespacename:defname>`` calling syntax is enormous. You can
create form widget libraries, such as an enclosing ``<FORM>``
tag and nested HTML input elements, or portable wrapping schemes
using ``<div>`` or other elements. You can create tags that
interpret rows of data, such as from a database, providing the
individual columns of each row to a ``body()`` callable which
lays out the row any way it wants. Basically anything you'd do
with a "custom tag" or tag library in some other system, Mako
provides via ``<%def>`` tags and plain Python callables which are
invoked via ``<%namespacename:defname>`` or ``<%call>``.

.. _blocks:

Using Blocks
============

The ``<%block>`` tag introduces some new twists on the
``<%def>`` tag which make it more closely tailored towards layout.

.. versionadded:: 0.4.1

An example of a block:

.. sourcecode:: mako

    <html>
        <body>
            <%block>
                this is a block.
            </%block>
        </body>
    </html>

In the above example, we define a simple block.  The block renders its content in the place
that it's defined.  Since the block is called for us, it doesn't need a name and the above
is referred to as an **anonymous block**.  So the output of the above template will be:

.. sourcecode:: html

    <html>
        <body>
                this is a block.
        </body>
    </html>

So in fact the above block has absolutely no effect.  Its usefulness comes when we start
using modifiers.  Such as, we can apply a filter to our block:

.. sourcecode:: mako

    <html>
        <body>
            <%block filter="h">
                <html>this is some escaped html.</html>
            </%block>
        </body>
    </html>

or perhaps a caching directive:

.. sourcecode:: mako

    <html>
        <body>
            <%block cached="True" cache_timeout="60">
                This content will be cached for 60 seconds.
            </%block>
        </body>
    </html>

Blocks also work in iterations, conditionals, just like defs:

.. sourcecode:: mako

    % if some_condition:
        <%block>condition is met</%block>
    % endif

While the block renders at the point it is defined in the template,
the underlying function is present in the generated Python code only
once, so there's no issue with placing a block inside of a loop or
similar. Anonymous blocks are defined as closures in the local
rendering body, so have access to local variable scope:

.. sourcecode:: mako

    % for i in range(1, 4):
        <%block>i is ${i}</%block>
    % endfor

Using Named Blocks
------------------

Possibly the more important area where blocks are useful is when we
do actually give them names. Named blocks are tailored to behave
somewhat closely to Jinja2's block tag, in that they define an area
of a layout which can be overridden by an inheriting template. In
sharp contrast to the ``<%def>`` tag, the name given to a block is
global for the entire template regardless of how deeply it's nested:

.. sourcecode:: mako

    <html>
    <%block name="header">
        <head>
            <title>
                <%block name="title">Title</%block>
            </title>
        </head>
    </%block>
    <body>
        ${next.body()}
    </body>
    </html>

The above example has two named blocks "``header``" and "``title``", both of which can be referred to
by an inheriting template. A detailed walkthrough of this usage can be found at :ref:`inheritance_toplevel`.

Note above that named blocks don't have any argument declaration the way defs do. They still implement themselves
as Python functions, however, so they can be invoked additional times beyond their initial definition:

.. sourcecode:: mako

    <div name="page">
        <%block name="pagecontrol">
            <a href="">previous page</a> |
            <a href="">next page</a>
        </%block>

        <table>
            ## some content
        </table>

        ${pagecontrol()}
    </div>

The content referenced by ``pagecontrol`` above will be rendered both above and below the ``<table>`` tags.

To keep things sane, named blocks have restrictions that defs do not:

* The ``<%block>`` declaration cannot have any argument signature.
* The name of a ``<%block>`` can only be defined once in a template -- an error is raised if two blocks of the same
  name occur anywhere in a single template, regardless of nesting.  A similar error is raised if a top level def
  shares the same name as that of a block.
* A named ``<%block>`` cannot be defined within a ``<%def>``, or inside the body of a "call", i.e.
  ``<%call>`` or ``<%namespacename:defname>`` tag.  Anonymous blocks can, however.

Using Page Arguments in Named Blocks
------------------------------------

A named block is very much like a top level def. It has a similar
restriction to these types of defs in that arguments passed to the
template via the ``<%page>`` tag aren't automatically available.
Using arguments with the ``<%page>`` tag is described in the section
:ref:`namespaces_body`, and refers to scenarios such as when the
``body()`` method of a template is called from an inherited template passing
arguments, or the template is invoked from an ``<%include>`` tag
with arguments. To allow a named block to share the same arguments
passed to the page, the ``args`` attribute can be used:

.. sourcecode:: mako

    <%page args="post"/>

    <a name="${post.title}" />

    <span class="post_prose">
        <%block name="post_prose" args="post">
            ${post.content}
        </%block>
    </span>

Where above, if the template is called via a directive like
``<%include file="post.mako" args="post=post" />``, the ``post``
variable is available both in the main body as well as the
``post_prose`` block.

Similarly, the ``**pageargs`` variable is present, in named blocks only,
for those arguments not explicit in the ``<%page>`` tag:

.. sourcecode:: mako

    <%block name="post_prose">
        ${pageargs['post'].content}
    </%block>

The ``args`` attribute is only allowed with named blocks. With
anonymous blocks, the Python function is always rendered in the same
scope as the call itself, so anything available directly outside the
anonymous block is available inside as well.
