// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.autofillPrivate API */
// TODO(crbug.com/1203307): Auto-generate this file.

import {ChromeEvent} from './chrome_event.js';

declare global {
  export namespace chrome {
    export namespace autofillPrivate {

      export interface AccountInfo {
        email: string;
        isSyncEnabledForAutofillProfiles: boolean;
        isEligibleForAddressAccountStorage: boolean;
      }

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

      export enum AddressSource {
        LOCAL_OR_SYNCABLE = 'LOCAL_OR_SYNCABLE',
        ACCOUNT = 'ACCOUNT',
      }

      export interface AutofillMetadata {
        summaryLabel: string;
        summarySublabel?: string;
        source?: AddressSource;
        isLocal?: boolean;
        isCached?: boolean;
        isMigratable?: boolean;
        isVirtualCardEnrollmentEligible?: boolean;
        isVirtualCardEnrolled?: boolean;
      }

      export interface AddressEntry {
        guid?: string;
        fullNames?: string[];
        honorific?: string;
        companyName?: string;
        addressLines?: string;
        addressLevel1?: string;
        addressLevel2?: string;
        addressLevel3?: string;
        postalCode?: string;
        sortingCode?: string;
        countryCode?: string;
        phoneNumbers?: string[];
        emailAddresses?: string[];
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
        isRequired: boolean;
        placeholder?: string;
      }

      export interface AddressComponentRow {
        row: AddressComponent[];
      }

      export interface AddressComponents {
        components: AddressComponentRow[];
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
        imageSrc?: string;
        metadata?: AutofillMetadata;
      }

      export interface IbanEntry {
        guid?: string;
        value?: string;
        nickname?: string;
        metadata?: AutofillMetadata;
      }

      export interface ValidatePhoneParams {
        phoneNumbers: string[];
        indexOfNewNumber: number;
        countryCode: string;
      }

      export function getAccountInfo(): Promise<AccountInfo|undefined>;
      export function saveAddress(address: AddressEntry): void;
      export function getCountryList(): Promise<CountryEntry[]>;
      export function getAddressComponents(
          countryCode: string): Promise<AddressComponents>;
      export function getAddressList(): Promise<AddressEntry[]>;
      export function saveCreditCard(card: CreditCardEntry): void;
      export function saveIban(iban: IbanEntry): void;
      export function removeEntry(guid: string): void;
      export function validatePhoneNumbers(
          params: ValidatePhoneParams): Promise<string[]>;
      export function getCreditCardList(): Promise<CreditCardEntry[]>;
      export function getIbanList(): Promise<IbanEntry[]>;
      export function isValidIban(ibanValue: string): Promise<boolean>;
      export function maskCreditCard(guid: string): void;
      export function migrateCreditCards(): void;
      export function logServerCardLinkClicked(): void;
      export function setCreditCardFIDOAuthEnabledState(enabled: boolean): void;
      export function getUpiIdList(): Promise<string[]>;
      export function addVirtualCard(cardId: string): void;
      export function removeVirtualCard(cardId: string): void;

      export const onPersonalDataChanged: ChromeEvent<
          (addresses: AddressEntry[], creditCards: CreditCardEntry[],
           ibans: IbanEntry[], accountInfo?: AccountInfo) => void>;
    }
  }
}
