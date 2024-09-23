// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.idl_basics API
 * Generated from: file.idl
 * run `tools/json_schema_compiler/compiler.py file.idl -g ts_definitions` to
 * regenerate.
 */

import {ChromeEvent} from './chrome_event.js';

declare global {
  export namespace chrome {

    export namespace idl_basics {

      export enum EnumType {
        NAME1 = 'name1',
        NAME2 = 'name2',
      }

      export enum EnumTypeWithNoDoc {
        NAME1 = 'name1',
        NAME2 = 'name2',
      }

      export enum EnumTypeWithNoDocValue {
        NAME1 = 'name1',
        NAME2 = 'name2',
        NAME3 = 'name3',
      }

      export interface MyType1 {
        x: number;
        y: string;
        z: string;
        a: string;
        b: string;
        c: string;
      }

      export interface MyType2 {
        x: string;
      }

      export interface ChoiceWithArraysType {
        entries: string|string[];
      }

      export interface ChoiceWithOptionalType {
        entries?: string|string[];
      }

      export interface UnionType {
        x?: EnumType|string;
        y: string|EnumType;
      }

      export interface IgnoreAdditionalPropertiesType {
        x: string;
      }

      export function function1(): void;

      export function function2(x: number): void;

      export function function3(arg: MyType1): void;

      export function function4(): Promise<void>;

      export function function5(): Promise<number>;

      export function function6(): Promise<MyType1>;

      export function function7(arg?: number): void;

      export function function8(arg1: number, arg2?: string): void;

      export function function9(arg?: MyType1): void;

      export function function10(x: number, y: number[]): void;

      export function function11(arg: MyType1[]): void;

      export function function12(): Promise<MyType2[]>;

      export function function13(type: EnumType): Promise<EnumType>;

      export function function14(types: EnumType[]): void;

      export function function15(switch: number): void;

      export function function16(): Promise<number>;

      export function function17(): Promise<number>;

      export function function18(): Promise<number>;

      export function function20(value: idl_other_namespace.SomeType): void;

      export function function21(values: idl_other_namespace.SomeType[]): void;

      export function function22(
          value: idl_other_namespace.sub_namespace.AnotherType): void;

      export function function23(
          values: idl_other_namespace.sub_namespace.AnotherType[]): void;

      export function function24(): number;

      export function function25(): MyType1;

      export function function26(): MyType1[];

      export function function27(): EnumType;

      export function function28(): EnumType[];

      export function function29(): idl_other_namespace.SomeType;

      export function function30(): idl_other_namespace.SomeType[];

      export function funcAsync(): Promise<MyType2[]>;

      export function funcOptionalArgAndNotPromiseBased(
          cb: (arg0?: string) => void): void;

      export function funcOptionalArgCallback(): Promise<string|undefined>;

      export function funcOptionalCallbackNotPromiseBased(
          cb?: (x: number) => void): void;

      export function funcOptionalCallback(): Promise<number>;

      export function funcWithEntry(entries: Entry[]): void;

      export function funcWithArrayObj(entries: Array<{
        [key: string]: any,
      }>): void;

      export const onFoo1: ChromeEvent<() => void>;

      export const onFoo2: ChromeEvent<(arg: MyType1) => void>;

      export const onFoo3: ChromeEvent<(type: EnumType) => void>;

    }
  }
}
