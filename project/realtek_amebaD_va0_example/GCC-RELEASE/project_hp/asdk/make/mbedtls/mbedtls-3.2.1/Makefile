
include $(MAKE_INCLUDE_GEN)

.PHONY: all clean

#*****************************************************************************#
#                               VARIABLES	                              #
#*****************************************************************************#
DIR = $(BASEDIR)/component/common/network/ssl/mbedtls-3.2.1
DIR_RAMMAP = $(BASEDIR)/component/common/network/ssl/ssl_ram_map

MODULE_IFLAGS += -DCONFIG_BUILD_NONSECURE=1

vpath %.c $(DIR) $(shell find $(DIR) -type d) $(DIR_RAMMAP) $(shell find $(DIR_RAMMAP) -type d)

#*****************************************************************************#
#                               Source FILE LIST                              #
#*****************************************************************************#
CSRC = $(DIR)/library/aes.c \
       $(DIR)/library/aesni.c \
       $(DIR)/library/aria.c \
       $(DIR)/library/asn1parse.c \
       $(DIR)/library/asn1write.c \
       $(DIR)/library/base64.c \
       $(DIR)/library/bignum.c \
       $(DIR)/library/camellia.c \
       $(DIR)/library/ccm.c \
       $(DIR)/library/chacha20.c \
       $(DIR)/library/chachapoly.c \
       $(DIR)/library/cipher.c \
       $(DIR)/library/cipher_wrap.c \
       $(DIR)/library/cmac.c \
       $(DIR)/library/constant_time.c \
       $(DIR)/library/ctr_drbg.c \
       $(DIR)/library/debug.c \
       $(DIR)/library/des.c \
       $(DIR)/library/dhm.c \
       $(DIR)/library/ecdh.c \
       $(DIR)/library/ecdsa.c \
       $(DIR)/library/ecjpake.c \
       $(DIR)/library/ecp.c \
       $(DIR)/library/ecp_curves.c \
       $(DIR)/library/entropy.c \
       $(DIR)/library/entropy_poll.c \
       $(DIR)/library/error.c \
       $(DIR)/library/gcm.c \
       $(DIR)/library/hmac_drbg.c \
       $(DIR)/library/md.c \
       $(DIR)/library/md5.c \
       $(DIR)/library/memory_buffer_alloc.c \
       $(DIR)/library/mps_reader.c \
       $(DIR)/library/mps_trace.c \
       $(DIR)/library/net_sockets.c \
       $(DIR)/library/nist_kw.c \
       $(DIR)/library/oid.c \
       $(DIR)/library/padlock.c \
       $(DIR)/library/pem.c \
       $(DIR)/library/pk.c \
       $(DIR)/library/pk_wrap.c \
       $(DIR)/library/pkcs12.c \
       $(DIR)/library/pkcs5.c \
       $(DIR)/library/pkparse.c \
       $(DIR)/library/pkwrite.c \
       $(DIR)/library/platform.c \
       $(DIR)/library/platform_util.c \
       $(DIR)/library/poly1305.c \
       $(DIR)/library/psa_crypto.c \
       $(DIR)/library/psa_crypto_aead.c \
       $(DIR)/library/psa_crypto_cipher.c \
       $(DIR)/library/psa_crypto_client.c \
       $(DIR)/library/psa_crypto_driver_wrappers.c \
       $(DIR)/library/psa_crypto_ecp.c \
       $(DIR)/library/psa_crypto_hash.c \
       $(DIR)/library/psa_crypto_mac.c \
       $(DIR)/library/psa_crypto_rsa.c \
       $(DIR)/library/psa_crypto_se.c \
       $(DIR)/library/psa_crypto_slot_management.c \
       $(DIR)/library/psa_crypto_storage.c \
       $(DIR)/library/psa_its_file.c \
       $(DIR)/library/ripemd160.c \
       $(DIR)/library/rsa.c \
       $(DIR)/library/rsa_alt_helpers.c \
       $(DIR)/library/sha1.c \
       $(DIR)/library/sha256.c \
       $(DIR)/library/sha512.c \
       $(DIR)/library/ssl_cache.c \
       $(DIR)/library/ssl_ciphersuites.c \
       $(DIR)/library/ssl_cli.c \
       $(DIR)/library/ssl_cookie.c \
       $(DIR)/library/ssl_debug_helpers_generated.c \
       $(DIR)/library/ssl_msg.c \
       $(DIR)/library/ssl_ticket.c \
       $(DIR)/library/ssl_tls.c \
       $(DIR)/library/ssl_tls12_client.c \
       $(DIR)/library/ssl_tls12_server.c \
       $(DIR)/library/ssl_tls13_client.c \
       $(DIR)/library/ssl_tls13_generic.c \
       $(DIR)/library/ssl_tls13_keys.c \
       $(DIR)/library/ssl_tls13_server.c \
       $(DIR)/library/threading.c \
       $(DIR)/library/timing.c \
       $(DIR)/library/version.c \
       $(DIR)/library/version_features.c \
       $(DIR)/library/x509.c \
       $(DIR)/library/x509_create.c \
       $(DIR)/library/x509_crl.c \
       $(DIR)/library/x509_crt.c \
       $(DIR)/library/x509_csr.c \
       $(DIR)/library/x509write_crt.c \
       $(DIR)/library/x509write_csr.c \
       $(DIR_RAMMAP)/ssl_ram_map.c	\

#*****************************************************************************#
#                               Object FILE LIST                              #
#*****************************************************************************#
OBJS = $(notdir $(CSRC:.c=.o))

#*****************************************************************************#
#                               Include Dependency                            #
#*****************************************************************************#
-include $(OBJS:.o=.d)

	
#*****************************************************************************#
#                        RULES TO GENERATE TARGETS                            #
#*****************************************************************************#
# Define the Rules to build the core targets
all: CORE_TARGETS COPY_RAM_OBJS

#*****************************************************************************#
#              GENERATE OBJECT FILE 
#*****************************************************************************#
CORE_TARGETS: $(OBJS)

#*****************************************************************************#
#              CLEAN GENERATED FILES                                          #
#*****************************************************************************#
clean: CLEAN_OBJS
	$(REMOVE) *.o
	$(REMOVE) *.i
	$(REMOVE) *.s
	$(REMOVE) *.d

-include $(DEPS)
