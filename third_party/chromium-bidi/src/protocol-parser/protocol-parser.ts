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
  Input as InputTypes,
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

const UnicodeCharacterSchema = zod.string().refine((value) => {
  // The spread is a little hack so JS gives us an array of unicode characters
  // to measure.
  return [...value].length === 1;
});

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

  // SerializationOptions = {
  //   ?maxDomDepth: (js-uint / null) .default 0,
  //   ?maxObjectDepth: (js-uint / null) .default null,
  //   ?includeShadowTree: ("none" / "open" / "all") .default "none",
  // }
  const SerializationOptionsSchema = zod.object({
    maxDomDepth: zod.union([zod.null(), zod.number().int().min(0)]).optional(),
    maxObjectDepth: zod
      .union([zod.null(), zod.number().int().min(0).max(MAX_INT)])
      .optional(),
    includeShadowTree: zod.enum(['none', 'open', 'all']).optional(),
  });

  // script.EvaluateParameters = {
  //   expression: text,
  //   target: script.Target,
  //   awaitPromise: bool,
  //   ?resultOwnership: script.ResultOwnership,
  //   ?serializationOptions: script.SerializationOptions,
  // }
  const EvaluateParametersSchema = zod.object({
    expression: zod.string(),
    awaitPromise: zod.boolean(),
    target: TargetSchema,
    resultOwnership: ResultOwnershipSchema.optional(),
    serializationOptions: SerializationOptionsSchema.optional(),
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

  const ChannelSchema = zod.string();

  const ChannelPropertiesSchema = zod.object({
    channel: ChannelSchema,
    // TODO(#294): maxDepth: CommonDataTypes.MaxDepthSchema.optional(),
    // See: https://github.com/w3c/webdriver-bidi/pull/361/files#r1141961142
    maxDepth: zod.number().int().min(1).max(1).optional(),
    ownership: ResultOwnershipSchema.optional(),
  });

  export const ChannelValueSchema = zod.object({
    type: zod.literal('channel'),
    value: ChannelPropertiesSchema,
  });

  export const PreloadScriptSchema = zod.string();

  export const AddPreloadScriptParametersSchema = zod.object({
    functionDeclaration: zod.string(),
    arguments: zod.array(ChannelValueSchema).optional(),
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

  // ArgumentValue = (
  //   RemoteReference //
  //   LocalValue //
  //   script.Channel
  // );
  const ArgumentValueSchema = zod.union([
    CommonDataTypes.RemoteReferenceSchema,
    CommonDataTypes.SharedReferenceSchema,
    CommonDataTypes.LocalValueSchema,
    Script.ChannelValueSchema,
  ]);

  // CallFunctionParameters = {
  //   functionDeclaration: text,
  //   awaitPromise: bool,
  //   target: script.Target,
  //   ?arguments: [*script.ArgumentValue],
  //   ?resultOwnership: script.ResultOwnership,
  //   ?serializationOptions: script.SerializationOptions,
  //   ?this: script.ArgumentValue,
  // }
  const CallFunctionParametersSchema = zod.object({
    functionDeclaration: zod.string(),
    awaitPromise: zod.boolean(),
    target: TargetSchema,
    arguments: zod.array(ArgumentValueSchema).optional(),
    resultOwnership: ResultOwnershipSchema.optional(),
    serializationOptions: SerializationOptionsSchema.optional(),
    this: ArgumentValueSchema.optional(),
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

  const ReloadParametersSchema = zod.object({
    context: CommonDataTypes.BrowsingContextSchema,
    ignoreCache: zod.boolean().optional(),
    wait: ReadinessStateSchema.optional(),
  });

  export function parseReloadParams(
    params: object
  ): BrowsingContextTypes.ReloadParameters {
    return parseObject(params, ReloadParametersSchema);
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

/** @see https://w3c.github.io/webdriver-bidi/#module-input */
export namespace Input {
  // input.ElementOrigin = {
  //   type: "element",
  //   element: script.SharedReference
  // }
  const ElementOriginSchema = zod.object({
    type: zod.literal('element'),
    element: CommonDataTypes.SharedReferenceSchema,
  });

  // input.Origin = "viewport" / "pointer" / input.ElementOrigin
  const OriginSchema = zod.union([
    zod.literal('viewport'),
    zod.literal('pointer'),
    ElementOriginSchema,
  ]);

  // input.PauseAction = {
  //   type: "pause",
  //   ? duration: js-uint
  // }
  const PauseActionSchema = zod.object({
    type: zod.literal(InputTypes.ActionType.Pause),
    duration: zod.number().nonnegative().int().optional(),
  });

  // input.KeyDownAction = {
  //   type: "keyDown",
  //   value: text
  // }
  const KeyDownActionSchema = zod.object({
    type: zod.literal(InputTypes.ActionType.KeyDown),
    value: UnicodeCharacterSchema,
  });

  // input.KeyUpAction = {
  //   type: "keyUp",
  //   value: text
  // }
  const KeyUpActionSchema = zod.object({
    type: zod.literal(InputTypes.ActionType.KeyUp),
    value: UnicodeCharacterSchema,
  });

  // input.TiltProperties = (
  //   ? tiltX: -90..90 .default 0,
  //   ? tiltY: -90..90 .default 0,
  // )
  const TiltPropertiesSchema = zod.object({
    tiltX: zod.number().min(-90).max(90).int().default(0).optional(),
    tiltY: zod.number().min(-90).max(90).int().default(0).optional(),
  });

  // input.AngleProperties = (
  //   ? altitudeAngle: float .default 0.0,
  //   ? azimuthAngle: float .default 0.0,
  // )
  const AnglePropertiesSchema = zod.object({
    altitudeAngle: zod
      .number()
      .min(0.0)
      .max(Math.PI / 2)
      .default(0.0)
      .optional(),
    azimuthAngle: zod
      .number()
      .min(0.0)
      .max(2 * Math.PI)
      .default(0.0)
      .optional(),
  });

  // input.PointerCommonProperties = (
  //   ? width: js-uint .default 1,
  //   ? height: js-uint .default 1,
  //   ? pressure: float .default 0.0,
  //   ? tangentialPressure: float .default 0.0,
  //   ? twist: 0..359 .default 0,
  //   (input.TiltProperties // input.AngleProperties)
  // )
  const PointerCommonPropertiesSchema = zod
    .object({
      width: zod.number().nonnegative().int().default(1),
      height: zod.number().nonnegative().int().default(1),
      pressure: zod.number().min(0.0).max(1.0).default(0.0),
      tangentialPressure: zod.number().min(-1.0).max(1.0).default(0.0),
      twist: zod.number().min(0).max(359).int().default(0),
    })
    .and(zod.union([TiltPropertiesSchema, AnglePropertiesSchema]));

  // input.PointerUpAction = {
  //   type: "pointerUp",
  //   button: js-uint,
  //   input.PointerCommonProperties
  // }
  const PointerUpActionSchema = zod
    .object({
      type: zod.literal(InputTypes.ActionType.PointerUp),
      button: zod.number().nonnegative().int(),
    })
    .and(PointerCommonPropertiesSchema);

  // input.PointerDownAction = {
  //   type: "pointerDown",
  //   button: js-uint,
  //   input.PointerCommonProperties
  // }
  const PointerDownActionSchema = zod
    .object({
      type: zod.literal(InputTypes.ActionType.PointerDown),
      button: zod.number().nonnegative().int(),
    })
    .and(PointerCommonPropertiesSchema);

  // input.PointerMoveAction = {
  //   type: "pointerMove",
  //   x: js-int,
  //   y: js-int,
  //   ? duration: js-uint,
  //   ? origin: input.Origin,
  //   input.PointerCommonProperties
  // }
  const PointerMoveActionSchema = zod
    .object({
      type: zod.literal(InputTypes.ActionType.PointerMove),
      x: zod.number().int(),
      y: zod.number().int(),
      duration: zod.number().nonnegative().int().optional(),
      origin: OriginSchema.optional().default('viewport'),
    })
    .and(PointerCommonPropertiesSchema);

  // input.WheelScrollAction = {
  //   type: "scroll",
  //   x: js-int,
  //   y: js-int,
  //   deltaX: js-int,
  //   deltaY: js-int,
  //   ? duration: js-uint,
  //   ? origin: input.Origin .default "viewport",
  // }
  const WheelScrollActionSchema = zod.object({
    type: zod.literal(InputTypes.ActionType.Scroll),
    x: zod.number().int(),
    y: zod.number().int(),
    deltaX: zod.number().int(),
    deltaY: zod.number().int(),
    duration: zod.number().nonnegative().int().optional(),
    origin: OriginSchema.optional().default('viewport'),
  });

  // input.WheelSourceAction = (
  //   input.PauseAction //
  //   input.WheelScrollAction
  // )
  const WheelSourceActionSchema = zod.discriminatedUnion('type', [
    PauseActionSchema,
    WheelScrollActionSchema,
  ]);

  // input.WheelSourceActions = {
  //   type: "wheel",
  //   id: text,
  //   actions: [*input.WheelSourceAction]
  // }
  const WheelSourceActionsSchema = zod.object({
    type: zod.literal(InputTypes.SourceActionsType.Wheel),
    id: zod.string(),
    actions: zod.array(WheelSourceActionSchema),
  });

  // input.PointerSourceAction = (
  //   input.PauseAction //
  //   input.PointerDownAction //
  //   input.PointerUpAction //
  //   input.PointerMoveAction
  // )
  const PointerSourceActionSchema = zod.union([
    PauseActionSchema,
    PointerDownActionSchema,
    PointerUpActionSchema,
    PointerMoveActionSchema,
  ]);

  // input.PointerType = "mouse" / "pen" / "touch"
  const PointerTypeSchema = zod.nativeEnum(InputTypes.PointerType);

  // input.PointerParameters = {
  //   ? pointerType: input.PointerType .default "mouse"
  // }
  const PointerParametersSchema = zod.object({
    pointerType: PointerTypeSchema.optional().default(
      InputTypes.PointerType.Mouse
    ),
  });

  // input.PointerSourceActions = {
  //   type: "pointer",
  //   id: text,
  //   ? parameters: input.PointerParameters,
  //   actions: [*input.PointerSourceAction]
  // }
  const PointerSourceActionsSchema = zod.object({
    type: zod.literal(InputTypes.SourceActionsType.Pointer),
    id: zod.string(),
    parameters: PointerParametersSchema.optional(),
    actions: zod.array(PointerSourceActionSchema),
  });

  // input.KeySourceAction = (
  //   input.PauseAction //
  //   input.KeyDownAction //
  //   input.KeyUpAction
  // )
  const KeySourceActionSchema = zod.discriminatedUnion('type', [
    PauseActionSchema,
    KeyDownActionSchema,
    KeyUpActionSchema,
  ]);

  // input.KeySourceActions = {
  //   type: "key",
  //   id: text,
  //   actions: [*input.KeySourceAction]
  // }
  const KeySourceActionsSchema = zod.object({
    type: zod.literal(InputTypes.SourceActionsType.Key),
    id: zod.string(),
    actions: zod.array(KeySourceActionSchema),
  });

  // input.NoneSourceAction = input.PauseAction
  const NoneSourceActionSchema = PauseActionSchema;

  // input.NoneSourceActions = {
  //   type: "none",
  //   id: text,
  //   actions: [*input.NoneSourceAction]
  // }
  const NoneSourceActionsSchema = zod.object({
    type: zod.literal(InputTypes.SourceActionsType.None),
    id: zod.string(),
    actions: zod.array(NoneSourceActionSchema),
  });

  // input.SourceActions = (
  //   input.NoneSourceActions //
  //   input.KeySourceActions //
  //   input.PointerSourceActions //
  //   input.WheelSourceActions
  // )
  const SourceActionsSchema = zod.discriminatedUnion('type', [
    NoneSourceActionsSchema,
    KeySourceActionsSchema,
    PointerSourceActionsSchema,
    WheelSourceActionsSchema,
  ]);

  // input.PerformActionsParameters = {
  //   context: browsingContext.BrowsingContext,
  //   actions: [*input.SourceActions]
  // }
  const PerformActionsParametersSchema = zod.object({
    context: CommonDataTypes.BrowsingContextSchema,
    actions: zod.array(SourceActionsSchema),
  });

  export function parsePerformActionsParams(
    params: object
  ): InputTypes.PerformActionsParameters {
    return parseObject(params, PerformActionsParametersSchema);
  }

  // input.ReleaseActionsParameters = {
  //   context: browsingContext.BrowsingContext,
  // }
  const ReleaseActionsParametersSchema = zod.object({
    context: CommonDataTypes.BrowsingContextSchema,
  });

  export function parseReleaseActionsParams(
    params: object
  ): InputTypes.ReleaseActionsParameters {
    return parseObject(params, ReleaseActionsParametersSchema);
  }
}
