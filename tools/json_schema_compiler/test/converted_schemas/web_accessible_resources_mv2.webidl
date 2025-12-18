// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Stub namespace for the "web_accessible_resources" manifest key.
[generate_error_messages, Namespace=webAccessibleResourcesMv2]
partial dictionary ExtensionManifest {
  // Relative paths within the extension package representing web accessible
  // resources.
  sequence<DOMString> web_accessible_resources;
};
