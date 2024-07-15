#include <FreeRTOS.h>
#include <task.h>
#include <platform_stdlib.h>
#include <httpd/httpd.h>

#define USE_HTTPS    0

#if USE_HTTPS
// use test_srv_crt, test_srv_key, test_ca_list in PolarSSL certs.c
#if (HTTPD_USE_TLS == HTTPD_TLS_POLARSSL)
#include <polarssl/certs.h>
#elif (HTTPD_USE_TLS == HTTPD_TLS_MBEDTLS)
#if defined(MBEDTLS_VERSION_NUMBER) && (MBEDTLS_VERSION_NUMBER>=0x03010000)
// mbedtls-3.x removed cert modules, so please embed your own test certificates in your test code
static const unsigned char *mbedtls_test_srv_crt =
"-----BEGIN CERTIFICATE-----\r\n"                                      \
"MIICHzCCAaWgAwIBAgIBCTAKBggqhkjOPQQDAjA+MQswCQYDVQQGEwJOTDERMA8G\r\n" \
"A1UEChMIUG9sYXJTU0wxHDAaBgNVBAMTE1BvbGFyc3NsIFRlc3QgRUMgQ0EwHhcN\r\n" \
"MTMwOTI0MTU1MjA0WhcNMjMwOTIyMTU1MjA0WjA0MQswCQYDVQQGEwJOTDERMA8G\r\n" \
"A1UEChMIUG9sYXJTU0wxEjAQBgNVBAMTCWxvY2FsaG9zdDBZMBMGByqGSM49AgEG\r\n" \
"CCqGSM49AwEHA0IABDfMVtl2CR5acj7HWS3/IG7ufPkGkXTQrRS192giWWKSTuUA\r\n" \
"2CMR/+ov0jRdXRa9iojCa3cNVc2KKg76Aci07f+jgZ0wgZowCQYDVR0TBAIwADAd\r\n" \
"BgNVHQ4EFgQUUGGlj9QH2deCAQzlZX+MY0anE74wbgYDVR0jBGcwZYAUnW0gJEkB\r\n" \
"PyvLeLUZvH4kydv7NnyhQqRAMD4xCzAJBgNVBAYTAk5MMREwDwYDVQQKEwhQb2xh\r\n" \
"clNTTDEcMBoGA1UEAxMTUG9sYXJzc2wgVGVzdCBFQyBDQYIJAMFD4n5iQ8zoMAoG\r\n" \
"CCqGSM49BAMCA2gAMGUCMQCaLFzXptui5WQN8LlO3ddh1hMxx6tzgLvT03MTVK2S\r\n" \
"C12r0Lz3ri/moSEpNZWqPjkCMCE2f53GXcYLqyfyJR078c/xNSUU5+Xxl7VZ414V\r\n" \
"fGa5kHvHARBPc8YAIVIqDvHH1Q==\r\n"                                     \
"-----END CERTIFICATE-----\r\n";

static const unsigned char *mbedtls_test_srv_key =
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

static const unsigned char *mbedtls_test_ca_crt =
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
"-----END CERTIFICATE-----\r\n";

#else
#include <mbedtls/certs.h>
#endif
#endif
#endif

void homepage_cb(struct httpd_conn *conn)
{
	char *user_agent = NULL;

	// test log to show brief header parsing
	httpd_conn_dump_header(conn);

	// test log to show extra User-Agent header field
	if(httpd_request_get_header_field(conn, "User-Agent", &user_agent) != -1) {
		printf("\nUser-Agent=[%s]\n", user_agent);
		httpd_free(user_agent);
	}

	// GET homepage
	if(httpd_request_is_method(conn, "GET")) {
		char *body = \
"<HTML><BODY>" \
"It Works<BR>" \
"<BR>" \
"Can test GET by <A href=\"/test_get?test1=one%20%26%202&test2=three%3D3\">/test_get?test1=one%20%26%202&test2=three%3D3</A><BR>" \
"Can test POST by UI in <A href=\"/test_post.htm\">/test_post.htm</A><BR>" \
"</BODY></HTML>";

		// write HTTP response
		httpd_response_write_header_start(conn, "200 OK", "text/html", strlen(body));
		httpd_response_write_header(conn, "Connection", "close");
		httpd_response_write_header_finish(conn);
		httpd_response_write_data(conn, (uint8_t*)body, strlen(body));
	}
	else {
		// HTTP/1.1 405 Method Not Allowed
		httpd_response_method_not_allowed(conn, NULL);
	}

	httpd_conn_close(conn);
}

void test_get_cb(struct httpd_conn *conn)
{
	// GET /test_post
	if(httpd_request_is_method(conn, "GET")) {
		char *test1 = NULL, *test2 = NULL;

		// get 'test1' and 'test2' in query string
		if((httpd_request_get_query_key(conn, "test1", &test1) != -1) &&
		   (httpd_request_get_query_key(conn, "test2", &test2) != -1)) {

			// write HTTP response
			httpd_response_write_header_start(conn, "200 OK", "text/plain", 0);
			httpd_response_write_header(conn, "Connection", "close");
			httpd_response_write_header_finish(conn);
			httpd_response_write_data(conn, (uint8_t*)"\r\nGET query string", strlen("\r\nGET query string"));
			httpd_response_write_data(conn, (uint8_t*)"\r\ntest1: ", strlen("\r\ntest1: "));
			httpd_response_write_data(conn, (uint8_t*)test1, strlen(test1));
			httpd_response_write_data(conn, (uint8_t*)"\r\ntest2: ", strlen("\r\ntest2: "));
			httpd_response_write_data(conn, (uint8_t*)test2, strlen(test2));
		}
		else {
			// HTTP/1.1 400 Bad Request
			httpd_response_bad_request(conn, "Bad Request - test1 or test2 not in query string");
		}

		if(test1)
			httpd_free(test1);

		if(test2)
			httpd_free(test2);
	}
	else {
		// HTTP/1.1 405 Method Not Allowed
		httpd_response_method_not_allowed(conn, NULL);
	}

	httpd_conn_close(conn);
}

void test_post_htm_cb(struct httpd_conn *conn)
{
	// GET /test_post.htm
	if(httpd_request_is_method(conn, "GET")) {
		char *body = \
"<HTML><BODY>" \
"<FORM action=\"/test_post\" method=\"post\">" \
"Text1: <INPUT type=\"text\" name=\"text1\" size=\"50\" maxlength=\"50\"><BR>" \
"Text2: <INPUT type=\"text\" name=\"text2\" size=\"50\" maxlength=\"50\"><BR>" \
"<INPUT type=\"submit\" value=\"POST\"><BR>" \
"</FORM>" \
"</BODY></HTML>";

		// write HTTP response
		httpd_response_write_header_start(conn, "200 OK", "text/html", strlen(body));
		httpd_response_write_header(conn, "Connection", "close");
		httpd_response_write_header_finish(conn);
		httpd_response_write_data(conn, (uint8_t*)body, strlen(body));
	}
	else {
		// HTTP/1.1 405 Method Not Allowed
		httpd_response_method_not_allowed(conn, NULL);
	}

	httpd_conn_close(conn);
}

void test_post_cb(struct httpd_conn *conn)
{
	// POST /test_post
	if(httpd_request_is_method(conn, "POST")) {
		size_t read_size;
		uint8_t buf[50];
		size_t content_len = conn->request.content_len;
		uint8_t *body = (uint8_t *) malloc(content_len + 1);

		if(body) {
			// read HTTP body
			memset(body, 0, content_len + 1);
			read_size = httpd_request_read_data(conn, body, content_len);

			// write HTTP response
			httpd_response_write_header_start(conn, "200 OK", "text/plain", 0);
			httpd_response_write_header(conn, "Connection", "close");
			httpd_response_write_header_finish(conn);
			memset(buf, 0, sizeof(buf));
			sprintf((char*)buf, "%d bytes from POST: ", read_size);
			httpd_response_write_data(conn, buf, strlen((char const*)buf));
			httpd_response_write_data(conn, body, strlen((char const*)body));
			free(body);
		}
		else {
			// HTTP/1.1 500 Internal Server Error
			httpd_response_internal_server_error(conn, NULL);
		}
	}
	else {
		// HTTP/1.1 405 Method Not Allowed
		httpd_response_method_not_allowed(conn, NULL);
	}

	httpd_conn_close(conn);
}

static void example_httpd_thread(void *param)
{
	/* To avoid gcc warnings */
	( void ) param;
#if USE_HTTPS
#if (HTTPD_USE_TLS == HTTPD_TLS_POLARSSL)
	if(httpd_setup_cert(test_srv_crt, test_srv_key, test_ca_crt) != 0) {
#elif (HTTPD_USE_TLS == HTTPD_TLS_MBEDTLS)
	if(httpd_setup_cert(mbedtls_test_srv_crt, mbedtls_test_srv_key, mbedtls_test_ca_crt) != 0) {
#endif
		printf("\nERROR: httpd_setup_cert\n");
		goto exit;
	}
#endif
	httpd_reg_page_callback("/", homepage_cb);
	httpd_reg_page_callback("/index.htm", homepage_cb);
	httpd_reg_page_callback("/test_get", test_get_cb);
	httpd_reg_page_callback("/test_post.htm", test_post_htm_cb);
	httpd_reg_page_callback("/test_post", test_post_cb);
#if USE_HTTPS
	if(httpd_start(443, 5, 4096, HTTPD_THREAD_SINGLE, HTTPD_SECURE_TLS) != 0) {
#else
	if(httpd_start(80, 5, 4096, HTTPD_THREAD_SINGLE, HTTPD_SECURE_NONE) != 0) {
#endif
		printf("ERROR: httpd_start");
		httpd_clear_page_callbacks();
	}
#if USE_HTTPS
exit:
#endif
	vTaskDelete(NULL);
}

void example_httpd(void)
{
	if(xTaskCreate(example_httpd_thread, ((const char*)"example_httpd_thread"), 2048, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS)
		printf("\n\r%s xTaskCreate(example_httpd_thread) failed", __FUNCTION__);
}
