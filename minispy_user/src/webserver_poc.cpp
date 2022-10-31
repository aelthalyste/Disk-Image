#define NO_SSL
#include "civetweb.h"


#include <assert.h>
#include <Windows.h>
#include <string.h>
#include <stdio.h>

struct user_data {
	int a;
	int b;
};

constexpr const char *HTTP_POST = "POST";
constexpr const char* HTTP_GET  = "GET";

int hello_handler(mg_connection* conn, void* cbdata) {

	const mg_request_info *inf = mg_get_request_info(conn);
	fprintf(stdout, "uri : %s\n", inf->request_uri);
	if (strcmp(inf->request_method, "GET") == 0) {
		mg_send_http_error(conn, 403, "some error about stuff");
		char msg[] = "this is an http ok response!";
		
		//mg_send_http_ok(conn, "application/octet-stream", 100);
		//mg_printf(conn, "myheader2 : somevalu66e\r\n");
		//mg_write(conn, msg, sizeof(msg));

		char dst[256] = {};
		char query_to_search[] = "some_squery";
		int ret = 0;
		if (inf->query_string) {
			ret = mg_get_var(inf->query_string, strlen(inf->query_string), query_to_search, dst, sizeof(dst));
		}
		fprintf(stdout, "searched for query : %s, found value : %s, ret : %d\n", query_to_search, dst, ret);
		// mg_printf(conn,
		// 	"HTTP/1.1 405 Method Not Allowed\r\nConnection: close\r\n");
		// mg_printf(conn, "Content-Type: text/plain\r\n\r\n");
		// mg_printf(conn, "HTTP/1.1 405 Method Not Allowed\r\n");
		// mg_printf(conn, "Content-Type: text/plain\r\n\r\n");
		
		// mg_response_header_add(conn, "CustomHeader2", "text/something22?", -1);
	}
	
	
	return 200;
}

#define LOG_FUNCTION fprintf(stdout, "%s\n", __FUNCTION__);

int hello_subpage_handler(mg_connection* conn, void* cbdata) {
	LOG_FUNCTION;
	const mg_request_info* inf = mg_get_request_info(conn);
	fprintf(stdout, "uri : %s\n", inf->request_uri);
	return 200;
}



int begin_request(struct mg_connection*) {
	LOG_FUNCTION;
	return 0;
}

void end_request(const struct mg_connection*, int reply_status_code) {
	LOG_FUNCTION;
	return;
}

int init_connection(const struct mg_connection* conn, void** conn_data) {
	LOG_FUNCTION;
	return 0;
}

void connection_close(const struct mg_connection* ctx) {
	LOG_FUNCTION;
}

void connection_closed(const struct mg_connection* ctx) {
	auto d = (user_data*)mg_get_user_context_data(ctx);
	LOG_FUNCTION;
}

void* init_thread(const struct mg_context* ctx, int thread_type) {
	// mg_set_user_connection_data
	// int index = TlsAlloc();	
	// TlsSetValue(index, );
	 
	LOG_FUNCTION;
	return NULL;
}

void exit_thread(const struct mg_context* ctx, int thread_type, void* thread_pointer) {
	LOG_FUNCTION;
}

int main() {
	
	user_data data;
	data.a = 25;
	data.b = 62;

	mg_context* ctx = NULL;

	mg_init_library(0);
	
	mg_callbacks callbacks = {};
	callbacks.begin_request = begin_request;
	callbacks.end_request = end_request;
	callbacks.init_connection = init_connection;
	callbacks.connection_close = connection_close;
	callbacks.connection_closed = connection_closed;
	callbacks.init_thread = init_thread;
	callbacks.exit_thread = exit_thread;

	mg_option a;
	
	const char* options[] = {
	  "listening_ports", "5003",
	  NULL
	};

	ctx = mg_start(&callbacks, &data, options);
	assert(ctx);

	mg_set_request_handler(ctx, "/hello/subpage", hello_subpage_handler, NULL);
	mg_set_request_handler(ctx, "/hello", hello_handler, NULL);

	
	while (1) {
		Sleep(50);
	}

	mg_stop(ctx);
	mg_exit_library();

	return 0;
}