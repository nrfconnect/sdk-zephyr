/*
 * Copyright (c) 2017 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file Shim layer for mbedTLS, crypto API compliant.
 */


#include <kernel.h>
#include <init.h>
#include <errno.h>
#include <crypto/cipher.h>

#if !defined(CONFIG_MBEDTLS_CFG_FILE)
#include "mbedtls/config.h"
#else
#include CONFIG_MBEDTLS_CFG_FILE
#endif /* CONFIG_MBEDTLS_CFG_FILE */

#include <mbedtls/ccm.h>
#include <mbedtls/aes.h>

#define MTLS_SUPPORT (CAP_RAW_KEY | CAP_SEPARATE_IO_BUFS | CAP_SYNC_OPS)

#define LOG_LEVEL CONFIG_CRYPTO_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(mbedtls);

struct mtls_shim_session {
	union {
		mbedtls_ccm_context mtls_ccm;
		mbedtls_aes_context mtls_aes;
	};
	bool in_use;
	enum cipher_mode mode;
};

#define CRYPTO_MAX_SESSION CONFIG_CRYPTO_MBEDTLS_SHIM_MAX_SESSION

struct mtls_shim_session mtls_sessions[CRYPTO_MAX_SESSION];

#if defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C)
#include "mbedtls/memory_buffer_alloc.h"
#else
#error "You need to define MBEDTLS_MEMORY_BUFFER_ALLOC_C"
#endif /* MBEDTLS_MEMORY_BUFFER_ALLOC_C */

#define MTLS_GET_CTX(c, m) \
	(&((struct mtls_shim_session *)c->drv_sessn_state)->mtls_ ## m)

int mtls_ecb_encrypt(struct cipher_ctx *ctx, struct cipher_pkt *pkt)
{
	int ret;
	mbedtls_aes_context *ecb_ctx = MTLS_GET_CTX(ctx, aes);

	/* For security reasons, ECB mode should not be used to encrypt
	 * more than one block. Use CBC mode instead.
	 */
	if (pkt->in_len > 16) {
		LOG_ERR("Cannot encrypt more than 1 block");
		return -EINVAL;
	}

	ret = mbedtls_aes_crypt_ecb(ecb_ctx, MBEDTLS_AES_ENCRYPT,
				    pkt->in_buf, pkt->out_buf);
	if (ret) {
		LOG_ERR("Could not encrypt (%d)", ret);
		return -EINVAL;
	}

	pkt->out_len = 16;

	return 0;
}

int mtls_ecb_decrypt(struct cipher_ctx *ctx, struct cipher_pkt *pkt)
{
	int ret;
	mbedtls_aes_context *ecb_ctx = MTLS_GET_CTX(ctx, aes);

	/* For security reasons, ECB mode should not be used to decrypt
	 * more than one block. Use CBC mode instead.
	 */
	if (pkt->in_len > 16) {
		LOG_ERR("Cannot decrypt more than 1 block");
		return -EINVAL;
	}

	ret = mbedtls_aes_crypt_ecb(ecb_ctx, MBEDTLS_AES_DECRYPT,
				    pkt->in_buf, pkt->out_buf);
	if (ret) {
		LOG_ERR("Could not encrypt (%d)", ret);
		return -EINVAL;
	}

	pkt->out_len = 16;

	return 0;
}

int mtls_cbc_encrypt(struct cipher_ctx *ctx, struct cipher_pkt *pkt, u8_t *iv)
{
	int ret;
	mbedtls_aes_context *cbc_ctx = MTLS_GET_CTX(ctx, aes);

	/* Prefix IV to ciphertext */
	memcpy(pkt->out_buf, iv, 16);
	ret = mbedtls_aes_crypt_cbc(cbc_ctx, MBEDTLS_AES_ENCRYPT, pkt->in_len,
				    iv, pkt->in_buf, pkt->out_buf + 16);
	if (ret) {
		LOG_ERR("Could not encrypt (%d)", ret);
		return -EINVAL;
	}

	pkt->out_len = pkt->in_len + 16;

	return 0;
}

int mtls_cbc_decrypt(struct cipher_ctx *ctx, struct cipher_pkt *pkt, u8_t *iv)
{
	int ret;
	mbedtls_aes_context *cbc_ctx = MTLS_GET_CTX(ctx, aes);

	ret = mbedtls_aes_crypt_cbc(cbc_ctx, MBEDTLS_AES_DECRYPT, pkt->in_len,
				    iv, pkt->in_buf + 16, pkt->out_buf);
	if (ret) {
		LOG_ERR("Could not encrypt (%d)", ret);
		return -EINVAL;
	}

	pkt->out_len = pkt->in_len - 16;

	return 0;
}

static int mtls_ccm_encrypt_auth(struct cipher_ctx *ctx,
				 struct cipher_aead_pkt *apkt,
				 u8_t *nonce)
{
	mbedtls_ccm_context *mtls_ctx = MTLS_GET_CTX(ctx, ccm);
	int ret;

	ret = mbedtls_ccm_encrypt_and_tag(mtls_ctx, apkt->pkt->in_len, nonce,
					  ctx->mode_params.ccm_info.nonce_len,
					  apkt->ad, apkt->ad_len,
					  apkt->pkt->in_buf,
					  apkt->pkt->out_buf, apkt->tag,
					  ctx->mode_params.ccm_info.tag_len);
	if (ret) {
		LOG_ERR("Could not encrypt/auth (%d)", ret);

		/*ToDo: try to return relevant code depending on ret? */
		return -EINVAL;
	}

	/* This is equivalent to what the TinyCrypt shim does in
	 * do_ccm_encrypt_mac().
	 */
	apkt->pkt->out_len = apkt->pkt->in_len;
	apkt->pkt->out_len += ctx->mode_params.ccm_info.tag_len;

	return 0;
}

static int mtls_ccm_decrypt_auth(struct cipher_ctx *ctx,
				 struct cipher_aead_pkt *apkt,
				 u8_t *nonce)
{
	mbedtls_ccm_context *mtls_ctx = MTLS_GET_CTX(ctx, ccm);
	int ret;

	ret = mbedtls_ccm_auth_decrypt(mtls_ctx, apkt->pkt->in_len, nonce,
				       ctx->mode_params.ccm_info.nonce_len,
				       apkt->ad, apkt->ad_len,
				       apkt->pkt->in_buf,
				       apkt->pkt->out_buf, apkt->tag,
				       ctx->mode_params.ccm_info.tag_len);
	if (ret) {
		LOG_ERR("Could non decrypt/auth (%d)", ret);

		/*ToDo: try to return relevant code depending on ret? */
		return -EINVAL;
	}

	apkt->pkt->out_len = apkt->pkt->in_len;
	apkt->pkt->out_len += ctx->mode_params.ccm_info.tag_len;

	return 0;
}

static int mtls_get_unused_session_index(void)
{
	int i;

	for (i = 0; i < CRYPTO_MAX_SESSION; i++) {
		if (!mtls_sessions[i].in_use) {
			mtls_sessions[i].in_use = true;
			return i;
		}
	}

	return -1;
}

static int mtls_session_setup(struct device *dev, struct cipher_ctx *ctx,
		       enum cipher_algo algo, enum cipher_mode mode,
		       enum cipher_op op_type)
{
	mbedtls_ccm_context *ccm_ctx;
	mbedtls_aes_context *aes_ctx;
	int ctx_idx;
	int ret;

	if (ctx->flags & ~(MTLS_SUPPORT)) {
		LOG_ERR("Unsupported flag");
		return -EINVAL;
	}

	if (algo != CRYPTO_CIPHER_ALGO_AES) {
		LOG_ERR("Unsupported algo");
		return -EINVAL;
	}

	if (mode != CRYPTO_CIPHER_MODE_CCM &&
	    mode != CRYPTO_CIPHER_MODE_CBC &&
	    mode != CRYPTO_CIPHER_MODE_ECB) {
		LOG_ERR("Unsupported mode");
		return -EINVAL;
	}

	if (ctx->keylen != 16U) {
		LOG_ERR("%u key size is not supported", ctx->keylen);
		return -EINVAL;
	}

	ctx_idx = mtls_get_unused_session_index();
	if (ctx_idx < 0) {
		LOG_ERR("No free session for now");
		return -ENOSPC;
	}


	switch (mode) {
	case CRYPTO_CIPHER_MODE_ECB:
		aes_ctx = &mtls_sessions[ctx_idx].mtls_aes;
		mbedtls_aes_init(aes_ctx);
		if (op_type == CRYPTO_CIPHER_OP_ENCRYPT) {
			ret = mbedtls_aes_setkey_enc(aes_ctx,
					ctx->key.bit_stream, ctx->keylen * 8U);
			ctx->ops.block_crypt_hndlr = mtls_ecb_encrypt;
		} else {
			ret = mbedtls_aes_setkey_dec(aes_ctx,
					ctx->key.bit_stream, ctx->keylen * 8U);
			ctx->ops.block_crypt_hndlr = mtls_ecb_decrypt;
		}
		if (ret) {
			LOG_ERR("AES_ECB: failed at setkey (%d)", ret);
			ctx->ops.block_crypt_hndlr = NULL;
			mtls_sessions[ctx_idx].in_use = false;
			return -EINVAL;
		}
		break;
	case CRYPTO_CIPHER_MODE_CTR:
		break;
	case CRYPTO_CIPHER_MODE_CBC:
		aes_ctx = &mtls_sessions[ctx_idx].mtls_aes;
		mbedtls_aes_init(aes_ctx);
		if (op_type == CRYPTO_CIPHER_OP_ENCRYPT) {
			ret = mbedtls_aes_setkey_enc(aes_ctx,
					ctx->key.bit_stream, ctx->keylen * 8U);
			ctx->ops.cbc_crypt_hndlr = mtls_cbc_encrypt;
		} else {
			ret = mbedtls_aes_setkey_dec(aes_ctx,
					ctx->key.bit_stream, ctx->keylen * 8U);
			ctx->ops.cbc_crypt_hndlr = mtls_cbc_decrypt;
		}
		if (ret) {
			LOG_ERR("AES_CBC: failed at setkey (%d)", ret);
			ctx->ops.cbc_crypt_hndlr = NULL;
			mtls_sessions[ctx_idx].in_use = false;
			return -EINVAL;
		}
		break;
	case CRYPTO_CIPHER_MODE_CCM:
		ccm_ctx = &mtls_sessions[ctx_idx].mtls_ccm;
		mbedtls_ccm_init(ccm_ctx);
		ret = mbedtls_ccm_setkey(ccm_ctx, MBEDTLS_CIPHER_ID_AES,
					 ctx->key.bit_stream, ctx->keylen * 8U);
		if (ret) {
			LOG_ERR("Could not setup the key (%d)", ret);
			mtls_sessions[ctx_idx].in_use = false;

			return -EINVAL;
		}
		if (op_type == CRYPTO_CIPHER_OP_ENCRYPT) {
			ctx->ops.ccm_crypt_hndlr = mtls_ccm_encrypt_auth;
		} else {
			ctx->ops.ccm_crypt_hndlr = mtls_ccm_decrypt_auth;
		}
		break;
	}

	mtls_sessions[ctx_idx].mode = mode;
	ctx->drv_sessn_state = &mtls_sessions[ctx_idx];

	return ret;
}

static int mtls_session_free(struct device *dev, struct cipher_ctx *ctx)
{
	struct mtls_shim_session *mtls_session =
		(struct mtls_shim_session *)ctx->drv_sessn_state;

	if (mtls_session->mode == CRYPTO_CIPHER_MODE_CCM) {
		mbedtls_ccm_free(&mtls_session->mtls_ccm);
	} else {
		mbedtls_aes_free(&mtls_session->mtls_aes);
	}
	mtls_session->in_use = false;

	return 0;
}

static int mtls_query_caps(struct device *dev)
{
	return MTLS_SUPPORT;
}

static int mtls_shim_init(struct device *dev)
{
	return 0;
}

static struct crypto_driver_api mtls_crypto_funcs = {
	.begin_session = mtls_session_setup,
	.free_session = mtls_session_free,
	.crypto_async_callback_set = NULL,
	.query_hw_caps = mtls_query_caps,
};

DEVICE_AND_API_INIT(crypto_mtls, CONFIG_CRYPTO_MBEDTLS_SHIM_DRV_NAME,
		    &mtls_shim_init, NULL, NULL,
		    POST_KERNEL, CONFIG_CRYPTO_INIT_PRIORITY,
		    (void *)&mtls_crypto_funcs);
