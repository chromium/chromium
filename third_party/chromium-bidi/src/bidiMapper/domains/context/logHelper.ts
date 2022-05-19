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

import { CommonDataTypes } from '../protocol/bidiProtocolTypes';

const specifiers = ['%s', '%d', '%i', '%f', '%o', '%O', '%c'];

function isFormmatSpecifier(str: string): boolean {
  return specifiers.some((spec) => str.includes(spec));
}

/**
 * @param args input remote values to be format printed
 * @returns parsed text of the remote values in specific format
 */
export function logMessageFormatter(
  args: CommonDataTypes.RemoteValue[]
): string {
  let output = '';
  const argFormat = (
    args[0] as { type: string; value: string }
  ).value.toString();
  const argValues = args.slice(1, undefined);
  const tokens = argFormat.split(
    new RegExp(specifiers.map((spec) => '(' + spec + ')').join('|'), 'g')
  );

  for (const token of tokens) {
    if (token === undefined || token == '') {
      continue;
    }
    if (isFormmatSpecifier(token)) {
      const arg = argValues.shift() as { type: string; value: any };
      // raise an exception when less value is provided
      if (arg === undefined) {
        throw new Error(
          'Less value is provided: "' + getRemoteValuesText(args, false) + '"'
        );
      }
      if (token === '%s') {
        output += arg.value.toString();
      } else if (token === '%d' || token === '%i') {
        output += parseInt(arg.value.toString(), 10);
      } else if (token === '%f') {
        output += parseFloat(arg.value.toString());
      } else {
        // %o, %O, %c
        output += getSingleRemoteValueText(arg as CommonDataTypes.RemoteValue);
      }
    } else {
      output += token;
    }
  }

  // raise an exception when more value is provided
  if (argValues.length > 0) {
    throw new Error(
      'More value is provided: "' + getRemoteValuesText(args, false) + '"'
    );
  }

  return output;
}

/**
 * @param arg input remote value to be parsed
 * @returns parsed text of the remote value
 *
 * input: {"type": "number", "value": 1}
 * output: 1
 *
 * input: {"type": "string", "value": "abc"}
 * output: "abc"
 *
 * input: {"type": "object",  "value": [["id", {"type": "number", "value": 1}]]}
 * output: '{"id": 1}'
 *
 * input: {"type": "object", "value": [["font-size", {"type": "string", "value": "20px"}]]}
 * output: '{"font-size": "20px"}'
 */
function getSingleRemoteValueText(arg: CommonDataTypes.RemoteValue): string {
  // arg type validation
  if (!['number', 'string', 'object'].includes(arg.type)) {
    throw Error('Invalid value type: ' + arg.toString());
  }

  if (arg.type === 'number' && typeof (arg.value, Number)) {
    return arg.value.toString();
  }

  if (arg.type === 'string' && typeof (arg.value, String)) {
    return '"' + arg.value.toString() + '"';
  }

  if (arg.type === 'object' && typeof (arg.value, Object)) {
    return (
      '{"' +
      (arg.value as any[])[0][0] +
      '": ' +
      getSingleRemoteValueText((arg.value as any[])[0][1]) +
      '}'
    );
  }

  throw Error('Invalid value type: ' + arg.toString());
}

export function getRemoteValuesText(
  args: CommonDataTypes.RemoteValue[],
  formatText: boolean
): string {
  if (args.length == 0) {
    return '';
  }

  // if args[0] is a format specifier, format the args as output
  if (
    args[0].type === 'string' &&
    isFormmatSpecifier(args[0].value.toString()) &&
    formatText
  ) {
    return logMessageFormatter(args);
  }

  // if args[0] is not a format specifier, just join the args with \u0020
  return args
    .map((arg) => (arg as { type: string; value: any }).value.toString())
    .join('\u0020');
}
