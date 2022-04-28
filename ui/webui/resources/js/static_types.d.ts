// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export function getTrustedHTML(literal: string[]|
                               TemplateStringsArray): TrustedHTML|string;
export function getTrustedScript(literal: string[]): TrustedScript|string;
export function getTrustedScriptURL(literal: string[]): TrustedScriptURL|string;
