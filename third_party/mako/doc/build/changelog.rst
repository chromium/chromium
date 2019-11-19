=========
Changelog
=========

1.0
===

.. changelog::
    :version: 1.0.14
    :released: Sat Jul 20 2019

    .. change::
        :tags: feature, template

        The ``n`` filter is now supported in the ``<%page>`` tag.  This allows a
        template to omit the default expression filters throughout a whole
        template, for those cases where a template-wide filter needs to have
        default filtering disabled.  Pull request courtesy Martin von Gagern.

        .. seealso::

            :ref:`expression_filtering_nfilter`



    .. change::
        :tags: bug, exceptions

        Fixed issue where the correct file URI would not be shown in the
        template-formatted exception traceback if the template filename were not
        known.  Additionally fixes an issue where stale filenames would be
        displayed if a stack trace alternated between different templates.  Pull
        request courtesy Martin von Gagern.


.. changelog::
    :version: 1.0.13
    :released: Mon Jul 1 2019

    .. change::
        :tags: bug, exceptions

        Improved the line-number tracking for source lines inside of Python  ``<%
        ... %>`` blocks, such that text- and HTML-formatted exception traces such
        as that of  :func:`.html_error_template` now report the correct source line
        inside the block, rather than the first line of the block itself.
        Exceptions in ``<%! ... %>`` blocks which get raised while loading the
        module are still not reported correctly, as these are handled before the
        Mako code is generated.  Pull request courtesy Martin von Gagern.

.. changelog::
    :version: 1.0.12
    :released: Wed Jun 5 2019

    .. change::
        :tags: bug, py3k
        :tickets: 296

        Fixed regression where import refactors in Mako 1.0.11 caused broken
        imports on Python 3.8.


.. changelog::
    :version: 1.0.11
    :released: Fri May 31 2019

    .. change::
        :tags: change

        Updated for additional project metadata in setup.py.   Additionally,
        the code has been reformatted using Black and zimports.

.. changelog::
    :version: 1.0.10
    :released: Fri May 10 2019

    .. change::
        :tags: bug, py3k
        :tickets: 293

     Added a default encoding of "utf-8" when the :class:`.RichTraceback`
     object retrieves Python source lines from a Python traceback; as these
     are bytes in Python 3 they need to be decoded so that they can be
     formatted in the template.

.. changelog::
    :version: 1.0.9
    :released: Mon Apr 15 2019

    .. change::
        :tags: bug
        :tickets: 287

     Further corrected the previous fix for :ticket:`287` as it relied upon
     an attribute that is monkeypatched by Python's ``ast`` module for some
     reason, which fails if ``ast`` hasn't been imported; the correct
     attribute ``Constant.value`` is now used.   Also note the issue
     was mis-numbered in the previous changelog note.

.. changelog::
    :version: 1.0.8
    :released: Wed Mar 20 2019
    :released: Wed Mar 20 2019

    .. change::
        :tags: bug
        :tickets: 287

     Fixed an element in the AST Python generator which changed
     for Python 3.8, causing expression generation to fail.

    .. change::
        :tags: feature
        :tickets: 271

     Added ``--output-encoding`` flag to the mako-render script.
     Pull request courtesy lacsaP.

    .. change::
        :tags: bug

     Removed unnecessary "usage" prefix from mako-render script.
     Pull request courtesy Hugo.

.. changelog::
    :version: 1.0.7
    :released: Thu Jul 13 2017

    .. change::
        :tags: bug

     Changed the "print" in the mako-render command to
     sys.stdout.write(), avoiding the extra newline at the end
     of the template output.  Pull request courtesy
     Yves Chevallier.

.. changelog::
    :version: 1.0.6
    :released: Wed Nov 9 2016

    .. change::
        :tags: feature

      Added new parameter :paramref:`.Template.include_error_handler` .
      This works like :paramref:`.Template.error_handler` but indicates the
      handler should take place when this template is included within another
      template via the ``<%include>`` tag.  Pull request courtesy
      Huayi Zhang.

.. changelog::
    :version: 1.0.5
    :released: Wed Nov 2 2016

    .. change::
        :tags: bug

      Updated the Sphinx documentation builder to work with recent
      versions of Sphinx.

.. changelog::
    :version: 1.0.4
    :released: Thu Mar 10 2016

    .. change::
        :tags: feature, test

      The default test runner is now py.test.  Running "python setup.py test"
      will make use of py.test instead of nose.  nose still works as a test
      runner as well, however.

    .. change::
        :tags: bug, lexer
        :pullreq: github:19

      Major improvements to lexing of intricate Python sections which may
      contain complex backslash sequences, as well as support for the bitwise
      operator (e.g. pipe symbol) inside of expression sections distinct
      from the Mako "filter" operator, provided the operator is enclosed
      within parentheses or brackets.  Pull request courtesy Daniel Martin.

    .. change::
        :tags: feature

      Added new method :meth:`.Template.list_defs`.   Pull request courtesy
      Jonathan Vanasco.

.. changelog::
    :version: 1.0.3
    :released: Tue Oct 27 2015

    .. change::
        :tags: bug, babel

      Fixed an issue where the Babel plugin would not handle a translation
      symbol that contained non-ascii characters.  Pull request courtesy
      Roman Imankulov.

.. changelog::
    :version: 1.0.2
    :released: Wed Aug 26 2015

    .. change::
        :tags: bug, installation
        :tickets: 249

      The "universal wheel" marker is removed from setup.cfg, because
      our setup.py currently makes use of conditional dependencies.
      In :ticket:`249`, the discussion is ongoing on how to correct our
      setup.cfg / setup.py fully so that we can handle the per-version
      dependency changes while still maintaining optimal wheel settings,
      so this issue is not yet fully resolved.

    .. change::
        :tags: bug, py3k
        :tickets: 250

      Repair some calls within the ast module that no longer work on Python3.5;
      additionally replace the use of ``inspect.getargspec()`` under
      Python 3 (seems to be called from the TG plugin) to avoid deprecation
      warnings.

    .. change::
        :tags: bug

      Update the Lingua translation extraction plugin to correctly
      handle templates mixing Python control statements (such as if,
      for and while) with template fragments. Pull request courtesy
      Laurent Daverio.

    .. change::
        :tags: feature
        :tickets: 236

      Added ``STOP_RENDERING`` keyword for returning/exiting from a
      template early, which is a synonym for an empty string ``""``.
      Previously, the docs suggested a bare
      ``return``, but this could cause ``None`` to appear in the
      rendered template result.

      .. seealso::

        :ref:`syntax_exiting_early`

.. changelog::
    :version: 1.0.1
    :released: Thu Jan 22 2015

    .. change::
        :tags: feature

      Added support for Lingua, a translation extraction system as an
      alternative to Babel.  Pull request courtesy Wichert Akkerman.

    .. change::
        :tags: bug, py3k

      Modernized the examples/wsgi/run_wsgi.py file for Py3k.
      Pull requset courtesy Cody Taylor.

.. changelog::
    :version: 1.0.0
    :released: Sun Jun 8 2014

    .. change::
        :tags: bug, py2k

      Improved the error re-raise operation when a custom
      :paramref:`.Template.error_handler` is used that does not handle
      the exception; the original stack trace etc. is now preserved.
      Pull request courtesy Manfred Haltner.

    .. change::
        :tags: bug, py2k, filters

      Added an html_escape filter that works in "non unicode" mode.
      Previously, when using ``disable_unicode=True``, the ``u`` filter
      would fail to handle non-ASCII bytes properly.  Pull request
      courtesy George Xie.

    .. change::
        :tags: general

      Compatibility changes; in order to modernize the codebase, Mako
      is now dropping support for Python 2.4 and Python 2.5 altogether.
      The source base is now targeted at Python 2.6 and forwards.

    .. change::
        :tags: feature

      Template modules now generate a JSON "metadata" structure at the bottom
      of the source file which includes parseable information about the
      templates' source file, encoding etc. as well as a mapping of module
      source lines to template lines, thus replacing the "# SOURCE LINE"
      markers throughout the source code.  The structure also indicates those
      lines that are explicitly not part of the template's source; the goal
      here is to allow better integration with coverage and other tools.

    .. change::
        :tags: bug, py3k

      Fixed bug in ``decode.<encoding>`` filter where a non-string object
      would not be correctly interpreted in Python 3.

    .. change::
        :tags: bug, py3k
        :tickets: 227

      Fixed bug in Python parsing logic which would fail on Python 3
      when a "try/except" targeted a tuple of exception types, rather
      than a single exception.

    .. change::
        :tags: feature

      mako-render is now implemented as a setuptools entrypoint script;
      a standalone mako.cmd.cmdline() callable is now available, and the
      system also uses argparse now instead of optparse.  Pull request
      courtesy Derek Harland.

    .. change::
        :tags: feature

      The mako-render script will now catch exceptions and run them
      into the text error handler, and exit with a non-zero exit code.
      Pull request courtesy Derek Harland.

    .. change::
        :tags: bug

      A rework of the mako-render script allows the script to run
      correctly when given a file pathname that is outside of the current
      directory, e.g. ``mako-render ../some_template.mako``.  In this case,
      the "template root" defaults to the directory in which the template
      is located, instead of ".".  The script also accepts a new argument
      ``--template-dir`` which can be specified multiple times to establish
      template lookup directories.  Standard input for templates also works
      now too.  Pull request courtesy Derek Harland.

    .. change::
        :tags: feature, py3k
        :pullreq: github:7

      Support is added for Python 3 "keyword only" arguments, as used in
      defs.  Pull request courtesy Eevee.


0.9
===

.. changelog::
    :version: 0.9.1
    :released: Thu Dec 26 2013

    .. change::
        :tags: bug
        :tickets: 225

      Fixed bug in Babel plugin where translator comments
      would be lost if intervening text nodes were encountered.
      Fix courtesy Ned Batchelder.

    .. change::
        :tags: bug
        :tickets:

      Fixed TGPlugin.render method to support unicode template
      names in Py2K - courtesy Vladimir Magamedov.

    .. change::
        :tags: bug
        :tickets:

      Fixed an AST issue that was preventing correct operation
      under alpha versions of Python 3.4.  Pullreq courtesy Zer0-.

    .. change::
        :tags: bug
        :tickets:

      Changed the format of the "source encoding" header output
      by the code generator to use the format ``# -*- coding:%s -*-``
      instead of ``# -*- encoding:%s -*-``; the former is more common
      and compatible with emacs.  Courtesy Martin Geisler.

    .. change::
        :tags: bug
        :tickets: 224

      Fixed issue where an old lexer rule prevented a template line
      which looked like "#*" from being correctly parsed.

.. changelog::
    :version: 0.9.0
    :released: Tue Aug 27 2013

    .. change::
        :tags: bug
        :tickets: 219

      The Context.locals_() method becomes a private underscored
      method, as this method has a specific internal use. The purpose
      of Context.kwargs has been clarified, in that it only delivers
      top level keyword arguments originally passed to template.render().

    .. change::
        :tags: bug
        :tickets:

      Fixed the babel plugin to properly interpret ${} sections
      inside of a "call" tag, i.e. <%self:some_tag attr="${_('foo')}"/>.
      Code that's subject to babel escapes in here needs to be
      specified as a Python expression, not a literal.  This change
      is backwards incompatible vs. code that is relying upon a _('')
      translation to be working within a call tag.

    .. change::
        :tags: bug
        :tickets: 187

      The Babel plugin has been repaired to work on Python 3.

    .. change::
        :tags: bug
        :tickets: 207

      Using <%namespace import="*" module="somemodule"/> now
      skips over module elements that are not explcitly callable,
      avoiding TypeError when trying to produce partials.

    .. change::
        :tags: bug
        :tickets: 190

      Fixed Py3K bug where a "lambda" expression was not
      interpreted correctly within a template tag; also
      fixed in Py2.4.

0.8
===

.. changelog::
    :version: 0.8.1
    :released: Fri May 24 2013

    .. change::
        :tags: bug
        :tickets: 216

      Changed setup.py to skip installing markupsafe
      if Python version is < 2.6 or is between 3.0 and
      less than 3.3, as Markupsafe now only supports 2.6->2.X,
      3.3->3.X.

    .. change::
        :tags: bug
        :tickets: 214

      Fixed regression where "entity" filter wasn't
      converted for py3k properly (added tests.)

    .. change::
        :tags: bug
        :tickets: 212

      Fixed bug where mako-render script wasn't
      compatible with Py3k.

    .. change::
        :tags: bug
        :tickets: 213

      Cleaned up all the various deprecation/
      file warnings when running the tests under
      various Pythons with warnings turned on.

.. changelog::
    :version: 0.8.0
    :released: Wed Apr 10 2013

    .. change::
        :tags: feature
        :tickets:

      Performance improvement to the
      "legacy" HTML escape feature, used for XML
      escaping and when markupsafe isn't present,
      courtesy George Xie.

    .. change::
        :tags: bug
        :tickets: 209

      Fixed bug whereby an exception in Python 3
      against a module compiled to the filesystem would
      fail trying to produce a RichTraceback due to the
      content being in bytes.

    .. change::
        :tags: bug
        :tickets: 208

      Change default for compile()->reserved_names
      from tuple to frozenset, as this is expected to be
      a set by default.

    .. change::
        :tags: feature
        :tickets:

      Code has been reworked to support Python 2.4->
      Python 3.xx in place.  2to3 no longer needed.

    .. change::
        :tags: feature
        :tickets:

      Added lexer_cls argument to Template,
      TemplateLookup, allows alternate Lexer classes
      to be used.

    .. change::
        :tags: feature
        :tickets:

      Added future_imports parameter to Template
      and TemplateLookup, renders the __future__ header
      with desired capabilities at the top of the generated
      template module.  Courtesy Ben Trofatter.

0.7
===

.. changelog::
    :version: 0.7.3
    :released: Wed Nov 7 2012

    .. change::
        :tags: bug
        :tickets:

      legacy_html_escape function, used when
      Markupsafe isn't installed, was using an inline-compiled
      regexp which causes major slowdowns on Python 3.3;
      is now precompiled.

    .. change::
        :tags: bug
        :tickets: 201

      AST supporting now supports tuple-packed
      function arguments inside pure-python def
      or lambda expressions.

    .. change::
        :tags: bug
        :tickets:

      Fixed Py3K bug in the Babel extension.

    .. change::
        :tags: bug
        :tickets:

      Fixed the "filter" attribute of the
      <%text> tag so that it pulls locally specified
      identifiers from the context the same
      way as that of <%block> and <%filter>.

    .. change::
        :tags: bug
        :tickets:

      Fixed bug in plugin loader to correctly
      raise exception when non-existent plugin
      is specified.

.. changelog::
    :version: 0.7.2
    :released: Fri Jul 20 2012

    .. change::
        :tags: bug
        :tickets: 193

      Fixed regression in 0.7.1 where AST
      parsing for Py2.4 was broken.

.. changelog::
    :version: 0.7.1
    :released: Sun Jul 8 2012

    .. change::
        :tags: feature
        :tickets: 146

      Control lines with no bodies will
      now succeed, as "pass" is added for these
      when no statements are otherwise present.
      Courtesy Ben Trofatter

    .. change::
        :tags: bug
        :tickets: 192

      Fixed some long-broken scoping behavior
      involving variables declared in defs and such,
      which only became apparent when
      the strict_undefined flag was turned on.

    .. change::
        :tags: bug
        :tickets: 191

      Can now use strict_undefined at the
      same time args passed to def() are used
      by other elements of the <%def> tag.

.. changelog::
    :version: 0.7.0
    :released: Fri Mar 30 2012

    .. change::
        :tags: feature
        :tickets: 125

      Added new "loop" variable to templates,
      is provided within a % for block to provide
      info about the loop such as index, first/last,
      odd/even, etc.  A migration path is also provided
      for legacy templates via the "enable_loop" argument
      available on Template, TemplateLookup, and <%page>.
      Thanks to Ben Trofatter for all
      the work on this

    .. change::
        :tags: feature
        :tickets:

      Added a real check for "reserved"
      names, that is names which are never pulled
      from the context and cannot be passed to
      the template.render() method.  Current names
      are "context", "loop", "UNDEFINED".

    .. change::
        :tags: feature
        :tickets: 95

      The html_error_template() will now
      apply Pygments highlighting to the source
      code displayed in the traceback, if Pygments
      if available.  Courtesy Ben Trofatter

    .. change::
        :tags: feature
        :tickets: 147

      Added support for context managers,
      i.e. "% with x as e:/ % endwith" support.
      Courtesy Ben Trofatter

    .. change::
        :tags: feature
        :tickets: 185

      Added class-level flag to CacheImpl
      "pass_context"; when True, the keyword argument
      'context' will be passed to get_or_create()
      containing the Mako Context object.

    .. change::
        :tags: bug
        :tickets: 182

      Fixed some Py3K resource warnings due
      to filehandles being implicitly closed.

    .. change::
        :tags: bug
        :tickets: 186

      Fixed endless recursion bug when
      nesting multiple def-calls with content.
      Thanks to Jeff Dairiki.

    .. change::
        :tags: feature
        :tickets:

      Added Jinja2 to the example
      benchmark suite, courtesy Vincent Férotin

Older Versions
==============

.. changelog::
    :version: 0.6.2
    :released: Thu Feb 2 2012

    .. change::
        :tags: bug
        :tickets: 86, 20

      The ${{"foo":"bar"}} parsing issue is fixed!!
      The legendary Eevee has slain the dragon!.  Also fixes quoting issue
      at.

.. changelog::
    :version: 0.6.1
    :released: Sat Jan 28 2012

    .. change::
        :tags: bug
        :tickets:

      Added special compatibility for the 0.5.0
      Cache() constructor, which was preventing file
      version checks and not allowing Mako 0.6 to
      recompile the module files.

.. changelog::
    :version: 0.6.0
    :released: Sat Jan 21 2012

    .. change::
        :tags: feature
        :tickets:

      Template caching has been converted into a plugin
      system, whereby the usage of Beaker is just the
      default plugin.   Template and TemplateLookup
      now accept a string "cache_impl" parameter which
      refers to the name of a cache plugin, defaulting
      to the name 'beaker'.  New plugins can be
      registered as pkg_resources entrypoints under
      the group "mako.cache", or registered directly
      using mako.cache.register_plugin().  The
      core plugin is the mako.cache.CacheImpl
      class.

    .. change::
        :tags: feature
        :tickets:

      Added support for Beaker cache regions
      in templates.   Usage of regions should be considered
      as superseding the very obsolete idea of passing in
      backend options, timeouts, etc. within templates.

    .. change::
        :tags: feature
        :tickets:

      The 'put' method on Cache is now
      'set'.  'put' is there for backwards compatibility.

    .. change::
        :tags: feature
        :tickets:

      The <%def>, <%block> and <%page> tags now accept
      any argument named "cache_*", and the key
      minus the "cache_" prefix will be passed as keyword
      arguments to the CacheImpl methods.

    .. change::
        :tags: feature
        :tickets:

      Template and TemplateLookup now accept an argument
      cache_args, which refers to a dictionary containing
      cache parameters.  The cache_dir, cache_url, cache_type,
      cache_timeout arguments are deprecated (will probably
      never be removed, however) and can be passed
      now as cache_args={'url':<some url>, 'type':'memcached',
      'timeout':50, 'dir':'/path/to/some/directory'}

    .. change::
        :tags: feature/bug
        :tickets: 180

      Can now refer to context variables
      within extra arguments to <%block>, <%def>, i.e.
      <%block name="foo" cache_key="${somekey}">.
      Filters can also be used in this way, i.e.
      <%def name="foo()" filter="myfilter">
      then template.render(myfilter=some_callable)

    .. change::
        :tags: feature
        :tickets: 178

      Added "--var name=value" option to the mako-render
      script, allows passing of kw to the template from
      the command line.

    .. change::
        :tags: feature
        :tickets: 181

      Added module_writer argument to Template,
      TemplateLookup, allows a callable to be passed which
      takes over the writing of the template's module source
      file, so that special environment-specific steps
      can be taken.

    .. change::
        :tags: bug
        :tickets: 142

      The exception message in the html_error_template
      is now escaped with the HTML filter.

    .. change::
        :tags: bug
        :tickets: 173

      Added "white-space:pre" style to html_error_template()
      for code blocks so that indentation is preserved

    .. change::
        :tags: bug
        :tickets: 175

      The "benchmark" example is now Python 3 compatible
      (even though several of those old template libs aren't
      available on Py3K, so YMMV)


.. changelog::
    :version: 0.5.0
    :released: Wed Sep 28 2011

    .. change::
        :tags:
        :tickets: 174

      A Template is explicitly disallowed
      from having a url that normalizes to relative outside
      of the root.   That is, if the Lookup is based
      at /home/mytemplates, an include that would place
      the ultimate template at
      /home/mytemplates/../some_other_directory,
      i.e. outside of /home/mytemplates,
      is disallowed.   This usage was never intended
      despite the lack of an explicit check.
      The main issue this causes
      is that module files can be written outside
      of the module root (or raise an error, if file perms aren't
      set up), and can also lead to the same template being
      cached in the lookup under multiple, relative roots.
      TemplateLookup instead has always supported multiple
      file roots for this purpose.


.. changelog::
    :version: 0.4.2
    :released: Fri Aug 5 2011

    .. change::
        :tags:
        :tickets: 170

      Fixed bug regarding <%call>/def calls w/ content
      whereby the identity of the "caller" callable
      inside the <%def> would be corrupted by the
      presence of another <%call> in the same block.

    .. change::
        :tags:
        :tickets: 169

      Fixed the babel plugin to accommodate <%block>

.. changelog::
    :version: 0.4.1
    :released: Wed Apr 6 2011

    .. change::
        :tags:
        :tickets: 164

      New tag: <%block>.  A variant on <%def> that
      evaluates its contents in-place.
      Can be named or anonymous,
      the named version is intended for inheritance
      layouts where any given section can be
      surrounded by the <%block> tag in order for
      it to become overrideable by inheriting
      templates, without the need to specify a
      top-level <%def> plus explicit call.
      Modified scoping and argument rules as well as a
      more strictly enforced usage scheme make it ideal
      for this purpose without at all replacing most
      other things that defs are still good for.
      Lots of new docs.

    .. change::
        :tags:
        :tickets: 165

      a slight adjustment to the "highlight" logic
      for generating template bound stacktraces.
      Will stick to known template source lines
      without any extra guessing.

.. changelog::
    :version: 0.4.0
    :released: Sun Mar 6 2011

    .. change::
        :tags:
        :tickets:

      A 20% speedup for a basic two-page
      inheritance setup rendering
      a table of escaped data
      (see http://techspot.zzzeek.org/2010/11/19/quick-mako-vs.-jinja-speed-test/).
      A few configurational changes which
      affect those in the I-don't-do-unicode
      camp should be noted below.

    .. change::
        :tags:
        :tickets:

      The FastEncodingBuffer is now used
      by default instead of cStringIO or StringIO,
      regardless of whether output_encoding
      is set to None or not.  FEB is faster than
      both.  Only StringIO allows bytestrings
      of unknown encoding to pass right
      through, however - while it is of course
      not recommended to send bytestrings of unknown
      encoding to the output stream, this
      mode of usage can be re-enabled by
      setting the flag bytestring_passthrough
      to True.

    .. change::
        :tags:
        :tickets:

      disable_unicode mode requires that
      output_encoding be set to None - it also
      forces the bytestring_passthrough flag
      to True.

    .. change::
        :tags:
        :tickets: 156

      the <%namespace> tag raises an error
      if the 'template' and 'module' attributes
      are specified at the same time in
      one tag.  A different class is used
      for each case which allows a reduction in
      runtime conditional logic and function
      call overhead.

    .. change::
        :tags:
        :tickets: 159

      the keys() in the Context, as well as
      it's internal _data dictionary, now
      include just what was specified to
      render() as well as Mako builtins
      'caller', 'capture'.  The contents
      of __builtin__ are no longer copied.
      Thanks to Daniel Lopez for pointing
      this out.


.. changelog::
    :version: 0.3.6
    :released: Sat Nov 13 2010

    .. change::
        :tags:
        :tickets: 126

      Documentation is on Sphinx.

    .. change::
        :tags:
        :tickets: 154

      Beaker is now part of "extras" in
      setup.py instead of "install_requires".
      This to produce a lighter weight install
      for those who don't use the caching
      as well as to conform to Pyramid
      deployment practices.

    .. change::
        :tags:
        :tickets: 153

      The Beaker import (or attempt thereof)
      is delayed until actually needed;
      this to remove the performance penalty
      from startup, particularly for
      "single execution" environments
      such as shell scripts.

    .. change::
        :tags:
        :tickets: 155

      Patch to lexer to not generate an empty
      '' write in the case of backslash-ended
      lines.

    .. change::
        :tags:
        :tickets: 148

      Fixed missing **extra collection in
      setup.py which prevented setup.py
      from running 2to3 on install.

    .. change::
        :tags:
        :tickets:

      New flag on Template, TemplateLookup -
      strict_undefined=True, will cause
      variables not found in the context to
      raise a NameError immediately, instead of
      defaulting to the UNDEFINED value.

    .. change::
        :tags:
        :tickets:

      The range of Python identifiers that
      are considered "undefined", meaning they
      are pulled from the context, has been
      trimmed back to not include variables
      declared inside of expressions (i.e. from
      list comprehensions), as well as
      in the argument list of lambdas.  This
      to better support the strict_undefined
      feature.  The change should be
      fully backwards-compatible but involved
      a little bit of tinkering in the AST code,
      which hadn't really been touched for
      a couple of years, just FYI.

.. changelog::
    :version: 0.3.5
    :released: Sun Oct 24 2010

    .. change::
        :tags:
        :tickets: 141

      The <%namespace> tag allows expressions
      for the `file` argument, i.e. with ${}.
      The `context` variable, if needed,
      must be referenced explicitly.

    .. change::
        :tags:
        :tickets:

      ${} expressions embedded in tags,
      such as <%foo:bar x="${...}">, now
      allow multiline Python expressions.

    .. change::
        :tags:
        :tickets:

      Fixed previously non-covered regular
      expression, such that using a ${} expression
      inside of a tag element that doesn't allow
      them raises a CompileException instead of
      silently failing.

    .. change::
        :tags:
        :tickets: 151

      Added a try/except around "import markupsafe".
      This to support GAE which can't run markupsafe. No idea whatsoever if the
      install_requires in setup.py also breaks GAE,
      couldn't get an answer on this.

.. changelog::
    :version: 0.3.4
    :released: Tue Jun 22 2010

    .. change::
        :tags:
        :tickets:

      Now using MarkupSafe for HTML escaping,
      i.e. in place of cgi.escape().  Faster
      C-based implementation and also escapes
      single quotes for additional security.
      Supports the __html__ attribute for
      the given expression as well.

      When using "disable_unicode" mode,
      a pure Python HTML escaper function
      is used which also quotes single quotes.

      Note that Pylons by default doesn't
      use Mako's filter - check your
      environment.py file.

    .. change::
        :tags:
        :tickets: 137

      Fixed call to "unicode.strip" in
      exceptions.text_error_template which
      is not Py3k compatible.

.. changelog::
    :version: 0.3.3
    :released: Mon May 31 2010

    .. change::
        :tags:
        :tickets: 135

      Added conditional to RichTraceback
      such that if no traceback is passed
      and sys.exc_info() has been reset,
      the formatter just returns blank
      for the "traceback" portion.

    .. change::
        :tags:
        :tickets: 131

      Fixed sometimes incorrect usage of
      exc.__class__.__name__
      in html/text error templates when using
      Python 2.4

    .. change::
        :tags:
        :tickets:

      Fixed broken @property decorator on
      template.last_modified

    .. change::
        :tags:
        :tickets: 132

      Fixed error formatting when a stacktrace
      line contains no line number, as in when
      inside an eval/exec-generated function.

    .. change::
        :tags:
        :tickets:

      When a .py is being created, the tempfile
      where the source is stored temporarily is
      now made in the same directory as that of
      the .py file.  This ensures that the two
      files share the same filesystem, thus
      avoiding cross-filesystem synchronization
      issues.  Thanks to Charles Cazabon.

.. changelog::
    :version: 0.3.2
    :released: Thu Mar 11 2010

    .. change::
        :tags:
        :tickets: 116

      Calling a def from the top, via
      template.get_def(...).render() now checks the
      argument signature the same way as it did in
      0.2.5, so that TypeError is not raised.
      reopen of

.. changelog::
    :version: 0.3.1
    :released: Sun Mar 7 2010

    .. change::
        :tags:
        :tickets: 129

      Fixed incorrect dir name in setup.py

.. changelog::
    :version: 0.3.0
    :released: Fri Mar 5 2010

    .. change::
        :tags:
        :tickets: 123

      Python 2.3 support is dropped.

    .. change::
        :tags:
        :tickets: 119

      Python 3 support is added ! See README.py3k
      for installation and testing notes.

    .. change::
        :tags:
        :tickets: 127

      Unit tests now run with nose.

    .. change::
        :tags:
        :tickets: 99

      Source code escaping has been simplified.
      In particular, module source files are now
      generated with the Python "magic encoding
      comment", and source code is passed through
      mostly unescaped, except for that code which
      is regenerated from parsed Python source.
      This fixes usage of unicode in
      <%namespace:defname> tags.

    .. change::
        :tags:
        :tickets: 122

      RichTraceback(), html_error_template().render(),
      text_error_template().render() now accept "error"
      and "traceback" as optional arguments, and
      these are now actually used.

    .. change::
        :tags:
        :tickets:

      The exception output generated when
      format_exceptions=True will now be as a Python
      unicode if it occurred during render_unicode(),
      or an encoded string if during render().

    .. change::
        :tags:
        :tickets: 112

      A percent sign can be emitted as the first
      non-whitespace character on a line by escaping
      it as in "%%".

    .. change::
        :tags:
        :tickets: 94

      Template accepts empty control structure, i.e.
      % if: %endif, etc.

    .. change::
        :tags:
        :tickets: 116

      The <%page args> tag can now be used in a base
      inheriting template - the full set of render()
      arguments are passed down through the inherits
      chain.  Undeclared arguments go into **pageargs
      as usual.

    .. change::
        :tags:
        :tickets: 109

      defs declared within a <%namespace> section, an
      uncommon feature, have been improved.  The defs
      no longer get doubly-rendered in the body() scope,
      and now allow local variable assignment without
      breakage.

    .. change::
        :tags:
        :tickets: 128

      Windows paths are handled correctly if a Template
      is passed only an absolute filename (i.e. with c:
      drive etc.)  and no URI - the URI is converted
      to a forward-slash path and module_directory
      is treated as a windows path.

    .. change::
        :tags:
        :tickets: 73

      TemplateLookup raises TopLevelLookupException for
      a given path that is a directory, not a filename,
      instead of passing through to the template to
      generate IOError.


.. changelog::
    :version: 0.2.6
    :released:

    .. change::
        :tags:
        :tickets:

      Fix mako function decorators to preserve the
      original function's name in all cases. Patch
      from Scott Torborg.

    .. change::
        :tags:
        :tickets: 118

      Support the <%namespacename:defname> syntax in
      the babel extractor.

    .. change::
        :tags:
        :tickets: 88

      Further fixes to unicode handling of .py files with the
      html_error_template.

.. changelog::
    :version: 0.2.5
    :released: Mon Sep  7 2009

    .. change::
        :tags:
        :tickets:

      Added a "decorator" kw argument to <%def>,
      allows custom decoration functions to wrap
      rendering callables.  Mainly intended for
      custom caching algorithms, not sure what
      other uses there may be (but there may be).
      Examples are in the "filtering" docs.

    .. change::
        :tags:
        :tickets: 101

      When Mako creates subdirectories in which
      to store templates, it uses the more
      permissive mode of 0775 instead of 0750,
      helping out with certain multi-process
      scenarios. Note that the mode is always
      subject to the restrictions of the existing
      umask.

    .. change::
        :tags:
        :tickets: 104

      Fixed namespace.__getattr__() to raise
      AttributeError on attribute not found
      instead of RuntimeError.

    .. change::
        :tags:
        :tickets: 97

      Added last_modified accessor to Template,
      returns the time.time() when the module
      was created.

    .. change::
        :tags:
        :tickets: 102

      Fixed lexing support for whitespace
      around '=' sign in defs.

    .. change::
        :tags:
        :tickets: 108

      Removed errant "lower()" in the lexer which
      was causing tags to compile with
      case-insensitive names, thus messing up
      custom <%call> names.

    .. change::
        :tags:
        :tickets: 110

      added "mako.__version__" attribute to
      the base module.

.. changelog::
    :version: 0.2.4
    :released: Tue Dec 23 2008

    .. change::
        :tags:
        :tickets:

      Fixed compatibility with Jython 2.5b1.

.. changelog::
    :version: 0.2.3
    :released: Sun Nov 23 2008

    .. change::
        :tags:
        :tickets:

      the <%namespacename:defname> syntax described at
      http://techspot.zzzeek.org/?p=28 has now
      been added as a built in syntax, and is recommended
      as a more modern syntax versus <%call expr="expression">.
      The %call tag itself will always remain,
      with <%namespacename:defname> presenting a more HTML-like
      alternative to calling defs, both plain and
      nested.  Many examples of the new syntax are in the
      "Calling a def with embedded content" section
      of the docs.

    .. change::
        :tags:
        :tickets:

      added support for Jython 2.5.

    .. change::
        :tags:
        :tickets:

      cache module now uses Beaker's CacheManager
      object directly, so that all cache types are included.
      memcached is available as both "ext:memcached" and
      "memcached", the latter for backwards compatibility.

    .. change::
        :tags:
        :tickets:

      added "cache" accessor to Template, Namespace.
      e.g.  ${local.cache.get('somekey')} or
      template.cache.invalidate_body()

    .. change::
        :tags:
        :tickets:

      added "cache_enabled=True" flag to Template,
      TemplateLookup.  Setting this to False causes cache
      operations to "pass through" and execute every time;
      this flag should be integrated in Pylons with its own
      cache_enabled configuration setting.

    .. change::
        :tags:
        :tickets: 92

      the Cache object now supports invalidate_def(name),
      invalidate_body(), invalidate_closure(name),
      invalidate(key), which will remove the given key
      from the cache, if it exists.  The cache arguments
      (i.e. storage type) are derived from whatever has
      been already persisted for that template.

    .. change::
        :tags:
        :tickets:

      For cache changes to work fully, Beaker 1.1 is required.
      1.0.1 and up will work as well with the exception of
      cache expiry.  Note that Beaker 1.1 is **required**
      for applications which use dynamically generated keys,
      since previous versions will permanently store state in memory
      for each individual key, thus consuming all available
      memory for an arbitrarily large number of distinct
      keys.

    .. change::
        :tags:
        :tickets: 93

      fixed bug whereby an <%included> template with
      <%page> args named the same as a __builtin__ would not
      honor the default value specified in <%page>

    .. change::
        :tags:
        :tickets: 88

      fixed the html_error_template not handling tracebacks from
      normal .py files with a magic encoding comment

    .. change::
        :tags:
        :tickets:

      RichTraceback() now accepts an optional traceback object
      to be used in place of sys.exc_info()[2].  html_error_template()
      and text_error_template() accept an optional
      render()-time argument "traceback" which is passed to the
      RichTraceback object.

    .. change::
        :tags:
        :tickets:

      added ModuleTemplate class, which allows the construction
      of a Template given a Python module generated by a previous
      Template.   This allows Python modules alone to be used
      as templates with no compilation step.   Source code
      and template source are optional but allow error reporting
      to work correctly.

    .. change::
        :tags:
        :tickets: 90

      fixed Python 2.3 compat. in mako.pyparser

    .. change::
        :tags:
        :tickets:

      fix Babel 0.9.3 compatibility; stripping comment tags is now
      optional (and enabled by default).

.. changelog::
    :version: 0.2.2
    :released: Mon Jun 23 2008

    .. change::
        :tags:
        :tickets: 87

      cached blocks now use the current context when rendering
      an expired section, instead of the original context
      passed in

    .. change::
        :tags:
        :tickets:

      fixed a critical issue regarding caching, whereby
      a cached block would raise an error when called within a
      cache-refresh operation that was initiated after the
      initiating template had completed rendering.

.. changelog::
    :version: 0.2.1
    :released: Mon Jun 16 2008

    .. change::
        :tags:
        :tickets:

      fixed bug where 'output_encoding' parameter would prevent
      render_unicode() from returning a unicode object.

    .. change::
        :tags:
        :tickets:

      bumped magic number, which forces template recompile for
      this version (fixes incompatible compile symbols from 0.1
      series).

    .. change::
        :tags:
        :tickets:

      added a few docs for cache options, specifically those that
      help with memcached.

.. changelog::
    :version: 0.2.0
    :released: Tue Jun  3 2008

    .. change::
        :tags:
        :tickets:

      Speed improvements (as though we needed them, but people
      contributed and there you go):

    .. change::
        :tags:
        :tickets: 77

      added "bytestring passthru" mode, via
      `disable_unicode=True` argument passed to Template or
      TemplateLookup. All unicode-awareness and filtering is
      turned off, and template modules are generated with
      the appropriate magic encoding comment. In this mode,
      template expressions can only receive raw bytestrings
      or Unicode objects which represent straight ASCII, and
      render_unicode() may not be used if multibyte
      characters are present. When enabled, speed
      improvement around 10-20%. (courtesy
      anonymous guest)

    .. change::
        :tags:
        :tickets: 76

      inlined the "write" function of Context into a local
      template variable. This affords a 12-30% speedup in
      template render time. (idea courtesy same anonymous
      guest)

    .. change::
        :tags:
        :tickets:

      New Features, API changes:

    .. change::
        :tags:
        :tickets: 62

      added "attr" accessor to namespaces. Returns
      attributes configured as module level attributes, i.e.
      within <%! %> sections.  i.e.:

      # somefile.html
      <%!
          foo = 27
      %>

      # some other template
      <%namespace name="myns" file="somefile.html"/>
      ${myns.attr.foo}

      The slight backwards incompatibility here is, you
      can't have namespace defs named "attr" since the
      "attr" descriptor will occlude it.

    .. change::
        :tags:
        :tickets: 78

      cache_key argument can now render arguments passed
      directly to the %page or %def, i.e. <%def
      name="foo(x)" cached="True" cache_key="${x}"/>

    .. change::
        :tags:
        :tickets:

      some functions on Context are now private:
      _push_buffer(), _pop_buffer(),
      caller_stack._push_frame(), caller_stack._pop_frame().

    .. change::
        :tags:
        :tickets: 56, 81

      added a runner script "mako-render" which renders
      standard input as a template to stdout

    .. change::
        :tags: bugfixes
        :tickets: 83, 84

      can now use most names from __builtins__ as variable
      names without explicit declaration (i.e. 'id',
      'exception', 'range', etc.)

    .. change::
        :tags: bugfixes
        :tickets: 84

      can also use builtin names as local variable names
      (i.e. dict, locals) (came from fix for)

    .. change::
        :tags: bugfixes
        :tickets: 68

      fixed bug in python generation when variable names are
      used with identifiers like "else", "finally", etc.
      inside them

    .. change::
        :tags: bugfixes
        :tickets: 69

      fixed codegen bug which occurred when using <%page>
      level caching, combined with an expression-based
      cache_key, combined with the usage of <%namespace
      import="*"/> - fixed lexer exceptions not cleaning up
      temporary files, which could lead to a maximum number
      of file descriptors used in the process

    .. change::
        :tags: bugfixes
        :tickets: 71

      fixed issue with inline format_exceptions that was
      producing blank exception pages when an inheriting
      template is present

    .. change::
        :tags: bugfixes
        :tickets:

      format_exceptions will apply the encoding options of
      html_error_template() to the buffered output

    .. change::
        :tags: bugfixes
        :tickets: 75

      rewrote the "whitespace adjuster" function to work
      with more elaborate combinations of quotes and
      comments


.. changelog::
    :version: 0.1.10
    :released:

    .. change::
        :tags:
        :tickets:

      fixed propagation of 'caller' such that nested %def calls
      within a <%call> tag's argument list propigates 'caller'
      to the %call function itself (propigates to the inner
      calls too, this is a slight side effect which previously
      existed anyway)

    .. change::
        :tags:
        :tickets:

      fixed bug where local.get_namespace() could put an
      incorrect "self" in the current context

    .. change::
        :tags:
        :tickets:

      fixed another namespace bug where the namespace functions
      did not have access to the correct context containing
      their 'self' and 'parent'

.. changelog::
    :version: 0.1.9
    :released:

    .. change::
        :tags:
        :tickets: 47

      filters.Decode filter can also accept a non-basestring
      object and will call str() + unicode() on it

    .. change::
        :tags:
        :tickets: 53

      comments can be placed at the end of control lines,
      i.e. if foo: # a comment,, thanks to
      Paul Colomiets

    .. change::
        :tags:
        :tickets: 16

      fixed expressions and page tag arguments and with embedded
      newlines in CRLF templates, follow up to, thanks
      Eric Woroshow

    .. change::
        :tags:
        :tickets: 51

      added an IOError catch for source file not found in RichTraceback
      exception reporter

.. changelog::
    :version: 0.1.8
    :released: Tue Jun 26 2007

    .. change::
        :tags:
        :tickets:

      variable names declared in render methods by internal
      codegen prefixed by "__M_" to prevent name collisions
      with user code

    .. change::
        :tags:
        :tickets: 45

      added a Babel (http://babel.edgewall.org/) extractor entry
      point, allowing extraction of gettext messages directly from
      mako templates via Babel

    .. change::
        :tags:
        :tickets:

      fix to turbogears plugin to work with dot-separated names
      (i.e. load_template('foo.bar')).  also takes file extension
      as a keyword argument (default is 'mak').

    .. change::
        :tags:
        :tickets: 35

      more tg fix:  fixed, allowing string-based
      templates with tgplugin even if non-compatible args were sent

.. changelog::
    :version: 0.1.7
    :released: Wed Jun 13 2007

    .. change::
        :tags:
        :tickets:

      one small fix to the unit tests to support python 2.3

    .. change::
        :tags:
        :tickets:

      a slight hack to how cache.py detects Beaker's memcached,
      works around unexplained import behavior observed on some
      python 2.3 installations

.. changelog::
    :version: 0.1.6
    :released: Fri May 18 2007

    .. change::
        :tags:
        :tickets:

      caching is now supplied directly by Beaker, which has
      all of MyghtyUtils merged into it now.  The latest Beaker
      (0.7.1) also fixes a bug related to how Mako was using the
      cache API.

    .. change::
        :tags:
        :tickets: 34

      fix to module_directory path generation when the path is "./"

    .. change::
        :tags:
        :tickets: 35

      TGPlugin passes options to string-based templates

    .. change::
        :tags:
        :tickets: 28

      added an explicit stack frame step to template runtime, which
      allows much simpler and hopefully bug-free tracking of 'caller',
      fixes

    .. change::
        :tags:
        :tickets:

      if plain Python defs are used with <%call>, a decorator
      @runtime.supports_callable exists to ensure that the "caller"
      stack is properly handled for the def.

    .. change::
        :tags:
        :tickets: 37

      fix to RichTraceback and exception reporting to get template
      source code as a unicode object

    .. change::
        :tags:
        :tickets: 39

      html_error_template includes options "full=True", "css=True"
      which control generation of HTML tags, CSS

    .. change::
        :tags:
        :tickets: 40

      added the 'encoding_errors' parameter to Template/TemplateLookup
      for specifying the error handler associated with encoding to
      'output_encoding'

    .. change::
        :tags:
        :tickets: 37

      the Template returned by html_error_template now defaults to
      output_encoding=sys.getdefaultencoding(),
      encoding_errors='htmlentityreplace'

    .. change::
        :tags:
        :tickets:

      control lines, i.e. % lines, support backslashes to continue long
      lines (#32)

    .. change::
        :tags:
        :tickets:

      fixed codegen bug when defining <%def> within <%call> within <%call>

    .. change::
        :tags:
        :tickets:

      leading utf-8 BOM in template files is honored according to pep-0263

.. changelog::
    :version: 0.1.5
    :released: Sat Mar 31 2007

    .. change::
        :tags:
        :tickets: 26

      AST expression generation - added in just about everything
      expression-wise from the AST module

    .. change::
        :tags:
        :tickets: 27

      AST parsing, properly detects imports of the form "import foo.bar"

    .. change::
        :tags:
        :tickets:

      fix to lexing of <%docs> tag nested in other tags

    .. change::
        :tags:
        :tickets: 29

      fix to context-arguments inside of <%include> tag which broke
      during 0.1.4

    .. change::
        :tags:
        :tickets:

      added "n" filter, disables *all* filters normally applied to an expression
      via <%page> or default_filters (but not those within the filter)

    .. change::
        :tags:
        :tickets:

      added buffer_filters argument, defines filters applied to the return value
      of buffered/cached/filtered %defs, after all filters defined with the %def
      itself have been applied.  allows the creation of default expression filters
      that let the output of return-valued %defs "opt out" of that filtering
      via passing special attributes or objects.

.. changelog::
    :version: 0.1.4
    :released: Sat Mar 10 2007

    .. change::
        :tags:
        :tickets:

      got defs-within-defs to be cacheable

    .. change::
        :tags:
        :tickets: 23

      fixes to code parsing/whitespace adjusting where plain python comments
      may contain quote characters

    .. change::
        :tags:
        :tickets:

      fix to variable scoping for identifiers only referenced within
      functions

    .. change::
        :tags:
        :tickets:

      added a path normalization step to lookup so URIs like
      "/foo/bar/../etc/../foo" pre-process the ".." tokens before checking
      the filesystem

    .. change::
        :tags:
        :tickets:

      fixed/improved "caller" semantics so that undefined caller is
      "UNDEFINED", propigates __nonzero__ method so it evaulates to False if
      not present, True otherwise. this way you can say % if caller:\n
      ${caller.body()}\n% endif

    .. change::
        :tags:
        :tickets:

      <%include> has an "args" attribute that can pass arguments to the
      called template (keyword arguments only, must be declared in that
      page's <%page> tag.)

    .. change::
        :tags:
        :tickets:

      <%include> plus arguments is also programmatically available via
      self.include_file(<filename>, **kwargs)

    .. change::
        :tags:
        :tickets: 24

      further escaping added for multibyte expressions in %def, %call
      attributes

.. changelog::
    :version: 0.1.3
    :released: Wed Feb 21 2007

    .. change::
        :tags:
        :tickets:

      ***Small Syntax Change*** - the single line comment character is now
      *two* hash signs, i.e. "## this is a comment".  This avoids a common
      collection with CSS selectors.

    .. change::
        :tags:
        :tickets:

      the magic "coding" comment (i.e. # coding:utf-8) will still work with
      either one "#" sign or two for now; two is preferred going forward, i.e.
      ## coding:<someencoding>.

    .. change::
        :tags:
        :tickets:

      new multiline comment form: "<%doc> a comment </%doc>"

    .. change::
        :tags:
        :tickets:

      UNDEFINED evaluates to False

    .. change::
        :tags:
        :tickets:

      improvement to scoping of "caller" variable when using <%call> tag

    .. change::
        :tags:
        :tickets:

      added lexer error for unclosed control-line (%) line

    .. change::
        :tags:
        :tickets:

      added "preprocessor" argument to Template, TemplateLookup - is a single
      callable or list of callables which will be applied to the template text
      before lexing.  given the text as an argument, returns the new text.

    .. change::
        :tags:
        :tickets:

      added mako.ext.preprocessors package, contains one preprocessor so far:
      'convert_comments', which will convert single # comments to the new ##
      format

.. changelog::
    :version: 0.1.2
    :released: Thu Feb  1 2007

    .. change::
        :tags:
        :tickets: 11

      fix to parsing of code/expression blocks to insure that non-ascii
      characters, combined with a template that indicates a non-standard
      encoding, are expanded into backslash-escaped glyphs before being AST
      parsed

    .. change::
        :tags:
        :tickets:

      all template lexing converts the template to unicode first, to
      immediately catch any encoding issues and ensure internal unicode
      representation.

    .. change::
        :tags:
        :tickets:

      added module_filename argument to Template to allow specification of a
      specific module file

    .. change::
        :tags:
        :tickets: 14

      added modulename_callable to TemplateLookup to allow a function to
      determine module filenames (takes filename, uri arguments). used for

    .. change::
        :tags:
        :tickets:

      added optional input_encoding flag to Template, to allow sending a
      unicode() object with no magic encoding comment

    .. change::
        :tags:
        :tickets:

      "expression_filter" argument in <%page> applies only to expressions

    .. change::
        :tags: "unicode"
        :tickets:

      added "default_filters" argument to Template, TemplateLookup. applies only
      to expressions, gets prepended to "expression_filter" arg from <%page>.
      defaults to, so that all expressions get stringified into u''
      by default (this is what Mako already does). By setting to [], expressions
      are passed through raw.

    .. change::
        :tags:
        :tickets:

      added "imports" argument to Template, TemplateLookup. so you can predefine
      a list of import statements at the top of the template. can be used in
      conjunction with default_filters.

    .. change::
        :tags:
        :tickets: 16

      support for CRLF templates...whoops ! welcome to all the windows users.

    .. change::
        :tags:
        :tickets:

      small fix to local variable propigation for locals that are conditionally
      declared

    .. change::
        :tags:
        :tickets:

      got "top level" def calls to work, i.e. template.get_def("somedef").render()

.. changelog::
    :version: 0.1.1
    :released: Sun Jan 14 2007

    .. change::
        :tags:
        :tickets: 8

      buffet plugin supports string-based templates, allows ToscaWidgets to work

    .. change::
        :tags:
        :tickets:

      AST parsing fixes: fixed TryExcept identifier parsing

    .. change::
        :tags:
        :tickets:

      removed textmate tmbundle from contrib and into separate SVN location;
      windows users cant handle those files, setuptools not very good at
      "pruning" certain directories

    .. change::
        :tags:
        :tickets:

      fix so that "cache_timeout" parameter is propigated

    .. change::
        :tags:
        :tickets:

      fix to expression filters so that string conversion (actually unicode)
      properly occurs before filtering

    .. change::
        :tags:
        :tickets:

      better error message when a lookup is attempted with a template that has no
      lookup

    .. change::
        :tags:
        :tickets:

      implemented "module" attribute for namespace

    .. change::
        :tags:
        :tickets:

      fix to code generation to correctly track multiple defs with the same name

    .. change::
        :tags:
        :tickets: 9

      "directories" can be passed to TemplateLookup as a scalar in which case it
      gets converted to a list
