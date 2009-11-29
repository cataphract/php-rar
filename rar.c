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

int le_rar_file;
char *le_rar_file_name = "Rar file";

/* {{{ internal functions protos */
static void _rar_file_list_dtor(zend_rsrc_list_entry * TSRMLS_DC);
static int _rar_list_files(rar_file_t * TSRMLS_DC);
static const char * _rar_error_to_string(int errcode);
static int _rar_raw_entries_to_files(rar_file_t *rar,
								     const wchar_t * const file, //can be NULL
								     zval *target TSRMLS_DC);
static void _rar_fix_wide(wchar_t *str);
/* }}} */

/* <global> */
/* Functions needed in other files */
int _rar_handle_error(int errcode TSRMLS_DC) /* {{{ */
{
	const char *err = _rar_error_to_string(errcode);

	if (err == NULL) {
		return SUCCESS;
	}
	
	php_error_docref(NULL TSRMLS_CC, E_WARNING, err);
	return FAILURE;
}

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

/* Missing functions from VC6 {{{ */
#if !HAVE_STRNLEN
static size_t strnlen(const char *s, size_t maxlen) /* {{{ */
{
	char *r = memchr(s, '\0', maxlen);
	return r ? r-s : maxlen;
}
/* }}} */
#endif
/* }}} */
/* }}} */

/* WARNING: It's the caller who must close the archive.
 * Kind of against the conventions */
int _rar_find_file(struct RAROpenArchiveDataEx *open_data, /* IN */
				   const char *const utf_file_name, /* IN */
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
/* <internal> */

static void _rar_file_list_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC) /* {{{ */
{
	rar_file_t *rar = (rar_file_t *)rsrc->ptr;
	int i = 0;
	if (rar->arch_handle) {
		RARCloseArchive(rar->arch_handle);
	}
	if (rar->password) {
		efree(rar->password);
	}
	if (rar->entries && rar->entry_count) {
		for(i = 0; i < rar->entry_count; i++) {
			efree(rar->entries[i]);
		}
		efree(rar->entries);
		rar->entry_count = 0;
	}
	efree(rar->list_open_data->ArcName);
	efree(rar->list_open_data->CmtBuf);
	efree(rar->list_open_data);
	efree(rar->extract_open_data->ArcName);
	efree(rar->extract_open_data);
	efree(rar);
}
/* }}} */

static int _rar_get_file_resource(zval *zval_file, rar_file_t **rar_file TSRMLS_DC) /* {{{ */
{
	*rar_file = (rar_file_t *) zend_fetch_resource(&zval_file TSRMLS_CC, -1, le_rar_file_name, NULL, 1, le_rar_file);

	if (*rar_file) {
		return 1;
	}
	return 0;
}
/* }}} */

static int _rar_list_files(rar_file_t *rar TSRMLS_DC) /* {{{ */
{
	int result = 0;
	int capacity = 0;
	while (result == 0) {
		struct RARHeaderDataEx entry;
		result = RARReadHeaderEx(rar->arch_handle, &entry);
		//value of 2nd argument is irrelevant in RAR_OM_LIST_[SPLIT] mode
		RARProcessFile(rar->arch_handle, RAR_SKIP, NULL, NULL);
		if (result == 0) {
			assert(capacity >= rar->entry_count);
			if (capacity == rar->entry_count) { //0, 2, 6, 14, 30...
				capacity = (capacity + 1) * 2;
				rar->entries = erealloc(rar->entries,
					sizeof(*rar->entries) * capacity);
				if (!rar->entries)
					return FAILURE;
			}
			assert(capacity > rar->entry_count);
			rar->entries[rar->entry_count] = emalloc(sizeof(*rar->entries[0]));
			memcpy(rar->entries[rar->entry_count], &entry,
				sizeof *rar->entries[0]);
			rar->entry_count++;
		}
	}
	return result;
}
/* }}} */
//returns a string or NULL if not an error
static const char * _rar_error_to_string(int errcode) /* {{{ */
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
		default:
			ret = "unknown RAR error (should not happen)";
			break;
	}
	return ret;
}
/* }}} */

static int _rar_raw_entries_to_files(rar_file_t *rar,
									 const wchar_t * const file, //can be NULL
									 zval *target TSRMLS_DC) /* {{{ */
{
	wchar_t last_name[1024] = {L'\0'};
	char strict_last_name[1024] = {'\0'};
	unsigned long packed_size = 0UL;
	struct RARHeaderDataEx *last_entry = NULL;
	int any_commit = FALSE;
	int first_file_check = TRUE;
	int i;

	for (i = 0; i <= rar->entry_count; i++) {
		struct RARHeaderDataEx *entry;
		const wchar_t *current_name;
		const char *strict_current_name;
		int read_entry = (i != rar->entry_count); //whether we have a new entry this iteration
		int ended_file = FALSE; //whether we've seen a file and entries for the that file have ended
		int commit_file = FALSE; //whether we are creating a new zval
		int has_last_entry = (*strict_last_name != '\0'); //whether we had an entry last iteration
		
		if (read_entry) {
			entry = rar->entries[i];
			current_name = entry->FileNameW;
			strict_current_name = entry->FileName;

			/* If file is continued from previous volume, skip it, as otherwise
			 * incorrect packed and unpacked sizes would be returned */
			if (first_file_check) {
				if (entry->Flags & 0x01)
					continue;
				else
					first_file_check = FALSE;
			}
		}
		
		/* The wide file name may result from conversion from the
		 * non-wide filename and this conversion may fail. In that
		 * case, we can have entries of different files with the
		 * the same wide file name. For this reason, we use the
		 * non-wide file name to check if we have a new file and
		 * don't trust the wide file name. */
		ended_file = has_last_entry && (!read_entry ||
			(strncmp(strict_last_name, strict_current_name, 1024) != 0));
		commit_file = ended_file && (file == NULL ||
			(file != NULL && wcsncmp(last_name, file, 1024) == 0));

		if (commit_file) { //this entry corresponds to a new file
			zval *entry_obj,
				 *rar_res;

			/* guaranteed by commit_file = ended_file &&...
			 * with ended_file = has_last_entry && ...
			 * with has_last_entry = (last_name != NULL)
			 * and last_entry is set when last_name is set */
			assert(last_entry != NULL);

			any_commit = TRUE; //at least one commit done

			//take care of last entry
			/* if file is NULL, assume target is a zval that will hold the
			 * entry, otherwise assume it is a numerical array */
			if (file == NULL) {
				MAKE_STD_ZVAL(entry_obj);
			}
			else
				entry_obj = target;

			object_init_ex(entry_obj, rar_class_entry_ptr);
			//add_property_resource(entry_obj, "rarfile", rar->id);
			MAKE_STD_ZVAL(rar_res);
			ZVAL_RESOURCE(rar_res, rar->id);
			zend_update_property(rar_class_entry_ptr, entry_obj, "rarfile",
				sizeof("rarfile") - 1, rar_res TSRMLS_CC);
			/* zend_update_property calls write_property handler, which
			 * increments the refcount. We must decrease it here */
			zval_ptr_dtor(&rar_res);
			/* to avoid destruction of the resource due to le->refcount hitting
			 * 0 when this new resource zval we created is destroyed? */
			zend_list_addref(rar->id);
			_rar_entry_to_zval(last_entry, entry_obj, packed_size TSRMLS_CC);
			if (file == NULL)
				add_next_index_zval(target, entry_obj);
		}

		if (ended_file) {
			packed_size = 0UL; //reset counter
		}

		if (read_entry) { //sum packed size of current entry
			//we would exceed size of ulong. cap at ulong_max
			if (ULONG_MAX - packed_size < entry->PackSize)
				packed_size = ULONG_MAX;
			else {
				packed_size += entry->PackSize;
				if (entry->UnpSizeHigh != 0) {
					if (sizeof(unsigned long) >= 8) {
						packed_size += ((unsigned long) entry->UnpSizeHigh) << 32;
					}
					else {
						packed_size = ULONG_MAX; //cap
					}
				}
			}

			//prepare for next entry
			last_entry = entry;
			wcsncpy(last_name, current_name, 1024);
			strncpy(strict_last_name, strict_current_name, 1024);
		}
	}

	return any_commit;
}
/* }}} */

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
/* </internal> */

#ifdef COMPILE_DL_RAR
BEGIN_EXTERN_C()
ZEND_GET_MODULE(rar)
END_EXTERN_C()
#endif

/* module functions */

/* {{{ proto resource rar_open(string filename [, string password])
   Open Rar archive and return resource */
PHP_FUNCTION(rar_open)
{
	char *filename;
	char *password = NULL;
	char resolved_path[MAXPATHLEN];
	int filename_len;
	int password_len = 0;
	rar_file_t *rar = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|s", &filename, &filename_len, &password, &password_len) == FAILURE) {
		return;
	}

	if (OPENBASEDIR_CHECKPATH(filename)) {
		RETURN_FALSE;
	}

	if (!expand_filepath(filename, resolved_path TSRMLS_CC)) {
		RETURN_FALSE;
	}
	
	rar = emalloc(sizeof *rar);
	rar->list_open_data = ecalloc(1, sizeof *rar->list_open_data);
	rar->list_open_data->ArcName = estrndup(resolved_path,
		strnlen(resolved_path, MAXPATHLEN));
	rar->list_open_data->OpenMode = RAR_OM_LIST_INCSPLIT;
	rar->list_open_data->CmtBuf = ecalloc(RAR_MAX_COMMENT_SIZE, 1);
	rar->list_open_data->CmtBufSize = RAR_MAX_COMMENT_SIZE;
	rar->extract_open_data = ecalloc(1, sizeof *rar->extract_open_data);
	rar->extract_open_data->ArcName = estrndup(resolved_path,
		strnlen(resolved_path, MAXPATHLEN));
	rar->extract_open_data->OpenMode = RAR_OM_EXTRACT;
	rar->extract_open_data->CmtBuf = NULL; //not interested in it again
	rar->password = NULL;
	rar->entries = NULL;
	rar->entry_count = 0;

	rar->arch_handle = RAROpenArchiveEx(rar->list_open_data);
	if (rar->arch_handle != NULL && rar->list_open_data->OpenResult == 0) {
		if (password_len) {
			rar->password = estrndup(password, password_len);
		}
		rar->id = ZEND_REGISTER_RESOURCE(return_value, rar, le_rar_file);
		return;
		//zend_list_insert(rar,le_rar_file);
		//RETURN_RESOURCE(rar->id);
	} else {
		const char *err_str = _rar_error_to_string(rar->list_open_data->OpenResult);
		if (err_str == NULL)
			err_str = "Unrar lib did not return an error. Should not happen.";
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to open %s: %s",
			filename, err_str);
		efree(rar->list_open_data->ArcName);
		efree(rar->list_open_data->CmtBuf);
		efree(rar->list_open_data);
		efree(rar->extract_open_data->ArcName);
		efree(rar->extract_open_data);
		efree(rar);
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto array rar_list(resource rarfile)
   Return entries from the rar archive */
PHP_FUNCTION(rar_list)
{
	zval *file;
	rar_file_t *rar = NULL;
	int result;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &file) == FAILURE) {
		return;
	}

	if (!_rar_get_file_resource(file, &rar TSRMLS_CC)) {
		RETURN_FALSE;
	}

	if (rar->entries == NULL) {
		result = _rar_list_files(rar TSRMLS_CC); 
		if (_rar_handle_error(result TSRMLS_CC) == FAILURE) {
			RETURN_FALSE;
		}
	}
	
	array_init(return_value);
	
	_rar_raw_entries_to_files(rar, NULL, return_value TSRMLS_CC);
}
/* }}} */

/* {{{ proto object rar_entry_get(resource rarfile, string filename)
   Return entry from the rar archive */
PHP_FUNCTION(rar_entry_get)
{
	zval *file;
	char *filename;
	rar_file_t *rar = NULL;
	int result;
	int found;
	int filename_len;
	wchar_t *filename_c = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &file, &filename, &filename_len) == FAILURE) {
		return;
	}

	if (!_rar_get_file_resource(file, &rar TSRMLS_CC)) {
		RETURN_FALSE;
	}

	if (rar->entries == NULL) {
		result = _rar_list_files(rar TSRMLS_CC); 
		if (_rar_handle_error(result TSRMLS_CC) == FAILURE) {
			RETURN_FALSE;
		}
	}

	filename_c = ecalloc(filename_len + 1, sizeof *filename_c); 
	_rar_utf_to_wide(filename, filename_c, filename_len + 1);

	found = _rar_raw_entries_to_files(rar, filename_c, return_value TSRMLS_CC);
	if (!found) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
			"cannot find file \"%s\" in Rar archive \"%s\".",
			filename, rar->list_open_data->ArcName);
		RETVAL_FALSE;	
	}
	
	efree(filename_c);
}
/* }}} */

/* {{{ proto string rar_comment_get(resource rarfile)
   Return comment of the rar archive */
PHP_FUNCTION(rar_comment_get)
{
	zval *file;
	rar_file_t *rar = NULL;
	unsigned cmt_state;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &file) == FAILURE) {
		return;
	}

	if (!_rar_get_file_resource(file, &rar TSRMLS_CC)) {
		RETURN_FALSE;
	}
	
	cmt_state = rar->list_open_data->CmtState;

	if (_rar_handle_error(cmt_state TSRMLS_CC) == FAILURE)
		RETURN_FALSE;

	if (cmt_state == 0) //comment not present
		RETURN_NULL(); //oddly, requires ()

	if (cmt_state == 1) { //comment read completely
		//CmtSize - 1 because we don't need the null terminator
		RETURN_STRINGL(rar->list_open_data->CmtBuf,
			rar->list_open_data->CmtSize - 1, 1);
	}
}
/* }}} */

/* {{{ proto bool rar_close(resource rarfile)
   Close Rar archive and free all resources */
PHP_FUNCTION(rar_close)
{
	zval *file;
	rar_file_t *rar = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &file) == FAILURE) {
		return;
	}

	if (!_rar_get_file_resource(file, &rar TSRMLS_CC)) {
		RETURN_FALSE;
	}

	zend_hash_index_del(&EG(regular_list), Z_RESVAL_P(file));
	RETURN_TRUE;
}
/* }}} */

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

ZEND_BEGIN_ARG_INFO_EX(arginfo_rarentry_extract, 0, 0, 1)
	ZEND_ARG_INFO(0, path)
	ZEND_ARG_INFO(0, filename)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_rar_void, 0)
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
	minit_rarentry(TSRMLS_C);

	le_rar_file = zend_register_list_destructors_ex(_rar_file_list_dtor, NULL, le_rar_file_name, module_number);
	
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
