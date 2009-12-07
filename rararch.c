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

#ifdef __cplusplus
extern "C" {
#endif

#include "php.h"
#include "php_rar.h"

/* {{{ Type definitions reserved for this translation unit */
typedef struct _ze_rararch_object {
	zend_object	parent;
	rar_file_t  *rar_file;
} ze_rararch_object;
/* }}} */

/* {{{ Globals with internal linkage */
static zend_class_entry *rararch_ce_ptr;
static zend_object_handlers rararch_object_handlers;
/* }}} */

/* {{{ Function prototypes for functions with internal linkage */
static int _rar_list_files(rar_file_t * TSRMLS_DC);
static int _rar_raw_entries_to_files(rar_file_t *rar,
								     const wchar_t * const file, //can be NULL
								     zval *target TSRMLS_DC);
static zend_object_value rararch_ce_create_object(zend_class_entry *class_type TSRMLS_DC);
static void rararch_ce_destroy_object(ze_rararch_object *object,
									  zend_object_handle handle TSRMLS_DC);
static void rararch_ce_free_object_storage(ze_rararch_object *object TSRMLS_DC);
/* }}} */

/* {{{ Function definitions with external linkage */
int _rar_get_file_resource(zval *zval_file, rar_file_t **rar_file TSRMLS_DC) /* {{{ */
{
	return _rar_get_file_resource_ex(zval_file, rar_file, FALSE TSRMLS_CC);
}
/* }}} */

int _rar_get_file_resource_ex(zval *zval_file, rar_file_t **rar_file, int silent TSRMLS_DC) /* {{{ */
{
	ze_rararch_object *zobj;
	zobj = zend_object_store_get_object(zval_file TSRMLS_CC);
	if (zobj == NULL) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
			"Could not find object in the store. This is a bug, please report it.");
		return 0;
	}

	*rar_file = zobj->rar_file;
	if ((*rar_file)->arch_handle == NULL && !silent) { //rar_close was called
		php_error_docref(NULL TSRMLS_CC, E_WARNING,	"The archive is already closed.");
		return 0;
	}

	return 1;
}
/* }}} */
/* }}} */

/* {{{ Helper functions and preprocessor definitions */
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
				 *rararch_obj;

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

			//create RarEntry object:
			object_init_ex(entry_obj, rar_class_entry_ptr);

			//rararch property:
			MAKE_STD_ZVAL(rararch_obj);
			Z_TYPE_P(rararch_obj) = IS_OBJECT;
			Z_OBJ_HANDLE_P(rararch_obj) = rar->id;
			Z_OBJ_HT_P(rararch_obj) = &rararch_object_handlers;
			zend_update_property(rar_class_entry_ptr, entry_obj, "rarfile",
				sizeof("rararch") - 1, rararch_obj TSRMLS_CC);
			/* zend_update_property calls write_property handler, which
			 * increments the refcount. We must decrease it here */
			zval_ptr_dtor(&rararch_obj); //restore refcount to 1
			/* to avoid destruction of the object due to refcount hitting
			 * 0 when this new object zval we created is destroyed */
			zend_objects_store_add_ref_by_handle(rar->id TSRMLS_CC);

			//remaining properties:
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

static zend_object_value rararch_ce_create_object(zend_class_entry *class_type TSRMLS_DC) /* {{{ */
{
	zend_object_value	zov;
	ze_rararch_object	*zobj;
	zval				*tmp;

	zobj = emalloc(sizeof *zobj);
	zend_object_std_init((zend_object*) zobj, class_type TSRMLS_CC);

	zend_hash_copy(((zend_object*)zobj)->properties,
		&(class_type->default_properties),
		(copy_ctor_func_t) zval_add_ref, &tmp, sizeof(zval*));
	zov.handle = zend_objects_store_put(zobj,
		(zend_objects_store_dtor_t) rararch_ce_destroy_object,
		(zend_objects_free_object_storage_t) rararch_ce_free_object_storage,
		NULL TSRMLS_CC);
	zov.handlers = &rararch_object_handlers;
	return zov;
}
/* }}} */

static void rararch_ce_destroy_object(ze_rararch_object *object,
									  zend_object_handle handle TSRMLS_DC) /* {{{ */
{
	rar_file_t *rar = object->rar_file;

	//not really relevant, calls destr. zend func. ce->destructor if it exists
	zend_objects_destroy_object((zend_object*) object, handle TSRMLS_CC);
	
	if (rar->arch_handle != NULL) {
		RARCloseArchive(rar->arch_handle);
	}
}
/* }}} */

static void rararch_ce_free_object_storage(ze_rararch_object *object TSRMLS_DC) /* {{{ */
{
	rar_file_t *rar = object->rar_file;
	int i;

	if (rar->password != NULL) {
		efree(rar->password);
	}
	if ((rar->entries != NULL) && (rar->entry_count > 0)) {
		for (i = 0; i < rar->entry_count; i++) {
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
	
	/* could call zend_objects_free_object_storage here (not before!), but
	 * instead I'll mimic its behaviour */
	zend_object_std_dtor((zend_object*) object TSRMLS_CC);
	efree(object);
}
/* }}} */


/* module functions */

/* {{{ proto RarArchive rar_open(string filename [, string password])
   Open RAR archive and return RarArchive object */
PHP_FUNCTION(rar_open)
{
	char *filename;
	char *password = NULL;
	char resolved_path[MAXPATHLEN];
	int filename_len;
	int password_len = 0;
	rar_file_t *rar = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|s", &filename,
		&filename_len, &password, &password_len) == FAILURE) {
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
		if (password_len > 0) {
			rar->password = estrndup(password, password_len);
		}
		//rar->id = ZEND_REGISTER_RESOURCE(return_value, rar, le_rar_file);
		object_init_ex(return_value, rararch_ce_ptr);
		{
			ze_rararch_object *zobj =
				zend_object_store_get_object(return_value TSRMLS_CC);
			zobj->rar_file = rar;
		}
		rar->id = Z_OBJ_HANDLE_P(return_value);
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
	RARSetCallback(rar->arch_handle, _rar_unrar_callback, (LPARAM) rar->password);
}
/* }}} */

/* {{{ proto array rar_list(RarArchive rarfile)
   Return entries from the rar archive */
PHP_FUNCTION(rar_list)
{
	zval *file = getThis();
	rar_file_t *rar = NULL;
	int result;

	if (file == NULL && zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O",
		&file, rararch_ce_ptr) == FAILURE) {
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

/* {{{ proto object rar_entry_get(RarArchive rarfile, string filename)
   Return entry from the rar archive */
PHP_FUNCTION(rar_entry_get)
{
	zval *file = getThis();
	char *filename;
	rar_file_t *rar = NULL;
	int result;
	int found;
	int filename_len;
	wchar_t *filename_c = NULL;

	if (file == NULL && zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Os",
		&file, rararch_ce_ptr, &filename, &filename_len) == FAILURE) {
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

/* {{{ proto string rar_comment_get(RarArchive rarfile)
   Return comment of the rar archive */
PHP_FUNCTION(rar_comment_get)
{
	zval *file = getThis();
	rar_file_t *rar = NULL;
	unsigned cmt_state;

	if (file == NULL && zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O",
		&file, rararch_ce_ptr) == FAILURE) {
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

/* {{{ proto bool rar_close(RarArchive rarfile)
   Close Rar archive and free all resources */
PHP_FUNCTION(rar_close)
{
	zval *file = getThis();
	rar_file_t *rar = NULL;

	if (file == NULL && zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O",
		&file, rararch_ce_ptr) == FAILURE) {
		return;
	}

	if (!_rar_get_file_resource(file, &rar TSRMLS_CC)) {
		RETURN_FALSE;
	}

	//zend_hash_index_del(&EG(regular_list), Z_RESVAL_P(file));
	/* When changed from resource to custom object, instead of fiddling
	 * with the refcount to force object destruction, an indication that
	 * the file is already closed is given by setting rar->arch_handle
	 * to NULL. This is checked by _rar_get_file_resource. */
	RARCloseArchive(rar->arch_handle);
	rar->arch_handle = NULL;

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string RarEntry::__toString()
   Return string representation for entry */
PHP_METHOD(rararch, __toString)
{
	zval				*arch_obj = getThis();
	rar_file_t			*rar = NULL;
	const char			format[] = "RAR Archive \"%s\"%s",
						closed[] = " (closed)";
	char				*restring;
	int					restring_len,
						is_closed;

	if (!_rar_get_file_resource_ex(arch_obj, &rar, TRUE TSRMLS_CC)) {
		RETURN_FALSE;
	}

	is_closed = (rar->arch_handle == NULL);

	//2 is size of %s, 1 is terminating 0
	restring_len = (sizeof(format) - 1) - 2 * 2 + 1;
	restring_len += strlen(rar->list_open_data->ArcName);
	if (is_closed)
		restring_len += sizeof(closed) - 1;

	restring = emalloc(restring_len);
	snprintf(restring, restring_len, format, rar->list_open_data->ArcName,
		is_closed?closed:"");
	restring[restring_len - 1] = '\0'; //just to be safe
	
	RETURN_STRING(restring, 0);
}
/* }}} */

/* {{{ arginfo */
ZEND_BEGIN_ARG_INFO_EX(arginfo_rararchive_open, 0, 0, 1)
	ZEND_ARG_INFO(0, filename)
	ZEND_ARG_INFO(0, password)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_rararchive_getentry, 0, 0, 1)
	ZEND_ARG_INFO(0, filename)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_rararchive_void, 0)
ZEND_END_ARG_INFO()
/* }}} */

static zend_function_entry php_rararch_class_functions[] = {
	PHP_ME_MAPPING(open,		rar_open,			arginfo_rararchive_open,		ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME_MAPPING(list,		rar_list,			arginfo_rararchive_void,		ZEND_ACC_PUBLIC)
	PHP_ME_MAPPING(getEntry,	rar_entry_get,		arginfo_rararchive_getentry,	ZEND_ACC_PUBLIC)
	PHP_ME_MAPPING(getComment,	rar_comment_get,	arginfo_rararchive_void,		ZEND_ACC_PUBLIC)
	PHP_ME_MAPPING(close,		rar_close,			arginfo_rararchive_void,		ZEND_ACC_PUBLIC)
	PHP_ME(rararch,				__toString,			arginfo_rararchive_void,		ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};

void minit_rararch(TSRMLS_D)
{
	zend_class_entry ce;

	memcpy(&rararch_object_handlers, zend_get_std_object_handlers(),
		sizeof rararch_object_handlers);

	INIT_CLASS_ENTRY(ce, "RarArchive", php_rararch_class_functions);
	rararch_ce_ptr = zend_register_internal_class(&ce TSRMLS_CC);
	rararch_ce_ptr->ce_flags |= ZEND_ACC_FINAL_CLASS;
	rararch_ce_ptr->clone = NULL;
	rararch_ce_ptr->create_object = &rararch_ce_create_object;
}

#ifdef __cplusplus
}
#endif
