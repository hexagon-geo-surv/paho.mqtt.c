//--------------------------------------------------------//
// Copyright (c) 2015-2023 Hexagon Technology Center GmbH
// 9435 Heerbrugg, Switzerland
// http://www.hexagon.com
//
// Unauthorized use or distribution prohibited. Access to
// and use of this code is permitted only under written
// permission from Hexagon Technology Center GmbH.
// All Rights Reserved.
//--------------------------------------------------------//

#if !defined(PKCS11HELPERS_H)
#define PKCS11HELPERS_H

#include <libp11.h>
#include <openssl/types.h>

// Return codes to be consistent with SSL_CTX_* functions
#define PAHO_PKCS11_OK 1
#define PAHO_PKCS11_ERROR 0

/**
 * MQTTClient_sslPkcs11Properties defines the settings for PKCS11 supported by
 * libp11
 */
typedef struct {
  const char *module; // pkcs11 library name, ignored if
                      // PAHO_USE_EXTERNAL_PKCS11_CONTEXT is set

  const char *caLabel;
  const char *caId;
  const char *caTokenLabel;
  const char *caTokenPin;

  const char *certLabel;
  const char *certId;
  const char *certTokenLabel;
  const char *certTokenPin;

  const char *privateKeyLabel;
  const char *privateKeyId;
  const char *privateKeyTokenLabel;
  const char *privateKeyTokenPin;

  PKCS11_CTX *context;
  PKCS11_SLOT **slots;
  unsigned int *slotsCount;
} MQTTClient_sslPkcs11Properties;

/**
 * Frees MQTTClient_sslPkcs11Properties struct members
 */
void free_sslPkcs11Properties_members(
    const MQTTClient_sslPkcs11Properties *pkcs11Properties);

/**
 * Safely duplicates MQTTClient_sslPkcs11Properties struct members
 */
void copy_sslPkcs11Properties_members(const MQTTClient_sslPkcs11Properties *src,
                                      MQTTClient_sslPkcs11Properties *dst);

/**
 * Loads pkcs11 context.
 */
int pkcs11_load_context(MQTTClient_sslPkcs11Properties *pkcs11Properties);

/**
 * Unloads pkcs11 context. Releases all allocated external resources.
 */
void pkcs11_unload_context(MQTTClient_sslPkcs11Properties *pkcs11Properties);

/**
 * Loads private key from PKCS11 token.
 */
int pkcs11_load_private_key(
    const SSL_CTX *ssl_ctx,
    const MQTTClient_sslPkcs11Properties *pkcs11Properties);

/**
 * Loads certificate from PKCS11 token. Verifies that requested locations are
 * accessible.
 */
int pkcs11_load_certificate(
    const SSL_CTX *ssl_ctx,
    const MQTTClient_sslPkcs11Properties *pkcs11Properties);

/**
 * Loads verify locations from PKCS11 token. Verifies that requested locations
 * are accessible.
 */
int pkcs11_load_ca(const SSL_CTX *ssl_ctx,
                   const MQTTClient_sslPkcs11Properties *pkcs11Properties);

#endif
