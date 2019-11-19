.. _runtime_toplevel:

============================
The Mako Runtime Environment
============================

This section describes a little bit about the objects and
built-in functions that are available in templates.

.. _context:

Context
=======

The :class:`.Context` is the central object that is created when
a template is first executed, and is responsible for handling
all communication with the outside world.  Within the template
environment, it is available via the :ref:`reserved name <reserved_names>`
``context``.  The :class:`.Context` includes two
major components, one of which is the output buffer, which is a
file-like object such as Python's ``StringIO`` or similar, and
the other a dictionary of variables that can be freely
referenced within a template; this dictionary is a combination
of the arguments sent to the :meth:`~.Template.render` function and
some built-in variables provided by Mako's runtime environment.

The Buffer
----------

The buffer is stored within the :class:`.Context`, and writing
to it is achieved by calling the :meth:`~.Context.write` method
-- in a template this looks like ``context.write('some string')``.
You usually don't need to care about this, as all text within a template, as
well as all expressions provided by ``${}``, automatically send
everything to this method. The cases you might want to be aware
of its existence are if you are dealing with various
filtering/buffering scenarios, which are described in
:ref:`filtering_toplevel`, or if you want to programmatically
send content to the output stream, such as within a ``<% %>``
block.

.. sourcecode:: mako

    <%
        context.write("some programmatic text")
    %>

The actual buffer may or may not be the original buffer sent to
the :class:`.Context` object, as various filtering/caching
scenarios may "push" a new buffer onto the context's underlying
buffer stack. For this reason, just stick with
``context.write()`` and content will always go to the topmost
buffer.

.. _context_vars:

Context Variables
-----------------

When your template is compiled into a Python module, the body
content is enclosed within a Python function called
``render_body``. Other top-level defs defined in the template are
defined within their own function bodies which are named after
the def's name with the prefix ``render_`` (i.e. ``render_mydef``).
One of the first things that happens within these functions is
that all variable names that are referenced within the function
which are not defined in some other way (i.e. such as via
assignment, module level imports, etc.) are pulled from the
:class:`.Context` object's dictionary of variables. This is how you're
able to freely reference variable names in a template which
automatically correspond to what was passed into the current
:class:`.Context`.

* **What happens if I reference a variable name that is not in
  the current context?** - The value you get back is a special
  value called ``UNDEFINED``, or if the ``strict_undefined=True`` flag
  is used a ``NameError`` is raised. ``UNDEFINED`` is just a simple global
  variable with the class :class:`mako.runtime.Undefined`. The
  ``UNDEFINED`` object throws an error when you call ``str()`` on
  it, which is what happens if you try to use it in an
  expression.
* **UNDEFINED makes it hard for me to find what name is missing** - An alternative
  is to specify the option ``strict_undefined=True``
  to the :class:`.Template` or :class:`.TemplateLookup`.  This will cause
  any non-present variables to raise an immediate ``NameError``
  which includes the name of the variable in its message
  when :meth:`~.Template.render` is called -- ``UNDEFINED`` is not used.

  .. versionadded:: 0.3.6

* **Why not just return None?** Using ``UNDEFINED``, or
  raising a ``NameError`` is more
  explicit and allows differentiation between a value of ``None``
  that was explicitly passed to the :class:`.Context` and a value that
  wasn't present at all.
* **Why raise an exception when you call str() on it ? Why not
  just return a blank string?** - Mako tries to stick to the
  Python philosophy of "explicit is better than implicit". In
  this case, it's decided that the template author should be made
  to specifically handle a missing value rather than
  experiencing what may be a silent failure. Since ``UNDEFINED``
  is a singleton object just like Python's ``True`` or ``False``,
  you can use the ``is`` operator to check for it:

  .. sourcecode:: mako

        % if someval is UNDEFINED:
            someval is: no value
        % else:
            someval is: ${someval}
        % endif

Another facet of the :class:`.Context` is that its dictionary of
variables is **immutable**. Whatever is set when
:meth:`~.Template.render` is called is what stays. Of course, since
its Python, you can hack around this and change values in the
context's internal dictionary, but this will probably will not
work as well as you'd think. The reason for this is that Mako in
many cases creates copies of the :class:`.Context` object, which
get sent to various elements of the template and inheriting
templates used in an execution. So changing the value in your
local :class:`.Context` will not necessarily make that value
available in other parts of the template's execution. Examples
of where Mako creates copies of the :class:`.Context` include
within top-level def calls from the main body of the template
(the context is used to propagate locally assigned variables
into the scope of defs; since in the template's body they appear
as inlined functions, Mako tries to make them act that way), and
within an inheritance chain (each template in an inheritance
chain has a different notion of ``parent`` and ``next``, which
are all stored in unique :class:`.Context` instances).

* **So what if I want to set values that are global to everyone
  within a template request?** - All you have to do is provide a
  dictionary to your :class:`.Context` when the template first
  runs, and everyone can just get/set variables from that. Lets
  say its called ``attributes``.

  Running the template looks like:

  .. sourcecode:: python

      output = template.render(attributes={})

  Within a template, just reference the dictionary:

  .. sourcecode:: mako

      <%
          attributes['foo'] = 'bar'
      %>
      'foo' attribute is: ${attributes['foo']}

* **Why can't "attributes" be a built-in feature of the
  Context?** - This is an area where Mako is trying to make as
  few decisions about your application as it possibly can.
  Perhaps you don't want your templates to use this technique of
  assigning and sharing data, or perhaps you have a different
  notion of the names and kinds of data structures that should
  be passed around. Once again Mako would rather ask the user to
  be explicit.

Context Methods and Accessors
-----------------------------

Significant members of :class:`.Context` include:

* ``context[key]`` / ``context.get(key, default=None)`` -
  dictionary-like accessors for the context. Normally, any
  variable you use in your template is automatically pulled from
  the context if it isn't defined somewhere already. Use the
  dictionary accessor and/or ``get`` method when you want a
  variable that *is* already defined somewhere else, such as in
  the local arguments sent to a ``%def`` call. If a key is not
  present, like a dictionary it raises ``KeyError``.
* ``keys()`` - all the names defined within this context.
* ``kwargs`` - this returns a **copy** of the context's
  dictionary of variables. This is useful when you want to
  propagate the variables in the current context to a function
  as keyword arguments, i.e.:

  .. sourcecode:: mako

        ${next.body(**context.kwargs)}

* ``write(text)`` - write some text to the current output
  stream.
* ``lookup`` - returns the :class:`.TemplateLookup` instance that is
  used for all file-lookups within the current execution (even
  though individual :class:`.Template` instances can conceivably have
  different instances of a :class:`.TemplateLookup`, only the
  :class:`.TemplateLookup` of the originally-called :class:`.Template` gets
  used in a particular execution).

.. _loop_context:

The Loop Context
================

Within ``% for`` blocks, the :ref:`reserved name<reserved_names>` ``loop``
is available.  ``loop`` tracks the progress of
the ``for`` loop and makes it easy to use the iteration state to control
template behavior:

.. sourcecode:: mako

    <ul>
    % for a in ("one", "two", "three"):
        <li>Item ${loop.index}: ${a}</li>
    % endfor
    </ul>

.. versionadded:: 0.7

Iterations
----------

Regardless of the type of iterable you're looping over, ``loop`` always tracks
the 0-indexed iteration count (available at ``loop.index``), its parity
(through the ``loop.even`` and ``loop.odd`` bools), and ``loop.first``, a bool
indicating whether the loop is on its first iteration.  If your iterable
provides a ``__len__`` method, ``loop`` also provides access to
a count of iterations remaining at ``loop.reverse_index`` and ``loop.last``,
a bool indicating whether the loop is on its last iteration; accessing these
without ``__len__`` will raise a ``TypeError``.

Cycling
-------

Cycling is available regardless of whether the iterable you're using provides
a ``__len__`` method.  Prior to Mako 0.7, you might have generated a simple
zebra striped list using ``enumerate``:

.. sourcecode:: mako

    <ul>
    % for i, item in enumerate(('spam', 'ham', 'eggs')):
      <li class="${'odd' if i % 2 else 'even'}">${item}</li>
    % endfor
    </ul>

With ``loop.cycle``, you get the same results with cleaner code and less prep work:

.. sourcecode:: mako

    <ul>
    % for item in ('spam', 'ham', 'eggs'):
      <li class="${loop.cycle('even', 'odd')}">${item}</li>
    % endfor
    </ul>

Both approaches produce output like the following:

.. sourcecode:: html

    <ul>
      <li class="even">spam</li>
      <li class="odd">ham</li>
      <li class="even">eggs</li>
    </ul>

Parent Loops
------------

Loop contexts can also be transparently nested, and the Mako runtime will do
the right thing and manage the scope for you.  You can access the parent loop
context through ``loop.parent``.

This allows you to reach all the way back up through the loop stack by
chaining ``parent`` attribute accesses, i.e. ``loop.parent.parent....`` as
long as the stack depth isn't exceeded.  For example, you can use the parent
loop to make a checkered table:

.. sourcecode:: mako

    <table>
    % for consonant in 'pbj':
      <tr>
      % for vowel in 'iou':
        <td class="${'black' if (loop.parent.even == loop.even) else 'red'}">
          ${consonant + vowel}t
        </td>
      % endfor
      </tr>
    % endfor
    </table>

.. sourcecode:: html

    <table>
      <tr>
        <td class="black">
          pit
        </td>
        <td class="red">
          pot
        </td>
        <td class="black">
          put
        </td>
      </tr>
      <tr>
        <td class="red">
          bit
        </td>
        <td class="black">
          bot
        </td>
        <td class="red">
          but
        </td>
      </tr>
      <tr>
        <td class="black">
          jit
        </td>
        <td class="red">
          jot
        </td>
        <td class="black">
          jut
        </td>
      </tr>
    </table>

.. _migrating_loop:

Migrating Legacy Templates that Use the Word "loop"
---------------------------------------------------

.. versionchanged:: 0.7
   The ``loop`` name is now :ref:`reserved <reserved_names>` in Mako,
   which means a template that refers to a variable named ``loop``
   won't function correctly when used in Mako 0.7.

To ease the transition for such systems, the feature can be disabled across the board for
all templates, then re-enabled on a per-template basis for those templates which wish
to make use of the new system.

First, the ``enable_loop=False`` flag is passed to either the :class:`.TemplateLookup`
or :class:`.Template` object in use:

.. sourcecode:: python

    lookup = TemplateLookup(directories=['/docs'], enable_loop=False)

or:

.. sourcecode:: python

    template = Template("some template", enable_loop=False)

An individual template can make usage of the feature when ``enable_loop`` is set to
``False`` by switching it back on within the ``<%page>`` tag:

.. sourcecode:: mako

    <%page enable_loop="True"/>

    % for i in collection:
        ${i} ${loop.index}
    % endfor

Using the above scheme, it's safe to pass the name ``loop`` to the :meth:`.Template.render`
method as well as to freely make usage of a variable named ``loop`` within a template, provided
the ``<%page>`` tag doesn't override it.  New templates that want to use the ``loop`` context
can then set up ``<%page enable_loop="True"/>`` to use the new feature without affecting
old templates.

All the Built-in Names
======================

A one-stop shop for all the names Mako defines. Most of these
names are instances of :class:`.Namespace`, which are described
in the next section, :ref:`namespaces_toplevel`. Also, most of
these names other than ``context``, ``UNDEFINED``, and ``loop`` are
also present *within* the :class:`.Context` itself.   The names
``context``, ``loop`` and ``UNDEFINED`` themselves can't be passed
to the context and can't be substituted -- see the section :ref:`reserved_names`.

* ``context`` - this is the :class:`.Context` object, introduced
  at :ref:`context`.
* ``local`` - the namespace of the current template, described
  in :ref:`namespaces_builtin`.
* ``self`` - the namespace of the topmost template in an
  inheritance chain (if any, otherwise the same as ``local``),
  mostly described in :ref:`inheritance_toplevel`.
* ``parent`` - the namespace of the parent template in an
  inheritance chain (otherwise undefined); see
  :ref:`inheritance_toplevel`.
* ``next`` - the namespace of the next template in an
  inheritance chain (otherwise undefined); see
  :ref:`inheritance_toplevel`.
* ``caller`` - a "mini" namespace created when using the
  ``<%call>`` tag to define a "def call with content"; described
  in :ref:`defs_with_content`.
* ``loop`` - this provides access to :class:`.LoopContext` objects when
  they are requested within ``% for`` loops, introduced at :ref:`loop_context`.
* ``capture`` - a function that calls a given def and captures
  its resulting content into a string, which is returned. Usage
  is described in :ref:`filtering_toplevel`.
* ``UNDEFINED`` - a global singleton that is applied to all
  otherwise uninitialized template variables that were not
  located within the :class:`.Context` when rendering began,
  unless the :class:`.Template` flag ``strict_undefined``
  is set to ``True``. ``UNDEFINED`` is
  an instance of :class:`.Undefined`, and raises an
  exception when its ``__str__()`` method is called.
* ``pageargs`` - this is a dictionary which is present in a
  template which does not define any ``**kwargs`` section in its
  ``<%page>`` tag. All keyword arguments sent to the ``body()``
  function of a template (when used via namespaces) go here by
  default unless otherwise defined as a page argument. If this
  makes no sense, it shouldn't; read the section
  :ref:`namespaces_body`.

.. _reserved_names:

Reserved Names
--------------

Mako has a few names that are considered to be "reserved" and can't be used
as variable names.

.. versionchanged:: 0.7
   Mako raises an error if these words are found passed to the template
   as context arguments, whereas in previous versions they'd be silently
   ignored or lead to other error messages.

* ``context`` - see :ref:`context`.
* ``UNDEFINED`` - see :ref:`context_vars`.
* ``loop`` - see :ref:`loop_context`.  Note this can be disabled for legacy templates
  via the ``enable_loop=False`` argument; see :ref:`migrating_loop`.

API Reference
=============

.. autoclass:: mako.runtime.Context
    :show-inheritance:
    :members:

.. autoclass:: mako.runtime.LoopContext
    :show-inheritance:
    :members:

.. autoclass:: mako.runtime.Undefined
    :show-inheritance:

