"""A trust token issuer implementing issuer protocol TrustTokenV3VOPRF.

TrustTokenV3VOPRF uses the elliptic curve P-384 for cryptographic operations.
Since this implementation is intended to be used for testing it is tailored
specifically for operations over P-384.

Parts of the documentation use the TLS presentation language (https://datatracker.ietf.org/doc/html/rfc8446#section-3)
to describe the structures used in the trust token API.

Token issuance example:
```
issuer = create_trust_token_issuer()
issuance_response = issue_trust_token(issuer=issuer,
                                      request_data=request_data)
```
where `request_data` is the contents of the `Sec-Private-State-Token` header.
"""

from __future__ import annotations

import base64
import hashlib
import importlib
import json
import logging
import os
import sys
from typing import List, Union

import ecdsa
import ecdsa.ellipticcurve

wpt_internal_dir = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
if wpt_internal_dir not in sys.path:
    sys.path.insert(0, wpt_internal_dir)

h2f = importlib.import_module("trust-tokens.resources.hash_to_field")
logger = logging.getLogger()

# The modulus of elliptic curve P-384
MODULUS_P384 = ecdsa.NIST384p.curve.p
# The order of elliptic curve P-384
ORDER_P384 = ecdsa.NIST384p.order
# The length in bytes of the order of curve P-384
ORDER_P384_LEN = 384 // 8
# The length of the trust token nonce
TRUST_TOKEN_NONCE_LEN = 64
# The WPT host to use in the key commitment
WPT_HOST = "https://web-platform.test:8444"
# The default redemption record to return in a redemption response
DEFAULT_REDEMPTION_RECORD = bytes("dummy redemption record", encoding="utf-8")


def bytes_to_base64_str(value: bytes) -> str:
    """Returns a Base64 string from bytes."""
    return base64.b64encode(value).decode("utf-8")


def base64_str_to_bytes(s: str) -> bytes:
    """Returns bytes from a Base64 string."""
    return base64.b64decode(s)


class DataBuffer:
    """A helper class that reads values from a byte string.

    This class helps to parse issuance and redemption requests.

    Attributes:
        buffer: The byte string to read values from.
        offset: The offset into the input buffer.
    """

    def __init__(self, buffer: bytes):
        self.buffer = buffer
        self.offset = 0

    def read_bytes(self, size: int) -> bytes:
        """Reads `size` bytes from the buffer and increments the offset by the
        same amount.

        Args:
            size: The number of bytes to read.

        Returns:
            A byte string containing the bytes read.

        Raises:
            BufferError: If `size` is greater than the number of bytes remaining
                in the buffer.
        """
        if self.offset + size > len(self.buffer):
            raise BufferError(
                "Failed to read bytes from buffer - too few bytes remain")
        value = self.buffer[self.offset:self.offset + size]
        self.offset += size
        return value

    def read_int(self, size: int, signed: bool = False) -> int:
        """Parses `size` bytes from the buffer as an integer and increments the
        offset by `size`.

        Args:
            size: The number of bytes to parse.
            signed: Whether the integer should be parsed using two's complement.

        Returns:
            The parsed integer.
        """
        value = self.read_bytes(size)
        return int.from_bytes(value, byteorder="big", signed=signed)


def issue_request_from_string(s: str) -> IssueRequest:
    """Creates an `IssueRequest` from a Base64 string.

    The decoded byte string must have the form:
    ```
    struct {
        uint16 count;
        ECPoint nonces[count];
    } IssueRequest;
    ```
    """
    decoded_bytes = base64_str_to_bytes(s)
    buf = DataBuffer(decoded_bytes)

    count = buf.read_int(2)
    nonces = []
    while len(nonces) < count:
        value = buf.read_bytes(ECPoint.length)
        ec_point = ECPoint(value)
        nonces.append(ec_point)
    return IssueRequest(count=count, nonces=nonces)


def redeem_request_from_string(s: str) -> RedeemRequest:
    """Creates a `RedeemRequest` from a Base64 string.

    The decoded byte string must have the form:
    ```
    struct {
        uint32 key_id;
        opaque nonce<nonce_size>;
        ECPoint W;
    } Token;

    struct {
        opaque token<1..2^16-1>; // Bytestring containing a serialized Token struct.
        opaque client_data<1..2^16-1>;
    } RedeemRequest;
    ```
    """
    decoded_bytes = base64_str_to_bytes(s)
    buf = DataBuffer(decoded_bytes)

    token_len = buf.read_int(2)
    key_id = buf.read_int(4)
    nonce = buf.read_bytes(TRUST_TOKEN_NONCE_LEN)

    value = buf.read_bytes(ECPoint.length)
    point = ECPoint(value)

    client_data_len = buf.read_int(2)
    client_data = buf.read_bytes(client_data_len)

    return RedeemRequest(key_id=key_id,
                         nonce=nonce,
                         point=point,
                         client_data=client_data)


class Scalar:
    """An integer value that can be used in elliptic curve operations over the
    curve P-384.

    A scalar is encoded as a big-endian byte string with length equal to that of
    the order of P-384.

    Attributes:
        value: The value of the scalar.
    """

    def __init__(self, value: int):
        self.value = value

    def __str__(self) -> str:
        return str(self.value)

    def to_bytes(self) -> bytes:
        """Returns the scalar value as big-endian bytes."""
        return self.value.to_bytes(ORDER_P384_LEN, byteorder="big")


class ECPoint:
    """An elliptic curve point used in elliptic curve operations over the curve
    P-384.

    Scalar multiplication can be written as `s*p` or `p*s`, where `s` is a
    `Scalar` and `p` is an `ECPoint`. An `ECPoint` is encoded as an X9.62
    uncompressed point.

     Attributes:
        value: An X9.62 encoded elliptic curve point.
        point: The `PointJacobi` representation of the point.
    """

    length = 1 + 2 * ORDER_P384_LEN

    def __init__(self, value: bytes):
        self.value = value
        self.point = ecdsa.ellipticcurve.PointJacobi.from_bytes(
            ecdsa.NIST384p.curve, value)

    def __str__(self) -> str:
        return str(self.__dict__)

    def __repr__(self) -> str:
        return self.to_string()

    def __add__(self, other):
        return self.point + other.point

    def __mul__(self, scalar: Scalar):
        res: ecdsa.ellipticcurve.PointJacobi = scalar.value * self.point
        return ECPoint(res.to_bytes(encoding="uncompressed"))

    def __rmul__(self, scalar: Scalar):
        return self * scalar

    def to_bytes(self) -> bytes:
        """Returns the point as an X9.62 uncompressed byte string."""
        return self.point.to_bytes(encoding="uncompressed")

    def to_string(self) -> str:
        """Returns the point as a Base64 string."""
        return bytes_to_base64_str(self.to_bytes())


def hash_to_field(
        message: Union[bytes, str],
        count: int,
        dst: str,
) -> List[List[int]]:
    """Converts a string to one or more elements of the finite field F of P-384.

    See https://datatracker.ietf.org/doc/html/draft-irtf-cfrg-hash-to-curve-07#section-5
    for implementation details.

    Attributes:
        dst: The domain separation tag.
        message: The message to convert.
        count: The number of field elements to generate.

    Returns:
        A list of the form `(u_0, ..., u_(count - 1))` where
            `u_i = (e_0, ..., e_(m - 1))` and m is the extension degree of F.
            For P-384, m is equal to 1.
    """
    expander = h2f.XMDExpander(dst=dst,
                               hash_fn=hashlib.sha512,
                               security_param=192)
    return h2f.hash_to_field(
        msg=message,
        count=count,
        modulus=MODULUS_P384,
        degree=1,
        blen=72,
        expander=expander,
    )


def hash_to_scalar(
        msg: Union[bytes, str],
        count: int,
        dst: str = "TrustToken VOPRF Experiment V2 HashToScalar\0",
) -> List[List[int]]:
    """Converts a string to one or more scalars.

    This is similar to `hash_to_field` except that the modulus used is
    `ORDER_P384` instead of `MODULUS_P384`.

    Returns:
        A list of the form `(u_0, ..., u_(count - 1))` where
            `u_i = (e_0, ..., e_(m - 1))` and m is the extension degree of F.
            For P-384, m is equal to 1.
    """
    expander = h2f.XMDExpander(dst=dst,
                               hash_fn=hashlib.sha512,
                               security_param=192)
    return h2f.hash_to_field(msg=msg,
                             count=count,
                             modulus=ORDER_P384,
                             degree=1,
                             blen=72,
                             expander=expander)


class TrustTokenSecretKey:
    """A secret key for a trust token issuer.

    The value of a secret key is a scalar. The secret key is used to sign
    blinded tokens from the client.

    Attributes:
        id: The ID of the secret key.
        value: The value of the secret key.
    """

    def __init__(self, id: int, value: int):
        self.id = id
        self.value = Scalar(value)

    def __str__(self) -> str:
        return str(self.__dict__)

    def __repr__(self) -> str:
        return self.to_string()

    def to_bytes(self) -> bytes:
        """Returns the value of the secret key as bytes."""
        buf = self.value.to_bytes()
        return buf

    def to_string(self) -> str:
        """Returns the secret key as a Base64 string."""
        return bytes_to_base64_str(self.to_bytes())


class TrustTokenPublicKey:
    """A public key for a trust token issuer.

    The value of a public key is an elliptic curve point. A public key is
    generated from a secret key.

    Attributes:
        id: The ID of the secret key.
        value: The value of the secret key.
    """

    def __init__(self, id: int, value: ECPoint):
        self.id = id
        self.value = value

    def __str__(self) -> str:
        return str(self.__dict__)

    def __repr__(self) -> str:
        return self.to_string()

    def to_bytes(self) -> bytes:
        """Returns the public key as bytes.

        The structure takes the form:
        ```
        struct {
            uint32 id;
            ECPoint pub;
        } TrustTokenPublicKey;
        ```
        """
        buf = self.id.to_bytes(4, byteorder="big")
        buf += self.value.to_bytes()
        return buf

    def to_string(self) -> str:
        """Returns the public key as a Base64 string suitable for use in key
        commitments."""
        return bytes_to_base64_str(self.to_bytes())


class TrustTokenKeyPair:
    """A key pair for a trust token issuer.

    Attributes:
        id: The ID associated with the key pair.
        public_key: The public component of the key pair.
        secret_key: The secret component of the key pair.
    """

    def __init__(self, id: int, public_key: TrustTokenPublicKey,
                 secret_key: TrustTokenSecretKey):
        self.id = id
        self.public_key = public_key
        self.secret_key = secret_key

    def __str__(self) -> str:
        return str(self.__dict__)


def generate_key_pair() -> TrustTokenKeyPair:
    """Generates a key pair for a trust token issuer.

    For testing purposes, the secret value is fixed. The public value is the
    product of the secret value and the generator of P-384.

    Returns:
        The key pair.
    """
    # priv cannot be 0
    priv = ORDER_P384 - 1
    # Changes to the public key must be reflected in the key commitment
    pub: ecdsa.ellipticcurve.PointJacobi = ecdsa.NIST384p.generator * priv
    # Fix the key ID
    id = 0
    public_key = TrustTokenPublicKey(
        id, ECPoint(pub.to_bytes(encoding="uncompressed")))
    secret_key = TrustTokenSecretKey(id, priv)
    return TrustTokenKeyPair(id=id,
                             public_key=public_key,
                             secret_key=secret_key)


class KeyCommitment:
    """A key commitment for the trust token issuer.

    A key commitment defines the trust token protocol version and public key
    information that the issuer will use for trust token operations.

    Attributes:
        protocol_version: The issuer protocol version.
        id: The ID of the key commitment.
        batchsize: The batch size for token issuance.
        public_keys: The public keys for the issuer.
        host: The server origin that maps to this key commitment.
    """

    def __init__(
            self,
            protocol_version: str,
            id: int,
            batchsize: int,
            public_keys: List[TrustTokenPublicKey],
            host: str,
    ):
        self.protocol_version = protocol_version
        self.id = id
        self.batchsize = batchsize
        self.public_keys = public_keys
        self.host = host

    def __str__(self) -> str:
        return str(self.__dict__)

    def __repr__(self) -> str:
        return self.to_string()

    def to_string(self, indent=None) -> str:
        """Returns a JSON representation of the key commitment valid for trust
        token issuers to use.
        """
        key_commitment = {}
        key_commitment["protocol_version"] = self.protocol_version
        key_commitment["id"] = self.id
        key_commitment["batchsize"] = self.batchsize
        key_commitment["keys"] = {}
        for public_key in self.public_keys:
            key_commitment["keys"][public_key.id] = {
                "Y": repr(public_key),
                # epoch timestamp in microseconds
                # Friday, December 31, 9999 11:59:59 PM GMT
                "expiry": "253402300799000000",
            }
        hosts_to_key_commitments = {
            self.host: {
                self.protocol_version: key_commitment
            }
        }
        return json.dumps(hosts_to_key_commitments, indent=indent)


class IssueRequest:
    """A trust token issuance request.

    Attributes:
        count: The number of token nonces in the request.
        nonces: A list of elliptic curve points to be used as token nonces.
    """

    def __init__(self, count: int, nonces: List[ECPoint]):
        self.count = count
        self.nonces = nonces

    def __str__(self) -> str:
        return str(self.__dict__)


class SignedNonce:
    """An entity representing a token signed by the issuer.

    Attributes:
        value: The elliptic curve point representing the signed token.
    """

    def __init__(self, value: ECPoint):
        self.value = value

    def __str__(self) -> str:
        return str(self.__dict__)

    def __repr__(self) -> str:
        return self.to_string()

    def to_bytes(self) -> bytes:
        """Returns the signed nonce as bytes."""
        return self.value.to_bytes()

    def to_string(self) -> str:
        """Returns the signed nonce as a Base64 encoded string."""
        return bytes_to_base64_str(self.to_bytes())


class IssueResponse:
    """The response to a token issuance request.

    The issuance response contains a list of signed nonces along with a DLEQ
    proof.

    Attributes:
        issued: The number of tokens issued.
        key_id: The ID of the key used for signing.
        signed: The list of signed nonces.
        proof: The DLEQ proof.
    """

    def __init__(self, issued: int, key_id: int, signed: List[SignedNonce],
                 proof: bytes):
        self.issued = issued
        self.key_id = key_id
        self.signed = signed
        self.proof = proof

    def __str__(self) -> str:
        return str(self.__dict__)

    def __repr__(self) -> str:
        return self.to_string()

    def to_bytes(self) -> bytes:
        """Returns the issue response as bytes.

        The structure has the form:
        ```
        struct {
            uint16 issued;
            uint32 key_id = keyID;
            SignedNonce signed[issued];
            opaque proof<1..2^16-1>; // Length-prefixed form of DLEQProof.
        } IssueResponse;
        ```
        """
        buf = self.issued.to_bytes(2, byteorder="big")
        buf += self.key_id.to_bytes(4, byteorder="big")
        for nonce in self.signed:
            buf += nonce.to_bytes()
        buf += len(self.proof).to_bytes(2, byteorder="big")
        buf += self.proof
        return buf

    def to_string(self) -> str:
        return bytes_to_base64_str(self.to_bytes())


class RedeemRequest:
    """A trust token redemption request.

    Attributes:
        key_id: The ID of the key used to sign the trust token.
        nonce: The nonce part of the token.
        point: The elliptic curve point part of the token.
        client_data: Client data associated with the request.
    """

    def __init__(self, key_id: int, nonce: bytes, point: ECPoint,
                 client_data: bytes):
        self.key_id = key_id
        self.nonce = nonce
        self.point = point
        self.client_data = client_data

    def __str__(self) -> str:
        return str(self.__dict__)


class RedeemResponse:
    """The response to a trust token redemption request.

    The client should treat the entire response as the redemption record. By
    default, for testing purposes, this class returns a fixed byte string as the
    redemption record.

    Attributes:
        redemption_record: Bytes representing a redemption record.
    """

    def __init__(self, redemption_record: bytes = DEFAULT_REDEMPTION_RECORD):
        # TODO(crbug/1335977): add per-token information.
        self.redemption_record = redemption_record

    def __str__(self) -> str:
        return str(self.__dict__)

    def to_bytes(self) -> bytes:
        """Returns the redemption response as bytes."""
        return self.redemption_record

    def to_string(self) -> str:
        """Returns the redemption response as a string."""
        return self.redemption_record.decode("utf-8")


class TrustTokenIssuer:
    """A trust token issuer implementation.

    For simplicity the issuer has a single key pair.

    Attributes:
        key_pair: A key pair to use for trust token operations.
        max_batchsize: The max batch size for trust tokens.
        key_commitment: The key commitment for this issuer.
        host: The server origin for this issuer.
    """

    KEY_COMMITMENT_ID = 1

    def __init__(self, key_pair: TrustTokenKeyPair, max_batchsize: int,
                 host: str):
        self.key_pair = key_pair
        self.max_batchsize = max_batchsize
        self.key_commitment = KeyCommitment(
            protocol_version="TrustTokenV3VOPRF",
            id=TrustTokenIssuer.KEY_COMMITMENT_ID,
            batchsize=self.max_batchsize,
            public_keys=[self.key_pair.public_key],
            host=host,
        )
        self.host = host

    def __str__(self) -> str:
        return str(self.__dict__)

    def issue(self, key_id: int, request: IssueRequest) -> IssueResponse:
        """Parses an issuance request and returns a response with a valid DLEQ
        proof.
        """
        logger.info(f"Issuance request {request}")
        blinded_tokens: List[ECPoint] = []
        zs: List[ECPoint] = []
        es: List[int] = []
        batch = self.key_pair.public_key.value.to_bytes()
        signed_nonces = []

        num_to_issue = min(request.count, 1)
        for i in range(num_to_issue):
            blinded_token = request.nonces[i]
            secret_key = self.key_pair.secret_key
            z = blinded_token * secret_key.value
            signed_nonces.append(SignedNonce(z))
            batch += blinded_token.to_bytes()
            batch += z.to_bytes()
            blinded_tokens.append(blinded_token)
            zs.append(z)

        # Batch DLEQ
        for i in range(num_to_issue):
            buf = bytes("DLEQ BATCH\0", encoding="utf-8")
            buf += batch
            buf += i.to_bytes(2, byteorder="big")
            e = hash_to_scalar(msg=buf, count=1)[0][0]
            es.append(e)

        bt_batch: ecdsa.ellipticcurve.PointJacobi = sum(
            (bt.point * e for bt, e in zip(blinded_tokens, es)),
            start=ecdsa.ellipticcurve.INFINITY,
        )
        z_batch: ecdsa.ellipticcurve.PointJacobi = sum(
            (z.point * e for z, e in zip(zs, es)),
            start=ecdsa.ellipticcurve.INFINITY)

        proof = self._dleq_generate(self.key_pair, bt_batch, z_batch)

        return IssueResponse(issued=len(signed_nonces),
                             key_id=key_id,
                             signed=signed_nonces,
                             proof=proof)

    def _dleq_generate(
            self,
            key_pair: TrustTokenKeyPair,
            bt_batch: ecdsa.ellipticcurve.PointJacobi,
            z_batch: ecdsa.ellipticcurve.PointJacobi,
    ) -> bytes:
        """Generates a DLEQ proof that the client may verify.

        Args:
            key_pair: The issuer's key pair.
            bt_batch: The batched form of the blinded tokens.
            z_batch: The batched form of the signed tokens.

        Returns:
            A byte string representing the DLEQ proof.
        """
        # Fix the random number r
        r = ORDER_P384 - 1
        k0: ecdsa.ellipticcurve.PointJacobi = ecdsa.NIST384p.generator * r
        k1: ecdsa.ellipticcurve.PointJacobi = bt_batch * r

        buf = bytes("DLEQ\0", encoding="utf-8")
        buf += key_pair.public_key.value.to_bytes()
        buf += bt_batch.to_bytes(encoding="uncompressed")
        buf += z_batch.to_bytes(encoding="uncompressed")
        buf += k0.to_bytes(encoding="uncompressed")
        buf += k1.to_bytes(encoding="uncompressed")
        c = hash_to_scalar(msg=buf, count=1)[0][0]
        u = (r + c * key_pair.secret_key.value.value) % ORDER_P384

        buf = c.to_bytes(ORDER_P384_LEN, byteorder="big")
        buf += u.to_bytes(ORDER_P384_LEN, byteorder="big")
        return buf

    def redeem(self, request: RedeemRequest) -> RedeemResponse:
        """Parses a redemption request and returns a response.

        This method ignores the input request and simply returns a redemption
        response with a fixed byte string as the redemption record.
        """
        return RedeemResponse()


def create_trust_token_issuer() -> TrustTokenIssuer:
    """Creates a trust token issuer. A key pair is generated and used to
    initialize the issuer.

    Returns:
        The trust token issuer.
    """
    key_pair = generate_key_pair()
    issuer = TrustTokenIssuer(key_pair=key_pair,
                              max_batchsize=1,
                              host=WPT_HOST)
    return issuer


def issue_trust_token(issuer: TrustTokenIssuer, request_data: str,
                      key_id) -> IssueResponse:
    """Sends an issuance request to an issuer.

    Args:
        issuer: A trust token issuer.
        request_data: The contents of the `Sec-Private-State-Token` header in the
            request.
        key_id: The ID of the issuer key to use to sign tokens.

    Returns:
        The issuance response.
    """
    request = issue_request_from_string(request_data)
    logger.info("Issue request: %s", request)
    return issuer.issue(key_id=key_id, request=request)


def redeem_trust_token(issuer: TrustTokenIssuer,
                       request_data: str) -> RedeemResponse:
    """Sends a redemption request to an issuer.

    Args:
        issuer: A trust token issuer.
        request_data: The contents of the `Sec-Private-State-Token` header in the
            request.

    Returns:
        The redemption response.
    """
    request = redeem_request_from_string(request_data)
    logger.info("Redemption request: %s", request)
    return issuer.redeem(request=request)


if __name__ == "__main__":
    # Log to the console a key commitment that can be imported into the browser
    logger = logging.getLogger(__name__)
    logger.setLevel(logging.INFO)
    ch = logging.StreamHandler()
    logger.addHandler(ch)

    issuer = create_trust_token_issuer()
    key_commitment = issuer.key_commitment
    logger.info(f"Key commitment: {key_commitment.to_string()}")
