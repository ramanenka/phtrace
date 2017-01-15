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
#include "buffer.h"

#define PHT_EVENT_REQUEST_BEGIN 1
#define PHT_EVENT_REQUEST_END   2
#define PHT_EVENT_CALL_BEGIN    3
#define PHT_EVENT_ICALL_BEGIN   6
#define PHT_EVENT_CALL_END      4
#define PHT_EVENT_DATA_STR      5

typedef unsigned char pht_event_t;

struct {
    pht_event_t EventCallBegin;
    pht_event_t EventCallEnd;
    pht_event_t EventICallEnd;
} EventTypes = {
    PHT_EVENT_CALL_BEGIN,
    PHT_EVENT_CALL_END,
    PHT_EVENT_ICALL_BEGIN
};

typedef struct _EventCallBegin {
    uint64_t tsc;
    uint32_t filename;
    uint32_t function_name;
    uint32_t class_name;
    uint32_t line_start;
} EventCallBegin;

typedef struct _EventICallBegin {
    uint64_t tsc;
    uint32_t function_name;
    uint32_t class_name;
} EventICallEnd;

typedef struct _EventCallEnd {
    uint64_t tsc;
} EventCallEnd;

static void phtrace_execute_ex(zend_execute_data *);
static void (*_zend_execute_ex) (zend_execute_data *);
static void phtrace_execute_internal(zend_execute_data *, zval *);
static void (*_zend_execute_internal)(zend_execute_data *, zval *);

static inline uint64_t rdtscp();

static inline uint32_t emit_event_data_zstr(zend_string *);
static inline uint32_t emit_event_data_zstr_cached(zend_string *s);

static inline void emit_event_call_begin(zend_execute_data *);
static inline void emit_event_icall_begin(zend_execute_data *);
static inline void emit_event_call_end();

ZEND_DECLARE_MODULE_GLOBALS(phtrace)

static int le_phtrace;

uint32_t stringCounter;
HashTable stringsCache;

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
    ZEND_INIT_SYMTABLE_EX(&stringsCache, 1000, 1);
    phtrace_buffer_allocate();
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(phtrace)
{
    phtrace_buffer_free();
    zend_hash_destroy(&stringsCache);
    UNREGISTER_INI_ENTRIES();
    return SUCCESS;
}

PHP_RINIT_FUNCTION(phtrace)
{
#if defined(COMPILE_DL_PHTRACE) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif

    stringCounter = 0;

    _zend_execute_ex = zend_execute_ex;
    zend_execute_ex = phtrace_execute_ex;

    _zend_execute_internal = zend_execute_internal;
    zend_execute_internal = phtrace_execute_internal;

    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(phtrace)
{
    zend_execute_ex = _zend_execute_ex;
    zend_execute_internal = _zend_execute_internal;

    phtrace_buffer_flush();
    zend_hash_clean(&stringsCache);

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
    emit_event_call_begin(execute_data);
    _zend_execute_ex(execute_data);
    emit_event_call_end();
}

static void phtrace_execute_internal(zend_execute_data *execute_data, zval *return_value) {
    emit_event_icall_begin(execute_data);
    EX(func)->internal_function.handler(execute_data, return_value);
    emit_event_call_end();
}

static inline uint64_t rdtscp() {
    uint64_t rax,rdx, aux;
    __asm__ volatile ( "rdtscp\n" : "=a" (rax), "=d" (rdx), "=c" (aux) : : );
    return (rdx << 32) + rax;
}

static inline uint32_t emit_event_data_zstr(zend_string *s) {
    stringCounter++;

    phtrace_buffer_ensure_size(1 + sizeof(uint32_t) + ZSTR_LEN(s) + 1);
    phtrace_buffer.data[phtrace_buffer.used] = PHT_EVENT_DATA_STR;
    phtrace_buffer.used++;

    *((uint32_t *) PHTRACE_BUFFER_CURRENT) = stringCounter;
    phtrace_buffer.used += sizeof(uint32_t);

    strncpy((char *) PHTRACE_BUFFER_CURRENT, ZSTR_VAL(s), ZSTR_LEN(s) + 1);
    phtrace_buffer.used += ZSTR_LEN(s) + 1;

    return stringCounter;
}

static inline uint32_t emit_event_data_zstr_cached(zend_string *s) {
    zval *cached = zend_hash_find(&stringsCache, s);
    if (cached) {
        return Z_LVAL_P(cached);
    } else {
        zval n;
        ZVAL_LONG(&n, emit_event_data_zstr(s));
        zend_hash_add_new(&stringsCache, s, &n);
        return Z_LVAL(n);
    }
}

static inline void emit_event_call_begin(zend_execute_data *execute_data) {
    uint32_t filename = emit_event_data_zstr_cached(EX(func)->op_array.filename);
    uint32_t function_name = 0;
    uint32_t class_name = 0;
    if (EX(func)->common.function_name) {
        function_name = emit_event_data_zstr_cached(EX(func)->common.function_name);
        if (EX(func)->common.scope) {
            class_name = emit_event_data_zstr_cached(EX(func)->common.scope->name);
        }
    }

    EventCallBegin *e;
    PHTRACE_ALLOC_EVENT(e, EventCallBegin);

    e->tsc = rdtscp();
    e->filename = filename;
    e->function_name = function_name;
    e->class_name = class_name;
    e->line_start = EX(func)->op_array.line_start;
}

static inline void emit_event_icall_begin(zend_execute_data *execute_data) {
    uint32_t function_name = emit_event_data_zstr_cached(EX(func)->common.function_name);
    uint32_t class_name = 0;
    if (EX(func)->common.scope) {
        class_name = emit_event_data_zstr_cached(EX(func)->common.scope->name);
    }

    EventICallEnd *e;
    PHTRACE_ALLOC_EVENT(e, EventICallEnd);

    e->tsc = rdtscp();
    e->function_name = function_name;
    e->class_name = class_name;
}

static inline void emit_event_call_end() {
    EventCallEnd *e;
    PHTRACE_ALLOC_EVENT(e, EventCallEnd);
    e->tsc = rdtscp();
}
