/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Validation for encoding begin/endable queries.

TODO:
- balance: {
    - begin 0, end 1
    - begin 1, end 0
    - begin 1, end 1
    - begin 2, end 2
    - }
    - x= {
        - render pass + pipeline statistics
        - compute pass + pipeline statistics
        - }
`;
import { pbool } from '../../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { ValidationTest } from '../../validation_test.js';

export const g = makeTestGroup(ValidationTest);

g.test('occlusion_query,begin_end_balance')
  .desc(
    `
Tests that begin/end occlusion queries mismatch on render pass:
- begin n queries, then end m queries, for various n and m.
  `
  )
  .subcases(() => [
    { begin: 0, end: 1 },
    { begin: 1, end: 0 },
    { begin: 1, end: 1 }, // control case
    { begin: 1, end: 2 },
    { begin: 2, end: 1 },
  ])
  .unimplemented();

g.test('occlusion_query,begin_end_invalid_nesting')
  .desc(
    `
Tests the invalid nesting of begin/end occlusion queries:
- begin index 0, end, begin index 0, end (control case)
- begin index 0, begin index 0, end, end
- begin index 0, begin index 1, end, end
  `
  )
  .subcases(() => [
    { calls: [0, 'end', 1, 'end'] }, // control case
    { calls: [0, 0, 'end', 'end'] },
    { calls: [0, 1, 'end', 'end'] },
  ])
  .unimplemented();

g.test('occlusion_query,disjoint_queries_with_same_query_index')
  .desc(
    `
Tests that two disjoint occlusion queries cannot be begun with same query index on same render pass:
- begin index 0, end, begin index 0, end
- call on {same (invalid), different (control case)} render pass
  `
  )
  .subcases(() => pbool('isOnSameRenderPass'))
  .unimplemented();

g.test('nesting')
  .desc(
    `
Tests that whether it's allowed to nest various types of queries:
- call {occlusion, pipeline-statistics, timestamp} query in same type or other type.
  `
  )
  .subcases(() => [
    { begin: 'occlusion', nest: 'timestamp', end: 'occlusion', _valid: true },
    { begin: 'occlusion', nest: 'occlusion', end: 'occlusion', _valid: false },
    { begin: 'occlusion', nest: 'pipeline-statistics', end: 'occlusion', _valid: true },
    {
      begin: 'occlusion',
      nest: 'pipeline-statistics',
      end: 'pipeline-statistics',
      _valid: true,
    },

    {
      begin: 'pipeline-statistics',
      nest: 'timestamp',
      end: 'pipeline-statistics',
      _valid: true,
    },

    {
      begin: 'pipeline-statistics',
      nest: 'pipeline-statistics',
      end: 'pipeline-statistics',
      _valid: false,
    },

    {
      begin: 'pipeline-statistics',
      nest: 'occlusion',
      end: 'pipeline-statistics',
      _valid: true,
    },

    { begin: 'pipeline-statistics', nest: 'occlusion', end: 'occlusion', _valid: true },
    { begin: 'timestamp', nest: 'occlusion', end: 'occlusion', _valid: true },
    {
      begin: 'timestamp',
      nest: 'pipeline-statistics',
      end: 'pipeline-statistics',
      _valid: true,
    },
  ])
  .unimplemented();
