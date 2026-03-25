// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Callback for the allow operation in EmbedRequest.
// |url| : Specifies the content to be embedded.
callback AllowCallback = undefined (DOMString url);

// Callback for the deny operation in EmbedRequest.
callback DenyCallback = undefined ();

// This object specifies details and operations to perform on the embedding
// request. The app to be embedded can make a decision on whether or not to
// allow the embedding and what to embed based on the embedder making the
// request.
dictionary EmbedRequest {
  // The ID of the app that sent the embedding request.
  required DOMString embedderId;

  // Optional developer specified data that the app to be embedded can use
  // when making an embedding decision.
  required object data;

  // Allows the embedding request.
  required AllowCallback allow;

  // Prevents the embedding request.
  required DenyCallback deny;
};

// Use the <code>appview</code> tag to embed other Chrome Apps within your
// Chrome App. (see <a href=#usage>Usage</a>).
interface AppviewTag {
  // Requests another app to be embedded.
  //
  // |app| : The extension id of the app to be embedded.
  // |data| : Optional developer specified data that the app to be embedded
  //   can use when making an embedding decision.
  // |Returns|: An optional function that's called after the embedding
  //   request is completed.
  // |PromiseValue|: success: True if the embedding request succeded.
  static Promise<boolean> connect(DOMString app, optional any data);
};

partial interface Browser {
  static attribute AppviewTag appviewTag;
};
