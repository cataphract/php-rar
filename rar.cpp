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
  | It's license states, that you MUST NOT use it's code to develop      |
  | a RAR (WinRAR) compatible archiver.                                  |
  | Please, read unRAR license for full information.                     |
  | unRAR & RAR copyrights are owned by Eugene Roshal                    |
  +----------------------------------------------------------------------+
  | Author: Antony Dovgal <tony2001@phpclub.net>                         |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"

#if HAVE_RAR
extern "C" {
#include "php_rar.h"
}

#include "unrar/rar.hpp"

static int le_rar_file;
#define le_rar_file_name "Rar"
static zend_class_entry *rar_class_entry_ptr;

/* {{{ rar_functions[]
 *
 */
function_entry rar_functions[] = {
	PHP_FE(rar_open,	NULL)
	PHP_FE(rar_list,	NULL)
	PHP_FE(rar_entry_get,	NULL)
	PHP_FE(rar_close,	NULL)
	{NULL, NULL, NULL}
};

static zend_function_entry php_rar_class_functions[] = {
	PHP_ME(rarentry,		extract,			NULL,	0)
	PHP_ME(rarentry,		getName,			NULL,	0)
	PHP_ME(rarentry,		getUnpackedSize,	NULL,	0)
	PHP_ME(rarentry,		getPackedSize,		NULL,	0)
	PHP_ME(rarentry,		getHostOs,			NULL,	0)
	PHP_ME(rarentry,		getFileTime,		NULL,	0)
	PHP_ME(rarentry,		getCrc,				NULL,	0)
	PHP_ME(rarentry,		getAttr,			NULL,	0)
	PHP_ME(rarentry,		getVersion,			NULL,	0)
	PHP_ME(rarentry,		getMethod,			NULL,	0)
	{NULL, NULL, NULL}
};
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
	PHP_MSHUTDOWN(rar),
	PHP_RINIT(rar),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(rar),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(rar),
#if ZEND_MODULE_API_NO >= 20010901
	"0.1", /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

/* {{{ internal functions protos */
static void _rar_file_list_dtor(zend_rsrc_list_entry * TSRMLS_DC);
static int _rar_list_files(rar_file_t * TSRMLS_DC);
static int _rar_handle_error(int TSRMLS_DC);
static void _rar_dos_date_to_text(int, char *);
static void _rar_entry_to_zval(struct RARHeaderData *, zval * TSRMLS_DC);
static zval **_rar_entry_get_property(zval *, char *, int TSRMLS_DC);
/* }}} */

/* <internal> */

static void _rar_file_list_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC) /* {{{ */
{
	rar_file_t *rar = (rar_file_t *)rsrc->ptr;
	int i = 0;
	if (rar->list_data) {
		RARCloseArchive(rar->list_data);
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
	efree(rar->list_handle->ArcName);
	efree(rar->list_handle);
	efree(rar->extract_handle->ArcName);
	efree(rar->extract_handle);
	efree(rar);
}
/* }}} */

static int _rar_get_file_resource(zval **zval_file, rar_file_t **rar_file TSRMLS_DC) /* {{{ */
{
	*rar_file = (rar_file_t *) zend_fetch_resource(zval_file TSRMLS_CC, -1, le_rar_file_name, NULL, 1, le_rar_file);

	if (*rar_file) {
		return 1;
	}
	php_error_docref(NULL TSRMLS_CC, E_WARNING, "cannot find Rar file resource");
	return 0;
}
/* }}} */

static int _rar_list_files(rar_file_t *rar TSRMLS_DC) /* {{{ */
{
	int result = 0;
	while (result == 0) {
		struct RARHeaderData entry;
		result = RARReadHeader(rar->list_data, &entry);
		RARProcessFile(rar->list_data, RAR_OM_LIST, NULL, NULL);
		if (result == 0) {
			rar->entries = (struct RARHeaderData **)erealloc(rar->entries, sizeof(struct RARHeaderData *) * (rar->entry_count + 1));
			if (!rar->entries) {
				return FAILURE;
			}
			rar->entries[rar->entry_count] = (struct RARHeaderData *)emalloc(sizeof(struct RARHeaderData));
			memcpy(rar->entries[rar->entry_count], &entry, sizeof(struct RARHeaderData));
			rar->entry_count++;
		}
	}
	return result;
}
/* }}} */

static int _rar_handle_error(int errcode TSRMLS_DC) /* {{{ */
{
	switch (errcode) {
		case 0:
			/* no error */
			return SUCCESS;
			break;
		case ERAR_END_ARCHIVE:
			/* no error */
			return SUCCESS;
			break;
		case ERAR_NO_MEMORY:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "ERAR_NO_MEMORY: not enough memory");
			break;
		case ERAR_BAD_DATA:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "ERAR_BAD_DATA");
			break;
		case ERAR_BAD_ARCHIVE:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "ERAR_BAD_ARCHIVE");
			break;
		case ERAR_UNKNOWN_FORMAT:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "ERAR_UNKNOWN_FORMAT");
			break;
		case ERAR_EOPEN:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "ERAR_EOPEN");
			break;
		case ERAR_ECREATE:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "ERAR_ECREATE");
			break;
		case ERAR_ECLOSE:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "ERAR_ECLOSE");
			break;
		case ERAR_EREAD:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "ERAR_EREAD");
			break;
		case ERAR_EWRITE:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "ERAR_EWRITE");
			break;
		case ERAR_SMALL_BUF:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "ERAR_SMALL_BUF");
			break;
		case ERAR_UNKNOWN:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "ERAR_UNKNOWN: unknown RAR error");
			break;
		default:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "unknown RAR error");
			break;
	}
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

static void _rar_entry_to_zval(struct RARHeaderData *entry, zval *object TSRMLS_DC) /* {{{ */
{
	char tmp_s [MAX_LENGTH_OF_LONG + 1];
	char time[50];
	
	add_property_string(object, "name", entry->FileName, 1);
	add_property_long(object, "unpacked_size", entry->UnpSize);
	add_property_long(object, "packed_size", entry->PackSize);
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

static zval **_rar_entry_get_property(zval *id, char *name, int namelen TSRMLS_DC) /* {{{ */
{
	zval **tmp;

	if (zend_hash_find(Z_OBJPROP_P(id), name, namelen, (void **)&tmp) == FAILURE) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to find property %s", name);
		return NULL;
	}
	return tmp;
}
/* }}} */

/* </internal> */

#ifdef COMPILE_DL_RAR
extern "C" {
ZEND_GET_MODULE(rar)
}
#endif

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

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(rar)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(rar)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(rar)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(rar)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "Rar support", "enabled");
	php_info_print_table_row(2, "Revision", "$Revision$");
	php_info_print_table_end();
}
/* }}} */

/* module functions */

/* {{{ proto rar_open(string filename [, string password])
   Open Rar archive and return resource */
PHP_FUNCTION(rar_open)
{
	zval **filename, **password;
	rar_file_t *rar = NULL;
	struct RARHeaderData entry;
	int ac = ZEND_NUM_ARGS(), result = 0;

	if (ac < 1 || ac > 2 || zend_get_parameters_ex(ac, &filename, &password) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_string_ex(filename);

	if (PG(safe_mode) && (!php_checkuid(Z_STRVAL_PP(filename), NULL, CHECKUID_CHECK_FILE_AND_DIR))) {
		RETURN_FALSE;
	}
	
	if (php_check_open_basedir(Z_STRVAL_PP(filename) TSRMLS_CC)) {
		RETURN_FALSE;
	}
	
	rar = (rar_file_t *) emalloc(sizeof(rar_file_t));
	rar->list_handle = (RAROpenArchiveData *) emalloc(sizeof(RAROpenArchiveData));
	rar->list_handle->ArcName = estrndup(Z_STRVAL_PP(filename), Z_STRLEN_PP(filename));
	rar->list_handle->OpenMode = RAR_OM_LIST;
	rar->extract_handle = (RAROpenArchiveData *) emalloc(sizeof(RAROpenArchiveData));
	rar->extract_handle->ArcName = estrndup(Z_STRVAL_PP(filename), Z_STRLEN_PP(filename));
	rar->extract_handle->OpenMode = RAR_OM_EXTRACT;
	rar->password = NULL;
	rar->entries = NULL;
	rar->entry_count = 0;

	rar->list_data = RAROpenArchive(rar->list_handle);
	if (rar->list_data != NULL && rar->list_handle->OpenResult == 0) {
		if (ac == 2) {
			rar->password = estrndup(Z_STRVAL_PP(password), Z_STRLEN_PP(password));
		}
		rar->id = zend_list_insert(rar,le_rar_file);
		RETURN_RESOURCE(rar->id);
	}
	else {
		efree(rar->list_handle->ArcName);
		efree(rar->list_handle);
		efree(rar->extract_handle->ArcName);
		efree(rar->extract_handle);
		efree(rar);
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto rar_list(resource rarfile)
   Return entries from the rar archive */
PHP_FUNCTION(rar_list)
{
	zval **file;
	rar_file_t *rar = NULL;
	int i = 0;
	int ac = ZEND_NUM_ARGS(), result = 0;

	if (ac < 1 || ac > 1 || zend_get_parameters_ex(ac, &file) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	if (!_rar_get_file_resource(file,&rar TSRMLS_CC)) {
		RETURN_FALSE;
	}

	if (rar->entries == NULL) {
		result = _rar_list_files(rar TSRMLS_CC); 
		if (_rar_handle_error(result TSRMLS_CC) == FAILURE) {
			RETURN_FALSE;
		}
	}
	
	array_init(return_value);
	
	for (i = 0; i < rar->entry_count; i++) {
		zval *tmp;
		
		MAKE_STD_ZVAL(tmp);
		object_init_ex(tmp, rar_class_entry_ptr);

		add_property_resource(tmp, "rarfile", rar->id);
		_rar_entry_to_zval(rar->entries[i], tmp TSRMLS_CC);
		
		zend_hash_next_index_insert(Z_ARRVAL_P(return_value), &tmp, sizeof(zval*), NULL);
	}
	if (!return_value) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "failed to list files from RAR archive");
		RETURN_FALSE;	
	}
}
/* }}} */

/* {{{ proto rar_entry_get(resource rarfile, string filename)
   Return entry from the rar archive */
PHP_FUNCTION(rar_entry_get)
{
	zval **file, **filename;
	rar_file_t *rar = NULL;
	int result = 0, i = 0;
	int ac = ZEND_NUM_ARGS();

	if (ac < 2 || ac > 2 || zend_get_parameters_ex(ac, &file, &filename) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	if (!_rar_get_file_resource(file,&rar TSRMLS_CC)) {
		RETURN_FALSE;
	}

	if (rar->entries == NULL) {
		result = _rar_list_files(rar TSRMLS_CC); 
		if (_rar_handle_error(result TSRMLS_CC) == FAILURE) {
			RETURN_FALSE;
		}
	}

	for(i = 0; i < rar->entry_count; i++){
		if (strcmp(rar->entries[i]->FileName, Z_STRVAL_PP(filename)) == 0) {
			object_init_ex(return_value, rar_class_entry_ptr);
			add_property_resource(return_value, "rarfile", rar->id);
			_rar_entry_to_zval(rar->entries[i], return_value TSRMLS_CC);
			break;
		}		
	}
	if (!return_value) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "cannot find such file in RAR archive");
		RETURN_FALSE;	
	}
}
/* }}} */

/* {{{ proto rar_close(resource rarfile)
   Close Rar archive and free all resources */
PHP_FUNCTION(rar_close)
{
	zval **file;
	rar_file_t *rar = NULL;
	int file_counter = 0;
	int ac = ZEND_NUM_ARGS();

	if (ac < 1 || ac > 1 || zend_get_parameters_ex(ac, &file) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	if (!_rar_get_file_resource(file,&rar TSRMLS_CC)) {
		RETURN_FALSE;
	}

	zend_list_delete(rar->id);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto RarEntry::extract(string path [, string filename ]);
   Extract file from the archive */
PHP_METHOD(rarentry, extract)
{
	zval **rarfile, **path, **filename, **tmp, **tmp_name;
	rar_file_t *rar = NULL;
	char filename_str[260];
	char *path_str = NULL, *extract_to_file = NULL;
	int ac = ZEND_NUM_ARGS(), resource_type = 0, result = 0, process_result = 0;
	zval *entry_obj = getThis();
	unsigned long data_len;
	struct RARHeaderData entry;
	void *extract_data;
	
	if (ac < 1 || ac > 2 || zend_get_parameters_ex(ac, &path, &filename) == FAILURE ) {
		WRONG_PARAM_COUNT;
	}

	if ((tmp = _rar_entry_get_property(entry_obj, "rarfile", sizeof("rarfile") TSRMLS_CC)) == NULL) {
		RETURN_FALSE;
	}

	rar = (rar_file_t *) zend_list_find(Z_LVAL_PP(tmp), &resource_type);

	convert_to_string_ex(path);
	path_str = Z_STRVAL_PP(path);

	if (Z_STRLEN_PP(path) && PG(safe_mode) && (!php_checkuid(Z_STRVAL_PP(path), NULL, CHECKUID_CHECK_FILE_AND_DIR))) {
		RETURN_FALSE;
	}
	
	if (Z_STRLEN_PP(path) && php_check_open_basedir(Z_STRVAL_PP(path) TSRMLS_CC)) {
		RETURN_FALSE;
	}
	
	if (ac == 2) {
		convert_to_string_ex(filename);
		extract_to_file = Z_STRVAL_PP(filename);

		if (PG(safe_mode) && (!php_checkuid(Z_STRVAL_PP(filename), NULL, CHECKUID_CHECK_FILE_AND_DIR))) {
			RETURN_FALSE;
		}
		
		if (php_check_open_basedir(Z_STRVAL_PP(filename) TSRMLS_CC)) {
			RETURN_FALSE;
		}
	}
	
	if ((tmp_name = _rar_entry_get_property(entry_obj, "name", sizeof("name") TSRMLS_CC)) == NULL) {
		RETURN_FALSE;
	}

	if (!rar || resource_type != le_rar_file) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Rar resource identifier not found");
		RETURN_FALSE;
	}

	extract_data = RAROpenArchive(rar->extract_handle);
	
	if (rar->extract_handle->OpenResult == 0 && extract_data != NULL) {
		if (rar->password) {
			RARSetPassword(extract_data, rar->password);
		}
	}
	else {
		_rar_handle_error(rar->extract_handle->OpenResult TSRMLS_CC);
		RETURN_FALSE;
	}

	while ((result = RARReadHeader(extract_data, &entry)) == 0) {
		if (strncmp(entry.FileName,Z_STRVAL_PP(tmp_name), sizeof(entry.FileName)) == 0) {
			process_result = RARProcessFile(extract_data, RAR_EXTRACT, path_str, extract_to_file);
			RETURN_TRUE;
		}
		else {
			process_result = RARProcessFile(extract_data, RAR_SKIP, NULL, NULL);
		}
		if (_rar_handle_error(process_result TSRMLS_CC) == FAILURE) {
			RETURN_FALSE;
		}
	}

	if (_rar_handle_error(result TSRMLS_CC) == FAILURE) {
		RETURN_FALSE;
	}

	php_error_docref(NULL TSRMLS_CC, E_WARNING, "Can't find file %s in archive %s", Z_STRVAL_PP(tmp_name), rar->extract_handle->ArcName);
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto RarEntry::getName();
   Return entry name */
PHP_METHOD(rarentry, getName)
{
	zval **rarfile, **tmp;
	rar_file_t *rar = NULL;
	zval *entry_obj = getThis();
	
	if ((tmp = _rar_entry_get_property(entry_obj, "name", sizeof("name") TSRMLS_CC)) == NULL) {
		RETURN_FALSE;
	}

	convert_to_string_ex(tmp);
	RETURN_STRINGL(Z_STRVAL_PP(tmp), Z_STRLEN_PP(tmp), 1);
}
/* }}} */

/* {{{ proto RarEntry::getUnpackedSize();
   Return unpacked size of the entry */
PHP_METHOD(rarentry, getUnpackedSize)
{
	zval **rarfile, **tmp;
	rar_file_t *rar = NULL;
	zval *entry_obj = getThis();
	
	if ((tmp = _rar_entry_get_property(entry_obj, "unpacked_size", sizeof("unpacked_size") TSRMLS_CC)) == NULL) {
		RETURN_FALSE;
	}

	convert_to_long_ex(tmp);
	RETURN_LONG(Z_LVAL_PP(tmp));
}
/* }}} */

/* {{{ proto RarEntry::getPackedSize();
   Return packed size of the entry */
PHP_METHOD(rarentry, getPackedSize)
{
	zval **rarfile, **tmp;
	rar_file_t *rar = NULL;
	zval *entry_obj = getThis();
	
	if ((tmp = _rar_entry_get_property(entry_obj, "packed_size", sizeof("packed_size") TSRMLS_CC)) == NULL) {
		RETURN_FALSE;
	}

	convert_to_long_ex(tmp);
	RETURN_LONG(Z_LVAL_PP(tmp));
}
/* }}} */

/* {{{ proto RarEntry::getHostOs();
   Return host OS of the entry */
PHP_METHOD(rarentry, getHostOs)
{
	zval **rarfile, **tmp;
	rar_file_t *rar = NULL;
	zval *entry_obj = getThis();
	
	if ((tmp = _rar_entry_get_property(entry_obj, "host_os", sizeof("host_os") TSRMLS_CC)) == NULL) {
		RETURN_FALSE;
	}

	convert_to_long_ex(tmp);
	RETURN_LONG(Z_LVAL_PP(tmp));
}
/* }}} */

/* {{{ proto RarEntry::getFileTime();
   Return modification time of the entry */
PHP_METHOD(rarentry, getFileTime)
{
	zval **rarfile, **tmp;
	rar_file_t *rar = NULL;
	zval *entry_obj = getThis();
	
	if ((tmp = _rar_entry_get_property(entry_obj, "file_time", sizeof("file_time") TSRMLS_CC)) == NULL) {
		RETURN_FALSE;
	}

	convert_to_string_ex(tmp);
	RETURN_STRINGL(Z_STRVAL_PP(tmp), Z_STRLEN_PP(tmp), 1);
}
/* }}} */

/* {{{ proto RarEntry::getCrc();
   Return CRC of the entry */
PHP_METHOD(rarentry, getCrc)
{
	zval **rarfile, **tmp;
	rar_file_t *rar = NULL;
	zval *entry_obj = getThis();
	
	if ((tmp = _rar_entry_get_property(entry_obj, "crc", sizeof("crc") TSRMLS_CC)) == NULL) {
		RETURN_FALSE;
	}

	convert_to_string_ex(tmp);
	RETURN_STRINGL(Z_STRVAL_PP(tmp), Z_STRLEN_PP(tmp), 1);
}
/* }}} */

/* {{{ proto RarEntry::getAttr();
   Return attributes of the entry */
PHP_METHOD(rarentry, getAttr)
{
	zval **rarfile, **tmp;
	rar_file_t *rar = NULL;
	zval *entry_obj = getThis();
	
	if ((tmp = _rar_entry_get_property(entry_obj, "attr", sizeof("attr") TSRMLS_CC)) == NULL) {
		RETURN_FALSE;
	}

	convert_to_long_ex(tmp);
	RETURN_LONG(Z_LVAL_PP(tmp));
}
/* }}} */

/* {{{ proto RarEntry::getVersion();
   Return version of the archiver, used to create this entry */
PHP_METHOD(rarentry, getVersion)
{
	zval **rarfile, **tmp;
	rar_file_t *rar = NULL;
	zval *entry_obj = getThis();
	
	if ((tmp = _rar_entry_get_property(entry_obj, "version", sizeof("version") TSRMLS_CC)) == NULL) {
		RETURN_FALSE;
	}

	convert_to_long_ex(tmp);
	RETURN_LONG(Z_LVAL_PP(tmp));
}
/* }}} */

/* {{{ proto RarEntry::getMethod();
   Return packing method */
PHP_METHOD(rarentry, getMethod)
{
	zval **rarfile, **tmp;
	rar_file_t *rar = NULL;
	zval *entry_obj = getThis();
	
	if ((tmp = _rar_entry_get_property(entry_obj, "method", sizeof("method") TSRMLS_CC)) == NULL) {
		RETURN_FALSE;
	}

	convert_to_long_ex(tmp);
	RETURN_LONG(Z_LVAL_PP(tmp));
}
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
