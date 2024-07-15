#include "FreeRTOS.h"
#include "task.h"
#include <platform_stdlib.h>
#include <lwip_netconf.h>
#include <osdep_service.h>

#include "platform_opts.h"

#if CONFIG_USE_POLARSSL

#include "polarssl/net.h"
#include "polarssl/ssl.h"
#include "polarssl/memory.h"
#include "polarssl/certs.h"
#include "polarssl/error.h"


#define SERVER_PORT   443
#define STACKSIZE     1150
extern struct netif xnetif[NET_IF_NUM];


static int my_random(void *p_rng, unsigned char *output, size_t output_len)
{
	rtw_get_random_bytes(output, output_len);
	return 0;
}

static void example_ssl_server_thread(void *param){
#if !defined(POLARSSL_BIGNUM_C) || !defined(POLARSSL_CERTS_C) || \
	!defined(POLARSSL_SSL_TLS_C) || !defined(POLARSSL_SSL_SRV_C) || \
	!defined(POLARSSL_RSA_C) || !defined(POLARSSL_NET_C) || \
	!defined(POLARSSL_X509_CRT_PARSE_C)

	printf("POLARSSL_BIGNUM_C and/or POLARSSL_CERTS_C and/or "
		"POLARSSL_SSL_TLS_C and/or POLARSSL_SSL_SRV_C and/or "
		"POLARSSL_RSA_C and/or POLARSSL_NET_C and/or "
		"POLARSSL_X509_CRT_PARSE_C not defined.\n");

#else
	int ret, server_fd = -1, client_fd = -1;
	x509_crt server_x509;
	pk_context server_pk;
	ssl_context ssl;
	unsigned char buf[512];
	uint8_t *ip;
	char *response = "<HTML><BODY>SSL OK</BODY></HTML>";

	// Delay to wait for IP by DHCP
	vTaskDelay(10000);
	printf("\nExample: SSL server\n");

	memory_set_own(pvPortMalloc, vPortFree);
	memset(&server_x509, 0, sizeof(x509_crt));
	memset(&server_pk, 0, sizeof(pk_context));
	memset(&ssl, 0, sizeof(ssl_context));

	/*
	 * 1. Prepare the certificate and key
	 */
	printf("\n\r  . Preparing the certificate and key...");

	x509_crt_init(&server_x509);
	pk_init(&server_pk);

	if((ret = x509_crt_parse(&server_x509, (const unsigned char *)test_srv_crt, strlen(test_srv_crt))) != 0) {
		printf(" failed\n  ! x509_crt_parse returned %d\n\n", ret);
		goto exit;
	}

	if((ret = x509_crt_parse(&server_x509, (const unsigned char *)test_ca_list, strlen(test_ca_list))) != 0) {
		printf(" failed\n  ! x509_crt_parse returned %d\n\n", ret);
		goto exit;
	}

	if((ret = pk_parse_key(&server_pk, test_srv_key, strlen(test_srv_key), NULL, 0)) != 0) {
		printf(" failed\n  ! pk_parse_key returned %d\n\n", ret);
		goto exit;
	}

	printf(" ok\n");

	/*
	 * 2. Start the connection
	 */
	ip = LwIP_GetIP(&xnetif[0]);
	printf("\n\r  . Starting tcp server /%d.%d.%d.%d/%d...", ip[0], ip[1], ip[2], ip[3], SERVER_PORT);

	if((ret = net_bind(&server_fd, NULL, SERVER_PORT)) != 0) {
		printf(" failed\n  ! net_bind returned %d\n\n", ret);
		goto exit;
	}

	printf(" ok\n");

	/*
	 * 3. Waiting for client to connect
	 */
	printf("\n\r  . Waiting for client to connect...\n\r");

	while((ret = net_accept(server_fd, &client_fd, NULL)) == 0) {
		printf("\n\r  . A client is connecting\n\r");
		/*
		 * 4. Setup stuff
		 */
		printf("\n\r  . Setting up the SSL/TLS structure...");

		if((ret = ssl_init(&ssl)) != 0) {
			printf(" failed\n  ! ssl_init returned %d\n\n", ret);
			goto close_client;
		}

		ssl_set_endpoint(&ssl, SSL_IS_SERVER);
		ssl_set_ca_chain(&ssl, server_x509.next, NULL, NULL);
		ssl_set_authmode(&ssl, SSL_VERIFY_NONE);
		ssl_set_rng(&ssl, my_random, NULL);
		ssl_set_bio(&ssl, net_recv, &client_fd, net_send, &client_fd);
		if((ret = ssl_set_own_cert(&ssl, &server_x509, &server_pk)) != 0) {
			printf(" failed\n  ! ssl_set_own_cert returned %d\n\n", ret);
			goto close_client;
		}
		printf(" ok\n");

		/*
		 * 5. Handshake
		 */
		printf("\n\r  . Performing the SSL/TLS handshake...");

		if((ret = ssl_handshake(&ssl)) != 0) {
			printf(" failed\n  ! ssl_handshake returned %d\n\n", ret);
			goto close_client;
		}
		printf(" ok\n");
		printf("\n\r  . Use ciphersuite %s\n", ssl_get_ciphersuite(&ssl));

		/*
		 * 6. Read the request from client
		 */
		printf("\n\r  > Read request from client:");

		memset(buf, 0, sizeof(buf));
		if((ret = ssl_read(&ssl, buf, sizeof(buf))) <= 0) {
			if(ret != POLARSSL_ERR_NET_WANT_READ && ret != POLARSSL_ERR_NET_WANT_WRITE) {
				printf(" failed\n\r  ! ssl_read returned %d\n", ret);
				goto close_client;
			}
		}
		printf(" %d bytes read\n\r\n\r%s\n", ret, (char *) buf);

		/*
		 * 7. Response the request
		 */
		printf("\n\r  > Response to client:");

		sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n%s", strlen(response), response);
		if((ret = ssl_write(&ssl, buf, strlen(buf))) <= 0) {
			if(ret != POLARSSL_ERR_NET_WANT_READ && ret != POLARSSL_ERR_NET_WANT_WRITE) {
				printf(" failed\n\r  ! ssl_write returned %d\n", ret);
				goto close_client;
			}
		}
		printf(" %d bytes written\n\r\n\r%s\n", ret, (char *)buf);

close_client:
#ifdef POLARSSL_ERROR_C
		if(ret != 0) {
			char error_buf[100];
			polarssl_strerror(ret, error_buf, 100);
			printf("\n\rLast error was: %d - %s\n", ret, error_buf);
		}
#endif
		ssl_close_notify(&ssl);
		net_close(client_fd);
		ssl_free(&ssl);
	}
	net_close(server_fd);
exit:
	x509_crt_free(&server_x509);
	pk_free(&server_pk);
#endif
	vTaskDelete(NULL);
}

void example_ssl_server(void)
{
	if(xTaskCreate(example_ssl_server_thread, "example_ssl_server_thread", STACKSIZE, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS)
		printf("\n\r%s xTaskCreate example_ssl_server_thread failed", __FUNCTION__);
}

#elif CONFIG_USE_MBEDTLS /* CONFIG_USE_POLARSSL */
#if defined(MBEDTLS_VERSION_NUMBER) && (MBEDTLS_VERSION_NUMBER>=0x03010000)
#define MBEDTLS_CERTS_C 1
#include "mbedtls/build_info.h"
// mbedtls-3.x removed cert modules, so please embed your own test certificates in your test code
#if defined(MBEDTLS_ECDSA_C)
#define TEST_CA_CRT_EC                                                 \
"-----BEGIN CERTIFICATE-----\r\n"                                      \
"MIICBDCCAYigAwIBAgIJAMFD4n5iQ8zoMAwGCCqGSM49BAMCBQAwPjELMAkGA1UE\r\n" \
"BhMCTkwxETAPBgNVBAoMCFBvbGFyU1NMMRwwGgYDVQQDDBNQb2xhcnNzbCBUZXN0\r\n" \
"IEVDIENBMB4XDTE5MDIxMDE0NDQwMFoXDTI5MDIxMDE0NDQwMFowPjELMAkGA1UE\r\n" \
"BhMCTkwxETAPBgNVBAoMCFBvbGFyU1NMMRwwGgYDVQQDDBNQb2xhcnNzbCBUZXN0\r\n" \
"IEVDIENBMHYwEAYHKoZIzj0CAQYFK4EEACIDYgAEw9orNEE3WC+HVv78ibopQ0tO\r\n" \
"4G7DDldTMzlY1FK0kZU5CyPfXxckYkj8GpUpziwth8KIUoCv1mqrId240xxuWLjK\r\n" \
"6LJpjvNBrSnDtF91p0dv1RkpVWmaUzsgtGYWYDMeo1AwTjAMBgNVHRMEBTADAQH/\r\n" \
"MB0GA1UdDgQWBBSdbSAkSQE/K8t4tRm8fiTJ2/s2fDAfBgNVHSMEGDAWgBSdbSAk\r\n" \
"SQE/K8t4tRm8fiTJ2/s2fDAMBggqhkjOPQQDAgUAA2gAMGUCMFHKrjAPpHB0BN1a\r\n" \
"LH8TwcJ3vh0AxeKZj30mRdOKBmg/jLS3rU3g8VQBHpn8sOTTBwIxANxPO5AerimZ\r\n" \
"hCjMe0d4CTHf1gFZMF70+IqEP+o5VHsIp2Cqvflb0VGWFC5l9a4cQg==\r\n"         \
"-----END CERTIFICATE-----\r\n"



const char mbedtls_test_srv_crt_ec[] =
"-----BEGIN CERTIFICATE-----\r\n"
"MIICHzCCAaWgAwIBAgIBCTAKBggqhkjOPQQDAjA+MQswCQYDVQQGEwJOTDERMA8G\r\n"
"A1UEChMIUG9sYXJTU0wxHDAaBgNVBAMTE1BvbGFyc3NsIFRlc3QgRUMgQ0EwHhcN\r\n"
"MTMwOTI0MTU1MjA0WhcNMjMwOTIyMTU1MjA0WjA0MQswCQYDVQQGEwJOTDERMA8G\r\n"
"A1UEChMIUG9sYXJTU0wxEjAQBgNVBAMTCWxvY2FsaG9zdDBZMBMGByqGSM49AgEG\r\n"
"CCqGSM49AwEHA0IABDfMVtl2CR5acj7HWS3/IG7ufPkGkXTQrRS192giWWKSTuUA\r\n"
"2CMR/+ov0jRdXRa9iojCa3cNVc2KKg76Aci07f+jgZ0wgZowCQYDVR0TBAIwADAd\r\n"
"BgNVHQ4EFgQUUGGlj9QH2deCAQzlZX+MY0anE74wbgYDVR0jBGcwZYAUnW0gJEkB\r\n"
"PyvLeLUZvH4kydv7NnyhQqRAMD4xCzAJBgNVBAYTAk5MMREwDwYDVQQKEwhQb2xh\r\n"
"clNTTDEcMBoGA1UEAxMTUG9sYXJzc2wgVGVzdCBFQyBDQYIJAMFD4n5iQ8zoMAoG\r\n"
"CCqGSM49BAMCA2gAMGUCMQCaLFzXptui5WQN8LlO3ddh1hMxx6tzgLvT03MTVK2S\r\n"
"C12r0Lz3ri/moSEpNZWqPjkCMCE2f53GXcYLqyfyJR078c/xNSUU5+Xxl7VZ414V\r\n"
"fGa5kHvHARBPc8YAIVIqDvHH1Q==\r\n"
"-----END CERTIFICATE-----\r\n";

const char mbedtls_test_srv_key_ec[] =
"-----BEGIN EC PRIVATE KEY-----\r\n"
"MHcCAQEEIPEqEyB2AnCoPL/9U/YDHvdqXYbIogTywwyp6/UfDw6noAoGCCqGSM49\r\n"
"AwEHoUQDQgAEN8xW2XYJHlpyPsdZLf8gbu58+QaRdNCtFLX3aCJZYpJO5QDYIxH/\r\n"
"6i/SNF1dFr2KiMJrdw1VzYoqDvoByLTt/w==\r\n"
"-----END EC PRIVATE KEY-----\r\n";

const size_t mbedtls_test_srv_crt_ec_len = sizeof( mbedtls_test_srv_crt_ec );
const size_t mbedtls_test_srv_key_ec_len = sizeof( mbedtls_test_srv_key_ec );
#else
#define TEST_CA_CRT_EC
#endif /* MBEDTLS_ECDSA_C */

#if defined(MBEDTLS_RSA_C)
#define TEST_CA_CRT_RSA                                                \
"-----BEGIN CERTIFICATE-----\r\n"                                      \
"MIIDQTCCAimgAwIBAgIBAzANBgkqhkiG9w0BAQsFADA7MQswCQYDVQQGEwJOTDER\r\n" \
"MA8GA1UECgwIUG9sYXJTU0wxGTAXBgNVBAMMEFBvbGFyU1NMIFRlc3QgQ0EwHhcN\r\n" \
"MTkwMjEwMTQ0NDAwWhcNMjkwMjEwMTQ0NDAwWjA7MQswCQYDVQQGEwJOTDERMA8G\r\n" \
"A1UECgwIUG9sYXJTU0wxGTAXBgNVBAMMEFBvbGFyU1NMIFRlc3QgQ0EwggEiMA0G\r\n" \
"CSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDA3zf8F7vglp0/ht6WMn1EpRagzSHx\r\n" \
"mdTs6st8GFgIlKXsm8WL3xoemTiZhx57wI053zhdcHgH057Zk+i5clHFzqMwUqny\r\n" \
"50BwFMtEonILwuVA+T7lpg6z+exKY8C4KQB0nFc7qKUEkHHxvYPZP9al4jwqj+8n\r\n" \
"YMPGn8u67GB9t+aEMr5P+1gmIgNb1LTV+/Xjli5wwOQuvfwu7uJBVcA0Ln0kcmnL\r\n" \
"R7EUQIN9Z/SG9jGr8XmksrUuEvmEF/Bibyc+E1ixVA0hmnM3oTDPb5Lc9un8rNsu\r\n" \
"KNF+AksjoBXyOGVkCeoMbo4bF6BxyLObyavpw/LPh5aPgAIynplYb6LVAgMBAAGj\r\n" \
"UDBOMAwGA1UdEwQFMAMBAf8wHQYDVR0OBBYEFLRa5KWz3tJS9rnVppUP6z68x/3/\r\n" \
"MB8GA1UdIwQYMBaAFLRa5KWz3tJS9rnVppUP6z68x/3/MA0GCSqGSIb3DQEBCwUA\r\n" \
"A4IBAQA4qFSCth2q22uJIdE4KGHJsJjVEfw2/xn+MkTvCMfxVrvmRvqCtjE4tKDl\r\n" \
"oK4MxFOek07oDZwvtAT9ijn1hHftTNS7RH9zd/fxNpfcHnMZXVC4w4DNA1fSANtW\r\n" \
"5sY1JB5Je9jScrsLSS+mAjyv0Ow3Hb2Bix8wu7xNNrV5fIf7Ubm+wt6SqEBxu3Kb\r\n" \
"+EfObAT4huf3czznhH3C17ed6NSbXwoXfby7stWUDeRJv08RaFOykf/Aae7bY5PL\r\n" \
"yTVrkAnikMntJ9YI+hNNYt3inqq11A5cN0+rVTst8UKCxzQ4GpvroSwPKTFkbMw4\r\n" \
"/anT1dVxr/BtwJfiESoK3/4CeXR1\r\n"                                     \
"-----END CERTIFICATE-----\r\n"


const char mbedtls_test_srv_crt_rsa[] =
"-----BEGIN CERTIFICATE-----\r\n"                                      \
"MIIDNzCCAh+gAwIBAgIBAjANBgkqhkiG9w0BAQsFADA7MQswCQYDVQQGEwJOTDER\r\n" \
"MA8GA1UECgwIUG9sYXJTU0wxGTAXBgNVBAMMEFBvbGFyU1NMIFRlc3QgQ0EwHhcN\r\n" \
"MTkwMjEwMTQ0NDA2WhcNMjkwMjEwMTQ0NDA2WjA0MQswCQYDVQQGEwJOTDERMA8G\r\n" \
"A1UECgwIUG9sYXJTU0wxEjAQBgNVBAMMCWxvY2FsaG9zdDCCASIwDQYJKoZIhvcN\r\n" \
"AQEBBQADggEPADCCAQoCggEBAMFNo93nzR3RBNdJcriZrA545Do8Ss86ExbQWuTN\r\n" \
"owCIp+4ea5anUrSQ7y1yej4kmvy2NKwk9XfgJmSMnLAofaHa6ozmyRyWvP7BBFKz\r\n" \
"NtSj+uGxdtiQwWG0ZlI2oiZTqqt0Xgd9GYLbKtgfoNkNHC1JZvdbJXNG6AuKT2kM\r\n" \
"tQCQ4dqCEGZ9rlQri2V5kaHiYcPNQEkI7mgM8YuG0ka/0LiqEQMef1aoGh5EGA8P\r\n" \
"hYvai0Re4hjGYi/HZo36Xdh98yeJKQHFkA4/J/EwyEoO79bex8cna8cFPXrEAjya\r\n" \
"HT4P6DSYW8tzS1KW2BGiLICIaTla0w+w3lkvEcf36hIBMJcCAwEAAaNNMEswCQYD\r\n" \
"VR0TBAIwADAdBgNVHQ4EFgQUpQXoZLjc32APUBJNYKhkr02LQ5MwHwYDVR0jBBgw\r\n" \
"FoAUtFrkpbPe0lL2udWmlQ/rPrzH/f8wDQYJKoZIhvcNAQELBQADggEBAC465FJh\r\n" \
"Pqel7zJngHIHJrqj/wVAxGAFOTF396XKATGAp+HRCqJ81Ry60CNK1jDzk8dv6M6U\r\n" \
"HoS7RIFiM/9rXQCbJfiPD5xMTejZp5n5UYHAmxsxDaazfA5FuBhkfokKK6jD4Eq9\r\n" \
"1C94xGKb6X4/VkaPF7cqoBBw/bHxawXc0UEPjqayiBpCYU/rJoVZgLqFVP7Px3sv\r\n" \
"a1nOrNx8rPPI1hJ+ZOg8maiPTxHZnBVLakSSLQy/sWeWyazO1RnrbxjrbgQtYKz0\r\n" \
"e3nwGpu1w13vfckFmUSBhHXH7AAS/HpKC4IH7G2GAk3+n8iSSN71sZzpxonQwVbo\r\n" \
"pMZqLmbBm/7WPLc=\r\n"                                                 \
"-----END CERTIFICATE-----\r\n";

const char mbedtls_test_srv_key_rsa[] =
"-----BEGIN RSA PRIVATE KEY-----\r\n"                                  \
"MIIEpAIBAAKCAQEAwU2j3efNHdEE10lyuJmsDnjkOjxKzzoTFtBa5M2jAIin7h5r\r\n" \
"lqdStJDvLXJ6PiSa/LY0rCT1d+AmZIycsCh9odrqjObJHJa8/sEEUrM21KP64bF2\r\n" \
"2JDBYbRmUjaiJlOqq3ReB30Zgtsq2B+g2Q0cLUlm91slc0boC4pPaQy1AJDh2oIQ\r\n" \
"Zn2uVCuLZXmRoeJhw81ASQjuaAzxi4bSRr/QuKoRAx5/VqgaHkQYDw+Fi9qLRF7i\r\n" \
"GMZiL8dmjfpd2H3zJ4kpAcWQDj8n8TDISg7v1t7HxydrxwU9esQCPJodPg/oNJhb\r\n" \
"y3NLUpbYEaIsgIhpOVrTD7DeWS8Rx/fqEgEwlwIDAQABAoIBAQCXR0S8EIHFGORZ\r\n" \
"++AtOg6eENxD+xVs0f1IeGz57Tjo3QnXX7VBZNdj+p1ECvhCE/G7XnkgU5hLZX+G\r\n" \
"Z0jkz/tqJOI0vRSdLBbipHnWouyBQ4e/A1yIJdlBtqXxJ1KE/ituHRbNc4j4kL8Z\r\n" \
"/r6pvwnTI0PSx2Eqs048YdS92LT6qAv4flbNDxMn2uY7s4ycS4Q8w1JXnCeaAnYm\r\n" \
"WYI5wxO+bvRELR2Mcz5DmVnL8jRyml6l6582bSv5oufReFIbyPZbQWlXgYnpu6He\r\n" \
"GTc7E1zKYQGG/9+DQUl/1vQuCPqQwny0tQoX2w5tdYpdMdVm+zkLtbajzdTviJJa\r\n" \
"TWzL6lt5AoGBAN86+SVeJDcmQJcv4Eq6UhtRr4QGMiQMz0Sod6ettYxYzMgxtw28\r\n" \
"CIrgpozCc+UaZJLo7UxvC6an85r1b2nKPCLQFaggJ0H4Q0J/sZOhBIXaoBzWxveK\r\n" \
"nupceKdVxGsFi8CDy86DBfiyFivfBj+47BbaQzPBj7C4rK7UlLjab2rDAoGBAN2u\r\n" \
"AM2gchoFiu4v1HFL8D7lweEpi6ZnMJjnEu/dEgGQJFjwdpLnPbsj4c75odQ4Gz8g\r\n" \
"sw9lao9VVzbusoRE/JGI4aTdO0pATXyG7eG1Qu+5Yc1YGXcCrliA2xM9xx+d7f+s\r\n" \
"mPzN+WIEg5GJDYZDjAzHG5BNvi/FfM1C9dOtjv2dAoGAF0t5KmwbjWHBhcVqO4Ic\r\n" \
"BVvN3BIlc1ue2YRXEDlxY5b0r8N4XceMgKmW18OHApZxfl8uPDauWZLXOgl4uepv\r\n" \
"whZC3EuWrSyyICNhLY21Ah7hbIEBPF3L3ZsOwC+UErL+dXWLdB56Jgy3gZaBeW7b\r\n" \
"vDrEnocJbqCm7IukhXHOBK8CgYEAwqdHB0hqyNSzIOGY7v9abzB6pUdA3BZiQvEs\r\n" \
"3LjHVd4HPJ2x0N8CgrBIWOE0q8+0hSMmeE96WW/7jD3fPWwCR5zlXknxBQsfv0gP\r\n" \
"3BC5PR0Qdypz+d+9zfMf625kyit4T/hzwhDveZUzHnk1Cf+IG7Q+TOEnLnWAWBED\r\n" \
"ISOWmrUCgYAFEmRxgwAc/u+D6t0syCwAYh6POtscq9Y0i9GyWk89NzgC4NdwwbBH\r\n" \
"4AgahOxIxXx2gxJnq3yfkJfIjwf0s2DyP0kY2y6Ua1OeomPeY9mrIS4tCuDQ6LrE\r\n" \
"TB6l9VGoxJL4fyHnZb8L5gGvnB1bbD8cL6YPaDiOhcRseC9vBiEuVg==\r\n"         \
"-----END RSA PRIVATE KEY-----\r\n";

const size_t mbedtls_test_srv_crt_rsa_len = sizeof( mbedtls_test_srv_crt_rsa );
const size_t mbedtls_test_srv_key_rsa_len = sizeof( mbedtls_test_srv_key_rsa );
#else
#define TEST_CA_CRT_RSA
#endif /* MBEDTLS_RSA_C */

#if defined(MBEDTLS_PEM_PARSE_C)
/* Concatenation of all available CA certificates */
const char mbedtls_test_cas_pem[] = TEST_CA_CRT_RSA TEST_CA_CRT_EC;
const size_t mbedtls_test_cas_pem_len = sizeof( mbedtls_test_cas_pem );
#endif

#if defined(MBEDTLS_RSA_C)

const char *mbedtls_test_srv_crt = mbedtls_test_srv_crt_rsa;
const char *mbedtls_test_srv_key = mbedtls_test_srv_key_rsa;
const size_t mbedtls_test_srv_crt_len = sizeof( mbedtls_test_srv_crt_rsa );
const size_t mbedtls_test_srv_key_len = sizeof( mbedtls_test_srv_key_rsa );
#else /* ! MBEDTLS_RSA_C, so MBEDTLS_ECDSA_C */

const char *mbedtls_test_srv_crt = mbedtls_test_srv_crt_ec;
const char *mbedtls_test_srv_key = mbedtls_test_srv_key_ec;
const size_t mbedtls_test_srv_crt_len = sizeof( mbedtls_test_srv_crt_ec );
const size_t mbedtls_test_srv_key_len = sizeof( mbedtls_test_srv_key_ec );
#endif /* MBEDTLS_RSA_C */

#else
#include "mbedtls/config.h"
#include "mbedtls/certs.h"
#endif
#include "mbedtls/platform.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"


#define SERVER_PORT   "443"
#define STACKSIZE     2048

extern struct netif xnetif[NET_IF_NUM];

#if !(!defined(MBEDTLS_BIGNUM_C) || !defined(MBEDTLS_CERTS_C) || \
	!defined(MBEDTLS_SSL_TLS_C) || !defined(MBEDTLS_SSL_SRV_C) || \
	!defined(MBEDTLS_RSA_C) || !defined(MBEDTLS_NET_C) || \
	!defined(MBEDTLS_PEM_PARSE_C) || !defined(MBEDTLS_X509_CRT_PARSE_C))
static int my_random(void *p_rng, unsigned char *output, size_t output_len)
{
	/* To avoid gcc warnings */
	( void ) p_rng;
	
	rtw_get_random_bytes(output, output_len);
	return 0;
}

static void* my_calloc(size_t nelements, size_t elementSize)
{
	size_t size;
	void *ptr = NULL;

	size = nelements * elementSize;
	ptr = pvPortMalloc(size);

	if(ptr)
		memset(ptr, 0, size);

	return ptr;
}
#endif

static void example_ssl_server_thread(void *param)
{
	/* To avoid gcc warnings */
	( void ) param;
	
#if !defined(MBEDTLS_BIGNUM_C) || !defined(MBEDTLS_CERTS_C) || \
	!defined(MBEDTLS_SSL_TLS_C) || !defined(MBEDTLS_SSL_SRV_C) || \
	!defined(MBEDTLS_RSA_C) || !defined(MBEDTLS_NET_C) || \
	!defined(MBEDTLS_PEM_PARSE_C) || !defined(MBEDTLS_X509_CRT_PARSE_C)

	printf("MBEDTLS_BIGNUM_C and/or MBEDTLS_CERTS_C and/or "
		"MBEDTLS_SSL_TLS_C and/or MBEDTLS_SSL_SRV_C and/or "
		"MBEDTLS_RSA_C and/or MBEDTLS_NET_C and/or "
		"MBEDTLS_PEM_PARSE_C and/or MBEDTLS_X509_CRT_PARSE_C not defined.\n");

#else
	int ret;
	unsigned char buf[512];
	uint8_t *ip;
	mbedtls_net_context server_fd, client_fd;
	mbedtls_ssl_context ssl;
	mbedtls_ssl_config conf;
	mbedtls_x509_crt server_x509;
	mbedtls_pk_context server_pk;

	char *response = "<HTML><BODY>TLS OK</BODY></HTML>";

	// Delay to wait for IP by DHCP
	vTaskDelay(10000);
	printf("\nExample: SSL server\n");

	mbedtls_platform_set_calloc_free(my_calloc, vPortFree);

	/*
	 * 1. Prepare the certificate and key
	 */
	printf("\n\r  . Preparing the certificate and key...");

	mbedtls_x509_crt_init(&server_x509);
	mbedtls_pk_init(&server_pk);

	if((ret = mbedtls_x509_crt_parse(&server_x509, (const unsigned char *) mbedtls_test_srv_crt, mbedtls_test_srv_crt_len)) != 0) {
		printf(" failed\n  ! mbedtls_x509_crt_parse returned %d\n\n", ret);
		goto exit;
	}
	printf("mbedtls_x509_crt_parse Line: %d", __LINE__);

	if((ret = mbedtls_x509_crt_parse(&server_x509, (const unsigned char *) mbedtls_test_cas_pem, mbedtls_test_cas_pem_len)) != 0) {
		printf(" failed\n  ! mbedtls_x509_crt_parse returned %d\n\n", ret);
		goto exit;
	}
#if defined(MBEDTLS_VERSION_NUMBER) && (MBEDTLS_VERSION_NUMBER>=0x03010000)
	if((ret = mbedtls_pk_parse_key(&server_pk, (const unsigned char *) mbedtls_test_srv_key, mbedtls_test_srv_key_len, NULL, 0, rtw_get_random_bytes_f_rng, (void*)1 )) != 0) {
#else
	if((ret = mbedtls_pk_parse_key(&server_pk, (const unsigned char *) mbedtls_test_srv_key, mbedtls_test_srv_key_len, NULL, 0)) != 0) {
#endif
		printf(" failed\n  ! mbedtls_pk_parse_key returned %d\n\n", ret);
		goto exit;
	}

	printf(" ok\n");

	/*
	 * 2. Start the connection
	 */
	ip = LwIP_GetIP(&xnetif[0]);
	printf("\n\r  . Starting tcp server /%d.%d.%d.%d/%s...", ip[0], ip[1], ip[2], ip[3], SERVER_PORT);
	mbedtls_net_init(&server_fd);

	if((ret = mbedtls_net_bind(&server_fd, NULL, SERVER_PORT, MBEDTLS_NET_PROTO_TCP)) != 0) {
		printf(" failed\n  ! mbedtls_net_bind returned %d\n\n", ret);
		goto exit;
	}

	printf(" ok\n");

	/*
	 * 3. Waiting for client to connect
	 */
	printf("\n\r  . Waiting for client to connect...\n\r");
	mbedtls_net_init(&client_fd);

	while((ret = mbedtls_net_accept(&server_fd, &client_fd, NULL, 0, NULL)) == 0) {
		printf("\n\r  . A client is connecting\n\r");
		/*
		 * 4. Setup stuff
		 */
		printf("\n\r  . Setting up the SSL/TLS structure...");
		mbedtls_ssl_init(&ssl);
		mbedtls_ssl_config_init(&conf);

		if((ret = mbedtls_ssl_config_defaults(&conf,
				MBEDTLS_SSL_IS_SERVER,
				MBEDTLS_SSL_TRANSPORT_STREAM,
				MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {

			printf(" failed\n  ! mbedtls_ssl_config_defaults returned %d\n\n", ret);
			goto close_client;
		}

		mbedtls_ssl_conf_ca_chain(&conf, server_x509.next, NULL);
		mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
		mbedtls_ssl_conf_rng(&conf, my_random, NULL);

		if((ret = mbedtls_ssl_conf_own_cert(&conf, &server_x509, &server_pk)) != 0) {
			printf(" failed\n  ! mbedtls_ssl_conf_own_cert returned %d\n\n", ret);
			goto close_client;
		}

		if((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0) {
			printf(" failed\n  ! mbedtls_ssl_setup returned %d\n\n", ret);
			goto close_client;
		}

		mbedtls_ssl_set_bio(&ssl, &client_fd, mbedtls_net_send, mbedtls_net_recv, NULL);
		printf(" ok\n");

		/*
		 * 5. Handshake
		 */
		printf("\n\r  . Performing the SSL/TLS handshake...");

		if((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
			printf(" failed\n  ! mbedtls_ssl_handshake returned %d\n\n", ret);
			goto close_client;
		}
		printf(" ok\n");
		printf("\n\r  . Use ciphersuite %s\n", mbedtls_ssl_get_ciphersuite(&ssl));

		/*
		 * 6. Read the request from client
		 */
		printf("\n\r  > Read request from client:");

		memset(buf, 0, sizeof(buf));
		if((ret = mbedtls_ssl_read(&ssl, buf, sizeof(buf))) <= 0) {
			if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
				printf(" failed\n\r  ! mbedtls_ssl_read returned %d\n", ret);
				goto close_client;
			}
		}
		printf(" %d bytes read\n\r\n\r%s\n", ret, (char *) buf);

		/*
		 * 7. Response the request
		 */
		printf("\n\r  > Response to client:");

		sprintf((char *)buf, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n%s", strlen(response), response);
		if((ret = mbedtls_ssl_write(&ssl, buf, strlen((const char *)buf))) <= 0) {
			if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
				printf(" failed\n\r  ! mbedtls_ssl_write returned %d\n", ret);
				goto close_client;
			}
		}
		printf(" %d bytes written\n\r\n\r%s\n", ret, (char *)buf);

close_client:
		mbedtls_ssl_close_notify(&ssl);
		mbedtls_net_free(&client_fd);
		mbedtls_ssl_free(&ssl);
		mbedtls_ssl_config_free(&conf);
	}

	mbedtls_net_free(&server_fd);

exit:
	mbedtls_x509_crt_free(&server_x509);
	mbedtls_pk_free(&server_pk);
#endif
	vTaskDelete(NULL);
}

void example_ssl_server(void)
{
	if(xTaskCreate(example_ssl_server_thread, "example_ssl_server_thread", STACKSIZE, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS)
		printf("\n\r%s xTaskCreate example_ssl_server_thread failed", __FUNCTION__);
}

#endif /* CONFIG_USE_POLARSSL */