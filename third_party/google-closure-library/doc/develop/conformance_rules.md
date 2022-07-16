---
title: JavaScript Conformance Rules for Closure Library
section: develop
layout: article
---

<!-- Documentation licensed under CC BY 4.0 -->
<!-- License available at https://creativecommons.org/licenses/by/4.0/ -->

The config file:
[closure/goog/conformance\_proto.txt](https://github.com/google/closure-library/tree/master/closure/goog/conformance_proto.txt)


## Introduction

Closure JavaScript code is expected to conform to a set of rules for security,
performance, code health, or other reasons. This configuration file for the
[JS Conformance Framework] enforces this.

The security-specific rules in here mostly target DOM (and Closure) APIs that
are prone to script-injection vulnerabilities ([XSS]). In these cases, the rules
will point to wrapper APIs that, instead of plain strings, consume values of
types with specific security contracts indicating that the value can be safely
used in a given context.



## Possible Violations

If you are adding code and the warning you are seeing doesn’t seem appropriate
and the warning is a "possible violation", then the compiler doesn’t have enough
type information to confirm that you aren’t violating a rule. As noted in the
[JS Conformance Framework] documentation, conformance rules are enforced
strictly so you aren’t allowed to "possibly violate".

## How to fix possible violations

Removing false-positive 'possible violations' requires providing more type
information. Often this is as simple as declaring array content types,
tightening an API's return type, or choosing a different API.

For example, many Closure DOM APIs return a precise type if passed a
`goog.dom.TagName` instance. Passing this instance instead of a string solves
many possible violations.

Examples:

```js
// Possible violation.
const img = goog.dom.createDom('img');
img.src = src;
// Clean.
const img = goog.dom.createDom(goog.dom.TagName.IMG);
img.src = src;
// Build error - native APIs don't support goog.dom.TagName.
const img = document.createElement(goog.dom.TagName.IMG);
img.src = src;
```

```js
// Possible violation.
const img = goog.dom.getElementByClass('avatar');
img.src = src;
// Clean.
const img = goog.dom.getElementByTagNameAndClass(goog.dom.TagName.IMG, 'avatar');
img.src = src;
```

```js
// Possible violation.
const img = goog.dom.getElement('avatar');
img.src = src;
// Clean.
const img = goog.dom.asserts.assertIsHtmlImageElement(goog.dom.getElement('avatar'));
img.src = src;
// No violation but unsafe - see below.
const img = /** @type {!HTMLImageElement} */ (goog.dom.getElement('avatar'));
img.src = src;
```

Summing it up:

*   Use `goog.dom` functions with `goog.dom.TagName` instances.
*   Use `getElementByTagNameAndClass`.
*   Use `goog.asserts.dom` if there's no better API.
*   Avoid type-casting as there's no check whether you actually cast a correct
    type. For example, type-casting `HTMLScriptElement` as an `Element` can lead
    it to being incorrectly treated as an `HTMLImageElement` elsewhere.

## Explanation of conformance rules

{: #logger}
### goog.debug.Logger 

`goog.debug.Logger` should not be used directly. Use the `goog.log` static
wrappers instead, as `goog.log` is safely strippable from production code. On
the other hand, `goog.debug.Logger` is only stripped from code if the `logger_`
suffix is used in the name.


Note:  You may see "possible violations" for code that is not a logger if the
code is badly typed. Verify that you have a dependency on the type you are
expecting.

### eval

`eval` is a security risk and is not allowed to be used. Since values passed to
`eval()` are evaluated and executed as any ordinary JavaScript, it is not
inherently safe to pass content to `eval()`. `eval()` is typically not necessary
for ordinary programming.

IE's `execScript` is also banned.

`Function`, `setTimeout`, `setInterval` and `requestAnimationFrame` with string
argument are also banned.

{: #throwOfNonErrorTypes}
### throw 'message' 

`throw` with a string literal can not have a stack trace attached to it, making
debugging significantly more difficult.  Use `throw new Error('message')`
instead.

{: #callee}
### Arguments.prototype.callee 

`Arguments.prototype.callee` is not allowed in EcmaScript
"[strict mode](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Strict_mode)"
code.

{: #documentWrite}
### Calls to Document.prototype.write 

Calling `Document.prototype.write` is a security risk and is banned. Any content
passed to `write()` will be automatically evaluated in the DOM, so the
assignment of user-controlled, insufficiently sanitized or escaped content can
result in [XSS] vulnerabilities.

`Document.prototype.write` is bad for performance as it forces document
re-parsing, has unpredictable semantics and disallows many optimizations a
browser may make. It is almost never needed. Only exception is writing to a
completely new window such as a popup or an iframe.


If you need to use it, use the type-safe [`goog.dom.safe.documentWrite`]
wrapper, or directly render a Strict Soy template using
[`goog.soy.Renderer.prototype.renderElement`] \(or similar\).

{: #innerHtml}
### Assignment to Element.prototype.innerHTML/outerHTML 

Direct assignment of a non-constant value to `innerHTML` and `outerHTML` is a
security risk and is banned. Any content passed to `innerHTML` or `outerHTML`
will be automatically evaluated in the DOM and therefore the assignment of
user-controlled, insufficiently sanitized or escaped content can result in [XSS]
vulnerabilities.

Instead, use the type-safe [`goog.dom.safe.setInnerHtml`] wrapper, or directly
render a Strict Soy template using [`goog.soy.Renderer.prototype.renderElement`]
\(or similar\).

NOTE: Reads of these properties are permitted.

{: #untypedElements}
### Creating untyped elements 

We have several conformance rules banning assignment to dangerous properties
such as `script.src`. These rules work only if we know the type of the
manipulated element, e.g. `HTMLScriptElement`. Unfortunately,
`document.createElement('script')` and similar APIs return only `Element` as
perceived by the compiler. For our rules to work, we need to know the exact type
which is returned by `goog.dom` methods when used together with
`goog.dom.TagName`. Typically, it's `goog.dom.createElement` and
`goog.dom.createDom`, but other methods such as `goog.dom.getElementsByTagName`
also work. `DomHelper` counterparts of these methods support `goog.dom.TagName`
too.

For this reason, we ban creating untyped `'script'`, `'iframe'`, `'frame'`,
`'embed'`, and `'object'` elements and require using `goog.dom` methods with
`goog.dom.TagName` with them.

{: #location}
### Assignment to Location.prototype.href and Window.prototype.location 

Direct assignment of a non-constant value to `Location.prototype.href` and
`Window.prototype.location` is a security risk and is banned. Externally
controlled strings assigned to `Location.href` can result in [XSS]
vulnerabilities, e.g. via "`javascript:evil()`" URLs.

Instead of directly assigning to `Location.prototype.href` or
`Window.prototype.location`, use the safe wrapper function
[`goog.dom.safe.setLocationHref`]. When passed a string, this wrapper sanitizes
the URL before passing it to the underlying DOM property. If passed a value of
type `goog.html.SafeUrl`, the value is assigned without further sanitization.

NOTE: Reads of this property are permitted.

{: #href}
### Assignment to .href property of Anchor, Link, etc elements 

Direct assignment of a non-constant value to the `href` property of Anchor,
Link, and similar elements is a security risk and is banned. Externally
controlled strings assigned to the href property can result in [XSS]
vulnerabilities, e.g. via "`javascript:evil()`" URLs.

Instead of directly assigning to the href property, use safe wrapper functions
such as [`goog.dom.safe.setAnchorHref`]. When passed a
string, this wrapper sanitizes the URL before passing it to the underlying DOM
property. If passed a value of type `goog.html.SafeUrl`, the value is assigned
without further sanitization.

NOTE: Reads of this property are permitted.

{: #trustedResourceUrl}
### Assignment to property requires a TrustedResourceUrl via goog.dom.safe 

Assignment of a non-constant value to certain URL-valued properties, like
Base.href and Script.src, via a string that is not fully application controlled
is a security risk and is banned. Attacker controlled values assigned to these
properties can result in loading code from an untrusted domain. For example, the
following would be unsafe if www.google.com were to have an open redirector and
attackerControlled were something like `'../redirect=http://evil.com/evil#'`:

```js
script.src = 'https://www.google.com/module/' + attackerControlled + '.js';
```

Instead of directly assigning to these properties use safe wrapper functions
which take `TrustedResourceUrl`, such as `goog.dom.safe.setScriptSrc`.

Note: Reads of this property are permitted.

{: #createDom}
### Assigning a variable to a dangerous property via createDom is forbidden. 

`goog.dom.createDom` and its version in `DomHelper` support assigning attributes
to the newly created elements. This conformance rule bans assigning attributes
that can load attacker controlled code, such as `script.src` or `innerHTML`.

To assign these attributes, create the element first and then assign the
attribute using `goog.dom.safe` functions like this:

```js
var script = goog.dom.createDom(goog.dom.TagName.SCRIPT);
goog.dom.safe.setScriptSrc(script, trustedResourceUrl);
```

Alternatively, use a function in `goog.html.SafeHtml` such as
`goog.html.SafeHtml.createScriptSrc`.

This rule might report a possible violation if the tag name or attributes are
not literals. To avoid this possible violation, structure the code like this:

```js
// Reports a possible violation.
var tag = 'img';
var attrs = {'src': ''};
goog.dom.createDom(tag, attrs);
// Passes.
goog.dom.createDom('img', {'src': ''});
```

Note that string literal values assigned to banned attributes are allowed as
they couldn't be attacker controlled.

{: #scriptContent}
### Setting content of Script element is not allowed 

Setting content of `<script>` and then appending it to the document has the same
effect as calling `eval()`. This coding pattern is prone to XSS vulnerabilities,
and therefore disallowed.

{: #postMessage}
### Window.prototype.postMessage 

Raw `postMessage()` does not restrict target and sender origins by default. This
can cause security vulnerabilities.


{: #globalVars}
### Global declarations 

Global functions and var declarations are not allowed, as these pollute global
scope. Top level namespaces are allowed if declared with "goog.provide" or
"goog.module".

{: #unknownThis}
### Unknown types 

Loose types `?` (unknown), `*` (all), `Object` and `Function` should be used
sparingly as they degrade available type information. `?` as a "this" type is
forbidden so that accidental unknowns (which are far more common) can be caught.


{: #storage}
### Client Side Storage (Closure library specific) 

Client side storage mechanisms are dangerous because of PII and security
implications.



{: #legacyApis}
### Unsafe legacy APIs 

Closure (as well as some libraries built on top of
Closure)
include several APIs that consume plain strings, and pass them on to an API that
process that string in an injection-vulnerability-prone way (most commonly, an
assignment to `.innerHTML`). Thus, use of such APIs incurs similar risks of
injection vulnerabilities as the underlying DOM API (e.g., `innerHTML`
assignment). Due to these risks, conformance rules disallow the use of such
APIs. The respective conformance rules' error message refers to the equivalent,
safe API to use instead. Typically, the safe API consumes values of an
appropriate security-contract type such as `goog.html.SafeHtml`.

<!-- Links -->

[JS Conformance Framework]: https://github.com/google/closure-compiler/wiki/JS-Conformance-Framework
[XSS]: https://en.wikipedia.org/wiki/Cross-site_scripting
[`goog.dom.safe.documentWrite`]: https://google.github.io/closure-library/api/goog.dom.safe#documentWrite
[`goog.dom.safe.setAnchorHref`]: https://google.github.io/closure-library/api/goog.dom.safe#setAnchorHref
[`goog.dom.safe.setInnerHtml`]: https://google.github.io/closure-library/api/goog.dom.safe#setInnerHtml
[`goog.dom.safe.setLocationHref`]:  https://google.github.io/closure-library/api/goog.dom.safe#setLocationHref
[`goog.soy.Renderer.prototype.renderElement`]: https://google.github.io/closure-library/api/goog.soy.Renderer#renderElement

