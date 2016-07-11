/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2016 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include <netdb.h>

#include "php_phtrace.h"

void close_connection();
void establish_connection();

ZEND_DECLARE_MODULE_GLOBALS(phtrace)

/* True global resources - no need for thread safety here */
static int le_phtrace;
static int server_socket = 0;

PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("phtrace.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_phtrace_globals, phtrace_globals)
    STD_PHP_INI_ENTRY("phtrace.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_phtrace_globals, phtrace_globals)
    STD_PHP_INI_ENTRY("phtrace.server_port", "19229", PHP_INI_ALL, OnUpdateString, server_port, zend_phtrace_globals, phtrace_globals)
PHP_INI_END()


/* Remove the following function when you have successfully modified config.m4
   so that your module can be compiled into PHP, it exists only for testing
   purposes. */

/* Every user-visible function in PHP should document itself in the source */
/* {{{ proto string confirm_phtrace_compiled(string arg)
   Return a string to confirm that the module is compiled in */
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
/* }}} */
/* The previous line is meant for vim and emacs, so it can correctly fold and
   unfold functions in source code. See the corresponding marks just before
   function definition, where the functions purpose is also documented. Please
   follow this convention for the convenience of others editing your code.
*/



//static void php_phtrace_init_globals(zend_phtrace_globals *phtrace_globals)
//{
//	phtrace_globals->global_value = 0;
//	phtrace_globals->global_string = NULL;
//}


/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(phtrace)
{
	REGISTER_INI_ENTRIES();
	establish_connection();
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(phtrace)
{
    close_connection();
	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(phtrace)
{
#if defined(COMPILE_DL_PHTRACE) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	const char *msg = "{\"some_json_field\":\"some_json_value\"}\n";
    write(server_socket, msg, strlen(msg));
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(phtrace)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(phtrace)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "phtrace support", "enabled");
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */

/* {{{ phtrace_functions[]
 *
 * Every user visible function must have an entry in phtrace_functions[].
 */
const zend_function_entry phtrace_functions[] = {
	PHP_FE(confirm_phtrace_compiled,	NULL)		/* For testing, remove later. */
	PHP_FE_END	/* Must be the last line in phtrace_functions[] */
};
/* }}} */

/* {{{ phtrace_module_entry
 */
zend_module_entry phtrace_module_entry = {
	STANDARD_MODULE_HEADER,
	"phtrace",
	phtrace_functions,
	PHP_MINIT(phtrace),
	PHP_MSHUTDOWN(phtrace),
	PHP_RINIT(phtrace),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(phtrace),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(phtrace),
	PHP_PHTRACE_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_PHTRACE
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(phtrace)
#endif

void establish_connection() {
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

void close_connection() {
    if (server_socket) {
        close(server_socket);
        server_socket = 0;
    }
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
