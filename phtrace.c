#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"

#include "zend_smart_str.h"
#include <netdb.h>
#include <uuid/uuid.h>

#include "php_phtrace.h"

#define PHT_EVENT_REQUEST_BEGIN  1
#define PHT_EVENT_REQUEST_END    2
#define PHT_EVENT_FUNCTION_BEGIN 3
#define PHT_EVENT_FUNCTION_END   4

typedef unsigned char pht_event_t;

static void phtrace_execute_ex(zend_execute_data *);
static void (*_zend_execute_ex) (zend_execute_data *);

static inline uint64_t rdtscp();

static void connection_close();
static void connection_establish();

static void buffer_allocate();
static void buffer_free();
static void buffer_flush();
static void buffer_print_last_bytes(size_t);

static void emit_event_request_begin();
static void emit_event_request_end();
static void emit_event_function_call_begin(uint32_t, zend_execute_data *);
static void emit_event_function_call_end(uint32_t);

ZEND_DECLARE_MODULE_GLOBALS(phtrace)

struct {
	size_t size;
	size_t used;
	unsigned char *data;
} buffer;

static int le_phtrace;
static int server_socket = 0;
static uint32_t function_call_n = 0;

PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("phtrace.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_phtrace_globals, phtrace_globals)
    STD_PHP_INI_ENTRY("phtrace.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_phtrace_globals, phtrace_globals)
    STD_PHP_INI_ENTRY("phtrace.server_port", "19229", PHP_INI_ALL, OnUpdateString, server_port, zend_phtrace_globals, phtrace_globals)
PHP_INI_END()

PHP_FUNCTION(confirm_phtrace_compiled)
{
    char *arg = NULL;
    size_t arg_len, len;
    zend_string *strg;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &arg, &arg_len) == FAILURE) {
        return;
    }

    strg = strpprintf(0, "Congratulations! You have successfully modified ext/%.78s/config.m4. Module %.78s is now compiled into PHP.", "phtrace", arg);

    RETURN_STR(strg);
}

PHP_MINIT_FUNCTION(phtrace)
{
    REGISTER_INI_ENTRIES();
    connection_establish();
    buffer_allocate();
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(phtrace)
{
    connection_close();
    buffer_free();
    UNREGISTER_INI_ENTRIES();
    return SUCCESS;
}

PHP_RINIT_FUNCTION(phtrace)
{
#if defined(COMPILE_DL_PHTRACE) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif

    function_call_n = 0;

    _zend_execute_ex = zend_execute_ex;
    zend_execute_ex = phtrace_execute_ex;

    emit_event_request_begin();
    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(phtrace)
{
	zend_execute_ex = _zend_execute_ex;

	emit_event_request_end();
	buffer_flush();
    return SUCCESS;
}

PHP_MINFO_FUNCTION(phtrace)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "phtrace support", "enabled");
    php_info_print_table_end();

    DISPLAY_INI_ENTRIES();
}

const zend_function_entry phtrace_functions[] = {
    PHP_FE(confirm_phtrace_compiled,    NULL)
    PHP_FE_END	/* Must be the last line in phtrace_functions[] */
};

zend_module_entry phtrace_module_entry = {
    STANDARD_MODULE_HEADER,
    "phtrace",
    phtrace_functions,
    PHP_MINIT(phtrace),
    PHP_MSHUTDOWN(phtrace),
    PHP_RINIT(phtrace),
    PHP_RSHUTDOWN(phtrace),
    PHP_MINFO(phtrace),
    PHP_PHTRACE_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_PHTRACE
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(phtrace)
#endif


static void phtrace_execute_ex(zend_execute_data *execute_data) {
	function_call_n++;
	uint32_t current_function_call_number = function_call_n;
	emit_event_function_call_begin(current_function_call_number, execute_data);
	_zend_execute_ex(execute_data);
	emit_event_function_call_end(current_function_call_number);
}

static inline uint64_t rdtscp() {
    uint64_t rax,rdx, aux;
    __asm__ volatile ( "rdtscp\n" : "=a" (rax), "=d" (rdx), "=c" (aux) : : );
    return (rdx << 32) + rax;
}

static void connection_establish() {
    const char *hostname = "localhost";
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_ADDRCONFIG;

    struct addrinfo* res = 0;
    int err = getaddrinfo(hostname, PHTRACE_G(server_port), &hints, &res);
    if (err != 0) {
        printf("failed to resolve remote socket address (err=%d)\n", err);
        return;
    }

    server_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (server_socket == -1) {
        printf("%s\n", strerror(errno));
        return;
    }

    if (connect(server_socket, res->ai_addr, res->ai_addrlen) == -1) {
        printf("%s\n", strerror(errno));
        return;
    }

    freeaddrinfo(res);
}

static void connection_close() {
    if (server_socket) {
        close(server_socket);
        server_socket = 0;
    }
}

static void buffer_allocate() {
//	size_t size = 1024 * 1024 * 5;
	size_t size = 16384;
	buffer.data = (unsigned char *) malloc(size);
	if (!buffer.data) {
		printf("failed to allocate memory for the data buffer\n");
	}

	buffer.size = size;
	buffer.used = 0;
}

static void buffer_free() {
	if (buffer.data) {
		free(buffer.data);
		buffer.data = NULL;
	}

	buffer.size = 0;
	buffer.used = 0;
}

static void buffer_flush() {
	uint64_t used = buffer.used;
	write(server_socket, &used, sizeof(uint64_t));

	ssize_t written = write(server_socket, buffer.data, buffer.used);
	printf("%ld of %lu bytes were written to the network\n", written, buffer.used);
	if (written != buffer.used) {
		printf("could not write buffer to the socket partially or completely\n");
		printf("Error opening file: %s\n", strerror( errno ));
	}
	buffer.used = 0;
}

static void buffer_print_last_bytes(size_t n) {
	for (int i = buffer.used - n; i < buffer.used; i++) {
		printf("%02hhx ", buffer.data[i]);
	}
}

static void emit_event_request_begin() {
	size_t payloadLength = sizeof(uint64_t) + sizeof(uuid_t);
	size_t eventLength = payloadLength + sizeof(pht_event_t) + sizeof(uint32_t);

	if (buffer.used + eventLength >= buffer.size) {
		buffer_flush();
	}

	// write event type
	*(buffer.data + buffer.used) = PHT_EVENT_REQUEST_BEGIN;
	buffer.used += sizeof(pht_event_t);
//	buffer_print_last_bytes(sizeof(pht_event_t));

	// write payload size
	uint32_t *payloadLengthVar = (uint32_t *) (buffer.data + buffer.used);
	*payloadLengthVar = payloadLength;
	buffer.used += sizeof(uint32_t);
//	buffer_print_last_bytes(sizeof(uint32_t));

	// write tsc
	uint64_t *rdtscpvar = (uint64_t *) (buffer.data + buffer.used);
	*rdtscpvar = rdtscp();
	buffer.used += sizeof(uint64_t);
//	buffer_print_last_bytes(sizeof(uint64_t));

	// write request uuid
	uuid_generate(buffer.data + buffer.used);
	buffer.used += sizeof(uuid_t);
//	buffer_print_last_bytes(sizeof(uuid_t));

//	puts("");
}

static void emit_event_request_end() {
	size_t payloadLength = sizeof(uint64_t);
	size_t eventLength = payloadLength + sizeof(pht_event_t) + sizeof(uint32_t);

	if (buffer.used + eventLength >= buffer.size) {
		buffer_flush();
	}

	// write event type
	*(buffer.data + buffer.used) = PHT_EVENT_REQUEST_END;
	buffer.used += sizeof(pht_event_t);
//	buffer_print_last_bytes(sizeof(pht_event_t));

	// write payload size
	uint32_t *payloadLengthVar = (uint32_t *) (buffer.data + buffer.used);
	*payloadLengthVar = payloadLength;
	buffer.used += sizeof(uint32_t);
//	buffer_print_last_bytes(sizeof(uint32_t));

	// write tsc
	uint64_t *rdtscpvar = (uint64_t *) (buffer.data + buffer.used);
	*rdtscpvar = rdtscp();
	buffer.used += sizeof(uint64_t);
//	buffer_print_last_bytes(sizeof(uint64_t));

//	puts("");
}

static void emit_event_function_call_begin(uint32_t function_call_n, zend_execute_data *execute_data) {
	size_t nameLength = 0;
	if (EX(func)->common.function_name) {
		nameLength += EX(func)->common.function_name->len;
		if (EX(func)->common.scope) {
			nameLength += EX(func)->common.scope->name->len;
			nameLength += 2;
		}
	} else {
		nameLength += EX(func)->op_array.filename->len;
	}
	nameLength += 1;

	size_t payloadLength = sizeof(uint64_t) + sizeof(uint32_t) + nameLength;
	size_t eventLength = payloadLength + sizeof(pht_event_t) + sizeof(uint32_t);

	if (buffer.used + eventLength >= buffer.size) {
		buffer_flush();
	}

	// write event type
	*(buffer.data + buffer.used) = PHT_EVENT_FUNCTION_BEGIN;
	buffer.used += sizeof(pht_event_t);
//	buffer_print_last_bytes(sizeof(pht_event_t));

	// write payload size
	uint32_t *payloadLengthVar = (uint32_t *) (buffer.data + buffer.used);
	*payloadLengthVar = payloadLength;
	buffer.used += sizeof(uint32_t);
//	buffer_print_last_bytes(sizeof(uint32_t));

	// write tsc
	uint64_t *rdtscpvar = (uint64_t *) (buffer.data + buffer.used);
	*rdtscpvar = rdtscp();
	buffer.used += sizeof(uint64_t);
//	buffer_print_last_bytes(sizeof(uint64_t));

	// write function call number
	uint32_t *p_function_call_n = (uint32_t *) (buffer.data + buffer.used);
	*p_function_call_n = function_call_n;
	buffer.used += sizeof(uint32_t);
//	buffer_print_last_bytes(sizeof(uint32_t));

	// write name
	if (EX(func)->common.function_name) {
		if (EX(func)->common.scope) {
			snprintf(
				(char *) buffer.data + buffer.used,
				nameLength,
				EX(This).value.obj ? "%s->%s" : "%s::%s",
				EX(func)->common.scope->name->val,
				EX(func)->common.function_name->val
			);
		} else {
			strncpy((char *) buffer.data + buffer.used, EX(func)->common.function_name->val, nameLength);
		}
	} else {
		strncpy((char *) buffer.data + buffer.used, EX(func)->op_array.filename->val, nameLength);
	}

	buffer.used += nameLength;
//	buffer_print_last_bytes(nameLength);

//	puts("");
}

static void emit_event_function_call_end(uint32_t function_call_n) {
	size_t payloadLength = sizeof(uint64_t) + sizeof(uint32_t);
	size_t eventLength = payloadLength + sizeof(pht_event_t) + sizeof(uint32_t);

	if (buffer.used + eventLength >= buffer.size) {
		buffer_flush();
	}

	// write event type
	*(buffer.data + buffer.used) = PHT_EVENT_FUNCTION_END;
	buffer.used += sizeof(pht_event_t);
//	buffer_print_last_bytes(sizeof(pht_event_t));

	// write payload size
	uint32_t *payloadLengthVar = (uint32_t *) (buffer.data + buffer.used);
	*payloadLengthVar = payloadLength;
	buffer.used += sizeof(uint32_t);
//	buffer_print_last_bytes(sizeof(uint32_t));

	// write tsc
	uint64_t *rdtscpvar = (uint64_t *) (buffer.data + buffer.used);
	*rdtscpvar = rdtscp();
	buffer.used += sizeof(uint64_t);
//	buffer_print_last_bytes(sizeof(uint64_t));

	// write function call number
	uint32_t *p_function_call_n = (uint32_t *) (buffer.data + buffer.used);
	*p_function_call_n = function_call_n;
	buffer.used += sizeof(uint32_t);
//	buffer_print_last_bytes(sizeof(uint32_t));


//	puts("");
}

