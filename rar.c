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

#define _GNU_SOURCE
#include <string.h>

#ifdef PHP_WIN32
# include <math.h>
#endif

#include <wchar.h>

#include <php.h>
#include <php_ini.h>
#include <zend_exceptions.h>
#include <ext/standard/info.h>
#include <ext/spl/spl_exceptions.h>

#if HAVE_RAR

#include "php_rar.h"

/* {{{ Function prototypes for functions with internal linkage */
static void _rar_fix_wide(wchar_t *str, size_t max_size);
static int _rar_unrar_volume_user_callback(char* dst_buffer,
										   zend_fcall_info *fci,
										   zend_fcall_info_cache *cache
										   TSRMLS_DC);
static int _rar_make_userdata_fcall(zval *callable,
							 zend_fcall_info *fci,
							 zend_fcall_info_cache *cache TSRMLS_DC);
/* }}} */

/* {{{ Functions with external linkage */
#if !defined(HAVE_STRNLEN) || !HAVE_STRNLEN
size_t _rar_strnlen(const char *s, size_t maxlen) /* {{{ */
{
	char *r = memchr(s, '\0', maxlen);
	return r ? r-s : maxlen;
}
/* }}} */
#endif

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
			*(dest++) = (char) c;
		else if (c < 0x800 && --dsize >= 0)	{
			*(dest++) = (char) (0xc0 | (c >> 6));
			*(dest++) = (0x80 | (c & 0x3f));
		}
		else if (c < 0x10000 && (dsize -= 2) >= 0) {
			*(dest++) = (char) (0xe0 | (c >> 12));
			*(dest++) = (0x80 | ((c >> 6) & 0x3f));
			*(dest++) =  (0x80 | (c & 0x3f));
		}
		else if (c < 0x200000 && (dsize -= 3) >= 0) {
			*(dest++) = (char) (0xf0 | (c >> 18));
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
			*(dest++) = (wchar_t) (((d - 0x10000) >> 10) + 0xd800);
			*(dest++) = (d & 0x3ff) + 0xdc00;
		}
		else
			*(dest++) = (wchar_t) d;
	}
	*dest = 0;
}
/* }}} */

void _rar_destroy_userdata(rar_cb_user_data *udata) /* {{{ */
{
	assert(udata != NULL);

	if (udata->password != NULL) {
		efree(udata->password);
	}

	if (udata->callable != NULL) {
#if PHP_MAJOR_VERSION < 7
		zval_ptr_dtor(&udata->callable);
#else
		zval_ptr_dtor(udata->callable);
		efree(udata->callable);
#endif
	}

	udata->password = NULL;
	udata->callable = NULL;
}
/* }}} */

int _rar_find_file(struct RAROpenArchiveDataEx *open_data, /* IN */
				   const char *const utf_file_name, /* IN */
				   rar_cb_user_data *cb_udata, /* IN, must be managed outside */
				   void **arc_handle, /* OUT: where to store rar archive handle  */
				   int *found, /* OUT */
				   struct RARHeaderDataEx *header_data /* OUT, can be null */
				   ) /* {{{ */
{
	wchar_t *file_name = NULL;
	size_t utf_file_name_len = strlen(utf_file_name);
	int ret;

	file_name = ecalloc(utf_file_name_len + 1, sizeof *file_name);
	_rar_utf_to_wide(utf_file_name, file_name, utf_file_name_len + 1);
	ret = _rar_find_file_w(open_data, file_name, cb_udata, arc_handle, found,
		header_data);
	efree(file_name);
	return ret;
}
/* }}} */

/* WARNING: It's the caller who must close the archive and manage the lifecycle
of cb_udata (must be valid while the archive is opened). */
/*
 * This function opens a RAR file and looks for the file with the
 * name utf_file_name.
 * If the operation is sucessful, arc_handle is populated with the RAR file
 * handle, found is set to TRUE if the file is found and FALSE if it is not
 * found; additionaly, the optional header_data is populated with the first
 * header that corresponds to the request file. If the file is not found and
 * header_data is specified, its values are undefined.
 * Note that even when the file is not found, the caller must still close
 * the archive.
 */
int _rar_find_file_w(struct RAROpenArchiveDataEx *open_data, /* IN */
					 const wchar_t *const file_name, /* IN */
					 rar_cb_user_data *cb_udata, /* IN, must be managed outside */
					 void **arc_handle, /* OUT: where to store rar archive handle  */
					 int *found, /* OUT */
					 struct RARHeaderDataEx *header_data /* OUT, can be null */
					 ) /* {{{ */
{
	int						result,
							process_result;
	struct RARHeaderDataEx	*used_header_data;
	int						retval = 0; /* success in rar parlance */

	assert(open_data != NULL);
	assert(file_name != NULL);
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
	RARSetCallback(*arc_handle, _rar_unrar_callback, (LPARAM) cb_udata);

	while ((result = RARReadHeaderEx(*arc_handle, used_header_data)) == 0) {
#if WCHAR_MAX > 0xffff
			_rar_fix_wide(used_header_data->FileNameW, NM);
#endif

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
		/* 0 indicates success, 1 indicates normal end of file */
		retval = result;
		goto cleanup;
	}

cleanup:
	if (header_data == NULL)
		efree(used_header_data);

	return retval;
}
/* }}} */

int _rar_find_file_p(struct RAROpenArchiveDataEx *open_data, /* IN */
					 size_t position, /* IN */
					 rar_cb_user_data *cb_udata, /* IN, must be managed outside */
					 void **arc_handle, /* OUT: where to store rar archive handle  */
					 int *found, /* OUT */
					 struct RARHeaderDataEx *header_data /* OUT, can be null */
					 ) /* {{{ */
{
	int						result,
							process_result;
	struct RARHeaderDataEx	*used_header_data;
	int						retval = 0; /* success in rar parlance */
	size_t					curpos = 0;

	assert(open_data != NULL);
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
	RARSetCallback(*arc_handle, _rar_unrar_callback, (LPARAM) cb_udata);

	while ((result = RARReadHeaderEx(*arc_handle, used_header_data)) == 0) {
		/* skip entries that were split before with incrementing current pos */
		if ((used_header_data->Flags & RHDF_SPLITBEFORE) ||
				(curpos++ != position)) {
			process_result = RARProcessFile(*arc_handle, RAR_SKIP, NULL, NULL);
		} else {
			*found = TRUE;
			goto cleanup;
		}
		if (process_result != 0) {
			retval = process_result;
			goto cleanup;
		}
	}

	if (result != 0 && result != 1) {
		/* 0 indicates success, 1 indicates normal end of file */
		retval = result;
		goto cleanup;
	}

cleanup:
	if (header_data == NULL)
		efree(used_header_data);

	return retval;
}

/* An unRAR callback.
 * Processes requests for passwords and missing volumes
 * If there is (userland) volume find callback specified, try to use that
 * callback to retrieve the name of the missing volume. Otherwise, or if
 * the volume find callback returns null, cancel the operation. */
int CALLBACK _rar_unrar_callback(UINT msg, LPARAM UserData, LPARAM P1, LPARAM P2) /* {{{ */
{
	rar_cb_user_data *userdata = (rar_cb_user_data*)  UserData;
	TSRMLS_FETCH();

	if (msg == UCM_NEEDPASSWORD) {
		/* user data is the password or null if none */
		char *password = userdata->password;

		if (password == NULL || password[0] == '\0') {
			/*php_error_docref(NULL TSRMLS_CC, E_WARNING,
				"Password needed, but it has not been specified");*/
			return -1;
		}
		else {
			strncpy((char *) P1, password, (size_t) P2);
			assert((size_t) P2 > 0);
			((char *) P1)[(size_t) P2 - 1] = '\0';
		}
	}
	else if (msg == UCM_CHANGEVOLUME) {
		if (((int) P2) == RAR_VOL_ASK) {
			int ret, called_cb = 0;
			if (userdata->callable == NULL) {
				/* if there's no callback, abort */
				ret = -1;
			}
			else {
				zend_fcall_info fci;
				zend_fcall_info_cache cache;
				/* make_userdata_fcall and volume_user_callback are chatty */
				if (_rar_make_userdata_fcall(userdata->callable, &fci, &cache
						TSRMLS_CC) == SUCCESS) {
					ret = _rar_unrar_volume_user_callback(
						(char*) P1, &fci, &cache TSRMLS_CC);
					called_cb = 1;
				}
				else {
					ret = -1;
				}

			}

			/* always a warning, never an exception here */
			if (ret == -1 && !called_cb)
				php_error_docref(NULL TSRMLS_CC, E_WARNING,
					"Volume %s was not found", (char*) P1);

			return ret;
		}
	}

	return 0;
}
/* }}} */

PHP_FUNCTION(rar_bogus_ctor) /* {{{ */
{
	/* This exception should not be thrown. The point is to add this as
	 * a class constructor and make it private. This code would be able to
	 * run only if the constructor were made public */
	zend_throw_exception(NULL,
		"An object of this type cannot be created with the new operator.",
		0 TSRMLS_CC);
}
/* }}} */

PHP_FUNCTION(rar_wrapper_cache_stats) /* {{{ */
{
	char *result = NULL;
	int len;

	if (zend_parse_parameters_none() == FAILURE)
		return;

	len = spprintf(&result, 0, "%u/%u (hits/misses)",
		RAR_G(contents_cache).hits, RAR_G(contents_cache).misses);

	RAR_RETURN_STRINGL(result, len, 0);
}
/* }}} */
/* }}} */

/* {{{ Functions with internal linkage */
/*
 * Only relevant when sizeof(wchar_t) > 2 (so not windows).
 * Removes the characters use value if > 0x10ffff; these are not
 * valid UTF characters.
 */

static void _rar_fix_wide(wchar_t *str, size_t max_size) /* {{{ */
{
	wchar_t *write,
		    *read,
			*max_fin;
	max_fin = str + max_size;
	for (write = str, read = str; *read != L'\0' && read != max_fin; read++) {
		if ((unsigned) *read <= 0x10ffff)
			*(write++) = *read;
	}
	*write = L'\0';
}
/* }}} */

/* called from the RAR callback; calls a user callback in case a volume was
 * not found
 * This function sends messages instead of calling _rar_handle_ext_error
 * because, in case we're using exceptions, we want to let an exception with
 * error code ERAR_EOPEN to be thrown.
 */
static int _rar_unrar_volume_user_callback(char* dst_buffer,
										   zend_fcall_info *fci,
										   zend_fcall_info_cache *cache
										   TSRMLS_DC) /* {{{ */
{
#if PHP_MAJOR_VERSION < 7
	zval *failed_vol,
		 *retval_ptr = NULL,
		 **params;
#else
	zval failed_vol,
		 retval,
		 *params,
		 *const retval_ptr = &retval;
#endif
	int  ret = -1;

#if PHP_MAJOR_VERSION < 7
	MAKE_STD_ZVAL(failed_vol);
	RAR_ZVAL_STRING(failed_vol, dst_buffer, 1);
	params = &failed_vol;
	fci->retval_ptr_ptr = &retval_ptr;
	fci->params = &params;
#else
	ZVAL_STRING(&failed_vol, dst_buffer);
	ZVAL_NULL(&retval);
	params = &failed_vol;
	fci->retval = &retval;
	fci->params = params;
#endif
	fci->param_count = 1;

#if PHP_MAJOR_VERSION < 7
	if (zend_call_function(fci, cache TSRMLS_CC) != SUCCESS ||
			fci->retval_ptr_ptr == NULL ||
			*fci->retval_ptr_ptr == NULL) {
#else
	if (zend_call_function(fci, cache TSRMLS_CC) != SUCCESS || EG(exception)) {
#endif
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
			"Failure to call volume find callback");
		goto cleanup;
	}

#if PHP_MAJOR_VERSION < 7
	assert(*fci->retval_ptr_ptr == retval_ptr);
#else
	assert(fci->retval == &retval);
#endif
	if (Z_TYPE_P(retval_ptr) == IS_NULL) {
		/* let return -1 */
	}
	else if (Z_TYPE_P(retval_ptr) == IS_STRING) {
		char *filename = Z_STRVAL_P(retval_ptr);
		char resolved_path[MAXPATHLEN];
		size_t resolved_len;

		if (OPENBASEDIR_CHECKPATH(filename)) {
			goto cleanup;
		}
		if (!expand_filepath(filename, resolved_path TSRMLS_CC)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING,
				"Cound not expand filename %s", filename);
			goto cleanup;
		}

		resolved_len = _rar_strnlen(resolved_path, MAXPATHLEN);
		/* dst_buffer size is NM; first condition won't happen short of a bug
		 * in expand_filepath */
		if (resolved_len == MAXPATHLEN || resolved_len > NM - 1) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING,
				"Resolved path is too big for the unRAR library");
			goto cleanup;
		}

		strncpy(dst_buffer, resolved_path, NM);
		dst_buffer[NM - 1] = '\0';
		ret = 1; /* try this new filename */
	}
	else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
			"Wrong type returned by volume find callback, "
			"expected string or NULL");
		/* let return -1 */
	}

cleanup:
#if PHP_MAJOR_VERSION < 7
	zval_ptr_dtor(&failed_vol);
	if (retval_ptr != NULL) {
		zval_ptr_dtor(&retval_ptr);
	}
#else
	zval_ptr_dtor(&failed_vol);
	zval_ptr_dtor(&retval);
#endif
	return ret;
}
/* }}} */

static int _rar_make_userdata_fcall(zval *callable,
							 zend_fcall_info *fci,
							 zend_fcall_info_cache *cache TSRMLS_DC) /* {{{ */
{
	char *error = NULL;
	assert(callable != NULL);
	assert(fci != NULL);
	assert(cache != NULL);

	*cache = empty_fcall_info_cache;

#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION == 2
	if (zend_fcall_info_init(callable, fci, cache TSRMLS_CC) != SUCCESS) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
			"The RAR file was not opened in rar_open/RarArchive::open with a "
			"valid callback.", error);
		return FAILURE;
	}
	else {
		return SUCCESS;
	}
#else
	if (zend_fcall_info_init(callable, IS_CALLABLE_STRICT, fci, cache, NULL,
			&error TSRMLS_CC) == SUCCESS) {
		if (error) {
			php_error_docref(NULL TSRMLS_CC, E_STRICT,
				"The RAR file was not opened with a strictly valid callback (%s)",
				error);
			efree(error);
		}
		return SUCCESS;
	}
	else {
		if (error) {
			php_error_docref(NULL TSRMLS_CC, E_STRICT,
				"The RAR file was not opened with a valid callback (%s)",
				error);
			efree(error);
		}
		return FAILURE;
	}
#endif

}
/* }}} */

/* }}} */

#ifdef COMPILE_DL_RAR
ZEND_GET_MODULE(rar)
#endif

/* {{{ arginfo */
ZEND_BEGIN_ARG_INFO_EX(arginfo_rar_open, 0, 0, 1)
	ZEND_ARG_INFO(0, filename)
	ZEND_ARG_INFO(0, password)
	ZEND_ARG_INFO(0, volume_callback)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_rar_void_archmeth, 0, 0, 1)
#if 0 /* don't turn on type hinting yet */
	ZEND_ARG_OBJ_INFO(0, rarfile, RarArchive, 0)
#else
	ZEND_ARG_INFO(0, rarfile)
#endif
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_rar_entry_get, 0, 0, 2)
#if 0 /* don't turn on type hinting yet */
	ZEND_ARG_OBJ_INFO(0, rarfile, RarArchive, 0)
#else
	ZEND_ARG_INFO(0, rarfile)
#endif
	ZEND_ARG_INFO(0, filename)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_rar_allow_broken_set, 0, 0, 2)
#if 0 /* don't turn on type hinting yet */
	ZEND_ARG_OBJ_INFO(0, rarfile, RarArchive, 0)
#else
	ZEND_ARG_INFO(0, rarfile)
#endif
	ZEND_ARG_INFO(0, allow_broken)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_rar_wrapper_cache_stats, 0)
ZEND_END_ARG_INFO()
/* }}} */

/* {{{ rar_functions[]
 *
 */
static zend_function_entry rar_functions[] = {
	PHP_FE(rar_open,				arginfo_rar_open)
	PHP_FE(rar_list,				arginfo_rar_void_archmeth)
	PHP_FE(rar_entry_get,			arginfo_rar_entry_get)
	PHP_FE(rar_solid_is,			arginfo_rar_void_archmeth)
	PHP_FE(rar_comment_get,			arginfo_rar_void_archmeth)
	PHP_FE(rar_broken_is,			arginfo_rar_void_archmeth)
	PHP_FE(rar_allow_broken_set,	arginfo_rar_allow_broken_set)
	PHP_FE(rar_close,				arginfo_rar_void_archmeth)
	PHP_FE(rar_wrapper_cache_stats,	arginfo_rar_wrapper_cache_stats)
	{NULL, NULL, NULL}
};
/* }}} */

/* {{{ Globals' related activities */
ZEND_DECLARE_MODULE_GLOBALS(rar);

#if PHP_MAJOR_VERSION < 7
static int _rar_array_apply_remove_first(void *pDest TSRMLS_DC)
#else
static int _rar_array_apply_remove_first(zval *pDest TSRMLS_DC)
#endif
{
	return (ZEND_HASH_APPLY_STOP | ZEND_HASH_APPLY_REMOVE);
}

static void _rar_contents_cache_put(const char *key,
									uint key_len,
									zval *zv TSRMLS_DC)
{
	rar_contents_cache *cc = &RAR_G(contents_cache);
	int cur_size;

	cur_size = zend_hash_num_elements(cc->data);
	if (cur_size == cc->max_size) {
		zend_hash_apply(cc->data, _rar_array_apply_remove_first TSRMLS_CC);
		assert(zend_hash_num_elements(cc->data) == cur_size - 1);
	}
	rar_zval_add_ref(&zv);
#if PHP_MAJOR_VERSION < 7
	assert(Z_REFCOUNT_P(zv) > 1);
	SEPARATE_ZVAL(&zv); /* ensure we store a heap allocated copy */
	zend_hash_update(cc->data, key, key_len, &zv, sizeof(zv), NULL);
#else
	zend_hash_str_update(cc->data, key, key_len, zv);
#endif
}

static zval *_rar_contents_cache_get(const char *key,
									 uint key_len,
									 zval *rv TSRMLS_DC)
{
	rar_contents_cache *cc = &RAR_G(contents_cache);
	zval *element = NULL;
#if PHP_MAJOR_VERSION < 7
	zval **element_p = NULL;
	zend_hash_find(cc->data, key, key_len, (void **) &element_p);
	if (element_p) {
		element = *element_p;
	}
#else
	element = zend_hash_str_find(cc->data, key, key_len);
#endif

	if (element != NULL) {
		cc->hits++;
		INIT_ZVAL(*rv);
		ZVAL_COPY_VALUE(rv, element);
		zval_copy_ctor(rv);
		return rv;
	}
	else {
		cc->misses++;
		return NULL;
	}
}

/* ZEND_MODULE_GLOBALS_CTOR_D declares it receiving zend_rar_globals*,
 * which is incompatible; once cast into ts_allocate_ctor by the macro,
 * ZEND_INIT_MODULE_GLOBALS, it cannot (per the spec) be used. */
static void ZEND_MODULE_GLOBALS_CTOR_N(rar)(void *arg TSRMLS_DC) /* {{{ */
{
	zend_rar_globals *rar_globals = arg;
	rar_globals->contents_cache.max_size = 5; /* TODO make configurable */
	rar_globals->contents_cache.hits = 0;
	rar_globals->contents_cache.misses = 0;
	rar_globals->contents_cache.put = _rar_contents_cache_put;
	rar_globals->contents_cache.get = _rar_contents_cache_get;
	rar_globals->contents_cache.data =
		pemalloc(sizeof *rar_globals->contents_cache.data, 1);
	zend_hash_init(rar_globals->contents_cache.data,
		rar_globals->contents_cache.max_size, NULL,
		ZVAL_PTR_DTOR, 1);
}
/* }}} */

static void ZEND_MODULE_GLOBALS_DTOR_N(rar)(void *arg TSRMLS_DC) /* {{{ */
{
	zend_rar_globals *rar_globals = arg;
	zend_hash_destroy(rar_globals->contents_cache.data);
	pefree(rar_globals->contents_cache.data, 1);
}
/* }}} */
/* }}} */

/* {{{ ZEND_MODULE_STARTUP */
ZEND_MODULE_STARTUP_D(rar)
{
	minit_rararch(TSRMLS_C);
	minit_rarentry(TSRMLS_C);
	minit_rarerror(TSRMLS_C);

	/* This doesn't work, it tries to call the destructor after the
	 * module has been unloaded. This information is in the zend_module_entry
	 * instead; that information is correctly used before the module is
	 * unloaded */
	/* ZEND_INIT_MODULE_GLOBALS(rar, ZEND_MODULE_GLOBALS_CTOR_N(rar),
		ZEND_MODULE_GLOBALS_DTOR_N(rar)); */

	php_register_url_stream_wrapper("rar", &php_stream_rar_wrapper TSRMLS_CC);

	REGISTER_LONG_CONSTANT("RAR_HOST_MSDOS",	HOST_MSDOS,	CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("RAR_HOST_OS2",		HOST_OS2,	CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("RAR_HOST_WIN32",	HOST_WIN32,	CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("RAR_HOST_UNIX",		HOST_UNIX,	CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("RAR_HOST_MACOS",	HOST_MACOS,	CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("RAR_HOST_BEOS",		HOST_BEOS,	CONST_CS | CONST_PERSISTENT);
	/* PHP < 5.3 doesn't have the PHP_MAXPATHLEN constant */
#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 3
	REGISTER_LONG_CONSTANT("RAR_MAXPATHLEN",	MAXPATHLEN,	CONST_CS | CONST_PERSISTENT);
#endif
	return SUCCESS;
}
/* }}} */

/* {{{ ZEND_MODULE_DEACTIVATE */
ZEND_MODULE_DEACTIVATE_D(rar)
{
	/* clean cache on request shutdown */
	zend_hash_clean(RAR_G(contents_cache).data);

	return SUCCESS;
}
/* }}} */

/* {{{ ZEND_MODULE_INFO */
ZEND_MODULE_INFO_D(rar)
{
	char version[256];
	char api_version[256];

	php_info_print_table_start();
	php_info_print_table_header(2, "RAR support", "enabled");
	php_info_print_table_row(2, "RAR EXT version", PHP_RAR_VERSION);

#if	RARVER_BETA != 0
	sprintf(version,"%d.%02d beta%d patch%d %d-%02d-%02d", RARVER_MAJOR,
		RARVER_MINOR, RARVER_BETA, RARVER_PATCH, RARVER_YEAR, RARVER_MONTH,
		RARVER_DAY);
#else
	sprintf(version,"%d.%02d patch%d %d-%02d-%02d", RARVER_MAJOR, RARVER_MINOR,
		RARVER_PATCH, RARVER_YEAR, RARVER_MONTH, RARVER_DAY);
#endif

	sprintf(api_version,"%d extension %d", RAR_DLL_VERSION,
		RAR_DLL_EXT_VERSION);

	php_info_print_table_row(2, "UnRAR version", version);
	php_info_print_table_row(2, "UnRAR API version", api_version);
	php_info_print_table_end();
}
/* }}} */

/* {{{ rar_module_entry
 */
zend_module_entry rar_module_entry = {
	STANDARD_MODULE_HEADER,
	"rar",
	rar_functions,
	ZEND_MODULE_STARTUP_N(rar),
	/* ZEND_MODULE_SHUTDOWN_N(rar), */
	NULL,
	/* ZEND_MODULE_ACTIVATE_N(rar), */
	NULL,
	ZEND_MODULE_DEACTIVATE_N(rar),
	ZEND_MODULE_INFO_N(rar),
	PHP_RAR_VERSION,
	ZEND_MODULE_GLOBALS(rar),
	ZEND_MODULE_GLOBALS_CTOR_N(rar),
	ZEND_MODULE_GLOBALS_DTOR_N(rar),
	NULL, /* post_deactivate_func */
	STANDARD_MODULE_PROPERTIES_EX,
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
