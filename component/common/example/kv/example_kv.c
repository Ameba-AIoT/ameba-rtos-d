#include "kv.h"
#include "example_kv.h"
#include "lfs.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "littlefs_adapter.h"
#include "mbedtls/aes.h"

extern kv_reg_ptr kv_encrypt;
extern kv_reg_ptr kv_decrypt;

#define USE_ENCRYPT 0
#define DUMP_BUFFER 1
#define AES_KEY_LEN 16
static const unsigned char aeskey[32] = {
	0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
	0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,
	0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
	0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a
} ;

static void dump_buf(char *info, uint8_t *buf, uint32_t len)
{
	printf("%s", info);
	for (int i = 0; i < len; i++) {
		printf("%s%02X%s", i % 16 == 0 ? "\n     " : " ",
			   buf[i], i == len - 1 ? "\n" : "");
	}
}

//output buffer should be 16 bytes aligned for aes cbc demand
int vfs_mbedtls_aes_cbc_encrypt(unsigned char *input, unsigned char *output, unsigned int len)
{
	unsigned char Iv[16] = {0};
	mbedtls_aes_context ctx;
	unsigned char *message;
	unsigned short msglen = (len / 16 + 1) * 16;

	message = malloc(msglen * sizeof(unsigned char));
	memcpy(message, input, len);

#if DUMP_BUFFER
	dump_buf("write encrypt before", message, msglen);
#endif

	mbedtls_aes_init(&ctx);
	mbedtls_aes_setkey_enc(&ctx, aeskey, AES_KEY_LEN * 8);
	mbedtls_aes_crypt_cbc(&ctx, 1, msglen, Iv, message, output);
	mbedtls_aes_free(&ctx);

#if DUMP_BUFFER
	dump_buf("write encrypt after", output, msglen);
#endif

	free(message);

	return 0;
}

//input buffer should be 16 bytes aligned for aes cbc demand
int vfs_mbedtls_aes_cbc_decrypt(unsigned char *input, unsigned char *output, unsigned int len)
{
	unsigned char Iv[16] = {0};
	mbedtls_aes_context ctx;
	unsigned char *aesdecsw;
	unsigned short msglen = (len / 16 + 1) * 16;

	aesdecsw = malloc(msglen * sizeof(unsigned char));

#if DUMP_BUFFER
	dump_buf("read decrypt before ", input, msglen);
#endif

	mbedtls_aes_init(&ctx);
	mbedtls_aes_setkey_dec(&ctx, aeskey, AES_KEY_LEN * 8);
	mbedtls_aes_crypt_cbc(&ctx, 0, msglen, Iv, input, aesdecsw);
	mbedtls_aes_free(&ctx);

#if DUMP_BUFFER
	dump_buf("read decrypt after ", aesdecsw, msglen);
#endif

	memcpy(output, aesdecsw, len);
	free(aesdecsw);

	return 0;
}

void example_kv_thread(void *param)
{
	vTaskDelay(2000);
	char key[] = "T_FILE";
	unsigned char val[] = "KV module works normally !!!";
	char *buffer;
	int res = 0;

	rt_kv_init();

#if USE_ENCRYPT
	kv_encrypt = vfs_mbedtls_aes_cbc_encrypt;
	kv_decrypt = vfs_mbedtls_aes_cbc_decrypt;
#endif

	res = rt_kv_set(key, val, strlen(val));
	if (res != strlen(val)) {
		printf("rt_kv_set failed\r\n");
	} else {
		printf("rt_kv_set success, write %d letters.\r\n", strlen(val));
	}

	buffer = (char *)malloc(strlen(val) * sizeof(char));
	memset(buffer, 0, strlen(val));
	res = rt_kv_get(key, buffer, strlen(val));

	if (res != strlen(val)) {
		printf("rt_kv_get failed\r\n");
	} else {
		if (memcmp(buffer, val, strlen(val)) == 0) {
			printf("rt_kv_get success, read %d letters.\r\n", strlen(val));
		} else {
			printf("rt_kv_get fail, content has been changed.\r\n", strlen(val));
		}
	}

	res = rt_kv_delete(key);
	if (res) {
		printf("rt_kv_delete failed.\r\n");
	} else {
		printf("rt_kv_delete success.\r\n");
	}

	free(buffer);
	vTaskDelete(NULL);
}

void example_kv(void)
{
	if (xTaskCreate(example_kv_thread, ((const char *)"example_kv_thread"), 2048 * 4, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
		printf("\n\r%s rtos_task_create(example_kv_thread) failed", __FUNCTION__);
	}
}
