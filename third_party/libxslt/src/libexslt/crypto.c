#define IN_LIBEXSLT
#include "libexslt/libexslt.h"

#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/parser.h>
#include <libxml/encoding.h>
#include <libxml/uri.h>

#include <libxslt/xsltutils.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/extensions.h>

#include "exslt.h"

#ifdef EXSLT_CRYPTO_ENABLED

#define HASH_DIGEST_LENGTH 32
#define MD5_DIGEST_LENGTH 16
#define SHA1_DIGEST_LENGTH 20

/* gcrypt rc4 can do 256 bit keys, but cryptoapi limit
   seems to be 128 for the default provider */
#define RC4_KEY_LENGTH 128

/* The following routines have been declared static - this should be
   reviewed to consider whether we want to expose them to the API
   exsltCryptoBin2Hex
   exsltCryptoHex2Bin
   exsltCryptoGcryptInit
   exsltCryptoGcryptHash
   exsltCryptoGcryptRc4Encrypt
   exsltCryptoGcryptRC4Decrypt
*/

/**
 * exsltCryptoBin2Hex:
 * @bin: binary blob to convert
 * @binlen: length of binary blob
 * @hex: buffer to store hex version of blob
 * @hexlen: length of buffer to store hex version of blob
 *
 * Helper function which encodes a binary blob as hex.
 */
static void
exsltCryptoBin2Hex (const unsigned char *bin, int binlen,
		    unsigned char *hex, int hexlen) {
    static const char bin2hex[] = { '0', '1', '2', '3',
	'4', '5', '6', '7',
	'8', '9', 'a', 'b',
	'c', 'd', 'e', 'f'
    };

    unsigned char lo, hi;
    int i, pos;
    for (i = 0, pos = 0; (i < binlen && pos < hexlen); i++) {
	lo = bin[i] & 0xf;
	hi = bin[i] >> 4;
	hex[pos++] = bin2hex[hi];
	hex[pos++] = bin2hex[lo];
    }

    hex[pos] = '\0';
}

/**
 * exsltCryptoHex2Bin:
 * @hex: hex version of blob to convert
 * @hexlen: length of hex buffer
 * @bin: destination binary buffer
 * @binlen: length of binary buffer
 *
 * Helper function which decodes a hex blob to binary
 */
static int
exsltCryptoHex2Bin (const unsigned char *hex, int hexlen,
		    unsigned char *bin, int binlen) {
    int i = 0, j = 0;
    unsigned char lo, hi, result, tmp;

    while (i < hexlen && j < binlen) {
	hi = lo = 0;

	tmp = hex[i++];
	if (tmp >= '0' && tmp <= '9')
	    hi = tmp - '0';
	else if (tmp >= 'a' && tmp <= 'f')
	    hi = 10 + (tmp - 'a');

	tmp = hex[i++];
	if (tmp >= '0' && tmp <= '9')
	    lo = tmp - '0';
	else if (tmp >= 'a' && tmp <= 'f')
	    lo = 10 + (tmp - 'a');

	result = (unsigned char) (hi << 4);
	result += lo;
	bin[j++] = result;
    }

    return j;
}

#if defined(_WIN32) && !defined(__CYGWIN__)

#define HAVE_CRYPTO
#define PLATFORM_HASH	exsltCryptoCryptoApiHash
#define PLATFORM_RC4_ENCRYPT exsltCryptoCryptoApiRc4Encrypt
#define PLATFORM_RC4_DECRYPT exsltCryptoCryptoApiRc4Decrypt
#define PLATFORM_MD4 CALG_MD4
#define PLATFORM_MD5 CALG_MD5
#define PLATFORM_SHA1 CALG_SHA1

#include <windows.h>
#include <wincrypt.h>
#ifdef _MSC_VER
#pragma comment(lib, "advapi32.lib")
#endif

static void
exsltCryptoCryptoApiReportError (xmlXPathParserContextPtr ctxt,
				 int line) {
    char *lpMsgBuf;
    DWORD dw = GetLastError ();

    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		   FORMAT_MESSAGE_FROM_SYSTEM, NULL, dw,
		   MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
		   &lpMsgBuf, 0, NULL);

    xsltTransformError (xsltXPathGetTransformContext (ctxt), NULL, NULL,
			"exslt:crypto error (line %d). %s", line,
			lpMsgBuf);
    LocalFree (lpMsgBuf);
}

static HCRYPTHASH
exsltCryptoCryptoApiCreateHash (xmlXPathParserContextPtr ctxt,
				HCRYPTPROV hCryptProv, ALG_ID algorithm,
				const unsigned char *msg, unsigned int msglen,
				char *dest, unsigned int destlen)
{
    HCRYPTHASH hHash = 0;
    DWORD dwHashLen = destlen;

    if (!CryptCreateHash (hCryptProv, algorithm, 0, 0, &hHash)) {
	exsltCryptoCryptoApiReportError (ctxt, __LINE__);
	return 0;
    }

    if (!CryptHashData (hHash, msg, msglen, 0)) {
	exsltCryptoCryptoApiReportError (ctxt, __LINE__);
	goto fail;
    }

    if (!CryptGetHashParam (hHash, HP_HASHVAL, (BYTE *) dest, &dwHashLen, 0)) {
	exsltCryptoCryptoApiReportError (ctxt, __LINE__);
	goto fail;
    }

  fail:
    return hHash;
}

/**
 * exsltCryptoCryptoApiHash:
 * @ctxt: an XPath parser context
 * @algorithm: hashing algorithm to use
 * @msg: text to be hashed
 * @msglen: length of text to be hashed
 * @dest: buffer to place hash result
 *
 * Helper function which hashes a message using MD4, MD5, or SHA1.
 * Uses Win32 CryptoAPI.
 */
static void
exsltCryptoCryptoApiHash (xmlXPathParserContextPtr ctxt,
			  ALG_ID algorithm, const char *msg,
			  unsigned long msglen,
			  char dest[HASH_DIGEST_LENGTH]) {
    HCRYPTPROV hCryptProv;
    HCRYPTHASH hHash;

    if (!CryptAcquireContext (&hCryptProv, NULL, NULL, PROV_RSA_FULL,
			      CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
	exsltCryptoCryptoApiReportError (ctxt, __LINE__);
	return;
    }

    hHash = exsltCryptoCryptoApiCreateHash (ctxt, hCryptProv,
					    algorithm, (unsigned char *) msg,
                                            msglen, dest, HASH_DIGEST_LENGTH);
    if (0 != hHash) {
	CryptDestroyHash (hHash);
    }

    CryptReleaseContext (hCryptProv, 0);
}

static void
exsltCryptoCryptoApiRc4Encrypt (xmlXPathParserContextPtr ctxt,
				const unsigned char *key,
				const unsigned char *msg, int msglen,
				unsigned char *dest, int destlen) {
    HCRYPTPROV hCryptProv;
    HCRYPTKEY hKey;
    HCRYPTHASH hHash;
    DWORD dwDataLen;
    char hash[HASH_DIGEST_LENGTH];

    if (msglen > destlen) {
	xsltTransformError (xsltXPathGetTransformContext (ctxt), NULL,
			    NULL,
			    "exslt:crypto : internal error exsltCryptoCryptoApiRc4Encrypt dest buffer too small.\n");
	return;
    }

    if (!CryptAcquireContext (&hCryptProv, NULL, NULL, PROV_RSA_FULL,
			      CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
	exsltCryptoCryptoApiReportError (ctxt, __LINE__);
	return;
    }

    hHash = exsltCryptoCryptoApiCreateHash (ctxt, hCryptProv,
					    CALG_SHA1, key,
					    RC4_KEY_LENGTH, hash,
					    HASH_DIGEST_LENGTH);

    if (!CryptDeriveKey
	(hCryptProv, CALG_RC4, hHash, 0x00800000, &hKey)) {
	exsltCryptoCryptoApiReportError (ctxt, __LINE__);
	goto fail;
    }
/* Now encrypt data. */
    dwDataLen = msglen;
    memcpy (dest, msg, msglen);
    if (!CryptEncrypt (hKey, 0, TRUE, 0, dest, &dwDataLen, msglen)) {
	exsltCryptoCryptoApiReportError (ctxt, __LINE__);
	goto fail;
    }

  fail:
    if (0 != hHash) {
	CryptDestroyHash (hHash);
    }

    CryptDestroyKey (hKey);
    CryptReleaseContext (hCryptProv, 0);
}

static void
exsltCryptoCryptoApiRc4Decrypt (xmlXPathParserContextPtr ctxt,
				const unsigned char *key,
				const unsigned char *msg, int msglen,
				unsigned char *dest, int destlen) {
    HCRYPTPROV hCryptProv;
    HCRYPTKEY hKey;
    HCRYPTHASH hHash;
    DWORD dwDataLen;
    char hash[HASH_DIGEST_LENGTH];

    if (msglen > destlen) {
	xsltTransformError (xsltXPathGetTransformContext (ctxt), NULL,
			    NULL,
			    "exslt:crypto : internal error exsltCryptoCryptoApiRc4Encrypt dest buffer too small.\n");
	return;
    }

    if (!CryptAcquireContext (&hCryptProv, NULL, NULL, PROV_RSA_FULL,
			      CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
	exsltCryptoCryptoApiReportError (ctxt, __LINE__);
	return;
    }

    hHash = exsltCryptoCryptoApiCreateHash (ctxt, hCryptProv,
					    CALG_SHA1, key,
					    RC4_KEY_LENGTH, hash,
					    HASH_DIGEST_LENGTH);

    if (!CryptDeriveKey
	(hCryptProv, CALG_RC4, hHash, 0x00800000, &hKey)) {
	exsltCryptoCryptoApiReportError (ctxt, __LINE__);
	goto fail;
    }
/* Now encrypt data. */
    dwDataLen = msglen;
    memcpy (dest, msg, msglen);
    if (!CryptDecrypt (hKey, 0, TRUE, 0, dest, &dwDataLen)) {
	exsltCryptoCryptoApiReportError (ctxt, __LINE__);
	goto fail;
    }

  fail:
    if (0 != hHash) {
	CryptDestroyHash (hHash);
    }

    CryptDestroyKey (hKey);
    CryptReleaseContext (hCryptProv, 0);
}

#endif /* defined(_WIN32) */

#if defined(HAVE_GCRYPT)

#define HAVE_CRYPTO
#define PLATFORM_HASH	exsltCryptoGcryptHash
#define PLATFORM_RC4_ENCRYPT exsltCryptoGcryptRc4Encrypt
#define PLATFORM_RC4_DECRYPT exsltCryptoGcryptRc4Decrypt
#define PLATFORM_MD4 GCRY_MD_MD4
#define PLATFORM_MD5 GCRY_MD_MD5
#define PLATFORM_SHA1 GCRY_MD_SHA1

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>		/* needed by gcrypt.h 4 Jul 04 */
#endif
#include <gcrypt.h>

static void
exsltCryptoGcryptInit (void) {
    static int gcrypt_init;
    xmlLockLibrary ();

    if (!gcrypt_init) {
/* The function `gcry_check_version' must be called before any other
	 function in the library, because it initializes the thread support
	 subsystem in Libgcrypt. To achieve this in all generality, it is
	 necessary to synchronize the call to this function with all other calls
	 to functions in the library, using the synchronization mechanisms
	 available in your thread library. (from gcrypt.info)
*/
	gcry_check_version (GCRYPT_VERSION);
	gcrypt_init = 1;
    }

    xmlUnlockLibrary ();
}

/**
 * exsltCryptoGcryptHash:
 * @ctxt: an XPath parser context
 * @algorithm: hashing algorithm to use
 * @msg: text to be hashed
 * @msglen: length of text to be hashed
 * @dest: buffer to place hash result
 *
 * Helper function which hashes a message using MD4, MD5, or SHA1.
 * using gcrypt
 */
static void
exsltCryptoGcryptHash (xmlXPathParserContextPtr ctxt ATTRIBUTE_UNUSED,
/* changed the enum to int */
		       int algorithm, const char *msg,
		       unsigned long msglen,
		       char dest[HASH_DIGEST_LENGTH]) {
    exsltCryptoGcryptInit ();
    gcry_md_hash_buffer (algorithm, dest, msg, msglen);
}

static void
exsltCryptoGcryptRc4Encrypt (xmlXPathParserContextPtr ctxt,
			     const unsigned char *key,
			     const unsigned char *msg, int msglen,
			     unsigned char *dest, int destlen) {
    gcry_cipher_hd_t cipher;
    gcry_error_t rc = 0;

    exsltCryptoGcryptInit ();

    rc = gcry_cipher_open (&cipher, GCRY_CIPHER_ARCFOUR,
			   GCRY_CIPHER_MODE_STREAM, 0);
    if (rc) {
	xsltTransformError (xsltXPathGetTransformContext (ctxt), NULL,
			    NULL,
			    "exslt:crypto internal error %s (gcry_cipher_open)\n",
			    gcry_strerror (rc));
    }

    rc = gcry_cipher_setkey (cipher, key, RC4_KEY_LENGTH);
    if (rc) {
	xsltTransformError (xsltXPathGetTransformContext (ctxt), NULL,
			    NULL,
			    "exslt:crypto internal error %s (gcry_cipher_setkey)\n",
			    gcry_strerror (rc));
    }

    rc = gcry_cipher_encrypt (cipher, (unsigned char *) dest, destlen,
			      (const unsigned char *) msg, msglen);
    if (rc) {
	xsltTransformError (xsltXPathGetTransformContext (ctxt), NULL,
			    NULL,
			    "exslt:crypto internal error %s (gcry_cipher_encrypt)\n",
			    gcry_strerror (rc));
    }

    gcry_cipher_close (cipher);
}

static void
exsltCryptoGcryptRc4Decrypt (xmlXPathParserContextPtr ctxt,
			     const unsigned char *key,
			     const unsigned char *msg, int msglen,
			     unsigned char *dest, int destlen) {
    gcry_cipher_hd_t cipher;
    gcry_error_t rc = 0;

    exsltCryptoGcryptInit ();

    rc = gcry_cipher_open (&cipher, GCRY_CIPHER_ARCFOUR,
			   GCRY_CIPHER_MODE_STREAM, 0);
    if (rc) {
	xsltTransformError (xsltXPathGetTransformContext (ctxt), NULL,
			    NULL,
			    "exslt:crypto internal error %s (gcry_cipher_open)\n",
			    gcry_strerror (rc));
    }

    rc = gcry_cipher_setkey (cipher, key, RC4_KEY_LENGTH);
    if (rc) {
	xsltTransformError (xsltXPathGetTransformContext (ctxt), NULL,
			    NULL,
			    "exslt:crypto internal error %s (gcry_cipher_setkey)\n",
			    gcry_strerror (rc));
    }

    rc = gcry_cipher_decrypt (cipher, (unsigned char *) dest, destlen,
			      (const unsigned char *) msg, msglen);
    if (rc) {
	xsltTransformError (xsltXPathGetTransformContext (ctxt), NULL,
			    NULL,
			    "exslt:crypto internal error %s (gcry_cipher_decrypt)\n",
			    gcry_strerror (rc));
    }

    gcry_cipher_close (cipher);
}

#endif /* defined(HAVE_GCRYPT) */

#if defined(HAVE_CRYPTO)

/**
 * exsltCryptoPopString:
 * @ctxt: an XPath parser context
 * @nargs: the number of arguments
 *
 * Helper function which checks for and returns first string argument and its
 * length in bytes.
 */
static int
exsltCryptoPopString (xmlXPathParserContextPtr ctxt, int nargs,
		      xmlChar ** str) {

    int str_len = 0;

    if ((nargs < 1) || (nargs > 2)) {
	xmlXPathSetArityError (ctxt);
	return 0;
    }

    *str = xmlXPathPopString (ctxt);
    str_len = xmlStrlen (*str);

    if (str_len == 0) {
	xmlXPathReturnEmptyString (ctxt);
	xmlFree (*str);
	return 0;
    }

    return str_len;
}

/**
 * exsltCryptoMd4Function:
 * @ctxt: an XPath parser context
 * @nargs: the number of arguments
 *
 * computes the md4 hash of a string and returns as hex
 */
static void
exsltCryptoMd4Function (xmlXPathParserContextPtr ctxt, int nargs) {

    int str_len = 0;
    xmlChar *str = NULL, *ret = NULL;
    unsigned char hash[HASH_DIGEST_LENGTH];
    unsigned char hex[MD5_DIGEST_LENGTH * 2 + 1];

    str_len = exsltCryptoPopString (ctxt, nargs, &str);
    if (str_len == 0)
	return;

    PLATFORM_HASH (ctxt, PLATFORM_MD4, (const char *) str, str_len,
		   (char *) hash);
    exsltCryptoBin2Hex (hash, sizeof (hash) - 1, hex, sizeof (hex) - 1);

    ret = xmlStrdup ((xmlChar *) hex);
    xmlXPathReturnString (ctxt, ret);

    if (str != NULL)
	xmlFree (str);
}

/**
 * exsltCryptoMd5Function:
 * @ctxt: an XPath parser context
 * @nargs: the number of arguments
 *
 * computes the md5 hash of a string and returns as hex
 */
static void
exsltCryptoMd5Function (xmlXPathParserContextPtr ctxt, int nargs) {

    int str_len = 0;
    xmlChar *str = NULL, *ret = NULL;
    unsigned char hash[HASH_DIGEST_LENGTH];
    unsigned char hex[MD5_DIGEST_LENGTH * 2 + 1];

    str_len = exsltCryptoPopString (ctxt, nargs, &str);
    if (str_len == 0)
	return;

    PLATFORM_HASH (ctxt, PLATFORM_MD5, (const char *) str, str_len,
		   (char *) hash);
    exsltCryptoBin2Hex (hash, sizeof (hash) - 1, hex, sizeof (hex) - 1);

    ret = xmlStrdup ((xmlChar *) hex);
    xmlXPathReturnString (ctxt, ret);

    if (str != NULL)
	xmlFree (str);
}

/**
 * exsltCryptoSha1Function:
 * @ctxt: an XPath parser context
 * @nargs: the number of arguments
 *
 * computes the sha1 hash of a string and returns as hex
 */
static void
exsltCryptoSha1Function (xmlXPathParserContextPtr ctxt, int nargs) {

    int str_len = 0;
    xmlChar *str = NULL, *ret = NULL;
    unsigned char hash[HASH_DIGEST_LENGTH];
    unsigned char hex[SHA1_DIGEST_LENGTH * 2 + 1];

    str_len = exsltCryptoPopString (ctxt, nargs, &str);
    if (str_len == 0)
	return;

    PLATFORM_HASH (ctxt, PLATFORM_SHA1, (const char *) str, str_len,
		   (char *) hash);
    exsltCryptoBin2Hex (hash, sizeof (hash) - 1, hex, sizeof (hex) - 1);

    ret = xmlStrdup ((xmlChar *) hex);
    xmlXPathReturnString (ctxt, ret);

    if (str != NULL)
	xmlFree (str);
}

/**
 * exsltCryptoRc4EncryptFunction:
 * @ctxt: an XPath parser context
 * @nargs: the number of arguments
 *
 * computes the sha1 hash of a string and returns as hex
 */
static void
exsltCryptoRc4EncryptFunction (xmlXPathParserContextPtr ctxt, int nargs) {

    int key_len = 0;
    int str_len = 0, bin_len = 0, hex_len = 0;
    xmlChar *key = NULL, *str = NULL, *padkey = NULL;
    xmlChar *bin = NULL, *hex = NULL;
    xsltTransformContextPtr tctxt = NULL;

    if (nargs != 2) {
	xmlXPathSetArityError (ctxt);
	return;
    }
    tctxt = xsltXPathGetTransformContext(ctxt);

    str = xmlXPathPopString (ctxt);
    str_len = xmlStrlen (str);

    if (str_len == 0) {
	xmlXPathReturnEmptyString (ctxt);
	xmlFree (str);
	return;
    }

    key = xmlXPathPopString (ctxt);
    key_len = xmlStrlen (key);

    if (key_len == 0) {
	xmlXPathReturnEmptyString (ctxt);
	xmlFree (key);
	xmlFree (str);
	return;
    }

    padkey = xmlMallocAtomic (RC4_KEY_LENGTH + 1);
    if (padkey == NULL) {
	xsltTransformError(tctxt, NULL, tctxt->inst,
	    "exsltCryptoRc4EncryptFunction: Failed to allocate padkey\n");
	tctxt->state = XSLT_STATE_STOPPED;
	xmlXPathReturnEmptyString (ctxt);
	goto done;
    }
    memset(padkey, 0, RC4_KEY_LENGTH + 1);

    if ((key_len > RC4_KEY_LENGTH) || (key_len < 0)) {
	xsltTransformError(tctxt, NULL, tctxt->inst,
	    "exsltCryptoRc4EncryptFunction: key size too long or key broken\n");
	tctxt->state = XSLT_STATE_STOPPED;
	xmlXPathReturnEmptyString (ctxt);
	goto done;
    }
    memcpy (padkey, key, key_len);

/* encrypt it */
    bin_len = str_len;
    bin = xmlStrdup (str);
    if (bin == NULL) {
	xsltTransformError(tctxt, NULL, tctxt->inst,
	    "exsltCryptoRc4EncryptFunction: Failed to allocate string\n");
	tctxt->state = XSLT_STATE_STOPPED;
	xmlXPathReturnEmptyString (ctxt);
	goto done;
    }
    PLATFORM_RC4_ENCRYPT (ctxt, padkey, str, str_len, bin, bin_len);

/* encode it */
    hex_len = str_len * 2 + 1;
    hex = xmlMallocAtomic (hex_len);
    if (hex == NULL) {
	xsltTransformError(tctxt, NULL, tctxt->inst,
	    "exsltCryptoRc4EncryptFunction: Failed to allocate result\n");
	tctxt->state = XSLT_STATE_STOPPED;
	xmlXPathReturnEmptyString (ctxt);
	goto done;
    }

    exsltCryptoBin2Hex (bin, str_len, hex, hex_len);
    xmlXPathReturnString (ctxt, hex);

done:
    if (key != NULL)
	xmlFree (key);
    if (str != NULL)
	xmlFree (str);
    if (padkey != NULL)
	xmlFree (padkey);
    if (bin != NULL)
	xmlFree (bin);
}

/**
 * exsltCryptoRc4DecryptFunction:
 * @ctxt: an XPath parser context
 * @nargs: the number of arguments
 *
 * computes the sha1 hash of a string and returns as hex
 */
static void
exsltCryptoRc4DecryptFunction (xmlXPathParserContextPtr ctxt, int nargs) {

    int key_len = 0;
    int str_len = 0, bin_len = 0, ret_len = 0;
    xmlChar *key = NULL, *str = NULL, *padkey = NULL, *bin =
	NULL, *ret = NULL;
    xsltTransformContextPtr tctxt = NULL;

    if (nargs != 2) {
	xmlXPathSetArityError (ctxt);
	return;
    }
    tctxt = xsltXPathGetTransformContext(ctxt);

    str = xmlXPathPopString (ctxt);
    str_len = xmlStrlen (str);

    if (str_len == 0) {
	xmlXPathReturnEmptyString (ctxt);
	xmlFree (str);
	return;
    }

    key = xmlXPathPopString (ctxt);
    key_len = xmlStrlen (key);

    if (key_len == 0) {
	xmlXPathReturnEmptyString (ctxt);
	xmlFree (key);
	xmlFree (str);
	return;
    }

    padkey = xmlMallocAtomic (RC4_KEY_LENGTH + 1);
    if (padkey == NULL) {
	xsltTransformError(tctxt, NULL, tctxt->inst,
	    "exsltCryptoRc4EncryptFunction: Failed to allocate padkey\n");
	tctxt->state = XSLT_STATE_STOPPED;
	xmlXPathReturnEmptyString (ctxt);
	goto done;
    }
    memset(padkey, 0, RC4_KEY_LENGTH + 1);
    if ((key_len > RC4_KEY_LENGTH) || (key_len < 0)) {
	xsltTransformError(tctxt, NULL, tctxt->inst,
	    "exsltCryptoRc4EncryptFunction: key size too long or key broken\n");
	tctxt->state = XSLT_STATE_STOPPED;
	xmlXPathReturnEmptyString (ctxt);
	goto done;
    }
    memcpy (padkey, key, key_len);

/* decode hex to binary */
    bin_len = str_len;
    bin = xmlMallocAtomic (bin_len);
    if (bin == NULL) {
	xsltTransformError(tctxt, NULL, tctxt->inst,
	    "exsltCryptoRc4EncryptFunction: Failed to allocate string\n");
	tctxt->state = XSLT_STATE_STOPPED;
	xmlXPathReturnEmptyString (ctxt);
	goto done;
    }
    ret_len = exsltCryptoHex2Bin (str, str_len, bin, bin_len);

/* decrypt the binary blob */
    ret = xmlMallocAtomic (ret_len + 1);
    if (ret == NULL) {
	xsltTransformError(tctxt, NULL, tctxt->inst,
	    "exsltCryptoRc4EncryptFunction: Failed to allocate result\n");
	tctxt->state = XSLT_STATE_STOPPED;
	xmlXPathReturnEmptyString (ctxt);
	goto done;
    }
    PLATFORM_RC4_DECRYPT (ctxt, padkey, bin, ret_len, ret, ret_len);
    ret[ret_len] = 0;

    if (xmlCheckUTF8(ret) == 0) {
	xsltTransformError(tctxt, NULL, tctxt->inst,
	    "exsltCryptoRc4DecryptFunction: Invalid UTF-8\n");
        xmlFree(ret);
	xmlXPathReturnEmptyString(ctxt);
    } else {
        xmlXPathReturnString(ctxt, ret);
    }

done:
    if (key != NULL)
	xmlFree (key);
    if (str != NULL)
	xmlFree (str);
    if (padkey != NULL)
	xmlFree (padkey);
    if (bin != NULL)
	xmlFree (bin);
}

/**
 * exsltCryptoRegister:
 *
 * Registers the EXSLT - Crypto module
 */

void
exsltCryptoRegister (void) {
    xsltRegisterExtModuleFunction ((const xmlChar *) "md4",
				   EXSLT_CRYPTO_NAMESPACE,
				   exsltCryptoMd4Function);
    xsltRegisterExtModuleFunction ((const xmlChar *) "md5",
				   EXSLT_CRYPTO_NAMESPACE,
				   exsltCryptoMd5Function);
    xsltRegisterExtModuleFunction ((const xmlChar *) "sha1",
				   EXSLT_CRYPTO_NAMESPACE,
				   exsltCryptoSha1Function);
    xsltRegisterExtModuleFunction ((const xmlChar *) "rc4_encrypt",
				   EXSLT_CRYPTO_NAMESPACE,
				   exsltCryptoRc4EncryptFunction);
    xsltRegisterExtModuleFunction ((const xmlChar *) "rc4_decrypt",
				   EXSLT_CRYPTO_NAMESPACE,
				   exsltCryptoRc4DecryptFunction);
}

#else
/**
 * exsltCryptoRegister:
 *
 * Registers the EXSLT - Crypto module
 */
void
exsltCryptoRegister (void) {
}

#endif /* defined(HAVE_CRYPTO) */

#endif /* EXSLT_CRYPTO_ENABLED */
