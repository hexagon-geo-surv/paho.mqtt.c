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

#include <libp11.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include "Heap.h"
#include "MQTTProtocolClient.h"
#include "PKCS11Helpers.h"

int try_log_in(const PKCS11_SLOT *slot, const char *pin);
PKCS11_SLOT *find_token(const char *label, const PKCS11_CTX *context, const PKCS11_SLOT *slots,
                        unsigned int slotsCount);
EVP_PKEY *find_key(const char *label, const char *id, const PKCS11_SLOT *slot);
X509 *find_certificate(const char *label, const char *id, const PKCS11_SLOT *slot);

void free_sslPkcs11Properties_members(MQTTClient_sslPkcs11Properties *pkcs11Properties) {
    if (!pkcs11Properties) {
        return;
    }

    free((void *)pkcs11Properties->module);
    pkcs11Properties->module = NULL;

    free((void *)pkcs11Properties->caLabel);
    pkcs11Properties->caLabel = NULL;
    free((void *)pkcs11Properties->caId);
    pkcs11Properties->caId = NULL;
    free((void *)pkcs11Properties->caTokenLabel);
    pkcs11Properties->caTokenLabel = NULL;
    free((void *)pkcs11Properties->caTokenPin);
    pkcs11Properties->caTokenPin = NULL;

    free((void *)pkcs11Properties->certLabel);
    pkcs11Properties->certLabel = NULL;
    free((void *)pkcs11Properties->certId);
    pkcs11Properties->certId = NULL;
    free((void *)pkcs11Properties->certTokenLabel);
    pkcs11Properties->certTokenLabel = NULL;
    free((void *)pkcs11Properties->certTokenPin);
    pkcs11Properties->certTokenPin = NULL;

    free((void *)pkcs11Properties->privateKeyLabel);
    pkcs11Properties->privateKeyLabel = NULL;
    free((void *)pkcs11Properties->privateKeyId);
    pkcs11Properties->privateKeyId = NULL;
    free((void *)pkcs11Properties->privateKeyTokenLabel);
    pkcs11Properties->privateKeyTokenLabel = NULL;
    free((void *)pkcs11Properties->privateKeyTokenPin);
    pkcs11Properties->privateKeyTokenPin = NULL;
}

int pkcs11_load_context(MQTTClient_sslPkcs11Properties *pkcs11Properties) {
    if (!pkcs11Properties) {
        return PAHO_PKCS11_OK;
    }

    if (!pkcs11Properties->context) {
        pkcs11Properties->context = PKCS11_CTX_new();
        if (!pkcs11Properties->context ||
            PKCS11_CTX_load(pkcs11Properties->context, pkcs11Properties->module) < 0) {
            fprintf(stderr, "Can't load PKCS11 context\n");
            return PAHO_MEMORY_ERROR;
        }
    }

    if (!pkcs11Properties->slots) {
        if (PKCS11_enumerate_slots(pkcs11Properties->context, pkcs11Properties->slots,
                                   pkcs11Properties->slotsCount) < 0) {
            fprintf(stderr, "Can't enumerate PKCS11 slots\n");
            return PAHO_MEMORY_ERROR;
        }
    }

    return PAHO_PKCS11_OK;
}

void pkcs11_unload_context(MQTTClient_sslPkcs11Properties *pkcs11Properties) {
    if (!pkcs11Properties) {
        return;
    }

    if (pkcs11Properties->context) {
        if (pkcs11Properties->slots) {
            PKCS11_release_all_slots(pkcs11Properties->context, *pkcs11Properties->slots,
                                     *pkcs11Properties->slotsCount);
            pkcs11Properties->slots = NULL;
        }

        PKCS11_CTX_unload(pkcs11Properties->context);
        PKCS11_CTX_free(pkcs11Properties->context);
        pkcs11Properties->context = NULL;
    }
}

void copy_sslPkcs11Properties_members(const MQTTClient_sslPkcs11Properties *src,
                                      MQTTClient_sslPkcs11Properties *dst) {
    if (src->module)
        dst->module = MQTTStrdup(src->module);

    if (src->caLabel)
        dst->caLabel = MQTTStrdup(src->caLabel);
    if (src->caId)
        dst->caId = MQTTStrdup(src->caId);
    if (src->caTokenLabel)
        dst->caTokenLabel = MQTTStrdup(src->caTokenLabel);
    if (src->caTokenPin)
        dst->caTokenPin = MQTTStrdup(src->caTokenPin);

    if (src->certLabel)
        dst->certLabel = MQTTStrdup(src->certLabel);
    if (src->certId)
        dst->certId = MQTTStrdup(src->certId);
    if (src->certTokenLabel)
        dst->certTokenLabel = MQTTStrdup(src->certTokenLabel);
    if (src->certTokenPin)
        dst->certTokenPin = MQTTStrdup(src->certTokenPin);

    if (src->privateKeyLabel)
        dst->privateKeyLabel = MQTTStrdup(src->privateKeyLabel);
    if (src->privateKeyId)
        dst->privateKeyId = MQTTStrdup(src->privateKeyId);
    if (src->privateKeyTokenLabel)
        dst->privateKeyTokenLabel = MQTTStrdup(src->privateKeyTokenLabel);
    if (src->privateKeyTokenPin)
        dst->privateKeyTokenPin = MQTTStrdup(src->privateKeyTokenPin);

#if !defined(PAHO_USE_EXTERNAL_PKCS11_CONTEXT)
    if (dst->context && dst->context != src->context) {
        pkcs11_unload_context(dst);
    }
#endif

    dst->context = src->context;
    dst->slots = src->slots;
    dst->slotsCount = src->slotsCount;
}

int pkcs11_load_private_key(const SSL_CTX *ssl_ctx,
                            const MQTTClient_sslPkcs11Properties *pkcs11Properties) {
    int rc = PAHO_PKCS11_ERROR;
    EVP_PKEY *privateKey = NULL;

    const PKCS11_SLOT *slot =
        find_token(pkcs11Properties->privateKeyTokenLabel, pkcs11Properties->context,
                   *pkcs11Properties->slots, *pkcs11Properties->slotsCount);
    if (!slot) {
        goto failed;
    }

    if (try_log_in(slot, pkcs11Properties->privateKeyTokenPin) != PAHO_PKCS11_OK) {
        goto failed;
    }

    privateKey = find_key(pkcs11Properties->privateKeyLabel,
                          pkcs11Properties->privateKeyId, slot);
    if (!privateKey) {
        goto failed;
    }

    rc = SSL_CTX_use_PrivateKey(ssl_ctx, privateKey);

failed:
    if (rc != PAHO_PKCS11_OK)
        ERR_print_errors_fp(stderr);
    if (privateKey)
        EVP_PKEY_free(privateKey);

    return rc;
}

int pkcs11_load_certificate(const SSL_CTX *ssl_ctx,
                            const MQTTClient_sslPkcs11Properties *pkcs11Properties) {
    int rc = PAHO_PKCS11_ERROR;

    const PKCS11_SLOT *slot =
        find_token(pkcs11Properties->certTokenLabel, pkcs11Properties->context,
                   *pkcs11Properties->slots, *pkcs11Properties->slotsCount);
    if (!slot) {
        goto failed;
    }

    if (!try_log_in(slot, pkcs11Properties->certTokenPin)) {
        goto failed;
    }

    X509 *cert = find_certificate(pkcs11Properties->certLabel, pkcs11Properties->certId, slot);
    if (!cert) {
        goto failed;
    }

    if (!SSL_CTX_use_certificate(ssl_ctx, cert)) {
        goto failed;
    }

    rc = PAHO_PKCS11_OK;

failed:
    if (rc != PAHO_PKCS11_OK)
        ERR_print_errors_fp(stderr);

    return rc;
}

int pkcs11_load_ca(const SSL_CTX *ssl_ctx, const MQTTClient_sslPkcs11Properties *pkcs11Properties) {
    int rc = PAHO_PKCS11_ERROR;

    const PKCS11_SLOT *slot = find_token(pkcs11Properties->caTokenLabel, pkcs11Properties->context,
                                         *pkcs11Properties->slots, *pkcs11Properties->slotsCount);
    if (!slot) {
        goto failed;
    }

    if (!try_log_in(slot, pkcs11Properties->caTokenPin)) {
        goto failed;
    }

    X509 *cert = find_certificate(pkcs11Properties->caLabel, pkcs11Properties->caId, slot);
    if (!cert) {
        goto failed;
    }

    X509_STORE *x509_store = SSL_CTX_get_cert_store(ssl_ctx);
    if (!x509_store) {
        fprintf(stderr, "PKCS11: SSL_CTX_get_cert_store returned NULL\n");
        goto failed;
    }

    X509_LOOKUP *lookup;

    lookup = X509_STORE_add_lookup(x509_store, X509_LOOKUP_file());
    if (!lookup) {
        goto failed;
    }
    X509_STORE *lookup_store = X509_LOOKUP_get_store(lookup);
    if (!lookup_store) {
        fprintf(stderr, "PKCS11: X509_LOOKUP_get_store returned NULL\n");
        goto failed;
    }
    if (!X509_STORE_add_cert(lookup_store, cert)) {
        goto failed;
    }

    rc = PAHO_PKCS11_OK;

failed:
    if (rc != PAHO_PKCS11_OK)
        ERR_print_errors_fp(stderr);

    return rc;
}

int try_log_in(const PKCS11_SLOT *slot, const char *pin) {
    int isLoggedIn = 0;
    int rc = PKCS11_is_logged_in(slot, 0, &isLoggedIn);
    if (rc < 0) {
        return PAHO_PKCS11_ERROR;
    }
    if (!isLoggedIn) {
        rc = PKCS11_login(slot, 0, pin);
        if (rc < 0) {
            return PAHO_PKCS11_ERROR;
        }
    }
    return PAHO_PKCS11_OK;
}

PKCS11_SLOT *find_token(const char *label, const PKCS11_CTX *context, const PKCS11_SLOT *slots,
                        unsigned int slotsCount) {
    if (!label) {
        fprintf(stderr, "PKCS11: find_token called with NULL label\n");
        return NULL;
    }

    const unsigned int labelLength = strlen(label);
    PKCS11_SLOT *slot = PKCS11_find_token(context, slots, slotsCount);

    for (unsigned int i = 0; i < slotsCount; ++i) {
        if (slot == NULL || slot->token == NULL) {
            return NULL;
        }

        if (strlen(slot->token->label) == labelLength &&
            memcmp(slot->token->label, label, labelLength) == 0) {
            return slot;
        }

        slot = PKCS11_find_next_token(context, slots, slotsCount, slot);
    }

    return NULL;
}

EVP_PKEY *find_key(const char *label, const char *id, const PKCS11_SLOT *slot) {
    if (!label || !id) {
        fprintf(stderr, "PKCS11: find_key called with NULL label or id\n");
        return NULL;
    }

    PKCS11_KEY *keys;
    unsigned int keysCount;

    const unsigned int labelLength = strlen(label);
    const unsigned int idLength = strlen(id);
    const int rc = PKCS11_enumerate_keys(slot->token, &keys, &keysCount);
    if (rc < 0 || keysCount == 0) {
        return NULL;
    }

    for (unsigned int i = 0; i < keysCount; ++i) {
        PKCS11_KEY *key = &keys[i];
        if (!key->label) {
            continue;
        }
        if (strlen(key->label) == labelLength && memcmp(key->label, label, labelLength) == 0 &&
            key->id_len == idLength && memcmp(key->id, id, idLength) == 0) {
            return PKCS11_get_private_key(key);
        }
    }

    return NULL;
}

X509 *find_certificate(const char *label, const char *id, const PKCS11_SLOT *slot) {
    if (!label || !id) {
        fprintf(stderr, "PKCS11: find_certificate called with NULL label or id\n");
        return NULL;
    }

    PKCS11_CERT *certs;
    unsigned int certsCount;

    const unsigned int labelLength = strlen(label);
    const unsigned int idLength = strlen(id);
    const int rc = PKCS11_enumerate_certs(slot->token, &certs, &certsCount);
    if (rc < 0 || certsCount <= 0) {
        return NULL;
    }

    for (unsigned int i = 0; i < certsCount; ++i) {
        PKCS11_CERT *cert = &certs[i];
        if (!cert->label) {
            continue;
        }
        if (strlen(cert->label) == labelLength && memcmp(cert->label, label, labelLength) == 0 &&
            cert->id_len == idLength && memcmp(cert->id, id, idLength) == 0) {
                return cert->x509;
        }
    }

    return NULL;
}
