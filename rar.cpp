/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2004 The PHP Group                                |
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
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef PHP_WIN32
# include <math.h>
#endif

extern "C" {
#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
}

#if HAVE_RAR

#include "php_rar.h"
/* causes linking errors (multiple definitions) in functions
   that were requested inlining but were not inlined by the compiler */
/* #include "unrar/rar.hpp */
/* only these includes are necessary anyway: */
#include "unrar/raros.hpp"
/* no need to reinclude windows.h or new.h */
#define SKIP_WINDOWS_INCLUDES
#include "unrar/os.hpp"
#include "unrar/dll.hpp"
#include "unrar/version.hpp"
/* And these definitions, which should probably be moved to a header.
   These are in unrar/headers.hpp, but that header depends on several other */
enum HOST_SYSTEM {
  HOST_MSDOS=0,HOST_OS2=1,HOST_WIN32=2,HOST_UNIX=3,HOST_MACOS=4,
  HOST_BEOS=5,HOST_MAX
};

/* PHP4 compat {{{ */
#ifndef PHP_METHOD
#define ZEND_MN(name) zim_##name
#define PHP_METHOD(classname, name)    ZEND_NAMED_FUNCTION(ZEND_MN(classname##_##name))
#define ZEND_FENTRY(zend_name, name, arg_info, flags)   { #zend_name, name, arg_info},
#define PHP_ME(classname, name, arg_info, flags)   ZEND_FENTRY(name, ZEND_MN(classname##_##name), arg_info, flags)
#endif
/* }}} */


static int le_rar_file;
#define le_rar_file_name "Rar file"
static zend_class_entry *rar_class_entry_ptr;

/* {{{ internal functions protos */
static void _rar_file_list_dtor(zend_rsrc_list_entry * TSRMLS_DC);
static int _rar_list_files(rar_file_t * TSRMLS_DC);
static const char * _rar_error_to_string(int);
static int _rar_handle_error(int TSRMLS_DC);
static void _rar_dos_date_to_text(int, char *);
static void _rar_entry_to_zval(struct RARHeaderDataEx *, zval *, long TSRMLS_DC);
static int _rar_raw_entries_to_files(rar_file_t *rar,
								     const char * const file, //can be NULL
								     zval *target TSRMLS_DC);
static zval **_rar_entry_get_property(zval *, char *, int TSRMLS_DC);
/* }}} */

/* <internal> */

#define RAR_GET_PROPERTY(var, prop_name) \
	if (!entry_obj) { \
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "this method cannot be called statically"); \
		RETURN_FALSE; \
	} \
	if ((var = _rar_entry_get_property(entry_obj, prop_name, sizeof(prop_name) TSRMLS_CC)) == NULL) { \
		RETURN_FALSE; \
	}

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
	while (result == 0) {
		struct RARHeaderDataEx entry;
		result = RARReadHeaderEx(rar->arch_handle, &entry);
		//value of 2nd argument is irrelevant in RAR_OM_LIST_[SPLIT] mode
		RARProcessFile(rar->arch_handle, RAR_SKIP, NULL, NULL);
		if (result == 0) {
			rar->entries = (struct RARHeaderDataEx **) erealloc(rar->entries,
				sizeof(*rar->entries) * (rar->entry_count + 1));
			if (!rar->entries) {
				return FAILURE;
			}
			rar->entries[rar->entry_count] = (struct RARHeaderDataEx *)emalloc(
				sizeof(*rar->entries[0]));
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

static int _rar_handle_error(int errcode TSRMLS_DC) /* {{{ */
{
	const char *err = _rar_error_to_string(errcode);

	if (err == NULL) {
		return SUCCESS;
	}
	
	php_error_docref(NULL TSRMLS_CC, E_WARNING, err);
	return FAILURE;
}
/* }}} */

static void _rar_dos_date_to_text(int dos_time, char *date_string) /* {{{ */
{
	int second, minute, hour, day, month, year;
	/* following lines were taken from timefn.cpp */
	second = (dos_time & 0x1f)*2;
	minute = (dos_time>>5) & 0x3f;
	hour   = (dos_time>>11) & 0x1f;
	day    = (dos_time>>16) & 0x1f;
	month  = (dos_time>>21) & 0x0f;
	year   = (dos_time>>25)+1980;
	sprintf(date_string, "%u-%02u-%02u %02u:%02u:%02u", year, month, day, hour, minute, second);
}
/* }}} */

/* should be passed the last entry that corresponds to a given file
 * only that one has the correct CRC. Still, it may have a wrong packedSize */
static void _rar_entry_to_zval(struct RARHeaderDataEx *entry, zval *object,
							   unsigned long packed_size TSRMLS_DC) /* {{{ */
{
	char tmp_s [MAX_LENGTH_OF_LONG + 1];
	char time[50];
	long unpSize;

	if (sizeof(long) >= 8)
		unpSize = ((long) entry->UnpSize) + (((long) entry->UnpSizeHigh) << 32);
	else {
		//for 32-bit long, at least don't give negative values
		if ((unsigned long) entry->UnpSize > (unsigned long) LONG_MAX
				|| entry->UnpSizeHigh != 0)
			unpSize = LONG_MAX;
		else
			unpSize = (long) entry->UnpSize;
	}

	if (packed_size > (unsigned long) LONG_MAX)
		packed_size = LONG_MAX;
	
	add_property_string(object, "name", entry->FileName, 1);
	add_property_long(object, "unpacked_size", entry->UnpSize);
	//packed size can be wrong in multi-volume archives
	add_property_long(object, "packed_size", packed_size);
	add_property_long(object, "host_os", entry->HostOS);
	
	_rar_dos_date_to_text(entry->FileTime, time);
	add_property_string(object, "file_time", time, 1);
	
	sprintf(tmp_s, "%lx", entry->FileCRC);
	add_property_string(object, "crc", tmp_s, 1);
	
	add_property_long(object, "attr",  entry->FileAttr);
	add_property_long(object, "version",  entry->UnpVer);
	add_property_long(object, "method",  entry->Method);
}
/* }}} */

static int _rar_raw_entries_to_files(rar_file_t *rar,
									 const char * const file, //can be NULL
									 zval *target TSRMLS_DC) /* {{{ */
{
	const char * last_name = NULL;
	unsigned long packed_size = 0UL;
	RARHeaderDataEx *last_entry;
	int any_commit = FALSE;
	for (int i = 0; i <= rar->entry_count; i++) {
		RARHeaderDataEx *entry;
		const char *current_name;
		int read_entry = (i != rar->entry_count); //whether we have a new entry this iteration
		int ended_file = false; //whether we've seen a file and entries for the that file have ended
		int commit_file = false; //whether we are creating a new zval
		int has_last_entry = (i != 0); //whether we had an entry last iteration

		
		if (read_entry) {
			entry = rar->entries[i];
			current_name = entry->FileName;
		}
		
		ended_file = has_last_entry &&
			(!read_entry || (strncmp(last_name, current_name, 1024) != 0));
		commit_file = ended_file && (file == NULL ||
			(file != NULL && strncmp(last_name, file, 1024) == 0));

		if (commit_file) {
			//this entry corresponds to a new file
			zval *tmp;

			any_commit = TRUE;

			//take care of last entry
			/* if file is NULL, assume target is a zval that will hold the
			 * entry, otherwise assume it is a numerical array */
			if (file == NULL) {
				MAKE_STD_ZVAL(tmp);
			}
			else
				tmp = target;

			object_init_ex(tmp, rar_class_entry_ptr);
			add_property_resource(tmp, "rarfile", rar->id);
			zend_list_addref(rar->id);
			_rar_entry_to_zval(last_entry, tmp, packed_size TSRMLS_CC);
			if (file == NULL)
				add_next_index_zval(target, tmp);
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
			last_name = current_name;
		}
	}

	return any_commit;
}
/* }}} */

static zval **_rar_entry_get_property(zval *id, char *name, int namelen TSRMLS_DC) /* {{{ */
{
	zval **tmp;

	if (zend_hash_find(Z_OBJPROP_P(id), name, namelen, (void **)&tmp) == FAILURE) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to find property '%s'", name);
		return NULL;
	}
	return tmp;
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
	char *filename, *password = NULL;
	int filename_len, password_len = 0;
	rar_file_t *rar = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|s", &filename, &filename_len, &password, &password_len) == FAILURE) {
		return;
	}

#if PHP_MAJOR_VERSION < 6
	if (PG(safe_mode) && (!php_checkuid(filename, NULL, CHECKUID_CHECK_FILE_AND_DIR))) {
		RETURN_FALSE;
	}
#endif
	if (php_check_open_basedir(filename TSRMLS_CC)) {
		RETURN_FALSE;
	}
	
	rar = (rar_file_t *) emalloc(sizeof(rar_file_t));
	rar->list_open_data = (RAROpenArchiveDataEx *) ecalloc(1,
		sizeof *rar->list_open_data);
	rar->list_open_data->ArcName = estrndup(filename, filename_len);
	rar->list_open_data->OpenMode = RAR_OM_LIST_INCSPLIT;
	rar->list_open_data->CmtBuf = (char*) ecalloc(RAR_MAX_COMMENT_SIZE, 1);
	rar->list_open_data->CmtBufSize = RAR_MAX_COMMENT_SIZE;
	rar->extract_open_data = (RAROpenArchiveDataEx *) ecalloc(1,
		sizeof *rar->extract_open_data);
	rar->extract_open_data->ArcName = estrndup(filename, filename_len);
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
		rar->id = zend_list_insert(rar,le_rar_file);
		RETURN_RESOURCE(rar->id);
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
	int i, result;

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

	found = _rar_raw_entries_to_files(rar, filename, return_value TSRMLS_CC);
	if (!found) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
			"cannot find file \"%s\" in Rar archive \"%s\".",
			filename, rar->list_open_data->ArcName);
		RETURN_FALSE;	
	}
}
/* }}} */

/* {{{ proto string rar_comment_get(resource rarfile)
   Return comment of the rar archive */
PHP_FUNCTION(rar_comment_get)
{
	zval *file;
	rar_file_t *rar = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &file) == FAILURE) {
		return;
	}

	if (!_rar_get_file_resource(file, &rar TSRMLS_CC)) {
		RETURN_FALSE;
	}
	
	unsigned cmt_state = rar->list_open_data->CmtState;

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
	int file_counter = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &file) == FAILURE) {
		return;
	}

	if (!_rar_get_file_resource(file, &rar TSRMLS_CC)) {
		RETURN_FALSE;
	}

	zend_list_delete(rar->id);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool RarEntry::extract(string path [, string filename ])
   Extract file from the archive */
PHP_METHOD(rarentry, extract)
{
	char *path, *filename = NULL;
	int path_len, filename_len = 0;
	zval **tmp, **tmp_name;
	rar_file_t *rar = NULL;
	int result, process_result;
	zval *entry_obj = getThis();
	struct RARHeaderDataEx entry;
	HANDLE extract_handle;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|s", &path, &path_len, &filename, &filename_len) == FAILURE ) {
		return;
	}

	RAR_GET_PROPERTY(tmp, "rarfile");
	ZEND_FETCH_RESOURCE(rar, rar_file_t *, tmp, -1, "Rar file", le_rar_file);

#if PHP_MAJOR_VERSION < 6
	if (path_len && PG(safe_mode) && (!php_checkuid(path, NULL, CHECKUID_CHECK_FILE_AND_DIR))) {
		RETURN_FALSE;
	}
#endif

	if (php_check_open_basedir(path TSRMLS_CC)) {
		RETURN_FALSE;
	}
	
	if (filename_len) {
#if PHP_MAJOR_VERSION < 6
		if (PG(safe_mode) && (!php_checkuid(filename, NULL, CHECKUID_CHECK_FILE_AND_DIR))) {
			RETURN_FALSE;
		}
#endif		
		if (php_check_open_basedir(filename TSRMLS_CC)) {
			RETURN_FALSE;
		}
	}
	
	RAR_GET_PROPERTY(tmp_name, "name");

	extract_handle = RAROpenArchiveEx(rar->extract_open_data);
	
	if (rar->extract_open_data->OpenResult == 0 && extract_handle != NULL) {
		if (rar->password != NULL) {
			RARSetPassword(extract_handle, rar->password);
		}
	} else {
		_rar_handle_error(rar->extract_open_data->OpenResult TSRMLS_CC);
		RETURN_FALSE;
	}

	while ((result = RARReadHeaderEx(extract_handle, &entry)) == 0) {
		if (strncmp(entry.FileName,Z_STRVAL_PP(tmp_name), sizeof(entry.FileName)) == 0) {
			process_result = RARProcessFile(extract_handle, RAR_EXTRACT, path, filename);
			RETVAL_TRUE;
			goto cleanup;
		} else {
			process_result = RARProcessFile(extract_handle, RAR_SKIP, NULL, NULL);
		}
		if (_rar_handle_error(process_result TSRMLS_CC) == FAILURE) {
			RETVAL_FALSE;
			goto cleanup;
		}
	}

	if (_rar_handle_error(result TSRMLS_CC) == FAILURE) {
		RETVAL_FALSE;
		goto cleanup;
	}

	php_error_docref(NULL TSRMLS_CC, E_WARNING,
		"Can't find file %s in archive %s", Z_STRVAL_PP(tmp_name),
		rar->extract_open_data->ArcName);
	RETVAL_FALSE;

cleanup:
	RARCloseArchive(extract_handle);
}
/* }}} */

/* {{{ proto string RarEntry::getName()
   Return entry name */
PHP_METHOD(rarentry, getName)
{
	zval **tmp;
	rar_file_t *rar = NULL;
	zval *entry_obj = getThis();
	
	RAR_GET_PROPERTY(tmp, "name");

	convert_to_string_ex(tmp);
	RETURN_STRINGL(Z_STRVAL_PP(tmp), Z_STRLEN_PP(tmp), 1);
}
/* }}} */

/* {{{ proto int RarEntry::getUnpackedSize()
   Return unpacked size of the entry */
PHP_METHOD(rarentry, getUnpackedSize)
{
	zval **tmp;
	rar_file_t *rar = NULL;
	zval *entry_obj = getThis();
	
	RAR_GET_PROPERTY(tmp, "unpacked_size");

	convert_to_long_ex(tmp);
	RETURN_LONG(Z_LVAL_PP(tmp));
}
/* }}} */

/* {{{ proto int RarEntry::getPackedSize()
   Return packed size of the entry */
PHP_METHOD(rarentry, getPackedSize)
{
	zval **tmp;
	rar_file_t *rar = NULL;
	zval *entry_obj = getThis();
	
	RAR_GET_PROPERTY(tmp, "packed_size");

	convert_to_long_ex(tmp);
	RETURN_LONG(Z_LVAL_PP(tmp));
}
/* }}} */

/* {{{ proto int RarEntry::getHostOs()
   Return host OS of the entry */
PHP_METHOD(rarentry, getHostOs)
{
	zval **tmp;
	rar_file_t *rar = NULL;
	zval *entry_obj = getThis();
	
	RAR_GET_PROPERTY(tmp, "host_os");

	convert_to_long_ex(tmp);
	RETURN_LONG(Z_LVAL_PP(tmp));
}
/* }}} */

/* {{{ proto string RarEntry::getFileTime()
   Return modification time of the entry */
PHP_METHOD(rarentry, getFileTime)
{
	zval **tmp;
	rar_file_t *rar = NULL;
	zval *entry_obj = getThis();
	
	RAR_GET_PROPERTY(tmp, "file_time");

	convert_to_string_ex(tmp);
	RETURN_STRINGL(Z_STRVAL_PP(tmp), Z_STRLEN_PP(tmp), 1);
}
/* }}} */

/* {{{ proto string RarEntry::getCrc()
   Return CRC of the entry */
PHP_METHOD(rarentry, getCrc)
{
	zval **tmp;
	rar_file_t *rar = NULL;
	zval *entry_obj = getThis();
	
	RAR_GET_PROPERTY(tmp, "crc");

	convert_to_string_ex(tmp);
	RETURN_STRINGL(Z_STRVAL_PP(tmp), Z_STRLEN_PP(tmp), 1);
}
/* }}} */

/* {{{ proto int RarEntry::getAttr()
   Return attributes of the entry */
PHP_METHOD(rarentry, getAttr)
{
	zval **tmp;
	rar_file_t *rar = NULL;
	zval *entry_obj = getThis();
	
	RAR_GET_PROPERTY(tmp, "attr");

	convert_to_long_ex(tmp);
	RETURN_LONG(Z_LVAL_PP(tmp));
}
/* }}} */

/* {{{ proto int RarEntry::getVersion()
   Return version of the archiver, used to create this entry */
PHP_METHOD(rarentry, getVersion)
{
	zval **tmp;
	rar_file_t *rar = NULL;
	zval *entry_obj = getThis();
	
	RAR_GET_PROPERTY(tmp, "version");

	convert_to_long_ex(tmp);
	RETURN_LONG(Z_LVAL_PP(tmp));
}
/* }}} */

/* {{{ proto int RarEntry::getMethod()
   Return packing method */
PHP_METHOD(rarentry, getMethod)
{
	zval **tmp;
	rar_file_t *rar = NULL;
	zval *entry_obj = getThis();
	
	RAR_GET_PROPERTY(tmp, "method");

	convert_to_long_ex(tmp);
	RETURN_LONG(Z_LVAL_PP(tmp));
}
/* }}} */

/* {{{ rar_functions[]
 *
 */
function_entry rar_functions[] = {
	PHP_FE(rar_open,	NULL)
	PHP_FE(rar_list,	NULL)
	PHP_FE(rar_entry_get,	NULL)
	PHP_FE(rar_comment_get,	NULL)
	PHP_FE(rar_close,	NULL)
	{NULL, NULL, NULL}
};

#ifndef ZEND_ENGINE_2
#define ZEND_ACC_PUBLIC 0
#endif

static zend_function_entry php_rar_class_functions[] = {
	PHP_ME(rarentry,		extract,			NULL,	ZEND_ACC_PUBLIC)
	PHP_ME(rarentry,		getName,			NULL,	ZEND_ACC_PUBLIC)
	PHP_ME(rarentry,		getUnpackedSize,	NULL,	ZEND_ACC_PUBLIC)
	PHP_ME(rarentry,		getPackedSize,		NULL,	ZEND_ACC_PUBLIC)
	PHP_ME(rarentry,		getHostOs,			NULL,	ZEND_ACC_PUBLIC)
	PHP_ME(rarentry,		getFileTime,		NULL,	ZEND_ACC_PUBLIC)
	PHP_ME(rarentry,		getCrc,				NULL,	ZEND_ACC_PUBLIC)
	PHP_ME(rarentry,		getAttr,			NULL,	ZEND_ACC_PUBLIC)
	PHP_ME(rarentry,		getVersion,			NULL,	ZEND_ACC_PUBLIC)
	PHP_ME(rarentry,		getMethod,			NULL,	ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(rar)
{
	zend_class_entry rar_class_entry;	
	INIT_CLASS_ENTRY(rar_class_entry, "RarEntry", php_rar_class_functions);
	rar_class_entry_ptr = zend_register_internal_class(&rar_class_entry TSRMLS_CC);	

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
	php_info_print_table_header(2, "Extension version", PHP_RAR_VERSION);
	php_info_print_table_row(2, "Revision", "$Revision$");

	if (RARVER_BETA != 0) {
		sprintf(version,"%d.%02d beta%d", RARVER_MAJOR, RARVER_MINOR, RARVER_BETA);
	} else {
		sprintf(version,"%d.%02d", RARVER_MAJOR, RARVER_MINOR);
	}

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
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
