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

      /**
       * This enum must be kept in sync with:
       * components/autofill/core/browser/field_types.h.
       */
      export enum ServerFieldType {
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
        ADDRESS_HOME_PREMISE_NAME,
        ADDRESS_HOME_DEPENDENT_STREET_NAME,
        ADDRESS_HOME_STREET_AND_DEPENDENT_STREET_NAME,
        ADDRESS_HOME_ADDRESS,
        ADDRESS_HOME_ADDRESS_WITH_NAME,
        ADDRESS_HOME_FLOOR,
        NAME_FULL_WITH_HONORIFIC_PREFIX,
        BIRTHDATE_DAY,
        BIRTHDATE_MONTH,
        BIRTHDATE_4_DIGIT_YEAR,
        PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX,
        PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX,
        PHONE_HOME_NUMBER_PREFIX,
        PHONE_HOME_NUMBER_SUFFIX,
        IBAN_VALUE,
        CREDIT_CARD_STANDALONE_VERIFICATION_CODE,
        NUMERIC_QUANTITY,
        ONE_TIME_CODE,
        ADDRESS_HOME_LANDMARK,
        ADDRESS_HOME_BETWEEN_STREETS,
        ADDRESS_HOME_ADMIN_LEVEL2,
        SINGLE_USERNAME_FORGOT_PASSWORD,
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
        fullName?: string;
        honorific?: string;
        companyName?: string;
        addressLines?: string;
        addressLevel1?: string;
        addressLevel2?: string;
        addressLevel3?: string;
        postalCode?: string;
        sortingCode?: string;
        countryCode?: string;
        phoneNumber?: string;
        emailAddress?: string;
        languageCode?: string;
        metadata?: AutofillMetadata;
      }

      export interface CountryEntry {
        name?: string;
        countryCode?: string;
      }

      export interface AddressComponent {
        field: ServerFieldType;
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
      export function authenticateUserAndFlipMandatoryAuthToggle(): void;
      export function authenticateUserToEditLocalCard(): Promise<boolean>;
      export function checkIfDeviceAuthAvailable(): Promise<boolean>;

      export const onPersonalDataChanged: ChromeEvent<
          (addresses: AddressEntry[], creditCards: CreditCardEntry[],
           ibans: IbanEntry[], accountInfo?: AccountInfo) => void>;
    }
  }
}
