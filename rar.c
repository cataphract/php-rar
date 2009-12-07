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

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef PHP_WIN32
# include <math.h>
#endif

#include <wchar.h>

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"

#if HAVE_RAR

#include "php_rar.h"

/* {{{ Function prototypes for functions with internal linkage */
static void _rar_fix_wide(wchar_t *str);
/* }}} */

/* Functions with external linkage {{{ */
int _rar_handle_error(int errcode TSRMLS_DC) /* {{{ */
{
	const char *err = _rar_error_to_string(errcode);

	if (err == NULL) {
		return SUCCESS;
	}
	
	php_error_docref(NULL TSRMLS_CC, E_WARNING, err);
	return FAILURE;
}
/* }}} */

//returns a string or NULL if not an error
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

/* From unicode.cpp
 * I can't use that one directy because it takes a const wchar, not wchar_t.
 * And I shouldn't because it's not a public API.
 */
void _rar_wide_to_utf(const wchar_t *src, char *dest, size_t dest_size) /* {{{ */
{
	long dsize= (long) dest_size;
	dsize--;
	while (*src != 0 && --dsize >= 0) {
		uint c =*(src++);
		if (c < 0x80)
			*(dest++) = c;
		else if (c < 0x800 && --dsize >= 0)	{
			*(dest++) = (0xc0 | (c >> 6));
			*(dest++) = (0x80 | (c & 0x3f));
		}
		else if (c < 0x10000 && (dsize -= 2) >= 0) {
			*(dest++) = (0xe0 | (c >> 12));
			*(dest++) = (0x80 | ((c >> 6) & 0x3f));
			*(dest++) = (0x80 | (c & 0x3f));
		}
		else if (c < 0x200000 && (dsize -= 3) >= 0) {
			*(dest++) = (0xf0 | (c >> 18));
			*(dest++) = (0x80 | ((c >> 12) & 0x3f));
			*(dest++) = (0x80 | ((c >> 6) & 0x3f));
			*(dest++) = (0x80 | (c & 0x3f));
		}
	}
	*dest = 0;
}
/* }}} */

/* idem */
void _rar_utf_to_wide(const char *src, wchar_t *dest, size_t dest_size) /* {{{ */
{
	long dsize = (long) dest_size;
	dsize--;
	while (*src != 0) {
		uint c = (unsigned char) *(src++),
			 d;
		if (c < 0x80)
			d = c;
		else if ((c >> 5) == 6) {
			if ((*src & 0xc0) != 0x80)
				break;
			d=((c & 0x1f) << 6)|(*src & 0x3f);
			src++;
		}
		else if ((c>>4)==14) {
			if ((src[0] & 0xc0) != 0x80 || (src[1] & 0xc0) != 0x80)
				break;
			d = ((c & 0xf) << 12) | ((src[0] & 0x3f) << 6) | (src[1] & 0x3f);
			src += 2;
		}
		else if ((c>>3)==30) {
			if ((src[0] & 0xc0) != 0x80 || (src[1] & 0xc0) != 0x80 || (src[2] & 0xc0) != 0x80)
				break;
			d = ((c & 7) << 18) | ((src[0] & 0x3f) << 12) | ((src[1] & 0x3f) << 6) | (src[2] & 0x3f);
			src += 3;
		}
		else
			break;
		if (--dsize < 0)
			break;
		if (d > 0xffff) {
			if (--dsize < 0 || d > 0x10ffff)
				break;
			*(dest++) = ((d - 0x10000) >> 10) + 0xd800;
			*(dest++) = (d & 0x3ff) + 0xdc00;
		}
		else
			*(dest++) = d;
	}
	*dest = 0;
}
/* }}} */

/* WARNING: It's the caller who must close the archive. */
int _rar_find_file(struct RAROpenArchiveDataEx *open_data, /* IN */
				   const char *const utf_file_name, /* IN */
				   char *password, /* IN, can be null */
				   void **arc_handle, /* OUT: where to store rar archive handle  */
				   int *found, /* OUT */
				   struct RARHeaderDataEx *header_data /* OUT, can be null */
				   ) /* {{{ */
{
	int						result,
							process_result;
	wchar_t					*file_name = NULL;
	int						utf_file_name_len;
	struct RARHeaderDataEx	*used_header_data;
	int						retval = 0; /* success in rar parlance */

	assert(open_data != NULL);
	assert(utf_file_name != NULL);
	assert(arc_handle != NULL);
	assert(found != NULL);
	*found = FALSE;
	*arc_handle = NULL;
	used_header_data = header_data != NULL ?
		header_data :
		ecalloc(1, sizeof *used_header_data);

	*arc_handle	= RAROpenArchiveEx(open_data);
	if (*arc_handle == NULL) {
		retval = open_data->OpenResult;
		goto cleanup;
	}
	RARSetCallback(*arc_handle, _rar_unrar_callback, (LPARAM) password);

	utf_file_name_len = strlen(utf_file_name);
	file_name = ecalloc(utf_file_name_len + 1, sizeof *file_name); 
	_rar_utf_to_wide(utf_file_name, file_name, utf_file_name_len + 1);
	
	while ((result = RARReadHeaderEx(*arc_handle, used_header_data)) == 0) {
		if (sizeof(wchar_t) > 2)
			_rar_fix_wide(used_header_data->FileNameW);

		if (wcsncmp(used_header_data->FileNameW, file_name, NM) == 0) {
			*found = TRUE;
			goto cleanup;
		} else {
			process_result = RARProcessFile(*arc_handle, RAR_SKIP, NULL, NULL);
		}
		if (process_result != 0) {
			retval = process_result;
			goto cleanup;
		}
	}

	if (result != 0 && result != 1) {
		//0 indicates success, 1 indicates normal end of file
		retval = result;
		goto cleanup;
	}

cleanup:
	if (header_data == NULL)
		efree(used_header_data);
	if (file_name != NULL)
		efree(file_name);

	return retval;
}
/* }}} */

/* Only processes password callbacks */
int CALLBACK _rar_unrar_callback(UINT msg, LPARAM UserData, LPARAM P1, LPARAM P2) /* {{{ */
{
	//TSRMLS_FETCH();
	
	if (msg == UCM_NEEDPASSWORD) {
		//user data is the password or null if none
		char *password = (char*) UserData;

		if (password == NULL) {
			/*php_error_docref(NULL TSRMLS_CC, E_WARNING,
				"Password needed, but it has not been specified");*/
			return -1;
		}
		else {
			strncpy((char*) P1, password, (size_t) P2);
		}
	}

	return 0;
}
/* }}} */
/* }}} */

/* {{{ Functions with internal linkage */
static void _rar_fix_wide(wchar_t *str) /* {{{ */
{
	wchar_t *write,
		    *read;
	for (write = str, read = str; *read != L'\0'; read++) {
		if ((unsigned) *read <= 0x10ffff)
			*(write++) = *read;
	}
	*write = L'\0';
}
/* }}} */
/* }}} */

#ifdef COMPILE_DL_RAR
BEGIN_EXTERN_C()
ZEND_GET_MODULE(rar)
END_EXTERN_C()
#endif

/* {{{ arginfo */
ZEND_BEGIN_ARG_INFO_EX(arginfo_rar_open, 0, 0, 1)
	ZEND_ARG_INFO(0, filename)
	ZEND_ARG_INFO(0, password)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_rar_list, 0, 0, 1)
	ZEND_ARG_INFO(0, rarfile)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_rar_entry_get, 0, 0, 2)
	ZEND_ARG_INFO(0, rarfile)
	ZEND_ARG_INFO(0, filename)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_rar_comment_get, 0, 0, 1)
	ZEND_ARG_INFO(0, rarfile)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_rar_close, 0, 0, 1)
	ZEND_ARG_INFO(0, rarfile)
ZEND_END_ARG_INFO()
/* }}} */

/* {{{ rar_functions[]
 *
 */
static function_entry rar_functions[] = {
	PHP_FE(rar_open,		arginfo_rar_open)
	PHP_FE(rar_list,		arginfo_rar_list)
	PHP_FE(rar_entry_get,	arginfo_rar_entry_get)
	PHP_FE(rar_comment_get,	arginfo_rar_comment_get)
	PHP_FE(rar_close,		arginfo_rar_close)
	{NULL, NULL, NULL}
};
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(rar)
{
	minit_rararch(TSRMLS_C);
	minit_rarentry(TSRMLS_C);
	
	REGISTER_LONG_CONSTANT("RAR_HOST_MSDOS",	HOST_MSDOS,	CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("RAR_HOST_OS2",		HOST_OS2,	CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("RAR_HOST_WIN32",	HOST_WIN32,	CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("RAR_HOST_UNIX",		HOST_UNIX,	CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("RAR_HOST_MACOS",	HOST_MACOS,	CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("RAR_HOST_BEOS",		HOST_BEOS,	CONST_CS | CONST_PERSISTENT);
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(rar)
{
	char version[256];

	php_info_print_table_start();
	php_info_print_table_header(2, "Rar support", "enabled");
	php_info_print_table_row(2, "Rar EXT version", PHP_RAR_VERSION);
	php_info_print_table_row(2, "Revision", "$Revision$");

#if	RARVER_BETA != 0
	sprintf(version,"%d.%02d beta%d patch%d %d-%d-%d", RARVER_MAJOR,
		RARVER_MINOR, RARVER_BETA, RARVER_PATCH, RARVER_YEAR, RARVER_MONTH,
		RARVER_DAY);
#else
	sprintf(version,"%d.%02d patch%d %d-%d-%d", RARVER_MAJOR, RARVER_MINOR,
		RARVER_PATCH, RARVER_YEAR, RARVER_MONTH, RARVER_DAY);
#endif

	php_info_print_table_row(2, "UnRAR version", version);
	php_info_print_table_end();
}
/* }}} */

/* {{{ rar_module_entry
 */
zend_module_entry rar_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"rar",
	rar_functions,
	PHP_MINIT(rar),
	NULL,
	NULL,
	NULL,
	PHP_MINFO(rar),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_RAR_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#endif /* HAVE_RAR */

#ifdef __cplusplus
}
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
