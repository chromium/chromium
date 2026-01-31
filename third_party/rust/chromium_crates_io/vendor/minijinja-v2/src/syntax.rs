#![cfg_attr(
    feature = "custom_syntax",
    doc = "Documents the syntax for templates and provides ways to reconfigure it."
)]
#![cfg_attr(
    not(feature = "custom_syntax"),
    doc = "Documents the syntax for templates."
)]
//!
//! <details><summary><strong style="cursor: pointer">Table of Contents</strong></summary>
//!
//! - [Synopsis](#synopsis)
//! - [Trailing Newlines](#trailing-newlines)
//! - [Expressions](#expressions)
//!   - [Literals](#literals)
//!   - [Math](#math)
//!   - [Comparisons](#comparisons)
//!   - [Logic](#logic)
//!   - [Other Operators](#other-operators)
//!   - [If Expressions](#if-expressions)
//! - [Tags](#tags)
//!   - [`{% for %}`](#-for-)
//!   - [`{% if %}`](#-if-)
//!   - [`{% extends %}`](#-extends-)
//!   - [`{% block %}`](#-block-)
//!   - [`{% include %}`](#-include-)
//!   - [`{% import %}`](#-import-)
//!   - [`{% with %}`](#-with-)
//!   - [`{% set %}`](#-set-)
//!   - [`{% filter %}`](#-filter-)
//!   - [`{% macro %}`](#-macro-)
//!   - [`{% call %}`](#-call-)
//!   - [`{% do %}`](#-do-)
//!   - [`{% autoescape %}`](#-autoescape-)
//!   - [`{% raw %}`](#-raw-)
//!   - [`{% break %} / {% continue %}`](#-break----continue-)
#![cfg_attr(
    feature = "custom_syntax",
    doc = "- [Custom Delimiters](#custom-delimiters)"
)]
//! - [Whitespace Control](#whitespace-control)
//!
//! </details>
//!
//! # Synopsis
//!
//! A MiniJinja template is simply a text file.  MiniJinja can generate any text-based
//! format (HTML, XML, CSV, LaTeX, etc.).  A template doesn’t need to have a specific extension
//! and in fact MiniJinja does not understand much about the file system.  However the default
//! configuration for [auto escaping](crate::Environment::set_auto_escape_callback) uses file
//! extensions to configure the initial behavior.
//!
//! A template contains [**expressions**](#expressions), which get replaced with values when a
//! template is rendered; and [**tags**](#tags), which control the logic of the template.  The
//! template syntax is heavily inspired by Jinja2, Django and Python.
//!
//! This is a minimal template that illustrates a few basics:
//!
//! ```jinja
//! <!doctype html>
//! <title>{% block title %}My Website{% endblock %}</title>
//! <ul id="navigation">
//! {% for item in navigation %}
//!     <li><a href="{{ item.href }}">{{ item.caption }}</a></li>
//! {% endfor %}
//! </ul>
//!
//! <h1>My Webpage</h1>
//! {% block body %}{% endblock %}
//!
//! {# a comment #}
//! ```
//!
//! # Trailing Newlines
//!
//! MiniJinja, like Jinja2, will remove one trailing newline from the end of the file automatically
//! on parsing.  This lets templates produce a consistent output no matter if the editor adds a
//! trailing newline or not.  If one wants a trailing newline an extra newline can be added or the
//! code rendering it adds it manually.
//!
//! # Expressions
//!
//! MiniJinja allows basic expressions everywhere. These work largely as you expect from Jinja2.
//! Even if you have not used Jinja2 you should feel comfortable with it.  To output the result
//! of an expression wrap it in `{{ .. }}`.
//!
//! ## Literals
//!
//! The simplest form of expressions are literals. Literals are representations for
//! objects such as strings and numbers. The following literals exist:
//!
//! - `"Hello World"`: Everything between two double or single quotes is a string. They are
//!   useful whenever you need a string in the template (e.g. as arguments to function calls
//!   and filters, or just to extend or include a template).
//! - `42`: Integers are whole numbers without a decimal part.  They can be prefixed with
//!   `0b` to indicate binary, `0o` to indicate octal and `0x` to indicate hexadecimal.
//!   Underscores are tolerated (and ignored) everywhere a digit is except in the last place.
//! - `42.0`: Floating point numbers can be written using a `.` as a decimal mark.
//!   Underscores are tolerated (and ignored) everywhere a digit is except in the last place.
//! - `['list', 'of', 'objects']`: Everything between two brackets is a list. Lists are useful
//!   for storing sequential data to be iterated over.
//!   for compatibility with Jinja2 `('list', 'of', 'objects')` is also allowed.
//! - `{'map': 'of', 'key': 'and', 'value': 'pairs'}`: A map is a structure that combines keys
//!   and values. Keys must be unique and always have exactly one value. Maps are rarely
//!   created in templates.
//! - `true` / `false` / `none`: boolean values and the special `none` value which maps to the
//!   unit type in Rust.
//!
//! ## Math
//!
//! MiniJinja allows you to calculate with values.  The following operators are supported:
//!
//! - ``+``: Adds two numbers up. ``{{ 1 + 1 }}`` is ``2``.
//! - ``-``: Subtract the second number from the first one.  ``{{ 3 - 2 }}`` is ``1``.
//! - ``/``: Divide two numbers. ``{{ 1 / 2 }}`` is ``0.5``.  See note on divisions below.
//! - ``//``: Integer divide two numbers. ``{{ 5 // 3 }}`` is ``1``.  See note on divisions below.
//! - ``%``: Calculate the remainder of an integer division.  ``{{ 11 % 7 }}`` is ``4``.
//! - ``*``: Multiply the left operand with the right one.  ``{{ 2 * 2 }}`` would return ``4``.
//! - ``**``: Raise the left operand to the power of the right operand.  ``{{ 2**3 }}``
//!   would return ``8``.
//!
//! Note on divisions: divisions in Jinja2 are flooring, divisions in MiniJinja
//! are at present using euclidean division.  They are almost the same but not quite.
//!
//! ## Comparisons
//!  
//! - ``==``: Compares two objects for equality.
//! - ``!=``: Compares two objects for inequality.
//! - ``>``: ``true`` if the left hand side is greater than the right hand side.
//! - ``>=``: ``true`` if the left hand side is greater or equal to the right hand side.
//! - ``<``:``true`` if the left hand side is less than the right hand side.
//! - ``<=``: ``true`` if the left hand side is less or equal to the right hand side.
//!
//! ## Logic
//!
//! For ``if`` statements it can be useful to combine multiple expressions:
//!
//! - ``and``: Return true if the left and the right operand are true.
//! - ``or``: Return true if the left or the right operand are true.
//! - ``not``: negate a statement (see below).
//! - ``(expr)``: Parentheses group an expression.
//!
//! ## Other Operators
//!
//! The following operators are very useful but don't fit into any of the other
//! two categories:
//!
//! - ``is``/``is not``: Performs a [test](crate::tests).
//! - ``in``/``not in``: Performs a containment check.
//! - ``|`` (pipe, vertical bar): Applies a [filter](crate::filters).
//! - ``~`` (tilde): Converts all operands into strings and concatenates them.
//!   ``{{ "Hello " ~ name ~ "!" }}`` would return (assuming `name` is set
//!   to ``'John'``) ``Hello John!``.
//! - ``()``: Call a callable: ``{{ super() }}``.  Inside of the parentheses you
//!   can use positional arguments.  Additionally keyword arguments are supported
//!   which are treated like a dict syntax.  Eg: `foo(a=1, b=2)` is the same as
//!   `foo({"a": 1, "b": 2})`.
//! - ``.`` / ``[]``: Get an attribute of an object.  If an object does not have a specific
//!   attribute or item then `undefined` is returned.  Accessing a property of an already
//!   undefined value will result in an error.
//! - ``[start:stop]`` / ``[start:stop:step]``: slices a list or string.  All three expressions
//!   are optional (`start`, `stop`, `step`).  For instance ``"Hello World"[:5]`` will return
//!   just `"Hello"`.  Likewise ``"Hello"[1:-1]`` will return `"ell"`.  The step component can
//!   be used to change the step size.  `"12345"[::2]` will return `"135"`.
//!
//! ### If Expressions
//!
//! It is also possible to use inline _if_ expressions. These are useful in some situations.
//! For example, you can use this to extend from one template if a variable is defined,
//! otherwise from the default layout template:
//!
//! ```jinja
//! {% extends layout_template if layout_template is defined else 'default.html' %}
//! ```
//!
//! The `else` part is optional. If not provided, the else block implicitly evaluates
//! into an undefined value:
//!
//! ```jinja
//! {{ title|upper if title }}
//! ```
//!
//! Note that for compatibility with Jinja2, when the `else` block is missing the undefined
//! value will be marked as "silent".  This means even if strict undefined behavior is
//! requested, this undefined value will print to an empty string.  This means
//! that this is always valid:
//!
//! ```jinja
//! {{ value if false }} -> prints an empty string (silent undefined returned from else)
//! ```
//!
//! # Tags
//!
//! Tags control logic in templates.  The following tags exist:
//!
//! ## `{% for %}`
//!
//! The for tag loops over each item in a sequence.  For example, to display a list
//! of users provided in a variable called `users`:
//!
//! ```jinja
//! <h1>Members</h1>
//! <ul>
//! {% for user in users %}
//!   <li>{{ user.username }}</li>
//! {% endfor %}
//! </ul>
//! ```
//!
//! It's also possible to unpack tuples while iterating:
//!
//! ```jinja
//! <h1>Members</h1>
//! <ul>
//! {% for (key, value) in list_of_pairs %}
//!   <li>{{ key }}: {{ value }}</li>
//! {% endfor %}
//! </ul>
//! ```
//!
//! Inside of the for block you can access some special variables:
//!
//! - `loop.index`: The current iteration of the loop. (1 indexed)
//! - `loop.index0`: The current iteration of the loop. (0 indexed)
//! - `loop.revindex`: The number of iterations from the end of the loop (1 indexed)
//! - `loop.revindex0`: The number of iterations from the end of the loop (0 indexed)
//! - `loop.first`: True if this is the first iteration.
//! - `loop.last`: True if this is the last iteration.
//! - `loop.length`: The number of items in the sequence.
//! - `loop.cycle`: A helper function to cycle between a list of sequences. See the explanation below.
//! - `loop.depth`: Indicates how deep in a recursive loop the rendering currently is. Starts at level 1
//! - `loop.depth0`: Indicates how deep in a recursive loop the rendering currently is. Starts at level 0
//! - `loop.previtem`: The item from the previous iteration of the loop. `Undefined` during the first iteration.
//! - `loop.nextitem`: The item from the previous iteration of the loop. `Undefined` during the last iteration.
//! - `loop.changed(...args)`: Returns true if the passed values have changed since the last time it was called with the same arguments.
//! - `loop.cycle(...args)`: Returns a value from the passed sequence in a cycle.
//!
//! A special note on iterators: in the current version of MiniJinja, some sequences are actually
//! lazy iterators.  They behave a bit like sequences not not entirely.  They can be iterated over,
//! will happily serialize once into a a list etc.  However when iterating over an actual iterator,
//! `last`, `revindex` and `revindex0` will always be undefined.
//!
//! Within a for-loop, it’s possible to cycle among a list of strings/variables each time through
//! the loop by using the special `loop.cycle` helper:
//!
//! ```jinja
//! {% for row in rows %}
//!   <li class="{{ loop.cycle('odd', 'even') }}">{{ row }}</li>
//! {% endfor %}
//! ```
//!
//! A `loop.changed()` helper is also available which can be used to detect when
//! a value changes between the last iteration and the current one.  The method
//! takes one or more arguments that are all compared.
//!
//! ```jinja
//! {% for entry in entries %}
//!   {% if loop.changed(entry.category) %}
//!     <h2>{{ entry.category }}</h2>
//!   {% endif %}
//!   <p>{{ entry.message }}</p>
//! {% endfor %}
//! ```
//!
//! Unlike in Rust or Python, it’s not possible to break or continue in a loop. You can,
//! however, filter the sequence during iteration, which allows you to skip items.  The
//! following example skips all the users which are hidden:
//!
//! ```jinja
//! {% for user in users if not user.hidden %}
//!   <li>{{ user.username }}</li>
//! {% endfor %}
//! ```
//!
//! If no iteration took place because the sequence was empty or the filtering
//! removed all the items from the sequence, you can render a default block by
//! using else:
//!
//! ```jinja
//! <ul>
//! {% for user in users %}
//!   <li>{{ user.username }}</li>
//! {% else %}
//!   <li><em>no users found</em></li>
//! {% endfor %}
//! </ul>
//! ```
//!
//! It is also possible to use loops recursively. This is useful if you are
//! dealing with recursive data such as sitemaps. To use loops recursively, you
//! basically have to add the ``recursive`` modifier to the loop definition and
//! call the loop variable with the new iterable where you want to recurse.
//!
//! ```jinja
//! <ul class="menu">
//! {% for item in menu recursive %}
//!   <li><a href="{{ item.href }}">{{ item.title }}</a>
//!   {% if item.children %}
//!     <ul class="submenu">{{ loop(item.children) }}</ul>
//!   {% endif %}</li>
//! {% endfor %}
//! </ul>
//! ```
//!
//! **Special note:** the `previtem` and `nextitem` attributes are available by default
//! but can be disabled by removing the `adjacent_loop_items` crate feature.  Removing
//! these attributes can provide meaningful speedups for templates with a lot of loops.
//!
//! ## `{% if %}`
//!
//! The `if` statement is comparable with the Python if statement. In the simplest form,
//! you can use it to test if a variable is defined, not empty and not false:
//!
//! ```jinja
//! {% if users %}
//!   <ul>
//!   {% for user in users %}
//!     <li>{{ user.username }}</li>
//!   {% endfor %}
//!   </ul>
//! {% endif %}
//! ```
//!
//! For multiple branches, `elif` and `else` can be used like in Python.  You can use more
//! complex expressions there too:
//!
//! ```jinja
//! {% if kenny.sick %}
//!   Kenny is sick.
//! {% elif kenny.dead %}
//!   You killed Kenny!  You bastard!!!
//! {% else %}
//!   Kenny looks okay --- so far
//! {% endif %}
//! ```
//!
//! ## `{% extends %}`
//!
//! **Feature:** `multi_template` (included by default)
//!
//! The `extends` tag can be used to extend one template from another.  You can have multiple
//! `extends` tags in a file, but only one of them may be executed at a time.  For more
//! information see [block](#-block-).
//!
//! ## `{% block %}`
//!
//! Blocks are used for inheritance and act as both placeholders and replacements at the
//! same time:
//!
//! The most powerful part of MiniJinja is template inheritance. Template inheritance allows
//! you to build a base "skeleton" template that contains all the common elements of your
//! site and defines **blocks** that child templates can override.
//!
//! **Base Template:**
//!
//! This template, which we'll call ``base.html``, defines a simple HTML skeleton
//! document that you might use for a simple two-column page. It's the job of
//! "child" templates to fill the empty blocks with content:
//!
//! ```jinja
//! <!doctype html>
//! {% block head %}
//! <title>{% block title %}{% endblock %}</title>
//! {% endblock %}
//! {% block body %}{% endblock %}
//! ```
//!
//! **Child Template:**
//!
//! ```jinja
//! {% extends "base.html" %}
//! {% block title %}Index{% endblock %}
//! {% block head %}
//!   {{ super() }}
//!   <style type="text/css">
//!     .important { color: #336699; }
//!   </style>
//! {% endblock %}
//! {% block body %}
//!   <h1>Index</h1>
//!   <p class="important">
//!     Welcome to my awesome homepage.
//!   </p>
//! {% endblock %}
//! ```
//!
//! The ``{% extends %}`` tag is the key here. It tells the template engine that
//! this template "extends" another template.  When the template system evaluates
//! this template, it first locates the parent.  The extends tag should be the
//! first tag in the template.
//!
//! As you can see it's also possible to render the contents of the parent block by calling
//! ``super()``. You can’t define multiple ``{% block %}`` tags with the same name in
//! the same template. This limitation exists because a block tag works in “both”
//! directions. That is, a block tag doesn’t just provide a placeholder to fill -
//! it also defines the content that fills the placeholder in the parent. If
//! there were two similarly-named ``{% block %}`` tags in a template, that
//! template’s parent wouldn’t know which one of the blocks’ content to use.
//!
//! If you want to print a block multiple times, you can, however, use the
//! special self variable and call the block with that name:
//!
//! ```jinja
//! <title>{% block title %}{% endblock %}</title>
//! <h1>{{ self.title() }}</h1>
//! {% block body %}{% endblock %}
//! ```
//!
//! MiniJinja allows you to put the name of the block after the end tag for better
//! readability:
//!
//! ```jinja
//! {% block sidebar %}
//!   {% block inner_sidebar %}
//!     ...
//!   {% endblock inner_sidebar %}
//! {% endblock sidebar %}
//! ```
//!
//! However, the name after the `endblock` word must match the block name.
//!
//! ## `{% include %}`
//!
//! **Feature:** `multi_template` (included by default)
//!  
//! The `include` tag is useful to include a template and return the rendered contents of that file
//! into the current namespace:
//!  
//! ```jinja
//! {% include 'header.html' %}
//!   Body
//! {% include 'footer.html' %}
//! ```
//!
//! Optionally `ignore missing` can be added in which case non existing templates
//! are silently ignored.
//!
//! ```jinja
//! {% include 'customization.html' ignore missing %}
//! ```
//!
//! You can also provide a list of templates that are checked for existence
//! before inclusion. The first template that exists will be included. If `ignore
//! missing` is set, it will fall back to rendering nothing if none of the
//! templates exist, otherwise it will fail with an error.
//!
//! ```jinja
//! {% include ['page_detailed.html', 'page.html'] %}
//! {% include ['special_sidebar.html', 'sidebar.html'] ignore missing %}
//! ```
//!  
//! Included templates have access to the variables of the active context.
//!
//! ## `{% import %}`
//!
//! **Feature:** `multi_template` (included by default)
//!
//! MiniJinja supports the `{% import %}` and `{% from ... import ... %}`
//! syntax.  With it variables or macros can be included from other templates:
//!
//! ```jinja
//! {% from "my_template.html" import my_macro, my_variable %}
//! ```
//!
//! Imports can also be aliased:
//!
//! ```jinja
//! {% from "my_template.html" import my_macro as other_name %}
//! {{ other_name() }}
//! ```
//!
//! Full modules can be imported with `{% import ... as ... %}`:
//!
//! ```jinja
//! {% import "my_template.html" as helpers %}
//! {{ helpers.my_macro() }}
//! ```
//!
//! Note that unlike Jinja2, exported modules do not contain any template code.  Only
//! variables and macros that are defined can be imported.  Also imports unlike in Jinja2
//! are not cached and they get access to the full template context.
//!
//! ## `{% with %}`
//!
//! The with statement makes it possible to create a new inner scope.  Variables set within
//! this scope are not visible outside of the scope:
//!
//! ```jinja
//! {% with foo = 42 %}
//!   {{ foo }}           foo is 42 here
//! {% endwith %}
//! foo is not visible here any longer
//! ```
//!
//! Multiple variables can be set at once and unpacking is supported:
//!
//! ```jinja
//! {% with a = 1, (b, c) = [2, 3] %}
//!   {{ a }}, {{ b }}, {{ c }}  (outputs 1, 2, 3)
//! {% endwith %}
//! ```
//!
//! ## `{% set %}`
//!
//! The `set` statement can be used to assign to variables on the same scope.  This is
//! similar to how `with` works but it won't introduce a new scope.
//!
//! ```jinja
//! {% set navigation = [('index.html', 'Index'), ('about.html', 'About')] %}
//! ```
//!
//! Please keep in mind that it is not possible to set variables inside a block
//! and have them show up outside of it.  This also applies to loops.  The only
//! exception to that rule are if statements which do not introduce a scope.
//!
//! It's also possible to capture blocks of template code into a variable by using
//! the `set` statement as a block.   In that case, instead of using an equals sign
//! and a value, you just write the variable name and then everything until
//! `{% endset %}` is captured.
//!
//! ```jinja
//! {% set navigation %}
//! <li><a href="/">Index</a>
//! <li><a href="/downloads">Downloads</a>
//! {% endset %}
//! ```
//!
//! The `navigation` variable then contains the navigation HTML source.
//!
//! This can also be combined with applying a filter:
//!
//! ```jinja
//! {% set title | upper %}Title of the page{% endset %}
//! ```
//!
//! More complex use cases can be handled using namespace objects which allow
//! propagating of changes across scopes:
//!
//! ```jinja
//! {% set ns = namespace(found=false) %}
//! {% for item in items %}
//!   {% if item.check_something() %}
//!     {% set ns.found = true %}
//!   {% endif %}
//!   * {{ item.title }}
//! {% endfor %}
//! Found item having something: {{ ns.found }}
//! ```
//!
//! Note that the `obj.attr` notation in the set tag is only allowed for namespace
//! objects; attempting to assign an attribute on any other object will cause
//! an error.
//!
//! ## `{% filter %}`
//!
//! Filter sections allow you to apply regular [filters](crate::filters) on a
//! block of template data. Just wrap the code in the special filter block:
//!
//! ```jinja
//! {% filter upper %}
//!   This text becomes uppercase
//! {% endfilter %}
//! ```
//!
//! ## `{% macro %}`
//!
//! **Feature:** `macros` (included by default)
//!
//! MiniJinja has limited support for macros.  They allow you to write reusable
//! template functions.  They are useful to put often used idioms into reusable
//! functions so you don't repeat yourself (“DRY”).
//!
//! Here’s a small example of a macro that renders a form element:
//!
//! ```jinja
//! {% macro input(name, value="", type="text") -%}
//! <input type="{{ type }}" name="{{ name }}" value="{{ value }}">
//! {%- endmacro %}
//! ```
//!
//! The macro can then be called like a function in the namespace:
//!
//! ```jinja
//! <p>{{ input('username') }}</p>
//! <p>{{ input('password', type='password') }}</p>
//! ```
//!
//! The behavior of macros with regards to undefined variables is that they capture
//! them at macro declaration time (eg: they use a closure).
//!
//! Macros can be imported via `{% import %}` or `{% from ... import %}`.
//!
//! Macros also accept a hidden `caller` keyword argument for the use with
//! `{% call %}`.
//!
//! ## `{% call %}`
//!
//! **Feature:** `macros` (included by default)
//!
//! This tag functions similar to a macro that is passed to another macro.  You can
//! think of it as a way to declare an anonymous macro and pass it to another macro
//! with the `caller` keyword argument.  The following example shows a macro that
//! takes advantage of the call functionality and how it can be used:
//!
//! ```jinja
//! {% macro dialog(title) %}
//!   <div class="dialog">
//!     <h3>{{ title }}</h3>
//!     <div class="contents">{{ caller() }}</div>
//!   </div>
//! {% endmacro %}
//!
//! {% call dialog(title="Hello World") %}
//!   This is the dialog body.
//! {% endcall %}
//! ```
//!
//! <details><summary><strong style="cursor: pointer">Macro Alternative:</strong></summary>
//!
//! The above example is more or less equivalent with the following:
//!
//! ```jinja
//! {% macro dialog(title) %}
//!   <div class="dialog">
//!     <h3>{{ title }}</h3>
//!     <div class="contents">{{ caller() }}</div>
//!   </div>
//! {% endmacro %}
//!
//! {% macro caller() %}
//!   This is the dialog body.
//! {% endmacro %}
//! {{ dialog(title="Hello World", caller=caller) }}
//! ```
//!
//! </details>
//!
//! It’s also possible to pass arguments back to the call block.  This makes it
//! useful to build macros that behave like if statements or loops.  Arguments
//! are placed surrounded in parentheses right after the `call` keyword and
//! is followed by the macro to be called.
//!
//! ```jinja
//! {% macro render_user_list(users) %}
//! <ul>
//! {% for user in users %}
//!   <li><p>{{ user.username }}</p>{{ caller(user) }}</li>
//! {% endfor %}
//! </ul>
//! {% endmacro %}
//!
//! {% call(user) render_user_list(list_of_user) %}
//! <dl>
//!   <dt>Name</dt>
//!   <dd>{{ user.name }}</dd>
//!   <dt>E-Mail</dt>
//!   <dd>{{ user.email }}</dd>
//! </dl>
//! {% endcall %}
//! ```
//!
//! ## `{% do %}`
//!
//! The do tag has the same functionality as regular template tags (`{{ ... }}`);
//! except it doesn't output anything when called.
//!
//! This is useful if you have a function or macro that has a side-effect, and
//! you don’t want to display output in the template. The following example
//! shows a macro that uses the do functionality, and how it can be used:
//!
//! ```jinja
//! {% macro dialog(title) %}
//!   Dialog is {{ title }}
//! {% endmacro %}
//!
//! {% do dialog(title="Hello World") %} <- does not output anything
//! ```
//!
//! The above example will not output anything when using the `do` tag.
//!
//! This tag exists for consistency with Jinja2 and can be useful if you have
//! custom functionality in templates that uses side-effects.  For instance if
//! you expose a function to templates that can be used to log warnings:
//!
//! ```jinja
//! {% for user in users %}
//!   {% if user.deleted %}
//!     {% log warn("Found unexpected deleted user in template") %}
//!   {% endif %}
//!   ...
//! {% endfor %}
//! ```
//!
//! ## `{% autoescape %}`
//!
//! If you want you can activate and deactivate the autoescaping from within
//! the templates.
//!
//! Example:
//!
//! ```jinja
//! {% autoescape true %}
//!   Autoescaping is active within this block
//! {% endautoescape %}
//!
//! {% autoescape false %}
//!   Autoescaping is inactive within this block
//! {% endautoescape %}
//! ```
//!
//! After an `endautoescape` the behavior is reverted to what it was before.
//!
//! The exact auto escaping behavior is determined by the value of
//! [`AutoEscape`](crate::AutoEscape) set to the template.
//!
//! ## `{% raw %}`
//!
//! A raw block is a special construct that lets you ignore the embedded template
//! syntax.  This is particularly useful if a segment of template code would
//! otherwise require constant escaping with things like `{{ "{{" }}`:
//!
//! Example:
//!
//! ```jinja
//! {% raw %}
//! <ul>
//! {% for item in seq %}
//!     <li>{{ item }}</li>
//! {% endfor %}
//! </ul>
//! {% endraw %}
//! ```
//!
//! ## `{% break %}` / `{% continue %}`
//!
//! If MiniJinja was compiled with the `loop_controls` feature, it’s possible to
//! use `break` and `continue` in loops.  When break is reached, the loop is
//! terminated; if continue is reached, the processing is stopped and continues
//! with the next iteration.
//!
//! Here’s a loop that skips every second item:
//!
//! ```jinja
//! {% for user in users %}
//! {%- if loop.index is even %}{% continue %}{% endif %}
//! ...
//! {% endfor %}
//! ```
//!
//! Likewise, a loop that stops processing after the 10th iteration:
//!
//! ```jinja
//! {% for user in users %}
//! {%- if loop.index >= 10 %}{% break %}{% endif %}
//! {%- endfor %}
//! ```
//!
//! **Note on one-shot iterators:** if you break from a loop but you have
//! accessed the `loop.nextitem` special variable, then you will lose one item.
//! This is because accessing that attribute will peak into the iterator and
//! there is no support for "putting values back".
//!
#![cfg_attr(
    feature = "custom_syntax",
    doc = r###"
# Custom Delimiters

When MiniJinja has been compiled with the `custom_syntax` feature (see
[`SyntaxConfig`]), it's possible to reconfigure the delimiters of the
templates.  This is generally not recommended but it's useful for situations
where Jinja templates are used to generate files with a syntax that would be
conflicting for Jinja.  With custom delimiters it can for instance be more
convenient to generate LaTeX files:

```
# use minijinja::{Environment, syntax::SyntaxConfig};
let mut environment = Environment::new();
environment.set_syntax(SyntaxConfig::builder()
    .block_delimiters("\\BLOCK{", "}")
    .variable_delimiters("\\VAR{", "}")
    .comment_delimiters("\\#{", "}")
    .build()
    .unwrap()
);
```

And then a template might look like this instead:

```latex
\begin{itemize}
\BLOCK{for item in sequence}
  \item \VAR{item}
\BLOCK{endfor}
\end{itemize}
```

# Line Statements and Comments

MiniJinja supports line statements and comments like Jinja2 does.  Line statements
and comments are an alternative syntax feature where blocks can be placed on their
own line if they are opened with a configured prefix.  They must appear on their own
line but can be prefixed with whitespace.  Line comments are similar but they can
also be trailing.  These syntax features need to be configured explicitly.
There are however small differences with regards to whitespace compared to
Jinja2.

To use line statements and comments the `custom_syntax` feature needs to be
enabled and they need to be configured (see [`SyntaxConfig`]).

```
# use minijinja::{Environment, syntax::SyntaxConfig};
let mut environment = Environment::new();
environment.set_syntax(SyntaxConfig::builder()
    .line_statement_prefix("#")
    .line_comment_prefix("##")
    .build()
    .unwrap()
);
```

With the above config you can render a template like this:

```jinja
## This block is
## completely removed
<ul>
  # for item in [1, 2]
    ## this is a comment that is also removed including leading whitespace
    <li>{{ item }}
  # endfor
</ul>
```

Renders into the following HTML:

```html
<ul>
    <li>1
    <li>2
</ul>
```

Note that this is slightly different than in Jinja2.  Specifically the following
rules apply with regards to whitespace:

* line statements remove all whitespace before and after including the newline
* line comments remove the trailing newline and leading whitespace.

This is different than in Jinja2 where empty lines can remain from line comments.
Additionally whitespace control is not available for line statements.
"###
)]
//! # Whitespace Control
//!
//! MiniJinja shares the same behavior with Jinja2 when it comes to
//! whitespace handling.  By default a single trailing newline is stripped if present
//! and all other whitespace is returned unchanged.
//!
//! If an application configures Jinja to [`trim_blocks`](crate::Environment::set_trim_blocks),
//! the first newline after a template tag is removed automatically (like in PHP).  The
//! [`lstrip_blocks`](crate::Environment::set_lstrip_blocks) option can also be set to strip
//! tabs and spaces from the beginning of a line to the start of a block. (Nothing will be
//! stripped if there are other characters before the start of the block.)
//!
//! With both `trim_blocks` and `lstrip_blocks` enabled, you can put block tags on their
//! own lines, and the entire block line will be removed when rendered, preserving the
//! whitespace of the contents.
//!
//! For example, without the `trim_blocks` and `lstrip_blocks` options, this template:
//!
//! ```jinja
//! <div>
//!   {% if True %}
//!     yay
//!   {% endif %}
//! </div>
//! ```
//!
//! gets rendered with blank lines inside the div:
//!
//! ```jinja
//! <div>
//!
//!     yay
//!
//! </div>
//! ```
//!
//! But with both `trim_blocks` and `lstrip_blocks` enabled, the template block lines
//! are removed and other whitespace is preserved:
//!
//! ```jinja
//! <div>
//!     yay
//! </div>
//! ````
//!
//! You can manually disable the `lstrip_blocks` behavior by putting a plus sign (`+`)
//! at the start of a block:
//!
//! ```jinja
//! <div>
//!   {%+ if something %}yay{% endif %}
//! </div>
//! ```
//!
//! Similarly, you can manually disable the `trim_blocks` behavior by putting a plus
//! sign (`+`) at the end of a block:
//!
//! ```jinja
//! <div>
//! {% if something +%}
//!     yay
//! {% endif %}
//! </div>
//! ```
//!
//! You can also strip whitespace in templates by hand. If you add a minus sign (`-`) to the
//! start or end of a block (e.g. a for tag), a comment, or a variable expression, the
//! whitespaces before or after that block will be removed:
//!
//! ```jinja
//! {% for item in range(1, 10) -%}
//! {{ item }}
//! {%- endfor %}
//! ```
//!
//! This will yield all elements without whitespace between them, in this case
//! the output would be `123456789`.
//!
//! By default, MiniJinja also removes trailing newlines.  To keep single trailing newlines,
//! configure MiniJinja to [`keep_trailing_newline`](crate::Environment::set_keep_trailing_newline).

#[cfg(feature = "custom_syntax")]
mod imp {
    use crate::compiler::lexer::StartMarker;
    use crate::error::{Error, ErrorKind};
    use aho_corasick::{AhoCorasick, PatternID};
    use std::borrow::Cow;
    use std::sync::Arc;

    #[derive(Debug, PartialEq, Clone)]
    pub(crate) struct Delims {
        block_start: Cow<'static, str>,
        block_end: Cow<'static, str>,
        variable_start: Cow<'static, str>,
        variable_end: Cow<'static, str>,
        comment_start: Cow<'static, str>,
        comment_end: Cow<'static, str>,
        line_statement_prefix: Cow<'static, str>,
        line_comment_prefix: Cow<'static, str>,
    }

    const DEFAULT_DELIMS: Delims = Delims {
        block_start: Cow::Borrowed("{%"),
        block_end: Cow::Borrowed("%}"),
        variable_start: Cow::Borrowed("{{"),
        variable_end: Cow::Borrowed("}}"),
        comment_start: Cow::Borrowed("{#"),
        comment_end: Cow::Borrowed("#}"),
        line_statement_prefix: Cow::Borrowed(""),
        line_comment_prefix: Cow::Borrowed(""),
    };

    impl Delims {
        fn validated_start_delims(&self) -> Result<Vec<&str>, Error> {
            let mut delims = Vec::with_capacity(5);
            for (delim, required) in [
                (&self.variable_start, true),
                (&self.block_start, true),
                (&self.comment_start, true),
                (&self.line_statement_prefix, false),
                (&self.line_comment_prefix, false),
            ] {
                let delim = delim as &str;
                if delim.is_empty() {
                    if required {
                        return Err(ErrorKind::InvalidDelimiter.into());
                    }
                } else if delims.contains(&delim) {
                    return Err(ErrorKind::InvalidDelimiter.into());
                } else {
                    delims.push(delim);
                }
            }

            Ok(delims)
        }
    }

    /// Builder helper to reconfigure the syntax.
    ///
    /// A new builder is returned by [`SyntaxConfig::builder`].
    #[derive(Debug)]
    #[cfg_attr(docsrs, doc(cfg(feature = "custom_syntax")))]
    pub struct SyntaxConfigBuilder {
        delims: Arc<Delims>,
    }

    impl SyntaxConfigBuilder {
        /// Sets the block start and end delimiters.
        pub fn block_delimiters<S, E>(&mut self, s: S, e: E) -> &mut Self
        where
            S: Into<Cow<'static, str>>,
            E: Into<Cow<'static, str>>,
        {
            let delims = Arc::make_mut(&mut self.delims);
            delims.block_start = s.into();
            delims.block_end = e.into();
            self
        }

        /// Sets the variable start and end delimiters.
        pub fn variable_delimiters<S, E>(&mut self, s: S, e: E) -> &mut Self
        where
            S: Into<Cow<'static, str>>,
            E: Into<Cow<'static, str>>,
        {
            let delims = Arc::make_mut(&mut self.delims);
            delims.variable_start = s.into();
            delims.variable_end = e.into();
            self
        }

        /// Sets the comment start and end delimiters.
        pub fn comment_delimiters<S, E>(&mut self, s: S, e: E) -> &mut Self
        where
            S: Into<Cow<'static, str>>,
            E: Into<Cow<'static, str>>,
        {
            let delims = Arc::make_mut(&mut self.delims);
            delims.comment_start = s.into();
            delims.comment_end = e.into();
            self
        }

        /// Sets the line statement prefix.
        ///
        /// By default this is the empty string which disables the line
        /// statement prefix feature.
        pub fn line_statement_prefix<S>(&mut self, s: S) -> &mut Self
        where
            S: Into<Cow<'static, str>>,
        {
            let delims = Arc::make_mut(&mut self.delims);
            delims.line_statement_prefix = s.into();
            self
        }

        /// Sets the line comment prefix.
        ///
        /// By default this is the empty string which disables the line
        /// comment prefix feature.
        pub fn line_comment_prefix<S>(&mut self, s: S) -> &mut Self
        where
            S: Into<Cow<'static, str>>,
        {
            let delims = Arc::make_mut(&mut self.delims);
            delims.line_comment_prefix = s.into();
            self
        }

        /// Builds the final syntax config.
        pub fn build(&self) -> Result<SyntaxConfig, Error> {
            let delims = self.delims.clone();
            if *delims == DEFAULT_DELIMS {
                return Ok(SyntaxConfig::default());
            }
            let aho_corasick = ok!(AhoCorasick::builder()
                .build(ok!(delims.validated_start_delims()))
                .map_err(|_| ErrorKind::InvalidDelimiter.into()));
            Ok(SyntaxConfig {
                delims,
                aho_corasick: Some(aho_corasick),
            })
        }
    }

    /// The delimiter configuration for the environment and the parser.
    ///
    /// Custom syntax configurations require the `custom_syntax` feature.
    /// Otherwise just the default syntax is available.
    ///
    /// You can  override the syntax configuration for templates by setting different
    /// delimiters.  The end markers can be shared, but the start markers need to be
    /// distinct.  It would thus not be valid to configure `{{` to be the marker for
    /// both variables and blocks.
    ///
    /// ```
    /// # use minijinja::{Environment, syntax::SyntaxConfig};
    /// let mut environment = Environment::new();
    /// environment.set_syntax(
    ///     SyntaxConfig::builder()
    ///         .block_delimiters("\\BLOCK{", "}")
    ///         .variable_delimiters("\\VAR{", "}")
    ///         .comment_delimiters("\\#{", "}")
    ///         .build()
    ///         .unwrap()
    /// );
    #[derive(Clone, Debug)]
    pub struct SyntaxConfig {
        delims: Arc<Delims>,
        pub(crate) aho_corasick: Option<aho_corasick::AhoCorasick>,
    }

    impl Default for SyntaxConfig {
        fn default() -> Self {
            Self {
                delims: Arc::new(DEFAULT_DELIMS),
                aho_corasick: None,
            }
        }
    }

    impl SyntaxConfig {
        /// Creates a syntax builder.
        #[cfg_attr(docsrs, doc(cfg(feature = "custom_syntax")))]
        pub fn builder() -> SyntaxConfigBuilder {
            SyntaxConfigBuilder {
                delims: Arc::new(DEFAULT_DELIMS),
            }
        }

        /// Returns the block delimiters.
        #[inline(always)]
        pub fn block_delimiters(&self) -> (&str, &str) {
            (&self.delims.block_start, &self.delims.block_end)
        }

        /// Returns the variable delimiters.
        #[inline(always)]
        pub fn variable_delimiters(&self) -> (&str, &str) {
            (&self.delims.variable_start, &self.delims.variable_end)
        }

        /// Returns the comment delimiters.
        #[inline(always)]
        pub fn comment_delimiters(&self) -> (&str, &str) {
            (&self.delims.comment_start, &self.delims.comment_end)
        }

        /// Returns the line statement prefix.
        #[inline(always)]
        pub fn line_statement_prefix(&self) -> Option<&str> {
            if self.delims.line_statement_prefix.is_empty() {
                None
            } else {
                Some(&self.delims.line_statement_prefix)
            }
        }

        /// Returns the line comment prefix.
        #[inline(always)]
        pub fn line_comment_prefix(&self) -> Option<&str> {
            if self.delims.line_comment_prefix.is_empty() {
                None
            } else {
                Some(&self.delims.line_comment_prefix)
            }
        }

        /// Reverse resolves the pattern to a start marker.
        pub(crate) fn pattern_to_marker(&self, pattern: PatternID) -> StartMarker {
            match pattern.as_usize() {
                0 => StartMarker::Variable,
                1 => StartMarker::Block,
                2 => StartMarker::Comment,
                3 => {
                    if self.line_statement_prefix().is_some() {
                        StartMarker::LineStatement
                    } else {
                        StartMarker::LineComment
                    }
                }
                4 => StartMarker::LineComment,
                _ => unreachable!(),
            }
        }
    }
}

#[cfg(not(feature = "custom_syntax"))]
mod imp {
    /// The default delimiter configuration for the environment and the parser.
    #[derive(Clone, Default, Debug)]
    pub struct SyntaxConfig;

    impl SyntaxConfig {
        /// Returns the block delimiters.
        #[inline(always)]
        pub fn block_delimiters(&self) -> (&str, &str) {
            ("{%", "%}")
        }

        /// Returns the variable delimiters.
        #[inline(always)]
        pub fn variable_delimiters(&self) -> (&str, &str) {
            ("{{", "}}")
        }

        /// Returns the comment delimiters.
        #[inline(always)]
        pub fn comment_delimiters(&self) -> (&str, &str) {
            ("{#", "#}")
        }

        /// Returns the line statement prefix.
        #[inline(always)]
        pub fn line_statement_prefix(&self) -> Option<&str> {
            None
        }

        /// Returns the line comment prefix.
        #[inline(always)]
        pub fn line_comment_prefix(&self) -> Option<&str> {
            None
        }
    }
}

pub use self::imp::*;
