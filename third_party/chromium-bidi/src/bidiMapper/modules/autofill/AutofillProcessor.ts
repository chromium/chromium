/**
 * Copyright 2026 Google LLC.
 * Copyright (c) Microsoft Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
import type {Protocol} from 'devtools-protocol';

import {
  Autofill,
  InvalidArgumentException,
  UnsupportedOperationException,
  type EmptyResult,
  type Script,
} from '../../../protocol/protocol.js';
import {assert} from '../../../utils/assert.js';
import {CdpErrorConstants} from '../../../utils/cdpErrorConstants.js';
import type {BrowsingContextImpl} from '../context/BrowsingContextImpl.js';
import type {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';
import {parseSharedId} from '../script/SharedId.js';

/**
 * Maps `autofill.FieldName` values (HTML autofill field names) to the Chromium
 * autofill field types expected by CDP `Autofill.trigger`, following
 * `HtmlFieldTypeToBestCorrespondingFieldType`. `null` means Chromium cannot
 * fill the field.
 *
 * @see https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/field_types.cc
 */
const CDP_FIELD_TYPES: Record<Autofill.FieldName, string | null> = {
  [Autofill.FieldName.Name]: 'NAME_FULL',
  [Autofill.FieldName.HonorificPrefix]: 'NAME_HONORIFIC_PREFIX',
  [Autofill.FieldName.GivenName]: 'NAME_FIRST',
  [Autofill.FieldName.AdditionalName]: 'NAME_MIDDLE',
  [Autofill.FieldName.FamilyName]: 'NAME_LAST',
  [Autofill.FieldName.HonorificSuffix]: null,
  [Autofill.FieldName.Nickname]: null,
  [Autofill.FieldName.Organization]: 'COMPANY_NAME',
  [Autofill.FieldName.StreetAddress]: 'ADDRESS_HOME_STREET_ADDRESS',
  [Autofill.FieldName.AddressLine1]: 'ADDRESS_HOME_LINE1',
  [Autofill.FieldName.AddressLine2]: 'ADDRESS_HOME_LINE2',
  [Autofill.FieldName.AddressLine3]: 'ADDRESS_HOME_LINE3',
  [Autofill.FieldName.AddressLevel4]: null,
  [Autofill.FieldName.AddressLevel3]: 'ADDRESS_HOME_DEPENDENT_LOCALITY',
  [Autofill.FieldName.AddressLevel2]: 'ADDRESS_HOME_CITY',
  [Autofill.FieldName.AddressLevel1]: 'ADDRESS_HOME_STATE',
  [Autofill.FieldName.Country]: 'ADDRESS_HOME_COUNTRY',
  // CDP interprets `ADDRESS_HOME_COUNTRY` values as country codes, so country
  // names cannot be passed through.
  [Autofill.FieldName.CountryName]: null,
  [Autofill.FieldName.PostalCode]: 'ADDRESS_HOME_ZIP',
  [Autofill.FieldName.Tel]: 'PHONE_HOME_WHOLE_NUMBER',
  [Autofill.FieldName.TelCountryCode]: 'PHONE_HOME_COUNTRY_CODE',
  [Autofill.FieldName.TelNational]: 'PHONE_HOME_CITY_AND_NUMBER',
  [Autofill.FieldName.TelAreaCode]: 'PHONE_HOME_CITY_CODE',
  [Autofill.FieldName.TelLocal]: 'PHONE_HOME_NUMBER',
  [Autofill.FieldName.Email]: 'EMAIL_ADDRESS',
};

/**
 * Responsible for handling the `autofill` module.
 * @see https://wicg.github.io/autofill-event/#webdriver-bidi-module
 */
export class AutofillProcessor {
  readonly #browsingContextStorage: BrowsingContextStorage;

  constructor(browsingContextStorage: BrowsingContextStorage) {
    this.#browsingContextStorage = browsingContextStorage;
  }

  /** @see https://wicg.github.io/autofill-event/#webdriver-bidi-trigger */
  async trigger(params: Autofill.TriggerParameters): Promise<EmptyResult> {
    const context = this.#browsingContextStorage.getContext(params.context);
    await this.#assertIsFormControlWithFormOwner(context, params.element);

    if (params.card !== undefined && params.address !== undefined) {
      throw new InvalidArgumentException(
        'Card and address cannot both be provided',
      );
    }
    if (params.card === undefined && params.address === undefined) {
      throw new InvalidArgumentException(
        'Either card or address must be provided',
      );
    }
    const address =
      params.address === undefined
        ? undefined
        : AutofillProcessor.#toCdpAddress(params.address);

    // The shared id was successfully deserialized in the context's document
    // above.
    const parsedSharedId = parseSharedId(params.element.sharedId);
    assert(parsedSharedId !== null);

    try {
      await context.cdpTarget.cdpClient.sendCommand('Autofill.trigger', {
        fieldId: parsedSharedId.backendNodeId,
        frameId: context.id,
        card: params.card,
        address,
      });
    } catch (error) {
      const cdpError = error as {code?: number; message?: string} | undefined;
      if (cdpError?.code === CdpErrorConstants.METHOD_NOT_FOUND) {
        throw new UnsupportedOperationException(
          'Autofill is not supported by this browser',
        );
      }
      if (
        cdpError?.code === CdpErrorConstants.INVALID_REQUEST &&
        cdpError.message === 'Field not found'
      ) {
        throw new InvalidArgumentException(
          `Element "${params.element.sharedId}" cannot be autofilled`,
        );
      }
      throw error;
    }
    return {};
  }

  /**
   * Throws unless `element` is a form control with a form owner. Only `input`,
   * `select` and `textarea` elements are considered form controls, as other
   * form-associated elements cannot be autofilled.
   */
  async #assertIsFormControlWithFormOwner(
    context: BrowsingContextImpl,
    element: Script.SharedReference,
  ): Promise<void> {
    const hiddenSandboxRealm = await context.getOrCreateHiddenSandbox();
    const result = await hiddenSandboxRealm.callFunction(
      String(function isFormControlWithFormOwner(this: unknown) {
        return (
          (this instanceof HTMLInputElement ||
            this instanceof HTMLSelectElement ||
            this instanceof HTMLTextAreaElement) &&
          this.form !== null
        );
      }),
      false,
      element,
    );
    assert(result.type === 'success');
    assert(result.result.type === 'boolean');
    if (!result.result.value) {
      throw new InvalidArgumentException(
        `Element "${element.sharedId}" is not a form control with a form owner`,
      );
    }
  }

  static #toCdpAddress(address: Autofill.Address): Protocol.Autofill.Address {
    return {
      fields: address.fields.map(({name, value}) => {
        const cdpFieldType = CDP_FIELD_TYPES[name];
        if (!cdpFieldType) {
          throw new UnsupportedOperationException(
            `Autofill field "${name}" is not supported`,
          );
        }
        return {name: cdpFieldType, value};
      }),
    };
  }
}
