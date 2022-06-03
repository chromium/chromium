/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.net.IpAddressTest');
goog.setTestOnly();

const Integer = goog.require('goog.math.Integer');
const testSuite = goog.require('goog.testing.testSuite');
const {IpAddress, Ipv4Address, Ipv6Address} = goog.require('goog.net.ipaddress');

testSuite({
  testInvalidStrings() {
    assertEquals(null, IpAddress.fromString(''));
    assertEquals(null, IpAddress.fromString('016.016.016.016'));
    assertEquals(null, IpAddress.fromString('016.016.016'));
    assertEquals(null, IpAddress.fromString('016.016'));
    assertEquals(null, IpAddress.fromString('016'));
    assertEquals(null, IpAddress.fromString('000.000.000.000'));
    assertEquals(null, IpAddress.fromString('000'));
    assertEquals(null, IpAddress.fromString('0x0a.0x0a.0x0a.0x0a'));
    assertEquals(null, IpAddress.fromString('0x0a.0x0a.0x0a'));
    assertEquals(null, IpAddress.fromString('0x0a.0x0a'));
    assertEquals(null, IpAddress.fromString('0x0a'));
    assertEquals(null, IpAddress.fromString('42.42.42.42.42'));
    assertEquals(null, IpAddress.fromString('42.42.42'));
    assertEquals(null, IpAddress.fromString('42.42'));
    assertEquals(null, IpAddress.fromString('42'));
    assertEquals(null, IpAddress.fromString('42..42.42'));
    assertEquals(null, IpAddress.fromString('42..42.42.42'));
    assertEquals(null, IpAddress.fromString('42.42.42.42.'));
    assertEquals(null, IpAddress.fromString('42.42.42.42...'));
    assertEquals(null, IpAddress.fromString('.42.42.42.42'));
    assertEquals(null, IpAddress.fromString('...42.42.42.42'));
    assertEquals(null, IpAddress.fromString('42.42.42.-0'));
    assertEquals(null, IpAddress.fromString('42.42.42.+0'));
    assertEquals(null, IpAddress.fromString('.'));
    assertEquals(null, IpAddress.fromString('...'));
    assertEquals(null, IpAddress.fromString('bogus'));
    assertEquals(null, IpAddress.fromString('bogus.com'));
    assertEquals(null, IpAddress.fromString('192.168.0.1.com'));
    assertEquals(null, IpAddress.fromString('12345.67899.-54321.-98765'));
    assertEquals(null, IpAddress.fromString('257.0.0.0'));
    assertEquals(null, IpAddress.fromString('42.42.42.-42'));
    assertEquals(null, IpAddress.fromString('3ff3:::1'));
    assertEquals(null, IpAddress.fromString('3ffe::1.net'));
    assertEquals(null, IpAddress.fromString('3ffe::1::1'));
    assertEquals(null, IpAddress.fromString('1::2::3::4:5'));
    assertEquals(null, IpAddress.fromString('::7:6:5:4:3:2:'));
    assertEquals(null, IpAddress.fromString(':6:5:4:3:2:1::'));
    assertEquals(null, IpAddress.fromString('2001::db:::1'));
    assertEquals(null, IpAddress.fromString('FEDC:9878'));
    assertEquals(null, IpAddress.fromString('+1.+2.+3.4'));
    assertEquals(null, IpAddress.fromString('1.2.3.4e0'));
    assertEquals(null, IpAddress.fromString('::7:6:5:4:3:2:1:0'));
    assertEquals(null, IpAddress.fromString('7:6:5:4:3:2:1:0::'));
    assertEquals(null, IpAddress.fromString('9:8:7:6:5:4:3::2:1'));
    assertEquals(null, IpAddress.fromString('0:1:2:3::4:5:6:7'));
    assertEquals(null, IpAddress.fromString('3ffe:0:0:0:0:0:0:0:1'));
    assertEquals(null, IpAddress.fromString('3ffe::10000'));
    assertEquals(null, IpAddress.fromString('3ffe::goog'));
    assertEquals(null, IpAddress.fromString('3ffe::-0'));
    assertEquals(null, IpAddress.fromString('3ffe::+0'));
    assertEquals(null, IpAddress.fromString('3ffe::-1'));
    assertEquals(null, IpAddress.fromString(':'));
    assertEquals(null, IpAddress.fromString(':::'));
    assertEquals(null, IpAddress.fromString('a:'));
    assertEquals(null, IpAddress.fromString('::a:'));
    assertEquals(null, IpAddress.fromString('0xa::'));
    assertEquals(null, IpAddress.fromString('::1.2.3'));
    assertEquals(null, IpAddress.fromString('::1.2.3.4.5'));
    assertEquals(null, IpAddress.fromString('::1.2.3.4:'));
    assertEquals(null, IpAddress.fromString('1.2.3.4::'));
    assertEquals(null, IpAddress.fromString('2001:db8::1:'));
    assertEquals(null, IpAddress.fromString(':2001:db8::1'));
  },

  testVersion() {
    const ip4 = IpAddress.fromString('1.2.3.4');
    assertEquals(ip4.getVersion(), 4);

    let ip6 = IpAddress.fromString('2001:dead::beef:1');
    assertEquals(ip6.getVersion(), 6);

    ip6 = IpAddress.fromString('::192.168.1.1');
    assertEquals(ip6.getVersion(), 6);
  },

  testStringIpv4Address() {
    assertEquals('192.168.1.1', new Ipv4Address('192.168.1.1').toString());
    assertEquals('1.1.1.1', new Ipv4Address('1.1.1.1').toString());
    assertEquals('224.56.33.2', new Ipv4Address('224.56.33.2').toString());
    assertEquals(
        '255.255.255.255', new Ipv4Address('255.255.255.255').toString());
    assertEquals('0.0.0.0', new Ipv4Address('0.0.0.0').toString());
  },

  testIntIpv4Address() {
    const ip4Str = new Ipv4Address('1.1.1.1');
    const ip4Int = new Ipv4Address(new Integer([16843009], 0));

    assertTrue(ip4Str.equals(ip4Int));
    assertEquals(ip4Str.toString(), ip4Int.toString());

    assertThrows('Ipv4(-1)', goog.partial(Ipv4Address, Integer.fromInt(-1)));
    assertThrows(
        'Ipv4(2**32)', goog.partial(Ipv4Address, Integer.ONE.shiftLeft(32)));
  },

  testStringIpv6Address() {
    assertEquals(
        '1:2:3:4:5:6:7:8', new Ipv6Address('1:2:3:4:5:6:7:8').toString());
    assertEquals(
        '::1:2:3:4:5:6:7', new Ipv6Address('::1:2:3:4:5:6:7').toString());
    assertEquals(
        '1:2:3:4:5:6:7::', new Ipv6Address('1:2:3:4:5:6:7:0').toString());
    assertEquals(
        '2001:0:0:4::8', new Ipv6Address('2001:0:0:4:0:0:0:8').toString());
    assertEquals(
        '2001::4:5:6:7:8', new Ipv6Address('2001:0:0:4:5:6:7:8').toString());
    assertEquals(
        '2001::3:4:5:6:7:8', new Ipv6Address('2001:0:3:4:5:6:7:8').toString());
    assertEquals(
        '0:0:3::ffff', new Ipv6Address('0:0:3:0:0:0:0:ffff').toString());
    assertEquals(
        '::4:0:0:0:ffff', new Ipv6Address('0:0:0:4:0:0:0:ffff').toString());
    assertEquals(
        '::5:0:0:ffff', new Ipv6Address('0:0:0:0:5:0:0:ffff').toString());
    assertEquals('1::4:0:0:7:8', new Ipv6Address('1:0:0:4:0:0:7:8').toString());
    assertEquals('::', new Ipv6Address('0:0:0:0:0:0:0:0').toString());
    assertEquals('::1', new Ipv6Address('0:0:0:0:0:0:0:1').toString());
    assertEquals(
        '2001:658:22a:cafe::',
        new Ipv6Address('2001:0658:022a:cafe:0000:0000:0000:0000').toString());
    assertEquals('::102:304', new Ipv6Address('::1.2.3.4').toString());
    assertEquals(
        '::ffff:303:303', new Ipv6Address('::ffff:3.3.3.3').toString());
    assertEquals(
        '::ffff:ffff', new Ipv6Address('::255.255.255.255').toString());
  },

  testIntIpv6Address() {
    const ip6Str = new Ipv6Address('2001::dead:beef:1');
    const ip6Int =
        new Ipv6Address(new Integer([3203334145, 57005, 0, 536936448], 0));

    assertTrue(ip6Str.equals(ip6Int));
    assertEquals(ip6Str.toString(), ip6Int.toString());

    assertThrows('Ipv6(-1)', goog.partial(Ipv6Address, Integer.fromInt(-1)));
    assertThrows(
        'Ipv6(2**128)', goog.partial(Ipv6Address, Integer.ONE.shiftLeft(128)));
  },

  testDottedQuadIpv6() {
    new Ipv6Address('7::0.128.0.127');
    new Ipv6Address('7::0.128.0.128');
    new Ipv6Address('7::128.128.0.127');
    new Ipv6Address('7::0.128.128.127');
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testMappedIpv4Address() {
    const testAddresses = ['::ffff:1.2.3.4', '::FFFF:102:304'];
    const ipv4Str = '1.2.3.4';

    const ip1 = new Ipv6Address(testAddresses[0]);
    const ip2 = new Ipv6Address(testAddresses[1]);
    const ipv4 = new Ipv4Address(ipv4Str);

    assertTrue(ip1.isMappedIpv4Address());
    assertTrue(ip2.isMappedIpv4Address());
    assertTrue(ip1.equals(ip2));
    assertTrue(ipv4.equals(ip1.getMappedIpv4Address()));
    assertTrue(ipv4.equals(ip2.getMappedIpv4Address()));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testUriString() {
    const ip4Str = '192.168.1.1';
    const ip4Uri = IpAddress.fromUriString(ip4Str);
    const ip4 = IpAddress.fromString(ip4Str);
    assertTrue(ip4Uri.equals(ip4));

    const ip6Str = '2001:dead::beef:1';
    assertEquals(null, IpAddress.fromUriString(ip6Str));

    const ip6Uri = IpAddress.fromUriString(`[${ip6Str}]`);
    const ip6 = IpAddress.fromString(ip6Str);
    assertTrue(ip6Uri.equals(ip6));
    assertEquals(ip6Uri.toString(), ip6Str);
    assertEquals(ip6Uri.toUriString(), `[${ip6Str}]`);
  },

  testIsSiteLocal() {
    const siteLocalAddresses = [
      '10.0.0.0',
      '10.255.255.255',
      '172.16.0.0',
      '172.31.255.255',
      '192.168.0.0',
      '192.168.255.255',
      'fd00::',
      'fdff:ffff:ffff:ffff:ffff:ffff:ffff:ffff',
    ];
    siteLocalAddresses.forEach(siteLocalAddress => {
      assertTrue(IpAddress.fromString(siteLocalAddress).isSiteLocal());
    });

    const nonSiteLocalAddresses = [
      '9.255.255.255',
      '11.0.0.0',
      '172.15.255.255',
      '172.32.0.0',
      '192.167.255.255',
      '192.169.0.0',
      'fcff:ffff:ffff:ffff:ffff:ffff:ffff:ffff',
      'fe00::',
    ];
    nonSiteLocalAddresses.forEach(nonSiteLocalAddress => {
      assertFalse(IpAddress.fromString(nonSiteLocalAddress).isSiteLocal());
    });
  },

  testIsLinkLocal() {
    const linkLocalAddresses = [
      '169.254.0.0',
      '169.254.255.255',
      'fe80::',
      'febf:ffff:ffff:ffff:ffff:ffff:ffff:ffff',
    ];
    linkLocalAddresses.forEach(linkLocalAddress => {
      assertTrue(IpAddress.fromString(linkLocalAddress).isLinkLocal());
    });

    const nonLinkLocalAddresses = [
      '169.253.255.255',
      '169.255.0.0',
      'fe7f:ffff:ffff:ffff:ffff:ffff:ffff:ffff',
      'fec0::',
    ];
    nonLinkLocalAddresses.forEach(nonLinkLocalAddress => {
      assertFalse(IpAddress.fromString(nonLinkLocalAddress).isLinkLocal());
    });
  },
});
