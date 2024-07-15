#include <FreeRTOS.h>
#include <task.h>
#include <platform_stdlib.h>
#include <httpd/httpd.h>
#include "lfs.h"
#include "httpd_littlefs/example_httpd_littlefs.h"

#define HTTPD_DEFAULT_CONFIG() {						\
		.port				= 80,						\
		.max_conn			= 10,						\
		.max_uri_handlers	= 8,						\
		.stack_bytes		= 3072,						\
		.thread_mode   		= HTTPD_THREAD_SINGLE,		\
		.secure         	= HTTPD_SECURE_NONE			\
}

#define MAX_PATH_LEN	200

extern httpd_handle_t httpd_handle;

extern lfs_t g_lfs;
extern struct lfs_config g_lfs_cfg;
lfs_file_t lfs_file;

extern void *httpd_malloc(size_t size);

int get_method_handler(struct httpd_conn *conn)
{
	char *user_agent = NULL;
	char *page_body = NULL;
	char *mime_type = MIME_TYPE_JS;
	char page_path[MAX_PATH_LEN] = {0};
	int  page_size;
	int  ret;
	char* index_page = "/index.html";
	int send_len = 2000;
	int i = 0;
	int remain_len;

	//printf("[%s] request.path = %s, request.path_len = %d\n", __func__, conn->request.path, conn->request.path_len);

	/*  Set mime type */
	if(conn->request.path_len >= 3 && strncmp((const char *)conn->request.path + conn->request.path_len - 3, ".js", 3) == 0)
		mime_type = MIME_TYPE_JS;
	else if (conn->request.path_len >= 4 && strncmp((const char *)conn->request.path + conn->request.path_len - 4, ".css", 4) == 0)
		mime_type = MIME_TYPE_CSS;
	else if ((conn->request.path_len >= 4 && strncmp((const char *)conn->request.path + conn->request.path_len - 4, ".jpg", 4) == 0) ||
			(conn->request.path_len >= 5 && strncmp((const char *)conn->request.path + conn->request.path_len - 5, ".jpeg", 5) == 0))
		mime_type = MIME_TYPE_JPG;
	else if (conn->request.path_len >= 4 && strncmp((const char *)conn->request.path + conn->request.path_len - 4, ".gif", 4) == 0)
		mime_type = MIME_TYPE_GIF;
	else if (conn->request.path_len >= 4 && strncmp((const char *)conn->request.path + conn->request.path_len - 4, ".png", 4) == 0)
		mime_type = MIME_TYPE_PNG;
	else if (conn->request.path_len >= 5 && strncmp((const char *)conn->request.path + conn->request.path_len - 5, ".html", 5) == 0)
		mime_type = MIME_TYPE_HTML;
	else
		mime_type = MIME_TYPE_PLAIN;

	strncpy(page_path, (const char *)conn->request.path, conn->request.path_len);

	/* homepage / */
	if(conn->request.path_len == 1)
	{
		memset(page_path, 0, MAX_PATH_LEN);
		memcpy(page_path, index_page, strlen(index_page));
		mime_type = MIME_TYPE_HTML;
	}
	//printf("[%s] page_path = %s\n", __func__, page_path);

	ret = lfs_file_open(&g_lfs, &lfs_file, page_path, LFS_O_RDONLY);
	if (ret < 0)
	{
		printf("%s lfs_file_open fail %d\r\n", page_path, ret);

	#if CONFIG_ENABLE_CAPTIVE_PORTAL	/* If enables captive portal, return homepage. */
		printf("return homepage.\n");

		memset(page_path, 0, MAX_PATH_LEN);
		memcpy(page_path, index_page, strlen(index_page));
		mime_type = MIME_TYPE_HTML;

		ret = lfs_file_open(&g_lfs, &lfs_file, page_path, LFS_O_RDONLY);
		if (ret < 0) {
			printf("%s lfs_file_open fail %d\r\n", page_path, ret);
			httpd_response_not_found(conn, NULL);
			httpd_conn_close(conn);
			return -1;
		}
	#else
		httpd_response_not_found(conn, NULL);
		httpd_conn_close(conn);
		return -1;
	#endif
	}
	page_size = lfs_file_size(&g_lfs, &lfs_file);
	if (page_size < 0)
	{
		printf("%s lfs_file_size fail %d\r\n", page_path, page_size);
		httpd_response_not_found(conn, NULL);
		goto EXIT;
	}
	printf("[%s] file:%s		size:%d\n", __func__, page_path, page_size);

	page_body = (char *)httpd_malloc(page_size);
	if(page_body == NULL)
	{
		printf("[%s] malloc failed\n", __func__);
		goto EXIT;
	}
	memset(page_body, 0, page_size);

	// test log to show brief header parsing
	httpd_conn_dump_header(conn);

	// test log to show extra User-Agent header field
	if(httpd_request_get_header_field(conn, "User-Agent", &user_agent) != -1) {
		printf("\nUser-Agent=[%s]\n", user_agent);
		httpd_free(user_agent);
	}


	ret = lfs_file_read(&g_lfs, &lfs_file, page_body, page_size);
	if(ret < 0)
	{
		printf("[%s] lfs_file_read failed\n", __func__);
		goto EXIT;
	}

	// write HTTP response
	httpd_response_write_header_start(conn, "200 OK", mime_type, page_size);
	httpd_response_write_header(conn, "Connection", "close");
	httpd_response_write_header_finish(conn);

	/* If it is greater than 6K, it may cause a hang */
	remain_len = page_size;
	while(remain_len > send_len)
	{
		httpd_response_write_data(conn, (uint8_t*)page_body + i*send_len, send_len);
		remain_len -= send_len;
		//printf("i=%d, remain_len=%d\n", i, remain_len);
		i++;
	}

	if(remain_len > 0)
		httpd_response_write_data(conn, (uint8_t*)page_body + i*send_len, remain_len);

EXIT:
	lfs_file_close(&g_lfs, &lfs_file);
	if(page_body)
		httpd_free(page_body);
	httpd_conn_close(conn);
	return 0;
}

int post_method_handler(struct httpd_conn *conn)
{
	char* msg = "[post] method is not supported!\n";
	printf("%s", msg);
	httpd_response_method_not_allowed(conn, msg);

	return 0;
}

int put_method_handler(struct httpd_conn *conn)
{
	char* msg = "[put] method is not supported!\n";
	printf("%s", msg);
	httpd_response_method_not_allowed(conn, msg);

	return 0;
}

static const httpd_uri_t put_callback = {
	.method  = HTTP_PUT,
	.handler = put_method_handler,
};

static const httpd_uri_t get_callback = {
	.method  = HTTP_GET,
	.handler = get_method_handler,
};

static const httpd_uri_t post_callback = {
	.method  = HTTP_POST,
	.handler = post_method_handler,
};


static void example_httpd_littlefs_thread(void *param)
{
	/* To avoid gcc warnings */
	( void ) param;

	int ret = 0;

	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	// mount the filesystem
	ret = lfs_mount(&g_lfs, &g_lfs_cfg);
	// reformat if we can't mount the filesystem
	// this should only happen on the first boot
	if (ret) {
		ret = lfs_format(&g_lfs, &g_lfs_cfg);
		if (ret) {
			printf("lfs_format fail %d\r\n", ret);
			goto exit;
		}
		ret = lfs_mount(&g_lfs, &g_lfs_cfg);
		if (ret < 0) {
			printf("lfs_mount fail %d\r\n", ret);
			goto exit;
		}
	}

	printf("lfs_mount ok.\n");

	struct httpd_data *hd = (struct httpd_data *)calloc(1, sizeof(struct httpd_data));
	if (!hd) {
		printf("Failed to allocate memory for HTTP data");
		goto exit; ;
	}
	hd->hd_calls = (httpd_uri_t **)calloc(config.max_uri_handlers, sizeof(httpd_uri_t *));
	if (!hd->hd_calls) {
		printf("Failed to allocate memory for HTTP URI handlers");
		free(hd);
		goto exit;
	}

	hd->config = config;
	httpd_handle = (httpd_handle_t *)hd;
	
	httpd_register_uri_handler(&get_callback);
	httpd_register_uri_handler(&post_callback);
	httpd_register_uri_handler(&put_callback);

	if(httpd_start_with_callback(config.port, config.max_conn, config.stack_bytes, config.thread_mode, config.secure) != 0) {
		printf("ERROR: example_httpd_littlefs_thread\n");
		httpd_unregister_uri_handler(&get_callback);
		httpd_unregister_uri_handler(&post_callback);
		httpd_unregister_uri_handler(&put_callback);
	}
exit:
	vTaskDelete(NULL);
}

void example_httpd_littlefs(void)
{
	if(xTaskCreate(example_httpd_littlefs_thread, ((const char*)"example_httpd_littlefs_thread"), 2048, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS)
		printf("\n\r%s xTaskCreate(example_httpd_littlefs_thread) failed", __FUNCTION__);
}
