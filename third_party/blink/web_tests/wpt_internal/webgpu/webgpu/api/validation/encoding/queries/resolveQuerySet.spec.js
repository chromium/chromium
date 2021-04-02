/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Validation tests for resolveQuerySet.
`;
import { poptions } from '../../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { GPUConst } from '../../../../constants.js';
import { ValidationTest } from '../../validation_test.js';

export const g = makeTestGroup(ValidationTest);

export const kQueryCount = 2;

g.test('resolveQuerySet,invalid_queryset_and_destination_buffer')
  .desc(
    `
Tests that resolve query set with invalid object.
- invalid GPUQuerySet that failed during creation.
- invalid destination buffer that failed during creation.
  `
  )
  .subcases(() => [
    { querySetState: 'valid', destinationState: 'valid' }, // control case
    { querySetState: 'invalid', destinationState: 'valid' },
    { querySetState: 'valid', destinationState: 'invalid' },
  ])
  .fn(async t => {
    const { querySetState, destinationState } = t.params;

    const querySet = t.createQuerySetWithState(querySetState);

    const destination = t.createBufferWithState(destinationState, {
      size: kQueryCount * 8,
      usage: GPUBufferUsage.QUERY_RESOLVE,
    });

    const encoder = t.device.createCommandEncoder();
    encoder.resolveQuerySet(querySet, 0, 1, destination, 0);

    t.expectValidationError(() => {
      encoder.finish();
    }, querySetState === 'invalid' || destinationState === 'invalid');
  });

g.test('resolveQuerySet,first_query_and_query_count')
  .desc(
    `
Tests that resolve query set with invalid firstQuery and queryCount:
- firstQuery and/or queryCount out of range
  `
  )
  .subcases(() => [
    { firstQuery: 0, queryCount: kQueryCount }, // control case
    { firstQuery: 0, queryCount: kQueryCount + 1 },
    { firstQuery: 1, queryCount: kQueryCount },
    { firstQuery: kQueryCount, queryCount: 1 },
  ])
  .fn(async t => {
    const { firstQuery, queryCount } = t.params;

    const querySet = t.device.createQuerySet({ type: 'occlusion', count: kQueryCount });
    const destination = t.device.createBuffer({
      size: kQueryCount * 8,
      usage: GPUBufferUsage.QUERY_RESOLVE,
    });

    const encoder = t.device.createCommandEncoder();
    encoder.resolveQuerySet(querySet, firstQuery, queryCount, destination, 0);

    t.expectValidationError(() => {
      encoder.finish();
    }, firstQuery + queryCount > kQueryCount);
  });

g.test('resolveQuerySet,destination_buffer_usage')
  .desc(
    `
Tests that resolve query set with invalid destinationBuffer:
- Buffer usage {with, without} QUERY_RESOLVE
  `
  )
  .subcases(() =>
    poptions('bufferUsage', [
      GPUConst.BufferUsage.STORAGE,
      GPUConst.BufferUsage.QUERY_RESOLVE, // control case
    ])
  )
  .fn(async t => {
    const querySet = t.device.createQuerySet({ type: 'occlusion', count: kQueryCount });
    const destination = t.device.createBuffer({
      size: kQueryCount * 8,
      usage: t.params.bufferUsage,
    });

    const encoder = t.device.createCommandEncoder();
    encoder.resolveQuerySet(querySet, 0, kQueryCount, destination, 0);

    t.expectValidationError(() => {
      encoder.finish();
    }, t.params.bufferUsage !== GPUConst.BufferUsage.QUERY_RESOLVE);
  });

g.test('resolveQuerySet,destination_offset')
  .desc(
    `
Tests that resolve query set with invalid destinationOffset:
- destinationOffset is not a multiple of 8
- The size of destinationBuffer - destinationOffset < queryCount * 8
- destinationOffset out of range
  `
  )
  .subcases(() => poptions('destinationOffset', [0, 6, 8, 16]))
  .fn(async t => {
    const querySet = t.device.createQuerySet({ type: 'occlusion', count: kQueryCount });
    const destination = t.device.createBuffer({
      size: kQueryCount * 8,
      usage: GPUBufferUsage.QUERY_RESOLVE,
    });

    const encoder = t.device.createCommandEncoder();
    encoder.resolveQuerySet(querySet, 0, kQueryCount, destination, t.params.destinationOffset);

    t.expectValidationError(() => {
      encoder.finish();
    }, t.params.destinationOffset > 0);
  });
