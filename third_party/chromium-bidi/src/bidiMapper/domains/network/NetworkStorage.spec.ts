/**
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
import * as sinon from 'sinon';

import {Network} from '../../../protocol/protocol.js';
import {EventManager} from '../events/EventManager.js';

import {NetworkStorage} from './NetworkStorage.js';

const UUID_REGEX =
  /^[0-9a-fA-F]{8}\b-[0-9a-fA-F]{4}\b-[0-9a-fA-F]{4}\b-[0-9a-fA-F]{4}\b-[0-9a-fA-F]{12}$/i;

describe('NetworkStorage', () => {
  let eventManager: sinon.SinonStubbedInstance<EventManager>;
  let networkStorage: NetworkStorage;

  beforeEach(() => {
    eventManager = sinon.createStubInstance(EventManager);
    networkStorage = new NetworkStorage(eventManager);
  });

  it('requestStageFromPhase', () => {
    expect(
      NetworkStorage.requestStageFromPhase(
        Network.InterceptPhase.BeforeRequestSent
      )
    ).to.equal('Request');
    expect(
      NetworkStorage.requestStageFromPhase(
        Network.InterceptPhase.ResponseStarted
      )
    ).to.equal('Response');
    expect(
      NetworkStorage.requestStageFromPhase(Network.InterceptPhase.AuthRequired)
    ).to.equal('Response');
  });

  describe('add intercept', () => {
    it('once with string type', () => {
      const intercept = networkStorage.addIntercept({
        urlPatterns: [
          {
            type: 'string',
            pattern: 'http://example.com',
          },
        ],
        phases: [Network.InterceptPhase.BeforeRequestSent],
      });

      expect(intercept).to.match(UUID_REGEX);
      expect(networkStorage.getFetchEnableParams().patterns).to.have.lengthOf(
        1
      );
    });

    it('once with pattern type', () => {
      const intercept = networkStorage.addIntercept({
        urlPatterns: [
          {
            type: 'pattern',
            protocol: 'https',
            hostname: 'example.com',
            port: '80',
            pathname: '/foo',
            search: 'bar=baz',
          } satisfies Network.UrlPattern,
        ],
        phases: [Network.InterceptPhase.BeforeRequestSent],
      });

      expect(intercept).to.match(UUID_REGEX);
      expect(networkStorage.getFetchEnableParams().patterns).to.have.lengthOf(
        1
      );
    });

    it('twice', () => {
      const intercept1 = networkStorage.addIntercept({
        urlPatterns: [
          {
            type: 'string',
            pattern: 'http://example.com',
          },
        ],
        phases: [Network.InterceptPhase.BeforeRequestSent],
      });
      const intercept2 = networkStorage.addIntercept({
        urlPatterns: [
          {
            type: 'string',
            pattern: 'http://example.org',
          },
        ],
        phases: [Network.InterceptPhase.BeforeRequestSent],
      });

      expect(intercept1).to.match(UUID_REGEX);
      expect(intercept2).to.match(UUID_REGEX);
      expect(intercept1).not.to.be.equal(intercept2);

      expect(networkStorage.getFetchEnableParams().patterns).to.have.lengthOf(
        2
      );
    });

    it('is idempotent', () => {
      const intercept1 = networkStorage.addIntercept({
        urlPatterns: [
          {
            type: 'string',
            pattern: 'http://example.com',
          },
        ],
        phases: [Network.InterceptPhase.BeforeRequestSent],
      });
      const intercept2 = networkStorage.addIntercept({
        urlPatterns: [
          {
            type: 'string',
            pattern: 'http://example.com',
          },
        ],
        phases: [Network.InterceptPhase.BeforeRequestSent],
      });

      expect(intercept1).to.match(UUID_REGEX);
      expect(intercept2).to.match(UUID_REGEX);
      expect(intercept1).to.be.equal(intercept2);

      expect(networkStorage.getFetchEnableParams().patterns).to.have.lengthOf(
        1
      );
    });
  });

  it('remove intercept', () => {
    const intercept = networkStorage.addIntercept({
      urlPatterns: [
        {
          type: 'string',
          pattern: 'http://example.com',
        },
      ],
      phases: [Network.InterceptPhase.BeforeRequestSent],
    });
    networkStorage.removeIntercept(intercept);

    expect(networkStorage.getFetchEnableParams()).to.deep.equal({
      handleAuthRequests: false,
      patterns: [],
    });
  });

  it('has intercepts', () => {
    expect(networkStorage.hasIntercepts()).to.be.false;

    const intercept = networkStorage.addIntercept({
      urlPatterns: [
        {
          type: 'string',
          pattern: 'http://example.com',
        },
      ],
      phases: [Network.InterceptPhase.BeforeRequestSent],
    });

    expect(networkStorage.hasIntercepts()).to.be.true;

    networkStorage.removeIntercept(intercept);
    expect(networkStorage.hasIntercepts()).to.be.false;
  });

  it('has blocked requests', () => {
    expect(networkStorage.hasBlockedRequests()).to.be.false;

    networkStorage.addBlockedRequest('REQUEST_ID', {
      request: '1',
      phase: Network.InterceptPhase.BeforeRequestSent,
      response: {
        url: '',
        protocol: '',
        status: 0,
        statusText: '',
        fromCache: false,
        headers: [],
        mimeType: '',
        bytesReceived: 0,
        headersSize: 0,
        bodySize: 0,
        content: {
          size: 0,
        },
      },
    });

    expect(networkStorage.hasBlockedRequests()).to.be.true;
  });

  describe('getFetchEnableParams', () => {
    it('no intercepts', () => {
      expect(networkStorage.getFetchEnableParams()).to.deep.equal({
        handleAuthRequests: false,
        patterns: [],
      });
    });

    [
      {
        description: 'one url pattern of string type',
        urlPatterns: [
          {
            type: 'string',
            pattern: 'http://example.com',
          } satisfies Network.UrlPattern,
        ],
        phases: [Network.InterceptPhase.BeforeRequestSent],
        expected: {
          handleAuthRequests: false,
          patterns: [
            {
              requestStage: 'Request',
              urlPattern: 'http://example.com',
            },
          ],
        },
      },
      {
        description: 'one url pattern of pattern type',
        urlPatterns: [
          {
            type: 'pattern',
            protocol: 'https',
            hostname: 'example.com',
            port: '80',
            pathname: '/foo',
            search: 'bar=baz',
          } satisfies Network.UrlPattern,
        ],
        phases: [Network.InterceptPhase.BeforeRequestSent],
        expected: {
          handleAuthRequests: false,
          patterns: [
            {
              requestStage: 'Request',
              urlPattern: 'https://example.com:80/foo?bar=baz',
            },
          ],
        },
      },
      {
        description: 'two url patterns',
        urlPatterns: [
          {
            type: 'string',
            pattern: 'http://example.com',
          } satisfies Network.UrlPattern,
          {
            type: 'string',
            pattern: 'http://example.org',
          } satisfies Network.UrlPattern,
        ],
        phases: [Network.InterceptPhase.ResponseStarted],
        expected: {
          handleAuthRequests: false,
          patterns: [
            {
              requestStage: 'Response',
              urlPattern: 'http://example.com',
            },
            {
              requestStage: 'Response',
              urlPattern: 'http://example.org',
            },
          ],
        },
      },
      {
        description: 'auth required phase',
        urlPatterns: [
          {
            type: 'string',
            pattern: 'http://example.org',
          } satisfies Network.UrlPattern,
        ],
        phases: [Network.InterceptPhase.AuthRequired],
        expected: {
          handleAuthRequests: true,
          patterns: [
            {
              requestStage: 'Response',
              urlPattern: 'http://example.org',
            },
          ],
        },
      },
      {
        description: 'auth required and request phases',
        urlPatterns: [
          {
            type: 'string',
            pattern: 'http://example.org',
          } satisfies Network.UrlPattern,
        ],
        phases: [
          Network.InterceptPhase.AuthRequired,
          Network.InterceptPhase.BeforeRequestSent,
        ],
        expected: {
          handleAuthRequests: true,
          patterns: [
            {
              requestStage: 'Response',
              urlPattern: 'http://example.org',
            },
            {
              requestStage: 'Request',
              urlPattern: 'http://example.org',
            },
          ],
        },
      },
      {
        description: 'two phases',
        urlPatterns: [
          {
            type: 'string',
            pattern: 'http://example.com',
          } satisfies Network.UrlPattern,
        ],
        phases: [
          Network.InterceptPhase.BeforeRequestSent,
          Network.InterceptPhase.ResponseStarted,
        ],
        expected: {
          handleAuthRequests: false,
          patterns: [
            {
              requestStage: 'Request',
              urlPattern: 'http://example.com',
            },
            {
              requestStage: 'Response',
              urlPattern: 'http://example.com',
            },
          ],
        },
      },
      {
        description: 'two patterns, two phases',
        urlPatterns: [
          {
            type: 'string',
            pattern: 'http://example.com',
          } satisfies Network.UrlPattern,
          {
            type: 'string',
            pattern: 'http://example.org',
          } satisfies Network.UrlPattern,
        ],
        phases: [
          Network.InterceptPhase.BeforeRequestSent,
          Network.InterceptPhase.ResponseStarted,
        ],
        expected: {
          handleAuthRequests: false,
          patterns: [
            {
              requestStage: 'Request',
              urlPattern: 'http://example.com',
            },
            {
              requestStage: 'Response',
              urlPattern: 'http://example.com',
            },
            {
              requestStage: 'Request',
              urlPattern: 'http://example.org',
            },
            {
              requestStage: 'Response',
              urlPattern: 'http://example.org',
            },
          ],
        },
      },
    ].forEach(({description, urlPatterns, phases, expected}) => {
      it(description, () => {
        networkStorage.addIntercept({
          urlPatterns,
          phases,
        });

        expect(networkStorage.getFetchEnableParams()).to.deep.equal(expected);
      });
    });
  });

  describe('cdpFromSpecUrlPattern', () => {
    it('string type', () => {
      expect(
        NetworkStorage.cdpFromSpecUrlPattern({
          type: 'string',
          pattern: 'https://example.com',
        } satisfies Network.UrlPattern)
      ).to.equal('https://example.com');
    });

    it('pattern type', () => {
      expect(
        NetworkStorage.cdpFromSpecUrlPattern({
          type: 'pattern',
          protocol: 'https',
          hostname: 'example.com',
          port: '80',
          pathname: '/foo',
          search: 'bar=baz',
        } satisfies Network.UrlPattern)
      ).to.equal('https://example.com:80/foo?bar=baz');
    });
  });

  describe('buildUrlPatternString', () => {
    describe('protocol', () => {
      it('empty', () => {
        expect(
          NetworkStorage.buildUrlPatternString({
            type: 'pattern',
            protocol: '',
            hostname: 'example.com',
            port: '80',
          } satisfies Network.UrlPatternPattern)
        ).to.equal('example.com:80');
      });

      it('without colon', () => {
        expect(
          NetworkStorage.buildUrlPatternString({
            type: 'pattern',
            protocol: 'https',
            hostname: 'example.com',
            port: '80',
          } satisfies Network.UrlPatternPattern)
        ).to.equal('https://example.com:80');
      });

      it('with colon', () => {
        expect(
          NetworkStorage.buildUrlPatternString({
            type: 'pattern',
            protocol: 'https:',
            hostname: 'example.com',
            port: '80',
          } satisfies Network.UrlPatternPattern)
        ).to.equal('https://example.com:80');
      });
    });

    describe('port', () => {
      it('empty', () => {
        expect(
          NetworkStorage.buildUrlPatternString({
            type: 'pattern',
            protocol: 'https',
            hostname: 'example.com',
            port: '',
          } satisfies Network.UrlPatternPattern)
        ).to.equal('https://example.com');
      });

      it('standard', () => {
        expect(
          NetworkStorage.buildUrlPatternString({
            type: 'pattern',
            protocol: 'https',
            hostname: 'example.com',
            port: '80',
          } satisfies Network.UrlPatternPattern)
        ).to.equal('https://example.com:80');
      });
    });

    describe('pathname', () => {
      it('empty', () => {
        expect(
          NetworkStorage.buildUrlPatternString({
            type: 'pattern',
            protocol: 'https',
            hostname: 'example.com',
            port: '80',
            pathname: '',
          } satisfies Network.UrlPatternPattern)
        ).to.equal('https://example.com:80');
      });

      it('without slash', () => {
        expect(
          NetworkStorage.buildUrlPatternString({
            type: 'pattern',
            protocol: 'https',
            hostname: 'example.com',
            port: '80',
            pathname: 'foo',
          } satisfies Network.UrlPatternPattern)
        ).to.equal('https://example.com:80/foo');
      });

      it('with slash', () => {
        expect(
          NetworkStorage.buildUrlPatternString({
            type: 'pattern',
            protocol: 'https',
            hostname: 'example.com',
            port: '80',
            pathname: '/foo',
          } satisfies Network.UrlPatternPattern)
        ).to.equal('https://example.com:80/foo');
      });
    });

    describe('search', () => {
      it('empty', () => {
        expect(
          NetworkStorage.buildUrlPatternString({
            type: 'pattern',
            protocol: 'https',
            hostname: 'example.com',
            port: '80',
            search: '',
          } satisfies Network.UrlPatternPattern)
        ).to.equal('https://example.com:80');
      });

      it('without question mark', () => {
        expect(
          NetworkStorage.buildUrlPatternString({
            type: 'pattern',
            protocol: 'https',
            hostname: 'example.com',
            port: '80',
            search: 'bar=baz',
          } satisfies Network.UrlPatternPattern)
        ).to.equal('https://example.com:80?bar=baz');
      });

      it('with question mark', () => {
        expect(
          NetworkStorage.buildUrlPatternString({
            type: 'pattern',
            protocol: 'https',
            hostname: 'example.com',
            port: '80',
            search: '?bar=baz',
          } satisfies Network.UrlPatternPattern)
        ).to.equal('https://example.com:80?bar=baz');
      });
    });
  });

  describe('isSpecialScheme', () => {
    it('http', () => {
      expect(NetworkStorage.isSpecialScheme('http')).to.be.true;
    });

    it('sftp', () => {
      expect(NetworkStorage.isSpecialScheme('sftp')).to.be.false;
    });
  });

  describe('matchUrlPattern', () => {
    it('string type', () => {
      expect(
        NetworkStorage.matchUrlPattern(
          {
            type: 'string',
            pattern: 'https://example.com',
          } satisfies Network.UrlPattern,
          'https://example.com'
        )
      ).to.be.true;
    });

    describe('pattern type', () => {
      it('positive match', () => {
        expect(
          NetworkStorage.matchUrlPattern(
            {
              type: 'pattern',
              protocol: 'https',
              hostname: 'example.com',
            } satisfies Network.UrlPattern,
            'https://example.com/aa'
          )
        ).to.be.true;
      });

      it('negative match', () => {
        expect(
          NetworkStorage.matchUrlPattern(
            {
              type: 'pattern',
              protocol: 'https',
              hostname: 'example.com',
            } satisfies Network.UrlPattern,
            'https://example.org/aa'
          )
        ).to.be.false;
      });
    });
  });
});
