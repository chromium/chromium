/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Unit tests for Uri. */

goog.module('goog.UriTest');
goog.setTestOnly();

const Uri = goog.require('goog.Uri');
const testSuite = goog.require('goog.testing.testSuite');

// Tests, that creating URI from components and then
// getting the components back yields equal results.
// The special attention is paid to test proper encoding
// and decoding of URI components.

// Tests setting the query string and then reading back
// query parameter values.

// Tests setting query parameter values and the reading back the query string.

// Tests that building a URI with a query string and then reading it back
// gives the same result.

// verifies bug http://b/9821952

function assertDotRemovedEquals(expected, path) {
  assertEquals(expected, Uri.removeDotSegments(path));
}

function assertResolvedEquals(expected, base, other) {
  assertEquals(expected, Uri.resolve(base, other).toString());
}
testSuite({
  testUriParse() {
    const uri = new Uri('http://www.google.com:80/path?q=query#fragmento');
    assertEquals('http', uri.getScheme());
    assertEquals('', uri.getUserInfo());
    assertEquals('www.google.com', uri.getDomain());
    assertEquals(80, uri.getPort());
    assertEquals('/path', uri.getPath());
    assertEquals('q=query', uri.getQuery());
    assertEquals('fragmento', uri.getFragment());

    assertEquals(
        'terer258+foo@gmail.com',
        Uri.parse('mailto:terer258+foo@gmail.com').getPath());
  },

  testUriParseWithNewline() {
    const uri = new Uri('http://www.google.com:80/path?q=query#frag\nmento');
    assertEquals('http', uri.getScheme());
    assertEquals('', uri.getUserInfo());
    assertEquals('www.google.com', uri.getDomain());
    assertEquals(80, uri.getPort());
    assertEquals('/path', uri.getPath());
    assertEquals('q=query', uri.getQuery());
    assertEquals('frag\nmento', uri.getFragment());
  },

  testUriParseAcceptsThingsWithToString() {
    // Ensure that the goog.Uri constructor coerces random types to strings.
    const uriStr = 'http://www.google.com:80/path?q=query#fragmento';
    const uri = new Uri({
      toString: function() {
        return uriStr;
      }
    });
    assertEquals(
        'http://www.google.com:80/path?q=query#fragmento', uri.toString());
  },

  testUriCreate() {
    assertEquals(
        'http://www.google.com:81/search%20path?q=what%20to%20eat%2Bdrink%3F',
        Uri.create(
               'http', null, 'www.google.com', 81, '/search path',
               (new Uri.QueryData).set('q', 'what to eat+drink?'), null)
            .toString());

    assertEquals(
        'http://www.google.com:80/search%20path?q=what%20to%20eat%2Bdrink%3F',
        Uri.create(
               'http', null, 'www.google.com', 80, '/search path',
               (new Uri.QueryData).set('q', 'what to eat+drink?'), null)
            .toString());

    assertEquals(
        'http://www.google.com/search%20path?q=what%20to%20eat%2Bdrink%3F',
        Uri.create(
               'http', null, 'www.google.com', null, '/search path',
               (new Uri.QueryData).set('q', 'what to eat+drink?'), null)
            .toString());

    const createdUri = Uri.create(
        'http', null, 'www.google.com', null, '/search path',
        new Uri.QueryData(null, true).set('Q', 'what to eat+drink?'), null);

    assertEquals(
        'http://www.google.com/search%20path?q=what%20to%20eat%2Bdrink%3F',
        createdUri.toString());
  },

  testClone() {
    const uri1 =
        new Uri('http://user:pass@www.google.com:8080/foo?a=1&b=2#c=3');
    // getCount forces instantiation of internal data structures to more
    // thoroughly test clone.
    uri1.getQueryData().getCount();
    const uri2 = uri1.clone();

    assertNotEquals(uri1, uri2);
    assertEquals(uri1.toString(), uri2.toString());
    assertEquals(2, uri2.getQueryData().getCount());

    uri2.setParameterValues('q', 'bar');
    assertFalse(uri1.getParameterValue('q') == 'bar');
  },

  testRelativeUris() {
    assertFalse(new Uri('?hello').hasPath());
  },

  testAbsolutePathResolution() {
    const uri1 = new Uri('http://www.google.com:8080/path?q=query#fragmento');

    assertEquals(
        'http://www.google.com:8080/foo',
        uri1.resolve(new Uri('/foo')).toString());

    assertEquals(
        'http://www.google.com:8080/foo/bar',
        Uri.resolve('http://www.google.com:8080/search/', '/foo/bar')
            .toString());

    assertEquals(
        'http://www.google.com:8080/path?q=que%2Br%20y#fragmento',
        Uri.resolve(
               'http://www.google.com:8080/', '/path?q=que%2Br%20y#fragmento')
            .toString());
  },

  testRelativePathResolution() {
    const uri1 = new Uri('http://www.google.com:8080/path?q=query#fragmento');
    assertEquals(
        'http://www.google.com:8080/foo',
        uri1.resolve(Uri.parse('foo')).toString());

    const uri2 = new Uri('http://www.google.com:8080/search');
    assertEquals(
        'http://www.google.com:8080/foo/bar',
        uri2.resolve(new Uri('foo/bar')).toString());

    const uri3 = new Uri('http://www.google.com:8080/search/');
    assertEquals(
        'http://www.google.com:8080/search/foo/bar',
        uri3.resolve(new Uri('foo/bar')).toString());

    const uri4 = new Uri('foo');
    assertEquals('bar', uri4.resolve(new Uri('bar')).toString());

    assertEquals(
        'http://www.google.com:8080/search/..%2ffoo/bar',
        uri3.resolve(new Uri('..%2ffoo/bar')).toString());
  },

  testDomainResolution() {
    assertEquals(
        'https://www.google.com/foo/bar',
        new Uri('https://www.fark.com:443/search/')
            .resolve(new Uri('//www.google.com/foo/bar'))
            .toString());

    assertEquals(
        'http://www.google.com/',
        Uri.resolve('http://www.fark.com/search/', '//www.google.com/')
            .toString());
  },

  testQueryResolution() {
    assertEquals(
        'http://www.google.com/search?q=new%20search',
        Uri.parse('http://www.google.com/search?q=old+search')
            .resolve(Uri.parse('?q=new%20search'))
            .toString());

    assertEquals(
        'http://www.google.com/search?q=new%20search',
        Uri.parse('http://www.google.com/search?q=old+search#hi')
            .resolve(Uri.parse('?q=new%20search'))
            .toString());
  },

  testFragmentResolution() {
    assertEquals(
        'http://www.google.com/foo/bar?q=hi#there',
        Uri.resolve('http://www.google.com/foo/bar?q=hi', '#there').toString());

    assertEquals(
        'http://www.google.com/foo/bar?q=hi#there',
        Uri.resolve('http://www.google.com/foo/bar?q=hi#you', '#there')
            .toString());
  },

  testBogusResolution() {
    const uri = Uri.parse('some:base/url')
                    .resolve(Uri.parse('a://completely.different/url'));
    assertEquals('a://completely.different/url', uri.toString());
  },

  testDotSegmentsRemovalRemoveLeadingDots() {
    // Test removing leading "../" and "./"
    assertEquals('bar', Uri.removeDotSegments('../bar'));
    assertEquals('bar', Uri.removeDotSegments('./bar'));
    assertEquals('bar', Uri.removeDotSegments('.././bar'));
    assertEquals('bar', Uri.removeDotSegments('.././bar'));
  },

  testDotSegmentRemovalRemoveSingleDot() {
    // Tests replacing "/./" with "/"
    assertEquals('/foo/bar', Uri.removeDotSegments('/foo/./bar'));
    assertEquals('/bar/', Uri.removeDotSegments('/bar/./'));

    // Test replacing trailing "/." with "/"
    assertEquals('/', Uri.removeDotSegments('/.'));
    assertEquals('/bar/', Uri.removeDotSegments('/bar/.'));
  },

  testDotSegmentRemovalRemoveDoubleDot() {
    // Test resolving "/../"
    assertEquals('/bar', Uri.removeDotSegments('/foo/../bar'));
    assertEquals('/', Uri.removeDotSegments('/bar/../'));

    // Test resolving trailing "/.."
    assertEquals('/', Uri.removeDotSegments('/..'));
    assertEquals('/', Uri.removeDotSegments('/bar/..'));
    assertEquals('/foo/', Uri.removeDotSegments('/foo/bar/..'));
  },

  testDotSegmentRemovalRemovePlainDots() {
    // RFC 3986, section 5.2.4, point 2.D.
    // Test resolving plain ".." or "."
    assertEquals('', Uri.removeDotSegments('.'));
    assertEquals('', Uri.removeDotSegments('..'));
  },

  testPathConcatenation() {
    // Check accordenance with RFC 3986, section 5.2.4
    assertResolvedEquals('bar', '', 'bar');
    assertResolvedEquals('/bar', '/', 'bar');
    assertResolvedEquals('/bar', '/foo', '/bar');
    assertResolvedEquals('/foo/foo', '/foo/bar', 'foo');
  },

  testPathConcatenationDontRemoveForEmptyUri() {
    // Resolving URIs with empty path should not result in dot segments removal.
    // See: algorithm in section 5.2.2: code inside 'if (R.path == "")' clause.
    assertResolvedEquals('/search/../foo', '/search/../foo', '');
    assertResolvedEquals('/search/./foo', '/search/./foo', '');
  },

  testParameterGetters() {
    function assertArraysEqual(l1, l2) {
      if (!l1 || !l2) {
        assertEquals(l1, l2);
        return;
      }
      const l1s = l1.toString();
      const l2s = l2.toString();

      assertEquals(l1s, l2s);
      assertEquals(l1s, l1.length, l2.length);
      for (let i = 0; i < l1.length; ++i) {
        assertEquals(`part ${i} of ` + l1.length + ' in ' + l1s, l1[i], l2[i]);
      }
    }

    assertArraysEqual(
        ['v1', 'v2'],
        Uri.parse('/path?a=b&key=v1&c=d&key=v2&keywithsuffix=v3')
            .getParameterValues('key'));

    assertArraysEqual(
        ['v1', 'v2'],
        Uri.parse('/path?a=b&keY=v1&c=d&KEy=v2&keywithsuffix=v3', true)
            .getParameterValues('kEy'));

    assertEquals(
        'v1',
        Uri.parse('/path?key=v1&c=d&keywithsuffix=v3&key=v2')
            .getParameterValue('key'));

    assertEquals(
        'v1',
        Uri.parse('/path?kEY=v1&c=d&keywithsuffix=v3&key=v2', true)
            .getParameterValue('Key'));

    assertEquals(
        'v1=v2', Uri.parse('/path?key=v1=v2', true).getParameterValue('key'));

    assertEquals(
        'v1=v2=v3',
        Uri.parse('/path?key=v1=v2=v3', true).getParameterValue('key'));

    assertArraysEqual(
        undefined,
        Uri.parse('/path?key=v1&c=d&keywithsuffix=v3&key=v2')
            .getParameterValue('nosuchkey'));

    // test boundary conditions
    assertArraysEqual(
        ['v1', 'v2'],
        Uri.parse('/path?key=v1&c=d&key=v2&keywithsuffix=v3')
            .getParameterValues('key'));
    assertArraysEqual(
        ['v1', 'v2'],
        Uri.parse('/path?key=v1&c=d&keywithsuffix=v3&key=v2')
            .getParameterValues('key'));

    // test no =
    assertArraysEqual([''], Uri.parse('/path?key').getParameterValues('key'));
    assertArraysEqual(
        [''], Uri.parse('/path?key', true).getParameterValues('key'));

    assertArraysEqual(
        [''], Uri.parse('/path?foo=bar&key').getParameterValues('key'));
    assertArraysEqual(
        [''], Uri.parse('/path?foo=bar&key', true).getParameterValues('key'));

    assertEquals('', Uri.parse('/path?key').getParameterValue('key'));
    assertEquals('', Uri.parse('/path?key', true).getParameterValue('key'));

    assertEquals('', Uri.parse('/path?foo=bar&key').getParameterValue('key'));
    assertEquals(
        '', Uri.parse('/path?foo=bar&key', true).getParameterValue('key'));

    let u = new Uri('/path?a=b&key=v1&c=d&key=v2&keywithsuffix=v3');
    assertArraysEqual(u.getParameterValues('a'), ['b']);
    assertArraysEqual(u.getParameterValues('key'), ['v1', 'v2']);
    assertArraysEqual(u.getParameterValues('c'), ['d']);
    assertArraysEqual(u.getParameterValues('keywithsuffix'), ['v3']);
    assertArraysEqual(u.getParameterValues('KeyWITHSuffix'), []);

    // Make sure constructing from another URI preserves case-sensitivity
    let u2 = new Uri(u);
    assertArraysEqual(u2.getParameterValues('a'), ['b']);
    assertArraysEqual(u2.getParameterValues('key'), ['v1', 'v2']);
    assertArraysEqual(u2.getParameterValues('c'), ['d']);
    assertArraysEqual(u2.getParameterValues('keywithsuffix'), ['v3']);
    assertArraysEqual(u2.getParameterValues('KeyWITHSuffix'), []);

    u = new Uri('/path?a=b&key=v1&c=d&kEy=v2&keywithsuffix=v3', true);
    assertArraysEqual(u.getParameterValues('A'), ['b']);
    assertArraysEqual(u.getParameterValues('keY'), ['v1', 'v2']);
    assertArraysEqual(u.getParameterValues('c'), ['d']);
    assertArraysEqual(u.getParameterValues('keyWITHsuffix'), ['v3']);

    // Make sure constructing from another URI preserves case-insensitivity
    u2 = new Uri(u);
    assertArraysEqual(u2.getParameterValues('A'), ['b']);
    assertArraysEqual(u2.getParameterValues('keY'), ['v1', 'v2']);
    assertArraysEqual(u2.getParameterValues('c'), ['d']);
    assertArraysEqual(u2.getParameterValues('keyWITHsuffix'), ['v3']);
  },

  testRemoveParameter() {
    assertEquals(
        '/path?a=b&c=d&keywithsuffix=v3',
        Uri.parse('/path?a=b&key=v1&c=d&key=v2&keywithsuffix=v3')
            .removeParameter('key')
            .toString());
  },

  testParameterSetters() {
    assertEquals(
        '/path?a=b&key=newval&c=d&keywithsuffix=v3',
        Uri.parse('/path?a=b&key=v1&c=d&key=v2&keywithsuffix=v3')
            .setParameterValue('key', 'newval')
            .toString());

    assertEquals(
        '/path?a=b&c=d&keywithsuffix=v3&key=1&key=2&key=3',
        Uri.parse('/path?a=b&key=v1&c=d&key=v2&keywithsuffix=v3')
            .setParameterValues('key', ['1', '2', '3'])
            .toString());

    assertEquals(
        '/path',
        Uri.parse('/path?key=v1&key=v2')
            .setParameterValues('key', [])
            .toString());

    // Test case-insensitive setters
    assertEquals(
        '/path?a=b&key=newval&c=d&keywithsuffix=v3',
        Uri.parse('/path?a=b&key=v1&c=d&key=v2&keywithsuffix=v3', true)
            .setParameterValue('KEY', 'newval')
            .toString());

    assertEquals(
        '/path?a=b&c=d&keywithsuffix=v3&key=1&key=2&key=3',
        Uri.parse('/path?a=b&key=v1&c=d&key=v2&keywithsuffix=v3', true)
            .setParameterValues('kEY', ['1', '2', '3'])
            .toString());
  },

  testEncoding() {
    assertEquals('/foo bar baz', Uri.parse('/foo%20bar%20baz').getPath());
    assertEquals('/foo+bar+baz', Uri.parse('/foo+bar+baz').getPath());
  },

  testSetScheme() {
    const uri = new Uri('http://www.google.com:80/path?q=query#fragmento');

    uri.setScheme('https');
    assertTrue(uri.hasScheme());
    assertEquals('https', uri.getScheme());
    assertEquals(
        'https://www.google.com:80/path?q=query#fragmento', uri.toString());

    uri.setScheme(encodeURIComponent('ab cd'), true);
    assertTrue(uri.hasScheme());
    assertEquals('ab cd', uri.getScheme());
    assertEquals(
        'ab%20cd://www.google.com:80/path?q=query#fragmento', uri.toString());

    uri.setScheme('http:');
    assertTrue(uri.hasScheme());
    assertEquals('http', uri.getScheme());
    assertEquals(
        'http://www.google.com:80/path?q=query#fragmento', uri.toString());

    uri.setScheme('');
    assertFalse(uri.hasScheme());
    assertEquals('', uri.getScheme());
    assertEquals('//www.google.com:80/path?q=query#fragmento', uri.toString());
  },

  testSetDomain() {
    const uri = new Uri('http://www.google.com:80/path?q=query#fragmento');

    uri.setDomain('\u1e21oogle.com');
    assertTrue(uri.hasDomain());
    assertEquals('\u1e21oogle.com', uri.getDomain());
    assertEquals(
        'http://%E1%B8%A1oogle.com:80/path?q=query#fragmento', uri.toString());

    uri.setDomain(encodeURIComponent('\u1e21oogle.com'), true);
    assertTrue(uri.hasDomain());
    assertEquals('\u1e21oogle.com', uri.getDomain());
    assertEquals(
        'http://%E1%B8%A1oogle.com:80/path?q=query#fragmento', uri.toString());

    uri.setDomain('');
    assertFalse(uri.hasDomain());
    assertEquals('', uri.getDomain());
    assertEquals('http:/path?q=query#fragmento', uri.toString());
  },

  testSetPort() {
    const uri = new Uri('http://www.google.com:80/path?q=query#fragmento');

    assertThrows(() => {
      uri.setPort(-1);
    });
    assertEquals(80, uri.getPort());

    assertThrows(() => {
      uri.setPort('a');
    });
    assertEquals(80, uri.getPort());

    uri.setPort(443);
    assertTrue(uri.hasPort());
    assertEquals(443, uri.getPort());
    assertEquals(
        'http://www.google.com:443/path?q=query#fragmento', uri.toString());

    // TODO(chrishenry): This is undocumented, but exist in previous unit
    // test. We should clarify whether this is intended (alternatively,
    // setPort(0) also works).
    uri.setPort(null);
    assertFalse(uri.hasPort());
    assertEquals(null, uri.getPort());
    assertEquals(
        'http://www.google.com/path?q=query#fragmento', uri.toString());
  },

  testSetPath() {
    const uri = new Uri('http://www.google.com:80/path?q=query#fragmento');

    uri.setPath('/search path/');
    assertTrue(uri.hasPath());
    assertEquals('/search path/', uri.getPath());
    assertEquals(
        'http://www.google.com:80/search%20path/?q=query#fragmento',
        uri.toString());

    uri.setPath(encodeURIComponent('search path 2/'), true);
    assertTrue(uri.hasPath());
    assertEquals('search path 2%2F', uri.getPath());
    assertEquals(
        'http://www.google.com:80/search%20path%202%2F?q=query#fragmento',
        uri.toString());

    uri.setPath('');
    assertFalse(uri.hasPath());
    assertEquals('', uri.getPath());
    assertEquals('http://www.google.com:80?q=query#fragmento', uri.toString());
  },

  testSetFragment() {
    const uri = new Uri('http://www.google.com:80/path?q=query#fragmento');

    uri.setFragment('foo?bar=a b&baz=2');
    assertTrue(uri.hasFragment());
    assertEquals('foo?bar=a b&baz=2', uri.getFragment());
    assertEquals(
        'http://www.google.com:80/path?q=query#foo?bar=a%20b&baz=2',
        uri.toString());

    uri.setFragment(encodeURIComponent('foo?bar=a b&baz=3'), true);
    assertTrue(uri.hasFragment());
    assertEquals('foo?bar=a b&baz=3', uri.getFragment());
    assertEquals(
        'http://www.google.com:80/path?q=query#foo?bar=a%20b&baz=3',
        uri.toString());

    uri.setFragment('');
    assertFalse(uri.hasFragment());
    assertEquals('', uri.getFragment());
    assertEquals('http://www.google.com:80/path?q=query', uri.toString());
  },

  testSetUserInfo() {
    const uri = new Uri('http://www.google.com:80/path?q=query#fragmento');

    uri.setUserInfo('user:pw d');
    assertTrue(uri.hasUserInfo());
    assertEquals('user:pw d', uri.getUserInfo());
    assertEquals(
        'http://user:pw%20d@www.google.com:80/path?q=query#fragmento',
        uri.toString());

    uri.setUserInfo(encodeURIComponent('user:pw d2'), true);
    assertTrue(uri.hasUserInfo());
    assertEquals('user:pw d2', uri.getUserInfo());
    assertEquals(
        'http://user:pw%20d2@www.google.com:80/path?q=query#fragmento',
        uri.toString());

    uri.setUserInfo('user');
    assertTrue(uri.hasUserInfo());
    assertEquals('user', uri.getUserInfo());
    assertEquals(
        'http://user@www.google.com:80/path?q=query#fragmento', uri.toString());

    uri.setUserInfo('');
    assertFalse(uri.hasUserInfo());
    assertEquals('', uri.getUserInfo());
    assertEquals(
        'http://www.google.com:80/path?q=query#fragmento', uri.toString());
  },

  testSetParameterValues() {
    const uri = new Uri('http://www.google.com:80/path?q=query#fragmento');

    uri.setParameterValues('q', ['foo', 'other query']);
    assertEquals(
        'http://www.google.com:80/path?q=foo&q=other%20query#fragmento',
        uri.toString());

    uri.setParameterValues('lang', 'en');
    assertEquals(
        'http://www.google.com:80/path?q=foo&q=other%20query&lang=en#fragmento',
        uri.toString());
  },

  testTreatmentOfAt1() {
    let uri = new Uri('http://www.google.com?q=johndoe@gmail.com');
    assertEquals('http', uri.getScheme());
    assertEquals('www.google.com', uri.getDomain());
    assertEquals('johndoe@gmail.com', uri.getParameterValue('q'));

    uri = Uri.create(
        'http', null, 'www.google.com', null, null, 'q=johndoe@gmail.com',
        null);
    assertEquals('http://www.google.com?q=johndoe%40gmail.com', uri.toString());
  },

  testTreatmentOfAt2() {
    const uri = new Uri('http://test/~johndoe@gmail.com/foo');
    assertEquals('http', uri.getScheme());
    assertEquals('test', uri.getDomain());
    assertEquals('/~johndoe@gmail.com/foo', uri.getPath());

    assertEquals(
        'http://test/~johndoe@gmail.com/foo',
        Uri.create(
               'http', null, 'test', null, '/~johndoe@gmail.com/foo', null,
               null)
            .toString());
  },

  testTreatmentOfAt3() {
    const uri = new Uri('ftp://skroob:1234@teleport/~skroob@vacuum');
    assertEquals('ftp', uri.getScheme());
    assertEquals('skroob:1234', uri.getUserInfo());
    assertEquals('teleport', uri.getDomain());
    assertEquals('/~skroob@vacuum', uri.getPath());

    assertEquals(
        'ftp://skroob:1234@teleport/~skroob@vacuum',
        Uri.create(
               'ftp', 'skroob:1234', 'teleport', null, '/~skroob@vacuum', null,
               null)
            .toString());
  },

  testTreatmentOfAt4() {
    assertEquals(
        'ftp://darkhelmet:45%4078@teleport/~dhelmet@vacuum',
        Uri.create(
               'ftp', 'darkhelmet:45@78', 'teleport', null, '/~dhelmet@vacuum',
               null, null)
            .toString());
  },

  testSameDomain1() {
    const uri1 = 'http://www.google.com/a';
    const uri2 = 'http://www.google.com/b';
    assertTrue(Uri.haveSameDomain(uri1, uri2));
    assertTrue(Uri.haveSameDomain(uri2, uri1));
  },

  testSameDomain2() {
    const uri1 = 'http://www.google.com:1234/a';
    const uri2 = 'http://www.google.com/b';
    assertFalse(Uri.haveSameDomain(uri1, uri2));
    assertFalse(Uri.haveSameDomain(uri2, uri1));
  },

  testSameDomain3() {
    const uri1 = 'www.google.com/a';
    const uri2 = 'http://www.google.com/b';
    assertFalse(Uri.haveSameDomain(uri1, uri2));
    assertFalse(Uri.haveSameDomain(uri2, uri1));
  },

  testSameDomain4() {
    const uri1 = '/a';
    const uri2 = 'http://www.google.com/b';
    assertFalse(Uri.haveSameDomain(uri1, uri2));
    assertFalse(Uri.haveSameDomain(uri2, uri1));
  },

  testSameDomain5() {
    const uri1 = 'http://www.google.com/a';
    const uri2 = 'http://mail.google.com/b';
    assertFalse(Uri.haveSameDomain(uri1, uri2));
    assertFalse(Uri.haveSameDomain(uri2, uri1));
  },

  testSameDomain6() {
    const uri1 = '/a';
    const uri2 = '/b';
    assertTrue(Uri.haveSameDomain(uri1, uri2));
    assertTrue(Uri.haveSameDomain(uri2, uri1));
  },

  testMakeUnique() {
    const uri1 = new Uri('http://www.google.com/setgmail');
    uri1.makeUnique();
    const uri2 = new Uri('http://www.google.com/setgmail');
    uri2.makeUnique();
    assertTrue(uri1.getQueryData().containsKey(Uri.RANDOM_PARAM));
    assertTrue(uri1.toString() != uri2.toString());
  },

  testSetReadOnly() {
    const uri = new Uri('http://www.google.com/setgmail');
    uri.setReadOnly(true);
    assertThrows(() => {
      uri.setParameterValue('cant', 'dothis');
    });
  },

  testSetReadOnlyChained() {
    const uri = new Uri('http://www.google.com/setgmail').setReadOnly(true);
    assertThrows(() => {
      uri.setParameterValue('cant', 'dothis');
    });
  },

  testQueryDataCount() {
    const qd = new Uri.QueryData('a=A&b=B&a=A2&b=B2&c=C');
    assertEquals(5, qd.getCount());
  },

  testQueryDataRemove() {
    const qd = new Uri.QueryData('a=A&b=B&a=A2&b=B2&c=C');
    qd.remove('c');
    assertEquals(4, qd.getCount());
    assertEquals('a=A&a=A2&b=B&b=B2', String(qd));
    qd.remove('a');
    assertEquals(2, qd.getCount());
    assertEquals('b=B&b=B2', String(qd));
  },

  testQueryDataClear() {
    const qd = new Uri.QueryData('a=A&b=B&a=A2&b=B2&c=C');
    qd.clear();
    assertEquals(0, qd.getCount());
    assertEquals('', String(qd));
  },

  testQueryDataIsEmpty() {
    let qd = new Uri.QueryData('a=A&b=B&a=A2&b=B2&c=C');
    qd.remove('a');
    assertFalse(qd.isEmpty());
    qd.remove('b');
    assertFalse(qd.isEmpty());
    qd.remove('c');
    assertTrue(qd.isEmpty());

    qd = new Uri.QueryData('a=A&b=B&a=A2&b=B2&c=C');
    qd.clear();
    assertTrue(qd.isEmpty());

    qd = new Uri.QueryData('');
    assertTrue(qd.isEmpty());
  },

  testQueryDataContainsKey() {
    let qd = new Uri.QueryData('a=A&b=B&a=A2&b=B2&c=C');
    assertTrue(qd.containsKey('a'));
    assertTrue(qd.containsKey('b'));
    assertTrue(qd.containsKey('c'));
    qd.remove('a');
    assertFalse(qd.containsKey('a'));
    assertTrue(qd.containsKey('b'));
    assertTrue(qd.containsKey('c'));
    qd.remove('b');
    assertFalse(qd.containsKey('a'));
    assertFalse(qd.containsKey('b'));
    assertTrue(qd.containsKey('c'));
    qd.remove('c');
    assertFalse(qd.containsKey('a'));
    assertFalse(qd.containsKey('b'));
    assertFalse(qd.containsKey('c'));

    qd = new Uri.QueryData('a=A&b=B&a=A2&b=B2&c=C');
    qd.clear();
    assertFalse(qd.containsKey('a'));
    assertFalse(qd.containsKey('b'));
    assertFalse(qd.containsKey('c'));

    // Test case-insensitive
    qd = new Uri.QueryData('aaa=A&bbb=B&aaa=A2&bbbb=B2&ccc=C', true);
    assertTrue(qd.containsKey('aaa'));
    assertTrue(qd.containsKey('bBb'));
    assertTrue(qd.containsKey('CCC'));

    qd = new Uri.QueryData('a=b=c');
    assertTrue(qd.containsKey('a'));
    assertFalse(qd.containsKey('b'));
  },

  testQueryDataContainsValue() {
    let qd = new Uri.QueryData('a=A&b=B&a=A2&b=B2&c=C');

    assertTrue(qd.containsValue('A'));
    assertTrue(qd.containsValue('B'));
    assertTrue(qd.containsValue('A2'));
    assertTrue(qd.containsValue('B2'));
    assertTrue(qd.containsValue('C'));
    qd.remove('a');
    assertFalse(qd.containsValue('A'));
    assertTrue(qd.containsValue('B'));
    assertFalse(qd.containsValue('A2'));
    assertTrue(qd.containsValue('B2'));
    assertTrue(qd.containsValue('C'));
    qd.remove('b');
    assertFalse(qd.containsValue('A'));
    assertFalse(qd.containsValue('B'));
    assertFalse(qd.containsValue('A2'));
    assertFalse(qd.containsValue('B2'));
    assertTrue(qd.containsValue('C'));
    qd.remove('c');
    assertFalse(qd.containsValue('A'));
    assertFalse(qd.containsValue('B'));
    assertFalse(qd.containsValue('A2'));
    assertFalse(qd.containsValue('B2'));
    assertFalse(qd.containsValue('C'));

    qd = new Uri.QueryData('a=A&b=B&a=A2&b=B2&c=C');
    qd.clear();
    assertFalse(qd.containsValue('A'));
    assertFalse(qd.containsValue('B'));
    assertFalse(qd.containsValue('A2'));
    assertFalse(qd.containsValue('B2'));
    assertFalse(qd.containsValue('C'));

    qd = new Uri.QueryData('a=b=c');
    assertTrue(qd.containsValue('b=c'));
    assertFalse(qd.containsValue('b'));
    assertFalse(qd.containsValue('c'));
  },

  testQueryDataGetKeys() {
    let qd = new Uri.QueryData('a=A&b=B&a=A2&b=B2&c=C=extra');

    assertEquals('aabbc', qd.getKeys().join(''));
    qd.remove('a');
    assertEquals('bbc', qd.getKeys().join(''));
    qd.add('d', 'D');
    qd.add('d', 'D');
    assertEquals('bbcdd', qd.getKeys().join(''));

    // Test case-insensitive
    qd = new Uri.QueryData('A=A&B=B&a=A2&b=B2&C=C=extra', true);

    assertEquals('aabbc', qd.getKeys().join(''));
    qd.remove('a');
    assertEquals('bbc', qd.getKeys().join(''));
    qd.add('d', 'D');
    qd.add('D', 'D');
    assertEquals('bbcdd', qd.getKeys().join(''));
  },

  testQueryDataForEach() {
    const qd = new Uri.QueryData('a=A&b=B&a=A2&b=B2&c=C=extra');

    const calls = [];
    qd.forEach((value, key) => {
      calls.push([value, key]);
    });
    assertArrayEquals(
        [
          // value, key
          ['A', 'a'],
          ['A2', 'a'],
          ['B', 'b'],
          ['B2', 'b'],
          ['C=extra', 'c'],
        ],
        calls);
  },

  testQueryDataGetValues() {
    const qd = new Uri.QueryData('a=A&b=B&a=A2&b=B2&c=C=extra');

    assertArrayEquals(['A', 'A2', 'B', 'B2', 'C=extra'], qd.getValues());
    qd.remove('a');
    assertArrayEquals(['B', 'B2', 'C=extra'], qd.getValues());
    qd.add('d', 'D');
    qd.add('d', 'D');
    assertArrayEquals(['B', 'B2', 'C=extra', 'D', 'D'], qd.getValues());

    qd.add('e', new String('Eee'));
    assertArrayEquals(['B', 'B2', 'C=extra', 'D', 'D', 'Eee'], qd.getValues());

    assertArrayEquals(['Eee'], qd.getValues('e'));
  },

  testQueryDataSet() {
    const qd = new Uri.QueryData('a=A&b=B&a=A2&b=B2&c=C');

    qd.set('d', 'D');
    assertEquals('a=A&a=A2&b=B&b=B2&c=C&d=D', String(qd));
    qd.set('d', 'D2');
    assertEquals('a=A&a=A2&b=B&b=B2&c=C&d=D2', String(qd));
    qd.set('a', 'A3');
    assertEquals('a=A3&b=B&b=B2&c=C&d=D2', String(qd));
    qd.remove('a');
    qd.set('a', 'A4');
    // this is different in IE and Mozilla so we cannot use the toString to test
    assertEquals('A4', qd.get('a'));
  },

  testQueryDataGet() {
    let qd = new Uri.QueryData('a=A&b=B&a=A2&b=B2&c=C=extra');

    assertEquals('A', qd.get('a'));
    assertEquals('B', qd.get('b'));
    assertEquals('C=extra', qd.get('c'));
    assertEquals('Default', qd.get('d', 'Default'));

    qd = new Uri.QueryData('a=A&b=B&a=A2&b=B2&c=C=extra', true);

    assertEquals('A', qd.get('A'));
    assertEquals('B', qd.get('b'));
    assertEquals('C=extra', qd.get('C'));
    assertEquals('Default', qd.get('D', 'Default'));

    // Some unit tests pass undefined to get method (even though the type
    // for key is {string}). This is not caught by JsCompiler as
    // tests aren't typically compiled.
    assertUndefined(qd.get(/** @type {?} */ (undefined)));
  },

  testQueryDataSetValues() {
    const qd = new Uri.QueryData('a=A&b=B&a=A2&b=B2&c=C');

    qd.setValues('a', ['A3', 'A4', 'A5']);
    assertEquals('b=B&b=B2&c=C&a=A3&a=A4&a=A5', String(qd));
    qd.setValues('d', ['D']);
    assertEquals('b=B&b=B2&c=C&a=A3&a=A4&a=A5&d=D', String(qd));
    qd.setValues('e', []);
    assertEquals('b=B&b=B2&c=C&a=A3&a=A4&a=A5&d=D', String(qd));
  },

  testQueryDataSetIgnoreCase() {
    const qd = new Uri.QueryData('aaA=one&BBb=two&cCc=three');
    assertEquals('one', qd.get('aaA'));
    assertEquals(undefined, qd.get('aaa'));
    qd.setIgnoreCase(true);
    assertEquals('one', qd.get('aaA'));
    assertEquals('one', qd.get('aaa'));
    qd.setIgnoreCase(false);
    assertEquals(undefined, qd.get('aaA'));
    assertEquals('one', qd.get('aaa'));
    qd.add('DdD', 'four');
    assertEquals('four', qd.get('DdD'));
    assertEquals(undefined, qd.get('ddd'));
  },

  testQueryDataSetIgnoreCaseWithMultipleValues() {
    const qd = new Uri.QueryData('aaA=one&aaA=two');
    qd.setIgnoreCase(true);
    assertArrayEquals(['one', 'two'], qd.getValues('aaA'));
    assertArrayEquals(['one', 'two'], qd.getValues('aaa'));
  },

  testQueryDataExtend() {
    let qd1 = new Uri.QueryData('a=A&b=B&c=C');
    let qd2 = new Uri.QueryData('d=D&e=E');
    qd1.extend(qd2);
    assertEquals('a=A&b=B&c=C&d=D&e=E', String(qd1));

    qd1 = new Uri.QueryData('a=A&b=B&c=C');
    qd2 = new Uri.QueryData('d=D&e=E');
    const qd3 = new Uri.QueryData('f=F&g=G');
    qd1.extend(qd2, qd3);
    assertEquals('a=A&b=B&c=C&d=D&e=E&f=F&g=G', String(qd1));

    qd1 = new Uri.QueryData('a=A&b=B&c=C');
    qd2 = new Uri.QueryData('a=A&c=C');
    qd1.extend(qd2);
    assertEquals('a=A&a=A&b=B&c=C&c=C', String(qd1));
  },

  testQueryDataCreateFromMap() {
    assertEquals('', String(Uri.QueryData.createFromMap({})));
    assertEquals(
        'a=A&b=B&c=C',
        String(Uri.QueryData.createFromMap({a: 'A', b: 'B', c: 'C'})));
    assertEquals(
        'a=foo%26bar', String(Uri.QueryData.createFromMap({a: 'foo&bar'})));
  },

  testQueryDataCreateFromMapWithArrayValues() {
    const obj = {'key': ['1', '2', '3']};
    const qd = Uri.QueryData.createFromMap(obj);
    assertEquals('key=1&key=2&key=3', qd.toString());
    qd.add('breakCache', 1);
    obj.key.push('4');
    assertEquals('key=1&key=2&key=3&breakCache=1', qd.toString());
  },

  testQueryDataCreateFromKeysValues() {
    assertEquals('', String(Uri.QueryData.createFromKeysValues([], [])));
    assertEquals(
        'a=A&b=B&c=C',
        String(Uri.QueryData.createFromKeysValues(
            ['a', 'b', 'c'], ['A', 'B', 'C'])));
    assertEquals(
        'a=A&a=B&a=C',
        String(Uri.QueryData.createFromKeysValues(
            ['a', 'a', 'a'], ['A', 'B', 'C'])));
  },

  testQueryDataAddMultipleValuesWithSameKey() {
    const qd = new Uri.QueryData();
    qd.add('abc', 'v');
    qd.add('abc', 'v2');
    qd.add('abc', 'v3');
    assertEquals('abc=v&abc=v2&abc=v3', qd.toString());
  },

  testQueryDataAddWithArray() {
    const qd = new Uri.QueryData();
    qd.add('abc', ['v', 'v2']);
    assertEquals('abc=v%2Cv2', qd.toString());
  },

  testFragmentEncoding() {
    const allowedInFragment = /[A-Za-z0-9\-._~!$&'()*+,;=:@/?]/g;

    const sb = [];
    for (let i = 33; i < 500; i++) {  // arbitrarily use first 500 chars.
      sb.push(String.fromCharCode(i));
    }
    const testString = sb.join('');

    let fragment = new Uri().setFragment(testString).toString();

    // Remove first '#' character.
    fragment = fragment.substr(1);

    // Strip all percent encoded characters, as they're ok.
    fragment = fragment.replace(/%[0-9A-F][0-9A-F]/g, '');

    // Remove allowed characters.
    fragment = fragment.replace(allowedInFragment, '');

    // Only illegal characters should remain, which is a fail.
    assertEquals('String should be empty', 0, fragment.length);
  },

  testStrictDoubleEncodingRemoval() {
    let url = Uri.parse('dummy/a%25invalid');
    assertEquals('dummy/a%25invalid', url.toString());
    url = Uri.parse('dummy/a%252fdouble-encoded-slash');
    assertEquals('dummy/a%252fdouble-encoded-slash', url.toString());
    url = Uri.parse('https://example.com/a%25%2f%25bcd%25%25');
    assertEquals('https://example.com/a%25%2f%25bcd%25%25', url.toString());
  },

  testComponentsAfterUriCreate() {
    const createdUri = Uri.create(
        '%40',   // scheme
        '%41',   // user info
        '%42',   // domain
        43,      // port
        '%44',   // path
        '%45',   // query
        '%46');  // fragment

    assertEquals('%40', createdUri.getScheme());
    assertEquals('%41', createdUri.getUserInfo());
    assertEquals('%42', createdUri.getDomain());
    assertEquals(43, createdUri.getPort());
    assertEquals('%44', createdUri.getPath());
    assertEquals('%2545', createdUri.getQuery());  // returns encoded value
    assertEquals('%45', createdUri.getDecodedQuery());
    assertEquals('%2545', createdUri.getEncodedQuery());
    assertEquals('%46', createdUri.getFragment());
  },

  testSetQueryAndGetParameterValue() {
    const uri = new Uri();

    // Sets query as decoded string.
    uri.setQuery('i=j&k');
    assertEquals('?i=j&k', uri.toString());
    assertEquals('i=j&k', uri.getDecodedQuery());
    assertEquals('i=j&k', uri.getEncodedQuery());
    assertEquals('i=j&k', uri.getQuery());  // returns encoded value
    assertEquals('j', uri.getParameterValue('i'));
    assertEquals('', uri.getParameterValue('k'));

    // Sets query as encoded string.
    uri.setQuery('i=j&k', true);
    assertEquals('?i=j&k', uri.toString());
    assertEquals('i=j&k', uri.getDecodedQuery());
    assertEquals('i=j&k', uri.getEncodedQuery());
    assertEquals('i=j&k', uri.getQuery());  // returns encoded value
    assertEquals('j', uri.getParameterValue('i'));
    assertEquals('', uri.getParameterValue('k'));

    // Sets query as decoded string.
    uri.setQuery('i=j%26k');
    assertEquals('?i=j%2526k', uri.toString());
    assertEquals('i=j%26k', uri.getDecodedQuery());
    assertEquals('i=j%2526k', uri.getEncodedQuery());
    assertEquals('i=j%2526k', uri.getQuery());  // returns encoded value
    assertEquals('j%26k', uri.getParameterValue('i'));
    assertUndefined(uri.getParameterValue('k'));

    // Sets query as encoded string.
    uri.setQuery('i=j%26k', true);
    assertEquals('?i=j%26k', uri.toString());
    assertEquals('i=j&k', uri.getDecodedQuery());
    assertEquals('i=j%26k', uri.getEncodedQuery());
    assertEquals('i=j%26k', uri.getQuery());  // returns encoded value
    assertEquals('j&k', uri.getParameterValue('i'));
    assertUndefined(uri.getParameterValue('k'));
  },

  testSetParameterValueAndGetQuery() {
    const uri = new Uri();

    uri.setParameterValue('a', 'b&c');
    assertEquals('?a=b%26c', uri.toString());
    assertEquals('a=b&c', uri.getDecodedQuery());
    assertEquals('a=b%26c', uri.getEncodedQuery());
    assertEquals('a=b%26c', uri.getQuery());  // returns encoded value

    uri.setParameterValue('a', 'b%26c');
    assertEquals('?a=b%2526c', uri.toString());
    assertEquals('a=b%26c', uri.getDecodedQuery());
    assertEquals('a=b%2526c', uri.getEncodedQuery());
    assertEquals('a=b%2526c', uri.getQuery());  // returns encoded value
  },

  testQueryNotModified() {
    assertEquals('?foo', new Uri('?foo').toString());
    assertEquals('?foo=', new Uri('?foo=').toString());
    assertEquals('?foo=bar', new Uri('?foo=bar').toString());
    assertEquals('?&=&=&', new Uri('?&=&=&').toString());
  },

  testRelativePathEscapesColon() {
    assertEquals(
        'javascript%3aalert(1)',
        new Uri().setPath('javascript:alert(1)').toString());
  },

  testAbsolutePathDoesNotEscapeColon() {
    assertEquals(
        '/javascript:alert(1)', new Uri('/javascript:alert(1)').toString());
  },

  testColonInPathNotUnescaped() {
    assertEquals(
        '/javascript%3aalert(1)', new Uri('/javascript%3aalert(1)').toString());
    assertEquals(
        'javascript%3aalert(1)', new Uri('javascript%3aalert(1)').toString());
    assertEquals(
        'javascript:alert(1)', new Uri('javascript:alert(1)').toString());
    assertEquals(
        'http://www.foo.bar/path:with:colon/x',
        new Uri('http://www.foo.bar/path:with:colon/x').toString());
    assertEquals(
        '//www.foo.bar/path:with:colon/x',
        new Uri('//www.foo.bar/path:with:colon/x').toString());
  },

  testGetQueryForEmptyString() {
    let queryData = new Uri.QueryData('a=b&c=d');
    assertArrayEquals(['b', 'd'], queryData.getValues());
    assertArrayEquals([], queryData.getValues(''));

    queryData = new Uri.QueryData('a=b&c=d&=e');
    assertArrayEquals(['e'], queryData.getValues(''));
  },

  testRestrictedCharactersArePreserved() {
    const uri = new Uri(
        'ht%74p://hos%74.example.%2f.com/pa%74h%2f-with-embedded-slash/');
    assertEquals('http', uri.getScheme());
    assertEquals('host.example.%2f.com', uri.getDomain());
    assertEquals('/path%2f-with-embedded-slash/', uri.getPath());
    assertEquals(
        'http://host.example.%2f.com/path%2f-with-embedded-slash/',
        uri.toString());
  },

  testFileUriWithNoDomainToString() {
    // Regression test for https://github.com/google/closure-library/issues/104.
    const uri = new Uri('file:///a/b');
    assertEquals('file:///a/b', uri.toString());
  },
});
