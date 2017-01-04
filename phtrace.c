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

typedef struct _EventFunctionCallBegin {
    uint64_t tsc;
} EventFunctionCallBegin;

static void phtrace_execute_ex(zend_execute_data *);
static void (*_zend_execute_ex) (zend_execute_data *);

static inline uint64_t rdtscp();

static void buffer_allocate();
static void buffer_free();
static void buffer_flush();
static void buffer_print_last_bytes(size_t);

static inline EventFunctionCallBegin *alloc_event_function_call_begin();

static inline void emit_event_function_call_begin(zend_execute_data *);
static inline void emit_event_function_call_end();

ZEND_DECLARE_MODULE_GLOBALS(phtrace)


struct {
    size_t size;
    size_t used;
    unsigned char *data;
} buffer;

static int le_phtrace;

FILE *f;

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
    buffer_allocate();
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(phtrace)
{
    buffer_free();
    UNREGISTER_INI_ENTRIES();
    return SUCCESS;
}

PHP_RINIT_FUNCTION(phtrace)
{
#if defined(COMPILE_DL_PHTRACE) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif

    _zend_execute_ex = zend_execute_ex;
    zend_execute_ex = phtrace_execute_ex;

    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(phtrace)
{
    zend_execute_ex = _zend_execute_ex;

    buffer_flush();

    if (f) {
      fclose(f);
      f = NULL;
    }

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
    emit_event_function_call_begin(execute_data);
    _zend_execute_ex(execute_data);
    emit_event_function_call_end();
}

static inline uint64_t rdtscp() {
    uint64_t rax,rdx, aux;
    __asm__ volatile ( "rdtscp\n" : "=a" (rax), "=d" (rdx), "=c" (aux) : : );
    return (rdx << 32) + rax;
}

static void buffer_allocate() {
    size_t size = 1024 * 1024 * 5;
//    size_t size = 16384;
    buffer.data = (unsigned char *) malloc(size);
    if (!buffer.data) {
        // TODO: find a proper way to signal about errors
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
    if (buffer.used == 0) {
        return;
    }
    if (!f) {
        f = fopen("/tmp/phtrace.phtrace", "w");
    }
    fwrite(buffer.data, 1, buffer.used, f);
    fflush(f);

    buffer.used = 0;
}

static void buffer_print_last_bytes(size_t n) {
    for (int i = buffer.used - n; i < buffer.used; i++) {
        printf("%02hhx ", buffer.data[i]);
    }
}

static inline EventFunctionCallBegin *alloc_event_function_call_begin() {
    EventFunctionCallBegin *result;

    int size = 1 + sizeof(EventFunctionCallBegin);
    if (buffer.size - size < buffer.used) {
        buffer_flush();
    }

    buffer.data[buffer.used] = PHT_EVENT_FUNCTION_BEGIN;
    buffer.used++;

    result = (EventFunctionCallBegin *)(buffer.data + buffer.used);
    buffer.used += sizeof(EventFunctionCallBegin);

    return result;
}

static inline void emit_event_function_call_begin(zend_execute_data *execute_data) {
    EventFunctionCallBegin *e = alloc_event_function_call_begin();
    e->tsc = rdtscp();
}

static inline void emit_event_function_call_end() {

}
