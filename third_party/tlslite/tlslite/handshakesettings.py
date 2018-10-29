# Authors: 
#   Trevor Perrin
#   Dave Baggett (Arcode Corporation) - cleanup handling of constants
#   Yngve Pettersen (ported by Paul Sokolovsky) - TLS 1.2
#
# See the LICENSE file for legal information regarding use of this file.

"""Class for setting handshake parameters."""

from .constants import CertificateType
from .utils import cryptomath
from .utils import cipherfactory

CIPHER_NAMES = ["aes128gcm", "rc4", "aes256", "aes128", "3des"]
MAC_NAMES = ["sha", "sha256", "aead"] # Don't allow "md5" by default.
ALL_MAC_NAMES = MAC_NAMES + ["md5"]
KEY_EXCHANGE_NAMES = ["rsa", "dhe_rsa", "ecdhe_rsa", "srp_sha", "srp_sha_rsa", "dh_anon"]
CIPHER_IMPLEMENTATIONS = ["openssl", "pycrypto", "python"]
CERTIFICATE_TYPES = ["x509"]
TLS_INTOLERANCE_TYPES = ["alert", "close", "reset"]

class HandshakeSettings(object):
    """This class encapsulates various parameters that can be used with
    a TLS handshake.
    @sort: minKeySize, maxKeySize, cipherNames, macNames, certificateTypes,
    minVersion, maxVersion

    @type minKeySize: int
    @ivar minKeySize: The minimum bit length for asymmetric keys.

    If the other party tries to use SRP, RSA, or Diffie-Hellman
    parameters smaller than this length, an alert will be
    signalled.  The default is 1023.

    @type maxKeySize: int
    @ivar maxKeySize: The maximum bit length for asymmetric keys.

    If the other party tries to use SRP, RSA, or Diffie-Hellman
    parameters larger than this length, an alert will be signalled.
    The default is 8193.

    @type cipherNames: list
    @ivar cipherNames: The allowed ciphers.

    The allowed values in this list are 'aes256', 'aes128', '3des', and
    'rc4'.  If these settings are used with a client handshake, they
    determine the order of the ciphersuites offered in the ClientHello
    message.

    If these settings are used with a server handshake, the server will
    choose whichever ciphersuite matches the earliest entry in this
    list.

    NOTE:  If '3des' is used in this list, but TLS Lite can't find an
    add-on library that supports 3DES, then '3des' will be silently
    removed.

    The default value is ['rc4', 'aes256', 'aes128', '3des'].

    @type macNames: list
    @ivar macNames: The allowed MAC algorithms.
    
    The allowed values in this list are 'sha' and 'md5'.
    
    The default value is ['sha'].


    @type certificateTypes: list
    @ivar certificateTypes: The allowed certificate types.

    The only allowed certificate type is 'x509'.  This list is only used with a
    client handshake.  The client will advertise to the server which certificate
    types are supported, and will check that the server uses one of the
    appropriate types.


    @type minVersion: tuple
    @ivar minVersion: The minimum allowed SSL/TLS version.

    This variable can be set to (3,0) for SSL 3.0, (3,1) for TLS 1.0, (3,2) for
    TLS 1.1, or (3,3) for TLS 1.2.  If the other party wishes to use a lower
    version, a protocol_version alert will be signalled.  The default is (3,1).

    @type maxVersion: tuple
    @ivar maxVersion: The maximum allowed SSL/TLS version.

    This variable can be set to (3,0) for SSL 3.0, (3,1) for TLS 1.0, (3,2) for
    TLS 1.1, or (3,3) for TLS 1.2.  If the other party wishes to use a higher
    version, a protocol_version alert will be signalled.  The default is (3,3).
    (WARNING: Some servers may (improperly) reject clients which offer support
    for TLS 1.1.  In this case, try lowering maxVersion to (3,1)).

    @type tlsIntolerant: tuple
    @ivar tlsIntolerant: The TLS ClientHello version which the server
    simulates intolerance of.

    If tlsIntolerant is not None, the server will simulate TLS version
    intolerance by aborting the handshake in response to all TLS versions
    tlsIntolerant or higher.

    @type tlsIntoleranceType: str
    @ivar tlsIntoleranceType: How the server should react when simulating TLS
    intolerance.

    The allowed values are "alert" (return a fatal handshake_failure alert),
    "close" (abruptly close the connection), and "reset" (send a TCP reset).
    
    @type useExperimentalTackExtension: bool
    @ivar useExperimentalTackExtension: Whether to enabled TACK support.

    @type alertAfterHandshake: bool
    @ivar alertAfterHandshake: If true, the server will send a fatal
    alert immediately after the handshake completes.

    @type enableChannelID: bool
    @ivar enableChannelID: If true, the server supports channel ID.

    @type enableExtendedMasterSecret: bool
    @ivar enableExtendedMasterSecret: If true, the server supports the extended
    master secret TLS extension and will negotiated it with supporting clients.

    @type supportedTokenBindingParams: list
    @ivar supportedTokenBindingParams: A list of token binding parameters that
    the server supports when negotiating token binding. List values are integers
    corresponding to the TokenBindingKeyParameters enum in the Token Binding
    Negotiation spec (draft-ietf-tokbind-negotiation-00). Values are in server's
    preference order, with most preferred params first.

    @type simulateTLS13Downgrade: bool
    @ivar simulateTLS13Downgrade: If true, the server will simulate a TLS 1.3
    to TLS 1.2 downgrade in the ServerHello random.

    @type simulateTLS12Downgrade: bool
    @ivar simulateTLS12Downgrade: If true, the server will simulate a TLS 1.2
    to TLS 1.1 downgrade in the ServerHello random.

    Note that TACK support is not standardized by IETF and uses a temporary
    TLS Extension number, so should NOT be used in production software.

    @type alpnProtos: list of strings.
    @param alpnProtos: A list of supported upper layer protocols to use in the
    Application-Layer Protocol Negotiation Extension (RFC 7301).  For the
    client, the order does not matter.  For the server, the list is in
    decreasing order of preference.
    """
    def __init__(self):
        self.minKeySize = 1023
        self.maxKeySize = 8193
        self.cipherNames = CIPHER_NAMES
        self.macNames = MAC_NAMES
        self.keyExchangeNames = KEY_EXCHANGE_NAMES
        self.cipherImplementations = CIPHER_IMPLEMENTATIONS
        self.certificateTypes = CERTIFICATE_TYPES
        self.minVersion = (3,1)
        self.maxVersion = (3,3)
        self.tlsIntolerant = None
        self.tlsIntoleranceType = 'alert'
        self.useExperimentalTackExtension = False
        self.alertAfterHandshake = False
        self.enableChannelID = True
        self.enableExtendedMasterSecret = True
        self.supportedTokenBindingParams = []
        self.alpnProtos = None
        self.simulateTLS13Downgrade = False
        self.simulateTLS12Downgrade = False

    # Validates the min/max fields, and certificateTypes
    # Filters out unsupported cipherNames and cipherImplementations
    def _filter(self):
        other = HandshakeSettings()
        other.minKeySize = self.minKeySize
        other.maxKeySize = self.maxKeySize
        other.cipherNames = self.cipherNames
        other.macNames = self.macNames
        other.keyExchangeNames = self.keyExchangeNames
        other.cipherImplementations = self.cipherImplementations
        other.certificateTypes = self.certificateTypes
        other.minVersion = self.minVersion
        other.maxVersion = self.maxVersion
        other.tlsIntolerant = self.tlsIntolerant
        other.tlsIntoleranceType = self.tlsIntoleranceType
        other.alertAfterHandshake = self.alertAfterHandshake
        other.enableChannelID = self.enableChannelID
        other.enableExtendedMasterSecret = self.enableExtendedMasterSecret
        other.supportedTokenBindingParams = self.supportedTokenBindingParams
        other.alpnProtos = self.alpnProtos;
        other.simulateTLS13Downgrade = self.simulateTLS13Downgrade
        other.simulateTLS12Downgrade = self.simulateTLS12Downgrade

        if not cipherfactory.tripleDESPresent:
            other.cipherNames = [e for e in self.cipherNames if e != "3des"]
        if len(other.cipherNames)==0:
            raise ValueError("No supported ciphers")
        if len(other.certificateTypes)==0:
            raise ValueError("No supported certificate types")

        if not cryptomath.m2cryptoLoaded:
            other.cipherImplementations = \
                [e for e in other.cipherImplementations if e != "openssl"]
        if not cryptomath.pycryptoLoaded:
            other.cipherImplementations = \
                [e for e in other.cipherImplementations if e != "pycrypto"]
        if len(other.cipherImplementations)==0:
            raise ValueError("No supported cipher implementations")

        if other.minKeySize<512:
            raise ValueError("minKeySize too small")
        if other.minKeySize>16384:
            raise ValueError("minKeySize too large")
        if other.maxKeySize<512:
            raise ValueError("maxKeySize too small")
        if other.maxKeySize>16384:
            raise ValueError("maxKeySize too large")
        for s in other.cipherNames:
            if s not in CIPHER_NAMES:
                raise ValueError("Unknown cipher name: '%s'" % s)
        for s in other.macNames:
            if s not in ALL_MAC_NAMES:
                raise ValueError("Unknown MAC name: '%s'" % s)
        for s in other.keyExchangeNames:
            if s not in KEY_EXCHANGE_NAMES:
                raise ValueError("Unknown key exchange name: '%s'" % s)
        for s in other.cipherImplementations:
            if s not in CIPHER_IMPLEMENTATIONS:
                raise ValueError("Unknown cipher implementation: '%s'" % s)
        for s in other.certificateTypes:
            if s not in CERTIFICATE_TYPES:
                raise ValueError("Unknown certificate type: '%s'" % s)

        if other.tlsIntoleranceType not in TLS_INTOLERANCE_TYPES:
            raise ValueError(
                "Unknown TLS intolerance type: '%s'" % other.tlsIntoleranceType)

        if other.minVersion > other.maxVersion:
            raise ValueError("Versions set incorrectly")

        if not other.minVersion in ((3,0), (3,1), (3,2), (3,3)):
            raise ValueError("minVersion set incorrectly")

        if not other.maxVersion in ((3,0), (3,1), (3,2), (3,3)):
            raise ValueError("maxVersion set incorrectly")

        return other

    def _getCertificateTypes(self):
        l = []
        for ct in self.certificateTypes:
            if ct == "x509":
                l.append(CertificateType.x509)
            else:
                raise AssertionError()
        return l
