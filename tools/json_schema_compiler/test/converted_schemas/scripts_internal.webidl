// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

[ExternalExtensionType="extensionTypes.RunAt"]
typedef object ExtensionTypesRunAt;

[ExternalExtensionType="extensionTypes.ExecutionWorld"]
typedef object ExtensionTypesExecutionWorld;

// The source of the user script. This will also determine certain
// capabilities of the script (such as whether it can use globs, raw strings
// for code, etc).
enum Source {
  "DYNAMIC_CONTENT_SCRIPT",
  "DYNAMIC_USER_SCRIPT",
  "MANIFEST_CONTENT_SCRIPT"
};

// The source of the script to inject.
dictionary ScriptSource {
  // A string containing the JavaScript code to inject. Exactly one of
  // <code>file</code> or <code>code</code> must be specified.
  DOMString code;

  // The path of the JavaScript file to inject relative to the extension's
  // root directory. Exactly one of <code>file</code> or <code>code</code>
  // must be specified.
  DOMString file;
};

// Describes a serialized script, intended for storage and persistence across
// browser sessions.
// Note: Though it is called "UserScript", this is used for scripts through
// the scripting API (dynamic content scripts), content scripts in the
// manifest (static content scripts), and user scripts through the userScripts
// API. "UserScript" was chosen because it matches the correspodning
// extenisons::UserScript object (the runtime representation of this) and
// because "Script" is ambiguous (e.g. background script, general JS script,
// etc).
dictionary SerializedUserScript {
  // Whether the script will inject into all frames, regardless if it is not
  // the top-most frame in the tab.
  boolean allFrames;

  // The list of CSS files to be injected into matching pages. Note that,
  // today, we only expect these to contain files. It is represented as a
  // ScriptSource for compatibility and consistency with `js`.
  sequence<ScriptSource> css;

  // Excludes pages that this user script would otherwise be injected into.
  sequence<DOMString> excludeMatches;

  // Specifies wildcard patterns for pages this user script will NOT be
  // injected into.
  sequence<DOMString> excludeGlobs;

  // The ID of the script.
  required DOMString id;

  // Specifies wildcard patterns for pages this user script will be injected
  // into.
  sequence<DOMString> includeGlobs;

  // The list of sources of javascript to be injected into matching pages.
  sequence<ScriptSource> js;

  // Specifies which pages this user script will be injected into.
  required sequence<DOMString> matches;

  // Whether the script should inject into any frames where the URL belongs to
  // a scheme that would never match a specified Match Pattern, including
  // about:, data:, blob:, and filesystem: schemes.
  boolean matchOriginAsFallback;

  // Specifies when JavaScript files are injected into the web page.
  ExtensionTypesRunAt runAt;

  // The "source" of the user script.
  required Source source;

  // The JavaScript "world" to run the script in.
  required ExtensionTypesExecutionWorld world;

  // The ID of the world into which to inject. If omitted, uses the default
  // world.
  DOMString worldId;
};

// Internal namespace for representing content scripts.
interface ScriptsInternal {
};

partial interface Browser {
  static attribute ScriptsInternal scriptsInternal;
};
