// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used to get a pluralized string.
 */

// clang-format off
import {sendWithPromise} from './cr.js';
// clang-format on

export interface PluralStringProxy {
  /**
   * Obtains a pluralized string for |messageName| with |itemCount| items.
   * @param messageName The name of the message.
   * @param itemCount The number of items.
   * @return Promise resolved with the appropriate plural string for
   *     |messageName| with |itemCount| items.
   */
  getPluralString(messageName: string, itemCount: number): Promise<string>;

  /**
   * Fetches both plural strings, concatenated to one string with a comma.
   * @param messageName1 The name of the first message.
   * @param itemCount1 The number of items in the first message.
   * @param messageName2 The name of the second message.
   * @param itemCount2 The number of items in the second message.
   * @return Promise resolved with the appropriate plural
   *     strings for both messages, concatenated with a comma+whitespace in
   *     between them.
   */
  getPluralStringTupleWithComma(
      messageName1: string, itemCount1: number, messageName2: string,
      itemCount2: number): Promise<string>;

  /**
   * Fetches both plural strings, concatenated to one string with periods.
   * @param messageName1 The name of the first message.
   * @param itemCount1 The number of items in the first message.
   * @param messageName2 The name of the second message.
   * @param itemCount2 The number of items in the second message.
   * @return Promise resolved with the appropriate plural
   *     strings for both messages, concatenated with a period+whitespace after
   *     the first message, and a period after the second message.
   */
  getPluralStringTupleWithPeriods(
      messageName1: string, itemCount1: number, messageName2: string,
      itemCount2: number): Promise<string>;
}

export class PluralStringProxyImpl implements PluralStringProxy {
  getPluralString(messageName: string, itemCount: number) {
    return sendWithPromise('getPluralString', messageName, itemCount);
  }

  getPluralStringTupleWithComma(
      messageName1: string, itemCount1: number, messageName2: string,
      itemCount2: number) {
    return sendWithPromise(
        'getPluralStringTupleWithComma', messageName1, itemCount1, messageName2,
        itemCount2);
  }

  getPluralStringTupleWithPeriods(
      messageName1: string, itemCount1: number, messageName2: string,
      itemCount2: number) {
    return sendWithPromise(
        'getPluralStringTupleWithPeriods', messageName1, itemCount1,
        messageName2, itemCount2);
  }

  static getInstance(): PluralStringProxy {
    return instance || (instance = new PluralStringProxyImpl());
  }

  static setInstance(obj: PluralStringProxy) {
    instance = obj;
  }
}

let instance: PluralStringProxy|null = null;
