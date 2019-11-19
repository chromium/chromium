.. _inheritance_toplevel:

===========
Inheritance
===========

.. note::  Most of the inheritance examples here take advantage of a feature that's
    new in Mako as of version 0.4.1 called the "block".  This tag is very similar to
    the "def" tag but is more streamlined for usage with inheritance.  Note that
    all of the examples here which use blocks can also use defs instead.  Contrasting
    usages will be illustrated.

Using template inheritance, two or more templates can organize
themselves into an **inheritance chain**, where content and
functions from all involved templates can be intermixed. The
general paradigm of template inheritance is this: if a template
``A`` inherits from template ``B``, then template ``A`` agrees
to send the executional control to template ``B`` at runtime
(``A`` is called the **inheriting** template). Template ``B``,
the **inherited** template, then makes decisions as to what
resources from ``A`` shall be executed.

In practice, it looks like this. Here's a hypothetical inheriting
template, ``index.html``:

.. sourcecode:: mako

    ## index.html
    <%inherit file="base.html"/>

    <%block name="header">
        this is some header content
    </%block>

    this is the body content.

And ``base.html``, the inherited template:

.. sourcecode:: mako

    ## base.html
    <html>
        <body>
            <div class="header">
                <%block name="header"/>
            </div>

            ${self.body()}

            <div class="footer">
                <%block name="footer">
                    this is the footer
                </%block>
            </div>
        </body>
    </html>

Here is a breakdown of the execution:

#. When ``index.html`` is rendered, control immediately passes to
   ``base.html``.
#. ``base.html`` then renders the top part of an HTML document,
   then invokes the ``<%block name="header">`` block.  It invokes the
   underlying ``header()`` function off of a built-in namespace
   called ``self`` (this namespace was first introduced in the
   :doc:`Namespaces chapter <namespaces>` in :ref:`namespace_self`). Since
   ``index.html`` is the topmost template and also defines a block
   called ``header``, it's this ``header`` block that ultimately gets
   executed -- instead of the one that's present in ``base.html``.
#. Control comes back to ``base.html``. Some more HTML is
   rendered.
#. ``base.html`` executes ``self.body()``. The ``body()``
   function on all template-based namespaces refers to the main
   body of the template, therefore the main body of
   ``index.html`` is rendered.
#. When ``<%block name="header">`` is encountered in ``index.html``
   during the ``self.body()`` call, a conditional is checked -- does the
   current inherited template, i.e. ``base.html``, also define this block? If yes,
   the ``<%block>`` is **not** executed here -- the inheritance
   mechanism knows that the parent template is responsible for rendering
   this block (and in fact it already has).  In other words a block
   only renders in its *basemost scope*.
#. Control comes back to ``base.html``. More HTML is rendered,
   then the ``<%block name="footer">`` expression is invoked.
#. The ``footer`` block is only defined in ``base.html``, so being
   the topmost definition of ``footer``, it's the one that
   executes. If ``index.html`` also specified ``footer``, then
   its version would **override** that of the base.
#. ``base.html`` finishes up rendering its HTML and the template
   is complete, producing:

   .. sourcecode:: html

        <html>
            <body>
                <div class="header">
                    this is some header content
                </div>

                this is the body content.

                <div class="footer">
                    this is the footer
                </div>
            </body>
        </html>

...and that is template inheritance in a nutshell. The main idea
is that the methods that you call upon ``self`` always
correspond to the topmost definition of that method. Very much
the way ``self`` works in a Python class, even though Mako is
not actually using Python class inheritance to implement this
functionality. (Mako doesn't take the "inheritance" metaphor too
seriously; while useful to setup some commonly recognized
semantics, a textual template is not very much like an
object-oriented class construct in practice).

Nesting Blocks
==============

The named blocks defined in an inherited template can also be nested within
other blocks.  The name given to each block is globally accessible via any inheriting
template.  We can add a new block ``title`` to our ``header`` block:

.. sourcecode:: mako

    ## base.html
    <html>
        <body>
            <div class="header">
                <%block name="header">
                    <h2>
                        <%block name="title"/>
                    </h2>
                </%block>
            </div>

            ${self.body()}

            <div class="footer">
                <%block name="footer">
                    this is the footer
                </%block>
            </div>
        </body>
    </html>

The inheriting template can name either or both of ``header`` and ``title``, separately
or nested themselves:

.. sourcecode:: mako

    ## index.html
    <%inherit file="base.html"/>

    <%block name="header">
        this is some header content
        ${parent.header()}
    </%block>

    <%block name="title">
        this is the title
    </%block>

    this is the body content.

Note when we overrode ``header``, we added an extra call ``${parent.header()}`` in order to invoke
the parent's ``header`` block in addition to our own.  That's described in more detail below,
in :ref:`parent_namespace`.

Rendering a Named Block Multiple Times
======================================

Recall from the section :ref:`blocks` that a named block is just like a ``<%def>``,
with some different usage rules.  We can call one of our named sections distinctly, for example
a section that is used more than once, such as the title of a page:

.. sourcecode:: mako

    <html>
        <head>
            <title>${self.title()}</title>
        </head>
        <body>
        <%block name="header">
            <h2><%block name="title"/></h2>
        </%block>
        ${self.body()}
        </body>
    </html>

Where above an inheriting template can define ``<%block name="title">`` just once, and it will be
used in the base template both in the ``<title>`` section as well as the ``<h2>``.



But what about Defs?
====================

The previous example used the ``<%block>`` tag to produce areas of content
to be overridden.  Before Mako 0.4.1, there wasn't any such tag -- instead
there was only the ``<%def>`` tag.   As it turns out, named blocks and defs are
largely interchangeable.  The def simply doesn't call itself automatically,
and has more open-ended naming and scoping rules that are more flexible and similar
to Python itself, but less suited towards layout.  The first example from
this chapter using defs would look like:

.. sourcecode:: mako

    ## index.html
    <%inherit file="base.html"/>

    <%def name="header()">
        this is some header content
    </%def>

    this is the body content.

And ``base.html``, the inherited template:

.. sourcecode:: mako

    ## base.html
    <html>
        <body>
            <div class="header">
                ${self.header()}
            </div>

            ${self.body()}

            <div class="footer">
                ${self.footer()}
            </div>
        </body>
    </html>

    <%def name="header()"/>
    <%def name="footer()">
        this is the footer
    </%def>

Above, we illustrate that defs differ from blocks in that their definition
and invocation are defined in two separate places, instead of at once. You can *almost* do exactly what a
block does if you put the two together:

.. sourcecode:: mako

    <div class="header">
        <%def name="header()"></%def>${self.header()}
    </div>

The ``<%block>`` is obviously more streamlined than the ``<%def>`` for this kind
of usage.  In addition,
the above "inline" approach with ``<%def>`` does not work with nesting:

.. sourcecode:: mako

    <head>
        <%def name="header()">
            <title>
            ## this won't work !
            <%def name="title()">default title</%def>${self.title()}
            </title>
        </%def>${self.header()}
    </head>

Where above, the ``title()`` def, because it's a def within a def, is not part of the
template's exported namespace and will not be part of ``self``.  If the inherited template
did define its own ``title`` def at the top level, it would be called, but the "default title"
above is not present at all on ``self`` no matter what.  For this to work as expected
you'd instead need to say:

.. sourcecode:: mako

    <head>
        <%def name="header()">
            <title>
            ${self.title()}
            </title>
        </%def>${self.header()}

        <%def name="title()"/>
    </head>

That is, ``title`` is defined outside of any other defs so that it is in the ``self`` namespace.
It works, but the definition needs to be potentially far away from the point of render.

A named block is always placed in the ``self`` namespace, regardless of nesting,
so this restriction is lifted:

.. sourcecode:: mako

    ## base.html
    <head>
        <%block name="header">
            <title>
            <%block name="title"/>
            </title>
        </%block>
    </head>

The above template defines ``title`` inside of ``header``, and an inheriting template can define
one or both in **any** configuration, nested inside each other or not, in order for them to be used:

.. sourcecode:: mako

    ## index.html
    <%inherit file="base.html"/>
    <%block name="title">
        the title
    </%block>
    <%block name="header">
        the header
    </%block>

So while the ``<%block>`` tag lifts the restriction of nested blocks not being available externally,
in order to achieve this it *adds* the restriction that all block names in a single template need
to be globally unique within the template, and additionally that a ``<%block>`` can't be defined
inside of a ``<%def>``. It's a more restricted tag suited towards a more specific use case than ``<%def>``.

Using the ``next`` Namespace to Produce Content Wrapping
========================================================

Sometimes you have an inheritance chain that spans more than two
templates. Or maybe you don't, but you'd like to build your
system such that extra inherited templates can be inserted in
the middle of a chain where they would be smoothly integrated.
If each template wants to define its layout just within its main
body, you can't just call ``self.body()`` to get at the
inheriting template's body, since that is only the topmost body.
To get at the body of the *next* template, you call upon the
namespace ``next``, which is the namespace of the template
**immediately following** the current template.

Lets change the line in ``base.html`` which calls upon
``self.body()`` to instead call upon ``next.body()``:

.. sourcecode:: mako

    ## base.html
    <html>
        <body>
            <div class="header">
                <%block name="header"/>
            </div>

            ${next.body()}

            <div class="footer">
                <%block name="footer">
                    this is the footer
                </%block>
            </div>
        </body>
    </html>


Lets also add an intermediate template called ``layout.html``,
which inherits from ``base.html``:

.. sourcecode:: mako

    ## layout.html
    <%inherit file="base.html"/>
    <ul>
        <%block name="toolbar">
            <li>selection 1</li>
            <li>selection 2</li>
            <li>selection 3</li>
        </%block>
    </ul>
    <div class="mainlayout">
        ${next.body()}
    </div>

And finally change ``index.html`` to inherit from
``layout.html`` instead:

.. sourcecode:: mako

    ## index.html
    <%inherit file="layout.html"/>

    ## .. rest of template

In this setup, each call to ``next.body()`` will render the body
of the next template in the inheritance chain (which can be
written as ``base.html -> layout.html -> index.html``). Control
is still first passed to the bottommost template ``base.html``,
and ``self`` still references the topmost definition of any
particular def.

The output we get would be:

.. sourcecode:: html

    <html>
        <body>
            <div class="header">
                this is some header content
            </div>

            <ul>
                <li>selection 1</li>
                <li>selection 2</li>
                <li>selection 3</li>
            </ul>

            <div class="mainlayout">
            this is the body content.
            </div>

            <div class="footer">
                this is the footer
            </div>
        </body>
    </html>

So above, we have the ``<html>``, ``<body>`` and
``header``/``footer`` layout of ``base.html``, we have the
``<ul>`` and ``mainlayout`` section of ``layout.html``, and the
main body of ``index.html`` as well as its overridden ``header``
def. The ``layout.html`` template is inserted into the middle of
the chain without ``base.html`` having to change anything.
Without the ``next`` namespace, only the main body of
``index.html`` could be used; there would be no way to call
``layout.html``'s body content.

.. _parent_namespace:

Using the ``parent`` Namespace to Augment Defs
==============================================

Lets now look at the other inheritance-specific namespace, the
opposite of ``next`` called ``parent``. ``parent`` is the
namespace of the template **immediately preceding** the current
template. What's useful about this namespace is that
defs or blocks can call upon their overridden versions.
This is not as hard as it sounds and
is very much like using the ``super`` keyword in Python. Lets
modify ``index.html`` to augment the list of selections provided
by the ``toolbar`` function in ``layout.html``:

.. sourcecode:: mako

    ## index.html
    <%inherit file="layout.html"/>

    <%block name="header">
        this is some header content
    </%block>

    <%block name="toolbar">
        ## call the parent's toolbar first
        ${parent.toolbar()}
        <li>selection 4</li>
        <li>selection 5</li>
    </%block>

    this is the body content.

Above, we implemented a ``toolbar()`` function, which is meant
to override the definition of ``toolbar`` within the inherited
template ``layout.html``. However, since we want the content
from that of ``layout.html`` as well, we call it via the
``parent`` namespace whenever we want it's content, in this case
before we add our own selections. So the output for the whole
thing is now:

.. sourcecode:: html

    <html>
        <body>
            <div class="header">
                this is some header content
            </div>

            <ul>
                <li>selection 1</li>
                <li>selection 2</li>
                <li>selection 3</li>
                <li>selection 4</li>
                <li>selection 5</li>
            </ul>

            <div class="mainlayout">
            this is the body content.
            </div>

            <div class="footer">
                this is the footer
            </div>
        </body>
    </html>

and you're now a template inheritance ninja!

Using ``<%include>`` with Template Inheritance
==============================================

A common source of confusion is the behavior of the ``<%include>`` tag,
often in conjunction with its interaction within template inheritance.
Key to understanding the ``<%include>`` tag is that it is a *dynamic*, e.g.
runtime, include, and not a static include.   The ``<%include>`` is only processed
as the template renders, and not at inheritance setup time.   When encountered,
the referenced template is run fully as an entirely separate template with no
linkage to any current inheritance structure.

If the tag were on the other hand a *static* include, this would allow source
within the included template to interact within the same inheritance context
as the calling template, but currently Mako has no static include facility.

In practice, this means that ``<%block>`` elements defined in an ``<%include>``
file will not interact with corresponding ``<%block>`` elements in the calling
template.

A common mistake is along these lines:

.. sourcecode:: mako

    ## partials.mako
    <%block name="header">
        Global Header
    </%block>

    ## parent.mako
    <%include file="partials.mako">

    ## child.mako
    <%inherit file="parent.mako">
    <%block name="header">
        Custom Header
    </%block>

Above, one might expect that the ``"header"`` block declared in ``child.mako``
might be invoked, as a result of it overriding the same block present in
``parent.mako`` via the include for ``partials.mako``.  But this is not the case.
Instead, ``parent.mako`` will invoke ``partials.mako``, which then invokes
``"header"`` in ``partials.mako``, and then is finished rendering.  Nothing
from ``child.mako`` will render; there is no interaction between the ``"header"``
block in ``child.mako`` and the ``"header"`` block in ``partials.mako``.

Instead, ``parent.mako`` must explicitly state the inheritance structure.
In order to call upon specific elements of ``partials.mako``, we will call upon
it as a namespace:

.. sourcecode:: mako

    ## partials.mako
    <%block name="header">
        Global Header
    </%block>

    ## parent.mako
    <%namespace name="partials" file="partials.mako"/>
    <%block name="header">
        ${partials.header()}
    </%block>

    ## child.mako
    <%inherit file="parent.mako">
    <%block name="header">
        Custom Header
    </%block>

Where above, ``parent.mako`` states the inheritance structure that ``child.mako``
is to participate within.  ``partials.mako`` only defines defs/blocks that can be
used on a per-name basis.

Another scenario is below, which results in both ``"SectionA"`` blocks being rendered for the ``child.mako`` document:

.. sourcecode:: mako

    ## base.mako
    ${self.body()}
    <%block name="SectionA">
        base.mako
    </%block>

    ## parent.mako
    <%inherit file="base.mako">
    <%include file="child.mako">

    ## child.mako
    <%block name="SectionA">
        child.mako
    </%block>

The resolution is similar; instead of using ``<%include>``, we call upon the blocks
of ``child.mako`` using a namespace:

.. sourcecode:: mako

    ## parent.mako
    <%inherit file="base.mako">
    <%namespace name="child" file="child.mako">

    <%block name="SectionA">
        ${child.SectionA()}
    </%block>


.. _inheritance_attr:

Inheritable Attributes
======================

The :attr:`attr <.Namespace.attr>` accessor of the :class:`.Namespace` object
allows access to module level variables declared in a template. By accessing
``self.attr``, you can access regular attributes from the
inheritance chain as declared in ``<%! %>`` sections. Such as:

.. sourcecode:: mako

    <%!
        class_ = "grey"
    %>

    <div class="${self.attr.class_}">
        ${self.body()}
    </div>

If an inheriting template overrides ``class_`` to be
``"white"``, as in:

.. sourcecode:: mako

    <%!
        class_ = "white"
    %>
    <%inherit file="parent.html"/>

    This is the body

you'll get output like:

.. sourcecode:: html

    <div class="white">
        This is the body
    </div>

.. seealso::

    :ref:`namespace_attr_for_includes` - a more sophisticated example using
    :attr:`.Namespace.attr`.
