// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.autofillPrivate API */
// TODO(crbug.com/1203307): Auto-generate this file.

import {ChromeEvent} from './chrome_event.js';

declare global {
  export namespace chrome {
    export namespace autofillPrivate {

      export enum AddressField {
        HONORIFIC = 'HONORIFIC',
        FULL_NAME = 'FULL_NAME',
        COMPANY_NAME = 'COMPANY_NAME',
        ADDRESS_LINES = 'ADDRESS_LINES',
        ADDRESS_LEVEL_1 = 'ADDRESS_LEVEL_1',
        ADDRESS_LEVEL_2 = 'ADDRESS_LEVEL_2',
        ADDRESS_LEVEL_3 = 'ADDRESS_LEVEL_3',
        POSTAL_CODE = 'POSTAL_CODE',
        SORTING_CODE = 'SORTING_CODE',
        COUNTRY_CODE = 'COUNTRY_CODE',
      }

      export interface AutofillMetadata {
        summaryLabel: string;
        summarySublabel?: string;
        isLocal?: boolean;
        isCached?: boolean;
        isMigratable?: boolean;
        isVirtualCardEnrollmentEligible?: boolean;
        isVirtualCardEnrolled?: boolean;
      }

      export interface AddressEntry {
        guid?: string;
        fullNames?: Array<string>;
        honorific?: string;
        companyName?: string;
        addressLines?: string;
        addressLevel1?: string;
        addressLevel2?: string;
        addressLevel3?: string;
        postalCode?: string;
        sortingCode?: string;
        countryCode?: string;
        phoneNumbers?: Array<string>;
        emailAddresses?: Array<string>;
        languageCode?: string;
        metadata?: AutofillMetadata;
      }

      export interface CountryEntry {
        name?: string;
        countryCode?: string;
      }

      export interface AddressComponent {
        field: AddressField;
        fieldName: string;
        isLongField: boolean;
        placeholder?: string;
      }

      export interface AddressComponentRow {
        row: Array<AddressComponent>;
      }

      export interface AddressComponents {
        components: Array<AddressComponentRow>;
        languageCode: string;
      }

      export interface CreditCardEntry {
        guid?: string;
        name?: string;
        cardNumber?: string;
        expirationMonth?: string;
        expirationYear?: string;
        nickname?: string;
        network?: string;
        metadata?: AutofillMetadata;
      }

      export interface ValidatePhoneParams {
        phoneNumbers: Array<string>;
        indexOfNewNumber: number;
        countryCode: string;
      }

      export function saveAddress(address: AddressEntry): void;
      export function getCountryList(
          callback: (entries: Array<CountryEntry>) => void): void;
      export function getAddressComponents(
          countryCode: string,
          callback: (components: AddressComponents) => void): void;
      export function getAddressList(
          callback: (entries: Array<AddressEntry>) => void): void;
      export function saveCreditCard(card: CreditCardEntry): void;
      export function removeEntry(guid: string): void;
      export function validatePhoneNumbers(
          params: ValidatePhoneParams,
          callback: (numbers: Array<string>) => void): void;
      export function getCreditCardList(
          callback: (entries: Array<CreditCardEntry>) => void): void;
      export function maskCreditCard(guid: string): void;
      export function migrateCreditCards(): void;
      export function logServerCardLinkClicked(): void;
      export function setCreditCardFIDOAuthEnabledState(enabled: boolean): void;
      export function getUpiIdList(callback: (items: Array<string>) => void):
          void;
      export function addVirtualCard(cardId: string): void;
      export function removeVirtualCard(cardId: string): void;

      export const onPersonalDataChanged: ChromeEvent<
          (addresses: Array<AddressEntry>,
           creditCards: Array<CreditCardEntry>) => void>;
    }
  }
}
