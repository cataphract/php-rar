/*
+----------------------------------------------------------------------+
| PHP Version 5                                                        |
+----------------------------------------------------------------------+
| Copyright (c) 1997-2009 The PHP Group                                |
+----------------------------------------------------------------------+
| This source file is subject to version 3.0 of the PHP license,       |
| that is bundled with this package in the file LICENSE, and is        |
| available through the world-wide-web at the following url:           |
| http://www.php.net/license/3_0.txt.                                  |
| If you did not receive a copy of the PHP license and are unable to   |
| obtain it through the world-wide-web, please send a note to          |
| license@php.net so we can mail you a copy immediately.               |
|                                                                      |
|                        **** WARNING ****                             |
|                                                                      |
| This module makes use of unRAR - free utility for RAR archives.      |
| Its license states that you MUST NOT use its code to develop         |
| a RAR (WinRAR) compatible archiver.                                  |
| Please, read unRAR license for full information.                     |
| unRAR & RAR copyrights are owned by Eugene Roshal                    |
+----------------------------------------------------------------------+
| Author: Antony Dovgal <tony@daylessday.org>                          |
| Author: Gustavo Lopes <cataphract@php.net>                           |
+----------------------------------------------------------------------+
*/

#ifdef __cplusplus
extern "C" {
#endif

#include <php.h>
#include <zend_exceptions.h>
#include "php_rar.h"

/* {{{ Globals with external linkage */
zend_class_entry *rarexception_ce_ptr;
/* }}} */

/* Functions with external linkage {{{ */
/* Functions with external linkage {{{ */
int _rar_handle_error(int errcode TSRMLS_DC) /* {{{ */
{
	return _rar_handle_error_ex("", errcode TSRMLS_CC);
}
/* }}} */

int _rar_handle_error_ex(const char *preamble, int errcode TSRMLS_DC) /* {{{ */
{
	const char *err = _rar_error_to_string(errcode);

	if (err == NULL) {
		return SUCCESS;
	}

	if (_rar_using_exceptions(TSRMLS_C)) {
		zend_throw_exception_ex(rarexception_ce_ptr, errcode TSRMLS_CC,
			"unRAR internal error: %s%s", preamble, err);
	}
	else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s%s", preamble, err);
	}
	return FAILURE;
}
/* }}} */

/* Errors not related to the unRAR library */
void _rar_handle_ext_error(const char *format TSRMLS_DC, ...) /* {{{ */
{
	va_list arg;
	char *message;

#if defined(ZTS) && PHP_MAJOR_VERSION < 7
	va_start(arg, TSRMLS_C);
#else
	va_start(arg, format);
#endif
	vspprintf(&message, 0, format, arg);
	va_end(arg);

	if (_rar_using_exceptions(TSRMLS_C))
		zend_throw_exception(rarexception_ce_ptr, message, -1L TSRMLS_CC);
	else
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s", message);
	efree(message);
}
/* }}} */

int _rar_using_exceptions(TSRMLS_D)
{
	zval *pval;
	pval = zend_read_static_property(rarexception_ce_ptr, "usingExceptions",
		sizeof("usingExceptions") -1, (zend_bool) 1 TSRMLS_CC);
#if PHP_MAJOR_VERSION < 7
	assert(Z_TYPE_P(pval) == IS_BOOL);
	return Z_BVAL_P(pval);
#else
	assert(Z_TYPE_P(pval) == IS_TRUE || Z_TYPE_P(pval) == IS_FALSE);
	return Z_TYPE_P(pval) == IS_TRUE;
#endif
}

/* returns a string or NULL if not an error */
const char * _rar_error_to_string(int errcode) /* {{{ */
{
	const char *ret;
	switch (errcode) {
		case 0:
			/* no error */
		case 1:
			/* no error (comment completely read) */
		case ERAR_END_ARCHIVE:
			/* no error */
			ret = NULL;
			break;
		case ERAR_NO_MEMORY:
			ret = "ERAR_NO_MEMORY (not enough memory)";
			break;
		case ERAR_BAD_DATA:
			ret = "ERAR_BAD_DATA";
			break;
		case ERAR_BAD_ARCHIVE:
			ret = "ERAR_BAD_ARCHIVE";
			break;
		case ERAR_UNKNOWN_FORMAT:
			ret = "ERAR_UNKNOWN_FORMAT";
			break;
		case ERAR_EOPEN:
			ret = "ERAR_EOPEN (file open error)";
			break;
		case ERAR_ECREATE:
			ret = "ERAR_ECREATE";
			break;
		case ERAR_ECLOSE:
			ret = "ERAR_ECLOSE (error closing file)";
			break;
		case ERAR_EREAD:
			ret = "ERAR_EREAD";
			break;
		case ERAR_EWRITE:
			ret = "ERAR_EWRITE";
			break;
		case ERAR_SMALL_BUF:
			ret = "ERAR_SMALL_BUF";
			break;
		case ERAR_UNKNOWN:
			ret = "ERAR_UNKNOWN (unknown RAR error)";
			break;
		case ERAR_MISSING_PASSWORD:
			ret = "ERAR_MISSING_PASSWORD (password needed but not specified)";
			break;
		default:
			ret = "unknown RAR error (should not happen)";
			break;
	}
	return ret;
}
/* }}} */
/* }}} */

/* {{{ proto bool RarException::setUsingExceptions(using_exceptions)
   Set whether exceptions are to be used */
PHP_METHOD(rarexception, setUsingExceptions)
{
	zend_bool argval;
	int result;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b", &argval) == FAILURE ) {
		return;
	}

	result = zend_update_static_property_bool(rarexception_ce_ptr,
		"usingExceptions", sizeof("usingExceptions") -1,
		(long) argval TSRMLS_CC);

	if (result == FAILURE) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
			"Could not set error handling mode. "
			"This is a bug, please report it.");
		return;
	}
}
/* }}} */

/* {{{ proto bool RarException::isUsingExceptions()
   Return whether exceptions are being used */
PHP_METHOD(rarexception, isUsingExceptions)
{
#if PHP_MAJOR_VERSION < 7
	zval **pval;
#else
	zval *pval;
#endif

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE ) {
		return;
	}

	/* or zend_read_static_property, which calls zend_std_get... after chg scope */
#if PHP_VERSION_ID < 50399
	pval = zend_std_get_static_property(rarexception_ce_ptr, "usingExceptions",
		sizeof("usingExceptions") -1, (zend_bool) 0 TSRMLS_CC);
#elif PHP_MAJOR_VERSION < 7
	pval = zend_std_get_static_property(rarexception_ce_ptr, "usingExceptions",
		sizeof("usingExceptions") -1, (zend_bool) 0, NULL TSRMLS_CC);
#else
	zend_string *prop_name =
		zend_string_init("usingExceptions", sizeof("usingExceptions") - 1, 0);
	pval = zend_std_get_static_property(rarexception_ce_ptr, prop_name,
		(zend_bool) 0);
	zend_string_release(prop_name);
#endif
	/* property always exists */
	assert(pval != NULL);
#if PHP_MAJOR_VERSION < 7
	assert(Z_TYPE_PP(pval) == IS_BOOL);
	RETURN_ZVAL(*pval, 0, 0);
#else
	assert(Z_TYPE_P(pval) == IS_TRUE || Z_TYPE_P(pval) == IS_FALSE);
	RETURN_ZVAL(pval, 0, 0);
#endif
}
/* }}} */

/* {{{ arginfo */
ZEND_BEGIN_ARG_INFO_EX(arginfo_rarexception_sue, 0, 0, 1)
	ZEND_ARG_INFO(0, using_exceptions)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_rarexception_void, 0)
ZEND_END_ARG_INFO()
/* }}} */

static zend_function_entry php_rarexception_class_functions[] = {
	PHP_ME(rarexception,	setUsingExceptions,	arginfo_rarexception_sue,	ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(rarexception,	isUsingExceptions,	arginfo_rarexception_void,	ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	{NULL, NULL, NULL}
};

void minit_rarerror(TSRMLS_D) /* {{{ */
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "RarException", php_rarexception_class_functions);
#if PHP_MAJOR_VERSION < 7
	rarexception_ce_ptr = zend_register_internal_class_ex(&ce,
		zend_exception_get_default(TSRMLS_C), NULL TSRMLS_CC);
#else
	rarexception_ce_ptr = zend_register_internal_class_ex(&ce,
		zend_exception_get_default(TSRMLS_C));
#endif
	rarexception_ce_ptr->ce_flags |= ZEND_ACC_FINAL;
	zend_declare_property_bool(rarexception_ce_ptr, "usingExceptions",
		sizeof("usingExceptions") -1, 0L /* FALSE */,
		ZEND_ACC_STATIC TSRMLS_CC);
}
/* }}} */

#ifdef __cplusplus
}
#endif
