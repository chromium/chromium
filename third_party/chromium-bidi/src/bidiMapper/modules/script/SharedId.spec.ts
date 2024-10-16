/*
 * Copyright 2023 Google LLC.
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

import {expect} from 'chai';

import {getSharedId, parseSharedId} from './SharedId.js';

describe('SharedId', () => {
  const PARSED_SHARED_ID = {
    frameId: 'frame_id',
    documentId: 'document_id',
    backendNodeId: 42,
  };
  const SHARED_ID = 'f.frame_id.d.document_id.e.42';

  describe('parseSharedId', () => {
    it('should parse proper formatted string', () => {
      expect(parseSharedId(SHARED_ID)).to.deep.equal(PARSED_SHARED_ID);
    });

    it('should not parse incorrectly formatted string', () => {
      expect(parseSharedId('some_incorrectly_formatted_string')).to.be.null;
    });
  });

  describe('getSharedId', () => {
    it('should generate', () => {
      expect(
        getSharedId(
          PARSED_SHARED_ID.frameId,
          PARSED_SHARED_ID.documentId,
          PARSED_SHARED_ID.backendNodeId,
        ),
      ).to.equal(SHARED_ID);
    });
  });
});
