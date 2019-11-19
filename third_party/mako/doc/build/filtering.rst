.. _filtering_toplevel:

=======================
Filtering and Buffering
=======================

.. _expression_filtering:

Expression Filtering
====================

As described in the chapter :ref:`syntax_toplevel`, the "``|``" operator can be
applied to a "``${}``" expression to apply escape filters to the
output:

.. sourcecode:: mako

    ${"this is some text" | u}

The above expression applies URL escaping to the expression, and
produces ``this+is+some+text``.

The built-in escape flags are:

* ``u`` : URL escaping, provided by
  ``urllib.quote_plus(string.encode('utf-8'))``
* ``h`` : HTML escaping, provided by
  ``markupsafe.escape(string)``

  .. versionadded:: 0.3.4
     Prior versions use ``cgi.escape(string, True)``.

* ``x`` : XML escaping
* ``trim`` : whitespace trimming, provided by ``string.strip()``
* ``entity`` : produces HTML entity references for applicable
  strings, derived from ``htmlentitydefs``
* ``unicode`` (``str`` on Python 3): produces a Python unicode
  string (this function is applied by default)
* ``decode.<some encoding>``: decode input into a Python
  unicode with the specified encoding
* ``n`` : disable all default filtering; only filters specified
  in the local expression tag will be applied.

To apply more than one filter, separate them by a comma:

.. sourcecode:: mako

    ${" <tag>some value</tag> " | h,trim}

The above produces ``&lt;tag&gt;some value&lt;/tag&gt;``, with
no leading or trailing whitespace. The HTML escaping function is
applied first, the "trim" function second.

Naturally, you can make your own filters too. A filter is just a
Python function that accepts a single string argument, and
returns the filtered result. The expressions after the ``|``
operator draw upon the local namespace of the template in which
they appear, meaning you can define escaping functions locally:

.. sourcecode:: mako

    <%!
        def myescape(text):
            return "<TAG>" + text + "</TAG>"
    %>

    Here's some tagged text: ${"text" | myescape}

Or from any Python module:

.. sourcecode:: mako

    <%!
        import myfilters
    %>

    Here's some tagged text: ${"text" | myfilters.tagfilter}

A page can apply a default set of filters to all expression tags
using the ``expression_filter`` argument to the ``%page`` tag:

.. sourcecode:: mako

    <%page expression_filter="h"/>

    Escaped text:  ${"<html>some html</html>"}

Result:

.. sourcecode:: html

    Escaped text: &lt;html&gt;some html&lt;/html&gt;

.. _filtering_default_filters:

The ``default_filters`` Argument
--------------------------------

In addition to the ``expression_filter`` argument, the
``default_filters`` argument to both :class:`.Template` and
:class:`.TemplateLookup` can specify filtering for all expression tags
at the programmatic level. This array-based argument, when given
its default argument of ``None``, will be internally set to
``["unicode"]`` (or ``["str"]`` on Python 3), except when
``disable_unicode=True`` is set in which case it defaults to
``["str"]``:

.. sourcecode:: python

    t = TemplateLookup(directories=['/tmp'], default_filters=['unicode'])

To replace the usual ``unicode``/``str`` function with a
specific encoding, the ``decode`` filter can be substituted:

.. sourcecode:: python

    t = TemplateLookup(directories=['/tmp'], default_filters=['decode.utf8'])

To disable ``default_filters`` entirely, set it to an empty
list:

.. sourcecode:: python

    t = TemplateLookup(directories=['/tmp'], default_filters=[])

Any string name can be added to ``default_filters`` where it
will be added to all expressions as a filter. The filters are
applied from left to right, meaning the leftmost filter is
applied first.

.. sourcecode:: python

    t = Template(templatetext, default_filters=['unicode', 'myfilter'])

To ease the usage of ``default_filters`` with custom filters,
you can also add imports (or other code) to all templates using
the ``imports`` argument:

.. sourcecode:: python

    t = TemplateLookup(directories=['/tmp'],
                       default_filters=['unicode', 'myfilter'],
                       imports=['from mypackage import myfilter'])

The above will generate templates something like this:

.. sourcecode:: python

    # ....
    from mypackage import myfilter

    def render_body(context):
        context.write(myfilter(unicode("some text")))

.. _expression_filtering_nfilter:

Turning off Filtering with the ``n`` Filter
-------------------------------------------

In all cases the special ``n`` filter, used locally within an
expression, will **disable** all filters declared in the
``<%page>`` tag as well as in ``default_filters``. Such as:

.. sourcecode:: mako

    ${'myexpression' | n}

will render ``myexpression`` with no filtering of any kind, and:

.. sourcecode:: mako

    ${'myexpression' | n,trim}

will render ``myexpression`` using the ``trim`` filter only.

Including the ``n`` filter in a ``<%page>`` tag will only disable
``default_filters``. In effect this makes the filters from the tag replace
default filters instead of adding to them. For example:

.. sourcecode:: mako

    <%page expression_filter="n, json.dumps"/>
    data = {a: ${123}, b: ${"123"}};

will suppress turning the values into strings using the default filter, so that
``json.dumps`` (which requires ``imports=["import json"]`` or something
equivalent) can take the value type into account, formatting numbers as numeric
literals and strings as string literals.

.. versionadded:: 1.0.14 The ``n`` filter can now be used in the ``<%page>`` tag.

Filtering Defs and Blocks
=========================

The ``%def`` and ``%block`` tags have an argument called ``filter`` which will apply the
given list of filter functions to the output of the ``%def``:

.. sourcecode:: mako

    <%def name="foo()" filter="h, trim">
        <b>this is bold</b>
    </%def>

When the ``filter`` attribute is applied to a def as above, the def
is automatically **buffered** as well. This is described next.

Buffering
=========

One of Mako's central design goals is speed. To this end, all of
the textual content within a template and its various callables
is by default piped directly to the single buffer that is stored
within the :class:`.Context` object. While this normally is easy to
miss, it has certain side effects. The main one is that when you
call a def using the normal expression syntax, i.e.
``${somedef()}``, it may appear that the return value of the
function is the content it produced, which is then delivered to
your template just like any other expression substitution,
except that normally, this is not the case; the return value of
``${somedef()}`` is simply the empty string ``''``. By the time
you receive this empty string, the output of ``somedef()`` has
been sent to the underlying buffer.

You may not want this effect, if for example you are doing
something like this:

.. sourcecode:: mako

    ${" results " + somedef() + " more results "}

If the ``somedef()`` function produced the content "``somedef's
results``", the above template would produce this output:

.. sourcecode:: html

    somedef's results results more results

This is because ``somedef()`` fully executes before the
expression returns the results of its concatenation; the
concatenation in turn receives just the empty string as its
middle expression.

Mako provides two ways to work around this. One is by applying
buffering to the ``%def`` itself:

.. sourcecode:: mako

    <%def name="somedef()" buffered="True">
        somedef's results
    </%def>

The above definition will generate code similar to this:

.. sourcecode:: python

    def somedef():
        context.push_buffer()
        try:
            context.write("somedef's results")
        finally:
            buf = context.pop_buffer()
        return buf.getvalue()

So that the content of ``somedef()`` is sent to a second buffer,
which is then popped off the stack and its value returned. The
speed hit inherent in buffering the output of a def is also
apparent.

Note that the ``filter`` argument on ``%def`` also causes the def to
be buffered. This is so that the final content of the ``%def`` can
be delivered to the escaping function in one batch, which
reduces method calls and also produces more deterministic
behavior for the filtering function itself, which can possibly
be useful for a filtering function that wishes to apply a
transformation to the text as a whole.

The other way to buffer the output of a def or any Mako callable
is by using the built-in ``capture`` function. This function
performs an operation similar to the above buffering operation
except it is specified by the caller.

.. sourcecode:: mako

    ${" results " + capture(somedef) + " more results "}

Note that the first argument to the ``capture`` function is
**the function itself**, not the result of calling it. This is
because the ``capture`` function takes over the job of actually
calling the target function, after setting up a buffered
environment. To send arguments to the function, just send them
to ``capture`` instead:

.. sourcecode:: mako

    ${capture(somedef, 17, 'hi', use_paging=True)}

The above call is equivalent to the unbuffered call:

.. sourcecode:: mako

    ${somedef(17, 'hi', use_paging=True)}

Decorating
==========

.. versionadded:: 0.2.5

Somewhat like a filter for a ``%def`` but more flexible, the ``decorator``
argument to ``%def`` allows the creation of a function that will
work in a similar manner to a Python decorator. The function can
control whether or not the function executes. The original
intent of this function is to allow the creation of custom cache
logic, but there may be other uses as well.

``decorator`` is intended to be used with a regular Python
function, such as one defined in a library module. Here we'll
illustrate the python function defined in the template for
simplicities' sake:

.. sourcecode:: mako

    <%!
        def bar(fn):
            def decorate(context, *args, **kw):
                context.write("BAR")
                fn(*args, **kw)
                context.write("BAR")
                return ''
            return decorate
    %>

    <%def name="foo()" decorator="bar">
        this is foo
    </%def>

    ${foo()}

The above template will return, with more whitespace than this,
``"BAR this is foo BAR"``. The function is the render callable
itself (or possibly a wrapper around it), and by default will
write to the context. To capture its output, use the :func:`.capture`
callable in the ``mako.runtime`` module (available in templates
as just ``runtime``):

.. sourcecode:: mako

    <%!
        def bar(fn):
            def decorate(context, *args, **kw):
                return "BAR" + runtime.capture(context, fn, *args, **kw) + "BAR"
            return decorate
    %>

    <%def name="foo()" decorator="bar">
        this is foo
    </%def>

    ${foo()}

The decorator can be used with top-level defs as well as nested
defs, and blocks too. Note that when calling a top-level def from the
:class:`.Template` API, i.e. ``template.get_def('somedef').render()``,
the decorator has to write the output to the ``context``, i.e.
as in the first example. The return value gets discarded.
