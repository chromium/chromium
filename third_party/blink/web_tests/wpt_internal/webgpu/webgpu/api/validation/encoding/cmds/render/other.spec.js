/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Does **not** test usage scopes (resource_usages/), programmable pass stuff (programmable,*),
or state tracking (state_tracking).

TODO: plan and implement. Notes:
> All x= {render pass, render bundle}
>
> - setPipeline
>     - {valid, invalid} GPURenderPipeline
> - setIndexBuffer
>     - buffer is {valid, invalid, doesn't have usage)
>     - (offset, size) is
>         - (0, 0)
>         - (0, 1)
>         - (0, 4)
>         - (0, 5)
>         - (0, b.size)
>         - (min alignment, b.size - 4)
>         - (4, b.size - 4)
>         - (b.size - 4, 4)
>         - (b.size, min size)
>         - (0, min size), and if that's valid:
>             - (b.size - min size, min size)
> - setVertexBuffer
>     - slot is {0, max, max+1}
>     - buffer is {valid, invalid,  doesn't have usage)
>     - (offset, size) is like above
`;
import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { ValidationTest } from '../../../validation_test.js';

export const g = makeTestGroup(ValidationTest);
