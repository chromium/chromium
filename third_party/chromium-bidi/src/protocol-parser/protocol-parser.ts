/**
 * Copyright 2022 Google LLC.
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

/**
 * @fileoverview Provides parsing and validator for WebDriver BiDi protocol.
 * Parser types should match the `../protocol` types.
 */

import {ZodType, z as zod} from 'zod';

import {
  BrowsingContext as BrowsingContextTypes,
  Script as ScriptTypes,
  Log as LogTypes,
  CDP as CdpTypes,
  Message as MessageTypes,
  Session as SessionTypes,
  Network as NetworkTypes,
  CommonDataTypes as CommonDataTypesTypes,
} from '../protocol/protocol.js';

const MAX_INT = 9007199254740991 as const;

export function parseObject<T extends ZodType>(
  obj: unknown,
  schema: T
): zod.infer<T> {
  const parseResult = schema.safeParse(obj);
  if (parseResult.success) {
    return parseResult.data;
  }
  const errorMessage = parseResult.error.errors
    .map(
      (e) =>
        `${e.message} in ` +
        `${e.path.map((p) => JSON.stringify(p)).join('/')}.`
    )
    .join(' ');

  throw new MessageTypes.InvalidArgumentException(errorMessage);
}

export namespace CommonDataTypes {
  export const SharedReferenceSchema = zod.object({
    sharedId: zod.string().min(1),
    handle: zod.string().optional(),
  });
  export const RemoteReferenceSchema = zod.object({
    handle: zod.string().min(1),
  });

  // UndefinedValue = {
  //   type: "undefined",
  // }
  const UndefinedValueSchema = zod.object({type: zod.literal('undefined')});

  // NullValue = {
  //   type: "null",
  // }
  const NullValueSchema = zod.object({type: zod.literal('null')});

  // StringValue = {
  //   type: "string",
  //   value: text,
  // }
  const StringValueSchema = zod.object({
    type: zod.literal('string'),
    value: zod.string(),
  });

  // SpecialNumber = "NaN" / "-0" / "Infinity" / "-Infinity";
  const SpecialNumberSchema = zod.enum(['NaN', '-0', 'Infinity', '-Infinity']);

  // NumberValue = {
  //   type: "number",
  //   value: number / SpecialNumber,
  // }
  const NumberValueSchema = zod.object({
    type: zod.literal('number'),
    value: zod.union([SpecialNumberSchema, zod.number()]),
  });

  // BooleanValue = {
  //   type: "boolean",
  //   value: bool,
  // }
  const BooleanValueSchema = zod.object({
    type: zod.literal('boolean'),
    value: zod.boolean(),
  });

  // BigIntValue = {
  //   type: "bigint",
  //   value: text,
  // }
  const BigIntValueSchema = zod.object({
    type: zod.literal('bigint'),
    value: zod.string(),
  });

  const PrimitiveProtocolValueSchema = zod.union([
    UndefinedValueSchema,
    NullValueSchema,
    StringValueSchema,
    NumberValueSchema,
    BooleanValueSchema,
    BigIntValueSchema,
  ]);

  export const LocalValueSchema: zod.ZodType<CommonDataTypesTypes.LocalValue> =
    zod.lazy(() =>
      zod.union([
        PrimitiveProtocolValueSchema,
        ArrayLocalValueSchema,
        DateLocalValueSchema,
        MapLocalValueSchema,
        ObjectLocalValueSchema,
        RegExpLocalValueSchema,
        SetLocalValueSchema,
      ])
    );

  // Order is important, as `parse` is processed in the same order.
  // `SharedReferenceSchema`->`RemoteReferenceSchema`->`LocalValueSchema`.
  const LocalOrRemoteValueSchema = zod.union([
    SharedReferenceSchema,
    RemoteReferenceSchema,
    LocalValueSchema,
  ]);

  // ListLocalValue = [*LocalValue];
  const ListLocalValueSchema = zod.array(LocalOrRemoteValueSchema);

  // ArrayLocalValue = {
  //   type: "array",
  //   value: ListLocalValue,
  // }
  const ArrayLocalValueSchema = zod.object({
    type: zod.literal('array'),
    value: ListLocalValueSchema,
  });

  // DateLocalValue = {
  //   type: "date",
  //   value: text
  // }
  const DateLocalValueSchema = zod.object({
    type: zod.literal('date'),
    value: zod.string().min(1),
  });

  // MappingLocalValue = [*[(LocalValue / text), LocalValue]];
  const MappingLocalValueSchema = zod.tuple([
    zod.union([zod.string(), LocalOrRemoteValueSchema]),
    LocalOrRemoteValueSchema,
  ]);

  // MapLocalValue = {
  //   type: "map",
  //   value: MappingLocalValue,
  // }
  const MapLocalValueSchema = zod.object({
    type: zod.literal('map'),
    value: zod.array(MappingLocalValueSchema),
  });

  // ObjectLocalValue = {
  //   type: "object",
  //   value: MappingLocalValue,
  // }
  const ObjectLocalValueSchema = zod.object({
    type: zod.literal('object'),
    value: zod.array(MappingLocalValueSchema),
  });

  // RegExpLocalValue = {
  //   type: "regexp",
  //   value: RegExpValue,
  // }
  const RegExpLocalValueSchema = zod.object({
    type: zod.literal('regexp'),
    value: zod.object({
      pattern: zod.string(),
      flags: zod.string().optional(),
    }),
  });

  // SetLocalValue = {
  //   type: "set",
  //   value: ListLocalValue,
  // }
  const SetLocalValueSchema: zod.ZodType = zod.lazy(() =>
    zod.object({
      type: zod.literal('set'),
      value: ListLocalValueSchema,
    })
  );

  // BrowsingContext = text;
  export const BrowsingContextSchema = zod.string();

  export const MaxDepthSchema = zod.number().int().nonnegative().max(MAX_INT);
}

/** @see https://w3c.github.io/webdriver-bidi/#module-script */
export namespace Script {
  const RealmTypeSchema = zod.enum([
    'window',
    'dedicated-worker',
    'shared-worker',
    'service-worker',
    'worker',
    'paint-worklet',
    'audio-worklet',
    'worklet',
  ]);

  export const GetRealmsParametersSchema = zod.object({
    context: CommonDataTypes.BrowsingContextSchema.optional(),
    type: RealmTypeSchema.optional(),
  });

  export function parseGetRealmsParams(
    params: object
  ): ScriptTypes.GetRealmsParameters {
    return parseObject(params, GetRealmsParametersSchema);
  }

  // ContextTarget = {
  //   context: BrowsingContext,
  //   ?sandbox: text
  // }
  const ContextTargetSchema = zod.object({
    context: CommonDataTypes.BrowsingContextSchema,
    sandbox: zod.string().optional(),
  });

  // RealmTarget = {realm: Realm};
  const RealmTargetSchema = zod.object({
    realm: zod.string().min(1),
  });

  // Target = (
  //   RealmTarget //
  //   ContextTarget
  // );
  // Order is important, as `parse` is processed in the same order.
  // `RealmTargetSchema` has higher priority.
  const TargetSchema = zod.union([RealmTargetSchema, ContextTargetSchema]);

  // ResultOwnership = "root" / "none"
  const ResultOwnershipSchema = zod.enum(['root', 'none']);

  // ScriptEvaluateParameters = {
  //   expression: text;
  //   target: Target;
  //   ?awaitPromise: bool;
  //   ?resultOwnership: ResultOwnership;
  // }
  const EvaluateParametersSchema = zod.object({
    expression: zod.string(),
    awaitPromise: zod.boolean(),
    target: TargetSchema,
    resultOwnership: ResultOwnershipSchema.optional(),
  });

  export function parseEvaluateParams(
    params: object
  ): ScriptTypes.EvaluateParameters {
    return parseObject(params, EvaluateParametersSchema);
  }

  // DisownParameters = {
  //   handles: [Handle]
  //   target: script.Target;
  // }
  const DisownParametersSchema = zod.object({
    target: TargetSchema,
    handles: zod.array(zod.string()),
  });

  export function parseDisownParams(
    params: object
  ): ScriptTypes.DisownParameters {
    return parseObject(params, DisownParametersSchema);
  }

  export const PreloadScriptSchema = zod.string();

  export const AddPreloadScriptParametersSchema = zod.object({
    functionDeclaration: zod.string(),
    sandbox: zod.string().optional(),
    context: CommonDataTypes.BrowsingContextSchema.optional(),
  });

  export function parseAddPreloadScriptParams(
    params: object
  ): ScriptTypes.AddPreloadScriptParameters {
    return parseObject(params, AddPreloadScriptParametersSchema);
  }

  export const RemovePreloadScriptParametersSchema = zod.object({
    script: PreloadScriptSchema,
  });

  export function parseRemovePreloadScriptParams(
    params: object
  ): ScriptTypes.RemovePreloadScriptParameters {
    return parseObject(params, RemovePreloadScriptParametersSchema);
  }

  const ChannelIdSchema = zod.string();

  const ChannelPropertiesSchema = zod.object({
    channel: ChannelIdSchema,
    // TODO(#294): maxDepth: CommonDataTypes.MaxDepthSchema.optional(),
    // See: https://github.com/w3c/webdriver-bidi/pull/361/files#r1141961142
    maxDepth: zod.number().int().min(1).max(1).optional(),
    ownership: ResultOwnershipSchema.optional(),
  });

  export const ChannelSchema = zod.object({
    type: zod.literal('channel'),
    value: ChannelPropertiesSchema,
  });

  // ArgumentValue = (
  //   RemoteReference //
  //   LocalValue //
  //   script.Channel
  // );
  const ArgumentValueSchema = zod.union([
    CommonDataTypes.RemoteReferenceSchema,
    CommonDataTypes.SharedReferenceSchema,
    CommonDataTypes.LocalValueSchema,
    Script.ChannelSchema,
  ]);

  // CallFunctionParameters = {
  //   functionDeclaration: text;
  //   awaitPromise: bool;
  //   target: script.Target;
  //   ?arguments: [*script.ArgumentValue];
  //   ?this: script.ArgumentValue;
  //   ?resultOwnership: script.ResultOwnership;
  // }
  const CallFunctionParametersSchema = zod.object({
    functionDeclaration: zod.string(),
    target: TargetSchema,
    arguments: zod.array(ArgumentValueSchema).optional(),
    this: ArgumentValueSchema.optional(),
    awaitPromise: zod.boolean(),
    resultOwnership: ResultOwnershipSchema.optional(),
  });

  export function parseCallFunctionParams(
    params: object
  ): ScriptTypes.CallFunctionParameters {
    return parseObject(params, CallFunctionParametersSchema);
  }
}

/** @see https://w3c.github.io/webdriver-bidi/#module-browsingContext */
export namespace BrowsingContext {
  // GetTreeParameters = {
  //   ?maxDepth: js-uint,
  //   ?root: browsingContext.BrowsingContext,
  // }
  const GetTreeParametersSchema = zod.object({
    maxDepth: CommonDataTypes.MaxDepthSchema.optional(),
    root: CommonDataTypes.BrowsingContextSchema.optional(),
  });

  export function parseGetTreeParams(
    params: object
  ): BrowsingContextTypes.GetTreeParameters {
    return parseObject(params, GetTreeParametersSchema);
  }

  // ReadinessState = "none" / "interactive" / "complete"
  const ReadinessStateSchema = zod.enum(['none', 'interactive', 'complete']);

  // BrowsingContextNavigateParameters = {
  //   context: BrowsingContext,
  //   url: text,
  //   ?wait: ReadinessState,
  // }
  // ReadinessState = "none" / "interactive" / "complete"
  const NavigateParametersSchema = zod.object({
    context: CommonDataTypes.BrowsingContextSchema,
    url: zod.string().url(),
    wait: ReadinessStateSchema.optional(),
  });

  export function parseNavigateParams(
    params: object
  ): BrowsingContextTypes.NavigateParameters {
    return parseObject(params, NavigateParametersSchema);
  }

  // BrowsingContextCreateType = "tab" / "window"
  // BrowsingContextCreateParameters = {
  //   type: BrowsingContextCreateType
  // }
  const CreateParametersSchema = zod.object({
    type: zod.enum(['tab', 'window']),
    referenceContext: CommonDataTypes.BrowsingContextSchema.optional(),
  });

  export function parseCreateParams(
    params: object
  ): BrowsingContextTypes.CreateParameters {
    return parseObject(params, CreateParametersSchema);
  }

  // BrowsingContextCloseParameters = {
  //   context: BrowsingContext
  // }
  const CloseParametersSchema = zod.object({
    context: CommonDataTypes.BrowsingContextSchema,
  });

  export function parseCloseParams(
    params: object
  ): BrowsingContextTypes.CloseParameters {
    return parseObject(params, CloseParametersSchema);
  }

  // browsingContext.CaptureScreenshotParameters = {
  //   context: browsingContext.BrowsingContext
  // }
  const CaptureScreenshotParametersSchema = zod.object({
    context: CommonDataTypes.BrowsingContextSchema,
  });

  export function parseCaptureScreenshotParams(
    params: object
  ): BrowsingContextTypes.CaptureScreenshotParameters {
    return parseObject(params, CaptureScreenshotParametersSchema);
  }

  // All units are in cm.
  // PrintPageParameters = {
  //   ?height: (float .ge 0.0) .default 27.94,
  //   ?width: (float .ge 0.0) .default 21.59,
  // }
  const PrintPageParametersSchema = zod.object({
    height: zod.number().min(0.0).default(27.94).optional(),
    width: zod.number().min(0.0).default(21.59).optional(),
  });

  // All units are in cm.
  // PrintMarginParameters = {
  //   ?bottom: (float .ge 0.0) .default 1.0,
  //   ?left: (float .ge 0.0) .default 1.0,
  //   ?right: (float .ge 0.0) .default 1.0,
  //   ?top: (float .ge 0.0) .default 1.0,
  // }
  const PrintMarginParametersSchema = zod.object({
    bottom: zod.number().min(0.0).default(1.0).optional(),
    left: zod.number().min(0.0).default(1.0).optional(),
    right: zod.number().min(0.0).default(1.0).optional(),
    top: zod.number().min(0.0).default(1.0).optional(),
  });

  /** @see https://w3c.github.io/webdriver/#dfn-parse-a-page-range */
  const PrintPageRangesSchema = zod
    .array(zod.union([zod.string().min(1), zod.number().int().nonnegative()]))
    .refine((pageRanges: (string | number)[]) => {
      return pageRanges.every((pageRange: string | number) => {
        const match = String(pageRange).match(
          // matches: '2' | '2-' | '-2' | '2-4'
          /^(?:(?:\d+)|(?:\d+[-])|(?:[-]\d+)|(?:(?<start>\d+)[-](?<end>\d+)))$/
        );

        // If a page range is specified, validate start <= end.
        const {start, end} = match?.groups ?? {};
        if (start && end && Number(start) > Number(end)) {
          return false;
        }

        return match;
      });
    });

  // PrintParameters = {
  //   context: browsingContext.BrowsingContext,
  //   ?background: bool .default false,
  //   ?margin: browsingContext.PrintMarginParameters,
  //   ?orientation: ("portrait" / "landscape") .default "portrait",
  //   ?page: browsingContext.PrintPageParameters,
  //   ?pageRanges: [*(js-uint / text)],
  //   ?scale: 0.1..2.0 .default 1.0,
  //   ?shrinkToFit: bool .default true,
  // }
  const PrintParametersSchema = zod.object({
    context: CommonDataTypes.BrowsingContextSchema,
    background: zod.boolean().default(false).optional(),
    margin: PrintMarginParametersSchema.optional(),
    orientation: zod
      .enum(['portrait', 'landscape'])
      .default('portrait')
      .optional(),
    page: PrintPageParametersSchema.optional(),
    pageRanges: PrintPageRangesSchema.default([]).optional(),
    scale: zod.number().min(0.1).max(2.0).default(1.0).optional(),
    shrinkToFit: zod.boolean().default(true).optional(),
  });

  export function parsePrintParams(
    params: object
  ): BrowsingContextTypes.PrintParameters {
    return parseObject(params, PrintParametersSchema);
  }
}

export namespace CDP {
  const SendCommandParamsSchema = zod.object({
    // Allowing any cdpMethod, and casting to proper type later on.
    cdpMethod: zod.string(),
    // `passthrough` allows object to have any fields.
    // https://github.com/colinhacks/zod#passthrough
    cdpParams: zod.object({}).passthrough(),
    cdpSession: zod.string().optional(),
  });

  export function parseSendCommandParams(
    params: object
  ): CdpTypes.SendCommandParams {
    return parseObject(
      params,
      SendCommandParamsSchema
    ) as CdpTypes.SendCommandParams;
  }

  const GetSessionParamsSchema = zod.object({
    context: CommonDataTypes.BrowsingContextSchema,
  });

  export function parseGetSessionParams(
    params: object
  ): CdpTypes.GetSessionParams {
    return parseObject(params, GetSessionParamsSchema);
  }
}

/** @see https://w3c.github.io/webdriver-bidi/#module-session */
export namespace Session {
  const SubscriptionRequestParametersEventsSchema = zod.enum([
    BrowsingContextTypes.AllEvents,
    ...Object.values(BrowsingContextTypes.EventNames),
    LogTypes.AllEvents,
    ...Object.values(LogTypes.EventNames),
    CdpTypes.AllEvents,
    ...Object.values(CdpTypes.EventNames),
    NetworkTypes.AllEvents,
    ...Object.values(NetworkTypes.EventNames),
    ScriptTypes.AllEvents,
    ...Object.values(ScriptTypes.EventNames),
  ]);

  // SessionSubscriptionRequest = {
  //   events: [*text],
  //   ?contexts: [*BrowsingContext],
  // }
  const SubscriptionRequestParametersSchema = zod.object({
    events: zod.array(SubscriptionRequestParametersEventsSchema),
    contexts: zod.array(CommonDataTypes.BrowsingContextSchema).optional(),
  });

  export function parseSubscribeParams(
    params: object
  ): SessionTypes.SubscriptionRequest {
    return parseObject(params, SubscriptionRequestParametersSchema);
  }
}
