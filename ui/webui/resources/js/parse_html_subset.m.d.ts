// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export type SanitizeInnerHtmlOpts = {
  substitutions?: string[],
  attrs?: string[],
  tags?: string[],
};

export function sanitizeInnerHtml(
    rawString: string, opts?: SanitizeInnerHtmlOpts): string;
export function parseHtmlSubset(
    s: string, extraTags?: string[], extraAttrs?: string[]): DocumentFragment;
