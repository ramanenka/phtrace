#ifndef PHP_PHTRACE_H
#define PHP_PHTRACE_H

extern zend_module_entry phtrace_module_entry;
#define phpext_phtrace_ptr &phtrace_module_entry

#define PHP_PHTRACE_VERSION "0.0.1"

#ifdef PHP_WIN32
#   define PHP_PHTRACE_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#   define PHP_PHTRACE_API __attribute__ ((visibility("default")))
#else
#   define PHP_PHTRACE_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

ZEND_BEGIN_MODULE_GLOBALS(phtrace)
    zend_long  global_value;
    char *global_string;
    char *server_port;
ZEND_END_MODULE_GLOBALS(phtrace)

#define PHTRACE_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(phtrace, v)

#if defined(ZTS) && defined(COMPILE_DL_PHTRACE)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

#endif
