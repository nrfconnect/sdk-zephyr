/*
 * Copyright (c) 2021 Intellinium <giuliano.franchetto@intellinium.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>
#include <nrfx.h>
#if defined(CONFIG_LORAWAN_SE_NRFX_GENERATE_DEVEUI)
#include <drivers/hwinfo.h>
#endif

#include "LoRaMacHeaderTypes.h"

#include "secure-element.h"
#include "secure-element-nvm.h"
#include "nrfx_se_priv.h"

#include <logging/log.h>

#include <hw_unique_key.h>
#include <random/rand32.h>
#include <aes_alt.h>
#include <aes.h>
#include <cmac.h>
#include "../lorawan_se.h"

LOG_MODULE_REGISTER(nrfx_se, CONFIG_LORAWAN_LOG_LEVEL);

static SecureElementNvmData_t *se_nvm;

#if defined(CONFIG_LORAWAN_SE_NRFX_GENERATE_DEVEUI)
static int nrfx_gen_deveui(uint8_t *buf)
{
	ssize_t len;

	len = hwinfo_get_device_id(buf, 8);
	if (len < 0) {
		return len;
	}

	__ASSERT_NO_MSG(len == 8);
	return 0;
}

static int nrfx_se_check_or_gen_deveui(SecureElementNvmData_t *nvm)
{
	int err;
	bool init = false;
	uint8_t devEUI[8] = {0};

	for (int i = 0; i < sizeof(nvm->DevEui); i++) {
		if (nvm->DevEui[i] != 0x00) {
			init = true;
			break;
		}
	}

	err = nrfx_gen_deveui(devEUI);
	if (err) {
		return err;
	}

	if (!init) {
		memcpy(nvm->DevEui, devEUI, 8);
	} else {
		__ASSERT(memcmp(nvm->DevEui, devEUI, 8) == 0,
			 "The stored devEUI is not the generated one!");
	}

	return 0;
}
#endif

static int nrfx_se_init(SecureElementNvmData_t *nvm)
{
	se_nvm = nvm;

	/* The KMU is not meant to be used for session key managed by
	 * the application.
	 * This statement is quoted from the nRF5340 product specification:
	 * The use of the key storage region in UICR should be limited to keys
	 * with a certain life span, and not per-session derived keys where
	 * the CPU is involved in the key exchange.
	 *
	 * This mean that we should not store the LoRaWAN keys directly in the
	 * KMU, but we don't want to store the private key unencrypted on either
	 * the internal or an external flash connected to the core.
	 *
	 * The chosen strategy is to use the HUK_KEYSLOT_MEXT hardware key,
	 * which will then be used to generate a external storage LoRaWAN key
	 * using derivation.
	 * This key will be used as a "Daughter LoRaWAN key".
	 * The random used to generate the daughter key must be stored.
	 *
	 * This is not the responsibility of this driver to generate the
	 * HUK_KEYSLOT_MEXT hardware key.
	 *
	 * The daughter LoRaWAN key will then be used to encrypt/decrypt the
	 * actual LoRaWAN key stored in the flash.
	 *
	 * If no HUK_KEYSLOT_MEXT key is stored, then the LoRaWAN keys will
	 * be stored without any encryption in the flash.
	 */

	if (!hw_unique_key_is_written(HUK_KEYSLOT_MEXT)) {
		LOG_WRN("No HUK_KEYSLOT_MEXT detected. LoRaWAN keys will"
			" be stored unencrypted.");
	}

#if defined(CONFIG_LORAWAN_SE_NRFX_GENERATE_DEVEUI)
	int err;

	err = nrfx_se_check_or_gen_deveui(nvm);
	if (err) {
		return err;
	}
#endif

	/* Nothing to do with JoinEUI as it is managed by the
	 * NVM backend, as the JoinEUI is public
	 */

	return SECURE_ELEMENT_SUCCESS;
}

static int nrfx_se_encrypt_key(struct nrfx_se_key *se_key,
			       uint8_t *key)
{
	int err;
	uint8_t dk[16] = {0};
	mbedtls_aes_context ctx = {0};

	/* We need to generate a random */
	sys_rand_get(se_key->random, ARRAY_SIZE(se_key->random));

	/* Then derivate the HUK_KEYSLOT_MEXT key.
	 * Note, the label is always "lorawan"
	 */
	err = hw_unique_key_derive_key(HUK_KEYSLOT_MEXT,
				       se_key->random,
				       ARRAY_SIZE(se_key->random),
				       "lorawan", 7,
				       dk, ARRAY_SIZE(dk));
	if (err) {
		LOG_ERR("Could not derivate key, error %d", err);
		return SECURE_ELEMENT_ERROR;
	}

	/* And finally, encrypt the key with the daughter key */
	mbedtls_aes_init(&ctx);
	err = mbedtls_aes_setkey_enc(&ctx, dk, 8 * ARRAY_SIZE(dk));
	if (err != 0) {
		LOG_ERR("Could not set key, error %d", err);
		memset(se_key->random, 0, ARRAY_SIZE(se_key->random));
		mbedtls_aes_free(&ctx);
		return SECURE_ELEMENT_ERROR;
	}

	mbedtls_internal_aes_encrypt(&ctx, key, se_key->value);
	mbedtls_aes_free(&ctx);

	return 0;
}

static int nrfx_se_set_key(KeyIdentifier_t keyID, uint8_t *key)
{
	int err = 0;
	struct nrfx_se_key se_key;
	bool need_encrypt = hw_unique_key_is_written(HUK_KEYSLOT_MEXT);

	/* The strategy used to store a new LoRaWAN key is as follow:
	 * - We need to check if a HUK_KEYSLOT_MEXT is available. If not,
	 * the key is written as-is in the flash.
	 * - If the HUK_KEYSLOT_MEXT is available, we generate a random used
	 * to derivate the HUK_KEYSLOT_MEXT key. The label is defined as
	 * "lorawan".
	 * - Once the Daughter LoRaWAN key is ready, we encrypt the key using it
	 * - The key is then passed written to the flash.
	 *
	 * This strategy is not used from MC_KEY_0 to MC_KEY_3 as these keys
	 * are already encrypted.
	 */
	if (!key) {
		return SECURE_ELEMENT_ERROR_NPE;
	}

	if (keyID == MC_KEY_0 ||
		keyID == MC_KEY_1 ||
		keyID == MC_KEY_2 ||
		keyID == MC_KEY_3) {

		need_encrypt = false;
		if (SecureElementAesEncrypt(key,
					    16,
					    MC_KE_KEY,
					    se_key.value)) {
			return SECURE_ELEMENT_FAIL_ENCRYPT;
		}
	}

	if (need_encrypt) {
		err = nrfx_se_encrypt_key(&se_key, key);
	} else {
		memcpy(se_key.value, key, 16);
	}

	if (err) {
		return err;
	}

	err = nrfx_se_keys_save(keyID, &se_key);
	if (err) {
		LOG_ERR("Could not save key, error %d", err);
		return SECURE_ELEMENT_ERROR;
	}

	return SECURE_ELEMENT_SUCCESS;
}

static int nrfx_se_decrypt_key(struct nrfx_se_key *se_key,
			       uint8_t *out)
{
	int err;
	uint8_t dk[16] = {0};
	mbedtls_aes_context ctx = {0};

	err = hw_unique_key_derive_key(HUK_KEYSLOT_MEXT,
				       se_key->random,
				       ARRAY_SIZE(se_key->random),
				       "lorawan", 7,
				       dk, ARRAY_SIZE(dk));
	if (err) {
		return SECURE_ELEMENT_ERROR;
	}

	mbedtls_aes_init(&ctx);
	err = mbedtls_aes_setkey_dec(&ctx, dk, 8 * ARRAY_SIZE(dk));
	if (err != 0) {
		mbedtls_aes_free(&ctx);
		return SECURE_ELEMENT_ERROR;
	}

	mbedtls_internal_aes_decrypt(&ctx, se_key->value, out);
	mbedtls_aes_free(&ctx);
	return SECURE_ELEMENT_SUCCESS;
}

static int nrfx_se_get_decrypted_key(KeyIdentifier_t id, uint8_t *out)
{
	int err;
	struct nrfx_se_key se_key;
	bool need_decrypt =
		hw_unique_key_is_written(HUK_KEYSLOT_MEXT) &&
			id != MC_KEY_0 &&
			id != MC_KEY_1 &&
			id != MC_KEY_2 &&
			id != MC_KEY_3;

	err = nrfx_se_keys_load(id, &se_key);
	if (err) {
		return err;
	}

	if (!need_decrypt) {
		memcpy(out, se_key.value, 16);
		return SECURE_ELEMENT_SUCCESS;
	} else {
		return nrfx_se_decrypt_key(&se_key, out);
	}
}

/** Computes a CMAC of a message using provided initial Bx block
 *
 *  cmac = aes128_cmac(keyID, blocks[i].Buffer)
 *
 * @param[IN]  micBxBuffer    - Buffer containing the initial Bx block
 * @param[IN]  buffer         - Data buffer
 * @param[IN]  size           - Data buffer size
 * @param[IN]  keyID          - Key identifier to determine the AES key to be used
 * @param[OUT] cmac           - Computed cmac
 * @retval                    - Status of the operation
 */
static SecureElementStatus_t compute_cmac(uint8_t *micBxBuffer,
					  uint8_t *buffer,
					  uint16_t size,
					  KeyIdentifier_t keyID,
					  uint32_t *cmac)
{
	int err;
	uint8_t cmac_val[16];
	uint8_t enc_key[16] = {0};
	mbedtls_cipher_context_t m_ctx;
	const mbedtls_cipher_info_t *cipher_info;

	if (!buffer || !cmac) {
		return SECURE_ELEMENT_ERROR_NPE;
	}

	cipher_info = mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_128_ECB);
	if (!cipher_info) {
		return SECURE_ELEMENT_FAIL_CMAC;
	}

	mbedtls_cipher_init(&m_ctx);

	if (mbedtls_cipher_setup(&m_ctx, cipher_info)) {
		mbedtls_cipher_free(&m_ctx);
		return SECURE_ELEMENT_FAIL_CMAC;
	}

	err = nrfx_se_get_decrypted_key(keyID, enc_key);
	if (err) {
		mbedtls_cipher_free(&m_ctx);
		return err;
	}

	err = mbedtls_cipher_cmac_starts(&m_ctx, enc_key, 16 * 8);
	if (err) {
		mbedtls_cipher_free(&m_ctx);
		return SECURE_ELEMENT_FAIL_CMAC;
	}

	if (micBxBuffer != NULL) {
		err = mbedtls_cipher_cmac_update(&m_ctx, micBxBuffer, 16);
		if (err) {
			mbedtls_cipher_free(&m_ctx);
			return SECURE_ELEMENT_FAIL_CMAC;
		}
	}

	err = mbedtls_cipher_cmac_update(&m_ctx, buffer, size);
	if (err) {
		mbedtls_cipher_free(&m_ctx);
		return SECURE_ELEMENT_FAIL_CMAC;
	}

	err = mbedtls_cipher_cmac_finish(&m_ctx, cmac_val);
	if (err) {
		mbedtls_cipher_free(&m_ctx);
		return SECURE_ELEMENT_FAIL_CMAC;
	}

	*cmac = (uint32_t) ((uint32_t) cmac_val[3] << 24 |
		(uint32_t) cmac_val[2] << 16 |
		(uint32_t) cmac_val[1] << 8 |
		(uint32_t) cmac_val[0]);

	mbedtls_cipher_free(&m_ctx);
	return SECURE_ELEMENT_SUCCESS;
}

static int nrfx_se_compute_cmac(uint8_t *micBxBuffer,
				uint8_t *buffer,
				uint16_t size,
				KeyIdentifier_t keyID,
				uint32_t *cmac)
{
	if (keyID >= LORAMAC_CRYPTO_MULTICAST_KEYS) {
		return SECURE_ELEMENT_ERROR_INVALID_KEY_ID;
	}

	return compute_cmac(micBxBuffer, buffer, size, keyID, cmac);
}

static int nrfx_se_verify_cmac(uint8_t *buffer, uint16_t size,
			       uint32_t expectedCmac,
			       KeyIdentifier_t keyID)
{
	uint32_t compCmac = 0;
	SecureElementStatus_t err;

	if (buffer == NULL) {
		return SECURE_ELEMENT_ERROR_NPE;
	}

	err = compute_cmac(NULL, buffer, size, keyID, &compCmac);
	if (err != SECURE_ELEMENT_SUCCESS) {
		return err;
	}

	if (expectedCmac != compCmac) {
		err = SECURE_ELEMENT_FAIL_CMAC;
	}

	return err;
}

static int nrfx_se_encrypt(uint8_t *buffer,
			   uint16_t size,
			   KeyIdentifier_t keyID,
			   uint8_t *encBuffer)
{
	uint8_t enc_key[16] = {0};
	SecureElementStatus_t err;
	mbedtls_aes_context ctx = {0};

	if (!buffer || !encBuffer) {
		return SECURE_ELEMENT_ERROR_NPE;
	}

	/* Check if the size is divisible by 16 */
	if ((size % 16) != 0) {
		return SECURE_ELEMENT_ERROR_BUF_SIZE;
	}

	err = nrfx_se_get_decrypted_key(keyID, enc_key);
	if (err) {
		return err;
	}

	mbedtls_aes_init(&ctx);

	err = mbedtls_aes_setkey_enc(&ctx, enc_key, 8 * 16);
	if (err != 0) {
		memset(enc_key, 0, 16);
		mbedtls_aes_free(&ctx);
		LOG_ERR("Could not set shadow KMU ECB encrypt key.");
		return err;
	}

	for (int i = 0; i < size / 16; i++) {
		mbedtls_aes_encrypt(&ctx,
				    &buffer[i * 16],
				    &encBuffer[i * 16]);
	}

	memset(enc_key, 0, 16);
	mbedtls_aes_free(&ctx);

	return 0;
}

static int nrfx_se_derive(uint8_t *input,
			  KeyIdentifier_t rootKeyID,
			  KeyIdentifier_t targetKeyID)
{
	uint8_t key[16] = {0};
	SecureElementStatus_t err;

	if (!input) {
		return SECURE_ELEMENT_ERROR_NPE;
	}

	/* In case of MC_KE_KEY, only McRootKey can be used as root key */
	if (targetKeyID == MC_KE_KEY && rootKeyID != MC_ROOT_KEY) {
		return SECURE_ELEMENT_ERROR_INVALID_KEY_ID;
	}

	err = SecureElementAesEncrypt(input, 16, rootKeyID, key);
	if (err != SECURE_ELEMENT_SUCCESS) {
		return err;
	}

	err = SecureElementSetKey(targetKeyID, key);
	if (err != SECURE_ELEMENT_SUCCESS) {
		return err;
	}

	return SECURE_ELEMENT_SUCCESS;
}

int nrfx_process_join_accept(JoinReqIdentifier_t joinReqType,
			     uint8_t *joinEui,
			     uint16_t devNonce,
			     uint8_t *encJoinAccept,
			     uint8_t encJoinAcceptSize,
			     uint8_t *decJoinAccept,
			     uint8_t *versionMinor)
{
#if (USE_LRWAN_1_1_X_CRYPTO == 0)
	ARG_UNUSED(joinEui);
	ARG_UNUSED(devNonce);
#endif

	uint32_t mic;
	KeyIdentifier_t encKeyID = NWK_KEY;

	if (!encJoinAccept ||
		!decJoinAccept ||
		!versionMinor) {
		return SECURE_ELEMENT_ERROR_NPE;
	}

	/* Check that frame size isn't bigger than a
	 * JoinAccept with CFList size
	 */
	if (encJoinAcceptSize > LORAMAC_JOIN_ACCEPT_FRAME_MAX_SIZE) {
		return SECURE_ELEMENT_ERROR_BUF_SIZE;
	}

	if (joinReqType != JOIN_REQ) {
		encKeyID = J_S_ENC_KEY;
	}

	memcpy(decJoinAccept, encJoinAccept, encJoinAcceptSize);

	if (SecureElementAesEncrypt(encJoinAccept + LORAMAC_MHDR_FIELD_SIZE,
				    encJoinAcceptSize - LORAMAC_MHDR_FIELD_SIZE,
				    encKeyID,
				    decJoinAccept + LORAMAC_MHDR_FIELD_SIZE)) {
		return SECURE_ELEMENT_FAIL_ENCRYPT;
	}

	*versionMinor = ((decJoinAccept[11] & 0x80) == 0x80) ? 1 : 0;

	mic = ((uint32_t) decJoinAccept[encJoinAcceptSize
		- LORAMAC_MIC_FIELD_SIZE] << 0);
	mic |= ((uint32_t) decJoinAccept[encJoinAcceptSize
		- LORAMAC_MIC_FIELD_SIZE + 1] << 8);
	mic |= ((uint32_t) decJoinAccept[encJoinAcceptSize
		- LORAMAC_MIC_FIELD_SIZE + 2] << 16);
	mic |= ((uint32_t) decJoinAccept[encJoinAcceptSize
		- LORAMAC_MIC_FIELD_SIZE + 3] << 24);

	/*  - Header buffer to be used for MIC computation
	 *        - LoRaWAN 1.0.x : micHeader = [MHDR(1)]
	 *        - LoRaWAN 1.1.x : micHeader = [JoinReqType(1), JoinEUI(8), DevNonce(2), MHDR(1)]
	 */

	/* Verify mic */
	if (*versionMinor == 0) {
		/* For LoRaWAN 1.0.x
		 *   cmac = aes128_cmac(NwkKey, MHDR |  JoinNonce | NetID |
		 *       DevAddr | DLSettings | RxDelay | CFList |
		 *   CFListType)
		 */
		size_t size = encJoinAcceptSize - LORAMAC_MIC_FIELD_SIZE;

		if (SecureElementVerifyAesCmac(decJoinAccept,
					       size,
					       mic,
					       NWK_KEY)) {
			return SECURE_ELEMENT_FAIL_CMAC;
		}
	}
#if (USE_LRWAN_1_1_X_CRYPTO == 1)
		/* TODO */
#endif
	else {
		return SECURE_ELEMENT_ERROR_INVALID_LORAWAM_SPEC_VERSION;
	}

	return SECURE_ELEMENT_SUCCESS;
}

static int nrfx_se_set_deveui(uint8_t *devEui)
{
#if defined(CONFIG_LORAWAN_SE_NRFX_GENERATE_DEVEUI)
	ARG_UNUSED(devEui);

	return SECURE_ELEMENT_SUCCESS;
#else
	if (!devEui) {
		return SECURE_ELEMENT_ERROR_NPE;
	}
	memcpy(se_nvm->DevEui, devEui, SE_EUI_SIZE);
	return SECURE_ELEMENT_SUCCESS;
#endif
}

uint8_t *nrfx_get_deveui(void)
{
	return se_nvm->DevEui;
}

static int nrfx_se_set_join_eui(uint8_t *joinEui)
{
	if (!joinEui) {
		return SECURE_ELEMENT_ERROR_NPE;
	}

	memcpy(se_nvm->JoinEui, joinEui, SE_EUI_SIZE);

	return SECURE_ELEMENT_SUCCESS;
}

uint8_t *nrfx_get_join_eui(void)
{
	return se_nvm->JoinEui;
}

static int nrfx_se_set_pin(uint8_t *pin)
{
	ARG_UNUSED(pin);

	return SECURE_ELEMENT_SUCCESS;
}

uint8_t *nrfx_se_get_pin(void)
{
	return se_nvm->Pin;
}

static const struct lorawan_se nrfx_se = {
	.init = nrfx_se_init,
	.set_key = nrfx_se_set_key,
	.compute_cmac = nrfx_se_compute_cmac,
	.verify_cmac = nrfx_se_verify_cmac,
	.encrypt = nrfx_se_encrypt,
	.derive = nrfx_se_derive,
	.process_join_accept = nrfx_process_join_accept,
	.set_deveui = nrfx_se_set_deveui,
	.get_deveui = nrfx_get_deveui,
	.set_joineui = nrfx_se_set_join_eui,
	.get_joineui = nrfx_get_join_eui,
	.set_pin = nrfx_se_set_pin,
	.get_pin = nrfx_se_get_pin
};

#include <init.h>
static int nrfx_se_register(const struct device *device)
{
	ARG_UNUSED(device);

	lorawan_register(&nrfx_se);

	return 0;
}
SYS_INIT(nrfx_se_register, POST_KERNEL, 0);
