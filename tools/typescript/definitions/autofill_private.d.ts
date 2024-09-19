// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.autofillPrivate API */
// TODO(crbug.com/40179454): Auto-generate this file.

import {ChromeEvent} from './chrome_event.js';

declare global {
  export namespace chrome {
    export namespace autofillPrivate {

      export interface AccountInfo {
        email: string;
        isSyncEnabledForAutofillProfiles: boolean;
        isEligibleForAddressAccountStorage: boolean;
        isAutofillSyncToggleEnabled: boolean;
        isAutofillSyncToggleAvailable: boolean;
      }

      /**
       * This enum must be kept in sync with:
       * components/autofill/core/browser/field_types.h.
       */
      export enum FieldType {
        NO_SERVER_DATA,
        UNKNOWN_TYPE,
        EMPTY_TYPE,
        NAME_FIRST,
        NAME_MIDDLE,
        NAME_LAST,
        NAME_MIDDLE_INITIAL,
        NAME_FULL,
        NAME_SUFFIX,
        EMAIL_ADDRESS,
        PHONE_HOME_NUMBER,
        PHONE_HOME_CITY_CODE,
        PHONE_HOME_COUNTRY_CODE,
        PHONE_HOME_CITY_AND_NUMBER,
        PHONE_HOME_WHOLE_NUMBER,
        ADDRESS_HOME_LINE1,
        ADDRESS_HOME_LINE2,
        ADDRESS_HOME_APT_NUM,
        ADDRESS_HOME_CITY,
        ADDRESS_HOME_STATE,
        ADDRESS_HOME_ZIP,
        ADDRESS_HOME_COUNTRY,
        CREDIT_CARD_NAME_FULL,
        CREDIT_CARD_NUMBER,
        CREDIT_CARD_EXP_MONTH,
        CREDIT_CARD_EXP_2_DIGIT_YEAR,
        CREDIT_CARD_EXP_4_DIGIT_YEAR,
        CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
        CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
        CREDIT_CARD_TYPE,
        CREDIT_CARD_VERIFICATION_CODE,
        COMPANY_NAME,
        FIELD_WITH_DEFAULT_VALUE,
        MERCHANT_EMAIL_SIGNUP,
        MERCHANT_PROMO_CODE,
        PASSWORD,
        ACCOUNT_CREATION_PASSWORD,
        ADDRESS_HOME_STREET_ADDRESS,
        ADDRESS_HOME_SORTING_CODE,
        ADDRESS_HOME_DEPENDENT_LOCALITY,
        ADDRESS_HOME_LINE3,
        NOT_ACCOUNT_CREATION_PASSWORD,
        USERNAME,
        USERNAME_AND_EMAIL_ADDRESS,
        NEW_PASSWORD,
        PROBABLY_NEW_PASSWORD,
        NOT_NEW_PASSWORD,
        CREDIT_CARD_NAME_FIRST,
        CREDIT_CARD_NAME_LAST,
        PHONE_HOME_EXTENSION,
        CONFIRMATION_PASSWORD,
        AMBIGUOUS_TYPE,
        SEARCH_TERM,
        PRICE,
        NOT_PASSWORD,
        SINGLE_USERNAME,
        NOT_USERNAME,
        UPI_VPA,
        ADDRESS_HOME_STREET_NAME,
        ADDRESS_HOME_HOUSE_NUMBER,
        ADDRESS_HOME_SUBPREMISE,
        ADDRESS_HOME_OTHER_SUBUNIT,
        NAME_LAST_FIRST,
        NAME_LAST_CONJUNCTION,
        NAME_LAST_SECOND,
        NAME_HONORIFIC_PREFIX,
        ADDRESS_HOME_ADDRESS,
        ADDRESS_HOME_ADDRESS_WITH_NAME,
        ADDRESS_HOME_FLOOR,
        PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX,
        PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX,
        PHONE_HOME_NUMBER_PREFIX,
        PHONE_HOME_NUMBER_SUFFIX,
        IBAN_VALUE,
        CREDIT_CARD_STANDALONE_VERIFICATION_CODE,
        NUMERIC_QUANTITY,
        ONE_TIME_CODE,
        DELIVERY_INSTRUCTIONS,
        ADDRESS_HOME_OVERFLOW,
        ADDRESS_HOME_LANDMARK,
        ADDRESS_HOME_OVERFLOW_AND_LANDMARK,
        ADDRESS_HOME_ADMIN_LEVEL2,
        ADDRESS_HOME_STREET_LOCATION,
        ADDRESS_HOME_BETWEEN_STREETS,
        ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK,
        ADDRESS_HOME_BETWEEN_STREETS_1,
        ADDRESS_HOME_BETWEEN_STREETS_2,
        SINGLE_USERNAME_FORGOT_PASSWORD,
        ADDRESS_HOME_APT,
        ADDRESS_HOME_APT_TYPE,
        ADDRESS_HOME_HOUSE_NUMBER_AND_APT,
        SINGLE_USERNAME_WITH_INTERMEDIATE_VALUES,
        IMPROVED_PREDICTION,
      }

      export enum AddressRecordType {
        LOCAL_OR_SYNCABLE = 'LOCAL_OR_SYNCABLE',
        ACCOUNT = 'ACCOUNT',
      }

      export interface AutofillMetadata {
        summaryLabel: string;
        summarySublabel?: string;
        recordType?: AddressRecordType;
        isLocal?: boolean;
        isMigratable?: boolean;
        isVirtualCardEnrollmentEligible?: boolean;
        isVirtualCardEnrolled?: boolean;
      }

      export interface AddressField {
        type: FieldType;
        value: string|undefined;
      }

      export interface AddressEntry {
        guid?: string;

        fields: AddressField[];

        languageCode?: string;
        metadata?: AutofillMetadata;
      }

      export interface CountryEntry {
        name?: string;
        countryCode?: string;
      }

      export interface AddressComponent {
        field: FieldType;
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
        instrumentId?: string;
        name?: string;
        cardNumber?: string;
        expirationMonth?: string;
        expirationYear?: string;
        nickname?: string;
        network?: string;
        imageSrc?: string;
        cvc?: string;
        productTermsUrl?: string;
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

      export interface UserAnnotationsEntry {
        entryId: number;
        key: string;
        value: string;
      }

      export function getAccountInfo(): Promise<AccountInfo|undefined>;
      export function saveAddress(address: AddressEntry): void;
      export function getCountryList(forAccountAddressProfile: boolean):
          Promise<CountryEntry[]>;
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
      export function migrateCreditCards(): void;
      export function logServerCardLinkClicked(): void;
      export function logServerIbanLinkClicked(): void;
      export function addVirtualCard(cardId: string): void;
      export function removeVirtualCard(cardId: string): void;
      export function authenticateUserAndFlipMandatoryAuthToggle(): void;
      export function getLocalCard(guid: string): Promise<CreditCardEntry|null>;
      export function checkIfDeviceAuthAvailable(): Promise<boolean>;
      export function bulkDeleteAllCvcs(): void;
      export function setAutofillSyncToggleEnabled(enabled: boolean): void;
      export function getUserAnnotationsEntries():
          Promise<UserAnnotationsEntry[]>;
      export function deleteUserAnnotationsEntry(entryId: number): void;
      export function deleteAllUserAnnotationsEntries(): void;
      export const onPersonalDataChanged: ChromeEvent<
          (addresses: AddressEntry[], creditCards: CreditCardEntry[],
           ibans: IbanEntry[], accountInfo?: AccountInfo) => void>;
    }
  }
}
