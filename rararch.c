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

#define _GNU_SOURCE
#include <string.h>

#include <wchar.h>
#include <php.h>
#include <zend_interfaces.h>
#include "php_rar.h"

/* {{{ Type definitions reserved for this translation unit */
typedef struct _ze_rararch_object {
    rar_file_t  *rar_file;
	zend_object	parent;
} ze_rararch_object;

typedef struct _rararch_iterator {
	zend_object_iterator	parent;
	rar_find_output			*state;
	zval					*value;
	int						empty_iterator; /* iterator should give nothing */
} rararch_iterator;
/* }}} */

/* {{{ Globals with internal linkage */
static zend_class_entry *rararch_ce_ptr;
static zend_object_handlers rararch_object_handlers;
/* }}} */

/* {{{ Helper macros */
#define RAR_THIS_OR_NO_ARGS(file) \
	if (file == NULL) { \
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", \
				&file, rararch_ce_ptr) == FAILURE) { \
			RETURN_NULL(); \
		} \
	} \
	else { \
		RAR_RETNULL_ON_ARGS(); \
	}
/* }}} */

/* {{{ Function prototypes for functions with internal linkage */
static zend_object* rararch_ce_create_object(zend_class_entry *class_type TSRMLS_DC);
static void rararch_ce_destroy_object(zend_object *object);
static void rararch_ce_free_object_storage(zend_object *object TSRMLS_DC);
/* }}} */

/* {{{ RarArchive handlers */
static int rararch_handlers_preamble(zval *object, rar_file_t **rar TSRMLS_DC);
static int rararch_dimensions_preamble(rar_file_t *rar, zval *offset, long *index, int quiet TSRMLS_DC);
static int rararch_count_elements(zval *object, long *count TSRMLS_DC);
static zval *rararch_read_dimension(zval *object, zval *offset, int type TSRMLS_DC, zval *rv);
static void rararch_write_dimension(zval *object, zval *offset, zval *value TSRMLS_DC);
static int rararch_has_dimension(zval *object, zval *offset, int check_empty TSRMLS_DC);
/* }}} */

static inline ze_rararch_object* php_ze_rararch_object_fetch_object(zend_object *obj) {
      return (ze_rararch_object*)((char*)obj - XtOffsetOf(ze_rararch_object, parent));
}
 
#define Z_ZE_RARARCH_OBJECT(zv) php_ze_rararch_object_fetch_object(zv)
#define Z_ZE_RARARCH_OBJECT_P(zv) Z_ZE_RARARCH_OBJECT(Z_OBJ_P(zv))

/* {{{ Function definitions with external linkage */
int _rar_get_file_resource(zval *zval_file, rar_file_t **rar_file TSRMLS_DC) /* {{{ */
{
	return _rar_get_file_resource_ex(zval_file, rar_file, FALSE TSRMLS_CC);
}
/* }}} */

/* Creates a RarArchive object, all three in args will be dupped */
int _rar_create_rararch_obj(const char* resolved_path,
							const char* open_password,
							zval *volume_callback, /* must be callable or NULL */
							zval *object,
							int *err_code TSRMLS_DC) /* {{{ */
{
	rar_file_t *rar = NULL;

	rar = emalloc(sizeof *rar);
	rar->list_open_data = ecalloc(1, sizeof *rar->list_open_data);
	rar->list_open_data->ArcName = estrdup(resolved_path);
	rar->list_open_data->OpenMode = RAR_OM_LIST_INCSPLIT;
	rar->list_open_data->CmtBuf = ecalloc(RAR_MAX_COMMENT_SIZE, 1);
	rar->list_open_data->CmtBufSize = RAR_MAX_COMMENT_SIZE;
	rar->extract_open_data = ecalloc(1, sizeof *rar->extract_open_data);
	rar->extract_open_data->ArcName = estrdup(resolved_path);
	rar->extract_open_data->OpenMode = RAR_OM_EXTRACT;
	rar->extract_open_data->CmtBuf = NULL; /* not interested in it again */
	rar->cb_userdata.password = NULL;
	rar->cb_userdata.callable = NULL;
	rar->entries = NULL;
	rar->allow_broken = 0;

	rar->arch_handle = RAROpenArchiveEx(rar->list_open_data);
	if (rar->arch_handle != NULL && rar->list_open_data->OpenResult == 0) {
		ze_rararch_object *zobj;

		if (open_password != NULL) {
			rar->cb_userdata.password = estrdup(open_password);
		}
		if (volume_callback != NULL) {
			rar->cb_userdata.callable = emalloc(sizeof(zval));
			ZVAL_NEW_REF(rar->cb_userdata.callable, volume_callback);
            if (Z_REFCOUNTED_P(volume_callback)) {
                Z_ADDREF_P(volume_callback);
            }
		}

		object_init_ex(object, rararch_ce_ptr);
		zobj = Z_ZE_RARARCH_OBJECT_P(object);
		zobj->rar_file = rar;
		rar->id = Z_OBJ_P(object);

		RARSetCallback(rar->arch_handle, _rar_unrar_callback,
			(LPARAM) &rar->cb_userdata);

		return SUCCESS;
	} else {
		*err_code = rar->list_open_data->OpenResult;

		efree(rar->list_open_data->ArcName);
		efree(rar->list_open_data->CmtBuf);
		efree(rar->list_open_data);
		efree(rar->extract_open_data->ArcName);
		efree(rar->extract_open_data);
		efree(rar);
		return FAILURE;
	}
}
/* }}} */

void _rar_close_file_resource(rar_file_t *rar) /* {{{ */
{
	assert(rar->arch_handle != NULL);

	/* When changed from resource to custom object, instead of fiddling
	 * with the refcount to force object destruction, an indication that
	 * the file is already closed is given by setting rar->arch_handle
	 * to NULL. This is checked by _rar_get_file_resource. */
	RARCloseArchive(rar->arch_handle);
	rar->arch_handle = NULL;
}
/* }}} */

/* Receives archive zval, returns object struct.
 * If silent is FALSE, it checks whether the archive is alredy closed, and if it
 * is, an exception/error is raised and FAILURE is returned
 */
int _rar_get_file_resource_ex(zval* zval_file, rar_file_t **rar_file, int silent TSRMLS_DC) /* {{{ */
{
	ze_rararch_object *zobj = Z_ZE_RARARCH_OBJECT_P(zval_file);
	if (zobj == NULL) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
			"Could not find object in the store. This is a bug, please report it.");
		return FAILURE;
	}

	*rar_file = zobj->rar_file;
	if ((*rar_file)->arch_handle == NULL && !silent) { /* rar_close was called */
		_rar_handle_ext_error("The archive is already closed" TSRMLS_CC);
		return FAILURE;
	}

	return SUCCESS;
}
/* }}} */
/* end functions with external linkage }}} */

/* {{{ Helper functions and preprocessor definitions */

/* target should be initialized */
static void _rar_raw_entries_to_array(rar_file_t *rar, zval *target TSRMLS_DC) /* {{{ */
{
	rar_find_output	*state;
	zval rararch_obj;

	/* create zval to point to the RarArchive object) */
	
	ZVAL_OBJ(&rararch_obj, rar->id);
	Z_OBJ_HT_P(&rararch_obj) = &rararch_object_handlers;
	/* object has a new reference; if not incremented, the object would be
	 * be destroyed when this new zval we created was destroyed */

	_rar_entry_search_start(rar, RAR_SEARCH_TRAVERSE, &state TSRMLS_CC);
	do {
		_rar_entry_search_advance(state, NULL, 0, 0);
		if (state->found) {
			zval entry_obj;
			_rar_entry_to_zval(&rararch_obj, state->header, state->packed_size,
				state->position, &entry_obj TSRMLS_CC);

			add_next_index_zval(target, &entry_obj);
		}
	} while (state->eof == 0);
	_rar_entry_search_end(state);

	/* it was created with refcount=1 and incremented for each RarEntry object
	 * created, so we must decrease by one (this will also destroy it if
	 * there were no entries */
}
/* }}} */

static zend_object* rararch_ce_create_object(zend_class_entry *class_type TSRMLS_DC) /* {{{ */
{
	ze_rararch_object *zobj = emalloc(sizeof (ze_rararch_object) + zend_object_properties_size(class_type));
	/* rararch_ce_free_object_storage will attempt to access it otherwise */
	zobj->rar_file = NULL;
	zend_object_std_init(&(zobj->parent), class_type TSRMLS_CC);
	object_properties_init(&(zobj->parent), class_type TSRMLS_CC);
	rararch_object_handlers.offset = XtOffsetOf(ze_rararch_object, parent);
	rararch_object_handlers.dtor_obj = rararch_ce_destroy_object;
	rararch_object_handlers.free_obj = rararch_ce_free_object_storage;
	zobj->parent.handlers = &rararch_object_handlers;
	return &(zobj->parent);
}
/* }}} */

static void rararch_ce_destroy_object(zend_object *object) /* {{{ */
{
	rar_file_t *rar = Z_ZE_RARARCH_OBJECT(object)->rar_file;

	/* may be NULL if the user did new RarArchive() */
	if (rar != NULL) {
		_rar_destroy_userdata(&rar->cb_userdata);

		_rar_delete_entries(rar TSRMLS_CC);

		efree(rar->list_open_data->ArcName);
		efree(rar->list_open_data->CmtBuf);
		efree(rar->list_open_data);
		efree(rar->extract_open_data->ArcName);
		efree(rar->extract_open_data);
		efree(rar);
	}
	zend_objects_destroy_object(object);

}
/* }}} */

static void rararch_ce_free_object_storage(zend_object *object TSRMLS_DC) /* {{{ */
{
	zend_object_std_dtor(&Z_ZE_RARARCH_OBJECT(object)->parent);
}
/* }}} */

/* }}} */

/* {{{ RarArchive handlers */
static int rararch_handlers_preamble(zval *object, rar_file_t **rar TSRMLS_DC) /* {{{ */
{
	/* don't call zend_objects_get_address or zend_object_store_get directly;
	 * _rar_get_file_resource checks if the archive was closed */
	if (_rar_get_file_resource(object, rar TSRMLS_CC) == FAILURE) {
		return FAILURE;
	}

	return _rar_handle_error(_rar_list_files(*rar TSRMLS_CC) TSRMLS_CC);
}
/* }}} */

#define RAR_DOCREF_IF_UNQUIET(...) \
	if (!quiet) { php_error_docref(__VA_ARGS__); }

/* {{{ rararch_dimensions_preamble - semi-strict parsing of int argument */
static int rararch_dimensions_preamble(rar_file_t *rar,
									   zval *offset,
									   long *index,
									   int quiet TSRMLS_DC)
{
	if (offset == NULL) {
		RAR_DOCREF_IF_UNQUIET(NULL TSRMLS_CC, E_ERROR,
			"Empty dimension syntax is not supported for RarArchive objects");
		return FAILURE;
	}

	if (Z_TYPE_P(offset) == IS_LONG) {
		*index = Z_LVAL_P(offset);
	}
	else if (Z_TYPE_P(offset) == IS_STRING) {
		int type;
		double d;

		if ((type = is_numeric_string(Z_STRVAL_P(offset), Z_STRLEN_P(offset),
				index, &d, -1)) == 0) {
			RAR_DOCREF_IF_UNQUIET(NULL TSRMLS_CC, E_WARNING,
				"Attempt to use a non-numeric dimension to access a "
				"RarArchive object (invalid string)");
			return FAILURE;
		}
		else if (type == IS_DOUBLE) {
			if (d > LONG_MAX || d < LONG_MIN) {
				RAR_DOCREF_IF_UNQUIET(NULL TSRMLS_CC, E_WARNING,
					"Dimension index is out of integer bounds");
				return FAILURE;
			}

			*index = (long) d;
		}
	}
	else if (Z_TYPE_P(offset) == IS_DOUBLE) {
		if (Z_DVAL_P(offset) > LONG_MAX || Z_DVAL_P(offset) < LONG_MIN) {
			RAR_DOCREF_IF_UNQUIET(NULL TSRMLS_CC, E_WARNING,
				"Dimension index is out of integer bounds");
			return FAILURE;
		}
		*index = (long) Z_DVAL_P(offset);
	}
	else if (Z_TYPE_P(offset) == IS_OBJECT) {
		if (Z_OBJ_HT_P(offset)->get) {
			/* get handler cannot return NULL */
			zval *newoffset = Z_OBJ_HT_P(offset)->get(offset, offset TSRMLS_CC);
			int ret;

			if (Z_TYPE_P(newoffset) == IS_OBJECT) {
				RAR_DOCREF_IF_UNQUIET(NULL TSRMLS_CC, E_WARNING,
					"Could not convert object given as dimension index into "
					"an integer (get handler returned another object)");
				return FAILURE;
			}

			ret = rararch_dimensions_preamble(rar, newoffset, index, quiet
				TSRMLS_CC);
			zval_ptr_dtor(newoffset);
			return ret;
		}
		else {
			RAR_DOCREF_IF_UNQUIET(NULL TSRMLS_CC, E_WARNING,
				"Attempt to use an object with no get handler as a dimension "
				"to access a RarArchive object");
			return FAILURE;
		}
	}
	else {
		RAR_DOCREF_IF_UNQUIET(NULL TSRMLS_CC, E_WARNING,
			"Attempt to use a non-numeric dimension to access a "
			"RarArchive object (invalid type)");
		return FAILURE;
	}

	if (*index < 0L) {
		RAR_DOCREF_IF_UNQUIET(NULL TSRMLS_CC, E_WARNING,
			"Dimension index must be non-negative, given %ld", *index);
		return FAILURE;
	}
	if ((size_t) *index >= _rar_entry_count(rar)) {
		RAR_DOCREF_IF_UNQUIET(NULL TSRMLS_CC, E_WARNING,
			"Dimension index exceeds or equals number of entries in RAR "
			"archive");
		return FAILURE;
	}

	return SUCCESS;
}
/* }}} */

/* {{{ RarArchive count_elements handler */
static int rararch_count_elements(zval *object, long *count TSRMLS_DC)
{
	rar_file_t	*rar = NULL;
	size_t		entry_count;

	if (rararch_handlers_preamble(object, &rar TSRMLS_CC) == FAILURE) {
		*count = 0L;
		return SUCCESS; /* intentional */
	}

	entry_count = _rar_entry_count(rar);
	if (entry_count > LONG_MAX)
		entry_count = (size_t) LONG_MAX;

	*count = (long) entry_count;

	return SUCCESS;
}
/* }}} */

/* {{{ RarArchive read_dimension handler */
static zval *rararch_read_dimension(zval *object, zval *offset, int type TSRMLS_DC, zval *rv)
{
	long					index;
	rar_file_t				*rar = NULL;
	struct _rar_find_output	*out;

	if (rararch_handlers_preamble(object, &rar TSRMLS_CC) == FAILURE) {
		return NULL;
	}
	if (rararch_dimensions_preamble(rar, offset, &index, (type == BP_VAR_IS)
			TSRMLS_CC) == FAILURE)
		return NULL;
	if (type == BP_VAR_RW || type == BP_VAR_W || type == BP_VAR_UNSET)
		php_error_docref(NULL TSRMLS_CC, E_ERROR,
			"A RarArchive object is not modifiable");
	_rar_entry_search_start(rar, RAR_SEARCH_INDEX, &out TSRMLS_CC);
	_rar_entry_search_seek(out, (size_t) index);
	_rar_entry_search_advance(out, NULL, 0, 0);
	assert(out->found);
	_rar_entry_to_zval(object, out->header, out->packed_size, out->position, rv TSRMLS_CC);
	_rar_entry_search_end(out);
	return rv;
}
/* }}} */

/* {{{ RarArchive write_dimension handler */
static void rararch_write_dimension(zval *object, zval *offset, zval *value TSRMLS_DC)
{
	php_error_docref(NULL TSRMLS_CC, E_ERROR,
		"A RarArchive object is not writable");
}
/* }}} */

/* {{{ RarArchive has_dimension handler */
static int rararch_has_dimension(zval *object, zval *offset, int check_empty TSRMLS_DC)
{
	long		index;
	rar_file_t	*rar = NULL;

	(void) check_empty; /* don't care */

	if (rararch_handlers_preamble(object, &rar TSRMLS_CC) == FAILURE) {
		return 0;
	}

	return (rararch_dimensions_preamble(rar, offset, &index, 1 TSRMLS_CC) ==
			SUCCESS);
}
/* }}} */

/* {{{ RarArchive unset_dimension handler */
static void rararch_unset_dimension(zval *object, zval *offset TSRMLS_DC)
{
	php_error_docref(NULL TSRMLS_CC, E_ERROR,
		"A RarArchive object is not writable");
}
/* }}} */
/* }}} */

/* module functions */

/* {{{ proto RarArchive rar_open(string filename [, string password = NULL
       [, callback volume_cb = NULL ]])
   Open RAR archive and return RarArchive object */
PHP_FUNCTION(rar_open)
{
	char *filename;
	char *password = NULL;
	char resolved_path[MAXPATHLEN];
	int filename_len;
	int password_len = 0;
	zval *callable = NULL;
	int err_code;

	/* Files are only opened here and in _rar_find_file */

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|s!z!", &filename,
		&filename_len, &password, &password_len, &callable) == FAILURE) {
		return;
	}

	if (OPENBASEDIR_CHECKPATH(filename)) {
		RETURN_FALSE;
	}

	if (!expand_filepath(filename, resolved_path TSRMLS_CC)) {
		RETURN_FALSE;
	}
	assert(strnlen(resolved_path, MAXPATHLEN) < MAXPATHLEN);

	if (callable != NULL) { /* given volume resolver callback */
		if (!zend_is_callable(callable, IS_CALLABLE_STRICT, NULL TSRMLS_CC)) {
			_rar_handle_ext_error("%s" TSRMLS_CC, "Expected the third "
				"argument, if provided, to be a valid callback");
			RETURN_FALSE;
		}
	}

	if (_rar_create_rararch_obj(resolved_path, password, callable,
			return_value, &err_code TSRMLS_CC) == FAILURE) {
		const char *err_str = _rar_error_to_string(err_code);
		if (err_str == NULL)
			_rar_handle_ext_error("%s" TSRMLS_CC, "Archive opened failed "
				"(returned NULL handle), but did not return an error. "
				"Should not happen.");
		else {
			char *preamble;
			spprintf(&preamble, 0, "Failed to open %s: ", resolved_path);
			_rar_handle_error_ex(preamble, err_code TSRMLS_CC);
			efree(preamble);
		}

		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto array rar_list(RarArchive rarfile)
   Return entries from the rar archive */
PHP_FUNCTION(rar_list)
{
	zval *file = getThis();
	rar_file_t *rar = NULL;

	RAR_THIS_OR_NO_ARGS(file);

	if (_rar_get_file_resource(file, &rar TSRMLS_CC) == FAILURE) {
		RETURN_FALSE;
	}

	if (_rar_handle_error(_rar_list_files(rar TSRMLS_CC) TSRMLS_CC) == FAILURE)
		RETURN_FALSE;

	array_init(return_value);

	_rar_raw_entries_to_array(rar, return_value TSRMLS_CC);
}
/* }}} */

/* {{{ proto object rar_entry_get(RarArchive rarfile, string filename)
   Return entry from the rar archive */
PHP_FUNCTION(rar_entry_get)
{
	zval *file = getThis();
	char *filename;
	rar_file_t *rar = NULL;
	int filename_len;
	wchar_t *filename_c = NULL;
	rar_find_output *sstate;

	zval filename_val;
	ZVAL_EMPTY_STRING(&filename_val);
	if (file == NULL) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "OS",
				&file, rararch_ce_ptr, &Z_STR(filename_val)) == FAILURE) {
			return;
		}
	} else if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "S", &Z_STR(filename_val)) == FAILURE) {
		zval_ptr_dtor(&filename_val);
		return;
	}

	filename_len = Z_STRLEN(filename_val);
	filename = Z_STRVAL(filename_val);

	if (_rar_get_file_resource(file, &rar TSRMLS_CC) == FAILURE) {
		RETURN_FALSE;
	}

	if (_rar_handle_error(_rar_list_files(rar TSRMLS_CC) TSRMLS_CC) == FAILURE)
		RETURN_FALSE;

	filename_c = ecalloc(filename_len + 1, sizeof *filename_c);
	_rar_utf_to_wide(filename, filename_c, filename_len + 1);

	_rar_entry_search_start(rar, RAR_SEARCH_NAME, &sstate TSRMLS_CC);
	_rar_entry_search_advance(sstate, filename_c, 0, 0);
	if (sstate->found) {
		_rar_entry_to_zval(file, sstate->header, sstate->packed_size,
			sstate->position, return_value TSRMLS_CC);
	}
	else {
		_rar_handle_ext_error(
			"cannot find file \"%s\" in Rar archive \"%s\""
			TSRMLS_CC, filename, rar->list_open_data->ArcName);
		RETVAL_FALSE;
	}
	_rar_entry_search_end(sstate);
	zval_ptr_dtor(&filename_val);
	efree(filename_c);
}
/* }}} */

/* {{{ proto string rar_solid_is(RarArchive rarfile)
   Return whether RAR archive is solid */
PHP_FUNCTION(rar_solid_is)
{
	zval *file = getThis();
	rar_file_t *rar = NULL;

	RAR_THIS_OR_NO_ARGS(file);

	if (_rar_get_file_resource(file, &rar TSRMLS_CC) == FAILURE) {
		RETURN_FALSE;
	}

	RETURN_BOOL((rar->list_open_data->Flags & 0x0008) != 0);
}
/* }}} */

/* {{{ proto string rar_comment_get(RarArchive rarfile)
   Return comment of the rar archive */
PHP_FUNCTION(rar_comment_get)
{
	zval *file = getThis();
	rar_file_t *rar = NULL;
	unsigned cmt_state;

	RAR_THIS_OR_NO_ARGS(file);

	if (_rar_get_file_resource(file, &rar TSRMLS_CC) == FAILURE) {
		RETURN_FALSE;
	}

	cmt_state = rar->list_open_data->CmtState;

	if (_rar_handle_error(cmt_state TSRMLS_CC) == FAILURE)
		RETURN_FALSE;

	if (cmt_state == 0) /* comment not present */
		RETURN_NULL();

	if (cmt_state == 1) { /* comment read completely */
		/* CmtSize - 1 because we don't need the null terminator */
		RETURN_STRINGL(rar->list_open_data->CmtBuf,
			rar->list_open_data->CmtSize - 1);
	}
}
/* }}} */

/* {{{ proto bool rar_is_broken(RarArchive rarfile)
   Check whether a RAR archive is broken */
PHP_FUNCTION(rar_broken_is)
{
	zval		*file = getThis();
	rar_file_t	*rar = NULL;
	int			result,
				orig_allow_broken;

	RAR_THIS_OR_NO_ARGS(file);

	if (_rar_get_file_resource(file, &rar TSRMLS_CC) == FAILURE) {
		RETURN_FALSE;
	}

	orig_allow_broken = rar->allow_broken;
	rar->allow_broken = 0; /* with 1 we'd always get success from list_files */
	result = _rar_list_files(rar TSRMLS_CC);
	rar->allow_broken = orig_allow_broken;

	RETURN_BOOL(_rar_error_to_string(result) != NULL);
}

/* {{{ proto bool rar_allow_broken_set(RarArchive rarfile, bool allow_broken)
   Whether to allow entry retrieval of broken RAR archives */
PHP_FUNCTION(rar_allow_broken_set)
{
	zval		*file = getThis();
	rar_file_t	*rar = NULL;
	zend_bool	allow_broken;

	if (file == NULL) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Ob",
				&file, rararch_ce_ptr, &allow_broken) == FAILURE) {
			return;
		}
	}
	else if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b",
			&allow_broken) == FAILURE) {
		return;
	}

	if (_rar_get_file_resource(file, &rar TSRMLS_CC) == FAILURE) {
		RETURN_FALSE;
	}

	rar->allow_broken = (int) allow_broken;

	RETURN_TRUE;
}

/* {{{ proto bool rar_close(RarArchive rarfile)
   Close Rar archive and free all resources */
PHP_FUNCTION(rar_close)
{
	zval *file = getThis();
	rar_file_t *rar = NULL;

	RAR_THIS_OR_NO_ARGS(file);

	if (_rar_get_file_resource(file, &rar TSRMLS_CC) == FAILURE) {
		RETURN_FALSE;
	}

	_rar_close_file_resource(rar);

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
	size_t				restring_size;
	int					is_closed;

	RAR_RETNULL_ON_ARGS();

	if (_rar_get_file_resource_ex(arch_obj, &rar, TRUE TSRMLS_CC) == FAILURE) {
		RETURN_FALSE; /* should never happen */
	}

	is_closed = (rar->arch_handle == NULL);

	/* 2 is size of %s, 1 is terminating 0 */
	restring_size = (sizeof(format) - 1) - 2 * 2 + 1;
	restring_size += strlen(rar->list_open_data->ArcName);
	if (is_closed)
		restring_size += sizeof(closed) - 1;

	restring = emalloc(restring_size);
	snprintf(restring, restring_size, format, rar->list_open_data->ArcName,
		is_closed?closed:"");
	restring[restring_size - 1] = '\0'; /* just to be safe */

	RETVAL_STRINGL(restring, (int)restring_size - 1);
	efree(restring);
	return;
	
}
/* }}} */

/* {{{ arginfo */
ZEND_BEGIN_ARG_INFO_EX(arginfo_rararchive_open, 0, 0, 1)
	ZEND_ARG_INFO(0, filename)
	ZEND_ARG_INFO(0, password)
	ZEND_ARG_INFO(0, volume_callback)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_rararchive_getentry, 0, 0, 1)
	ZEND_ARG_INFO(0, filename)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_rararchive_setallowbroken, 0, 0, 1)
	ZEND_ARG_INFO(0, allow_broken)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_rararchive_void, 0)
ZEND_END_ARG_INFO()
/* }}} */

static zend_function_entry php_rararch_class_functions[] = {
	PHP_ME_MAPPING(open,			rar_open,				arginfo_rararchive_open,		ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME_MAPPING(getEntries,		rar_list,				arginfo_rararchive_void,		ZEND_ACC_PUBLIC)
	PHP_ME_MAPPING(getEntry,		rar_entry_get,			arginfo_rararchive_getentry,	ZEND_ACC_PUBLIC)
#ifdef RAR_ARCHIVE_LIST_ALIAS
	PHP_ME_MAPPING(list,			rar_list,				arginfo_rararchive_void,		ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
#endif
	PHP_ME_MAPPING(isSolid,			rar_solid_is,			arginfo_rararchive_void,		ZEND_ACC_PUBLIC)
	PHP_ME_MAPPING(getComment,		rar_comment_get,		arginfo_rararchive_void,		ZEND_ACC_PUBLIC)
	PHP_ME_MAPPING(isBroken,		rar_broken_is,			arginfo_rararchive_void,		ZEND_ACC_PUBLIC)
	PHP_ME_MAPPING(setAllowBroken,	rar_allow_broken_set,	arginfo_rararchive_setallowbroken, ZEND_ACC_PUBLIC)
	PHP_ME_MAPPING(close,			rar_close,				arginfo_rararchive_void,		ZEND_ACC_PUBLIC)
	PHP_ME(rararch,					__toString,				arginfo_rararchive_void,		ZEND_ACC_PUBLIC)
	PHP_ME_MAPPING(__construct,		rar_bogus_ctor,			arginfo_rararchive_void,		ZEND_ACC_PRIVATE | ZEND_ACC_CTOR)
	{NULL, NULL, NULL}
};

/* {{{ Iteration. Very boring stuff indeed. */

/* {{{ Iteration Prototypes */
static zend_object_iterator *rararch_it_get_iterator(zend_class_entry *ce,
													 zval *object,
													 int by_ref TSRMLS_DC);
/* static void rararch_it_delete_cache(zend_object_iterator *iter TSRMLS_DC); */
static void rararch_it_dtor(zend_object_iterator *iter TSRMLS_DC);
static void rararch_it_fetch(rararch_iterator *it TSRMLS_DC);
static int rararch_it_valid(zend_object_iterator *iter TSRMLS_DC);
static zval* rararch_it_current_data(zend_object_iterator *iter);
static void rararch_it_move_forward(zend_object_iterator *iter TSRMLS_DC);
static void rararch_it_rewind(zend_object_iterator *iter TSRMLS_DC);
/* }}} */

/* {{{ rararch_it_get_iterator */
static zend_object_iterator *rararch_it_get_iterator(zend_class_entry *ce,
													 zval *object,
													 int by_ref TSRMLS_DC)
{
	rararch_iterator	*it;
	rar_file_t			*rar;
	int					res;

	if (by_ref) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR,
			"An iterator cannot be used with foreach by reference");
	}

	it = emalloc(sizeof *it);

	res = _rar_get_file_resource_ex(object, &rar, 1 TSRMLS_CC);
	if (res == FAILURE)
		php_error_docref(NULL TSRMLS_CC, E_ERROR,
			"Cannot fetch RarArchive object");
	if (rar->arch_handle == NULL)
		php_error_docref(NULL TSRMLS_CC, E_ERROR,
			"The archive is already closed, cannot give an iterator");
	res = _rar_list_files(rar TSRMLS_CC);
	if (_rar_handle_error(res TSRMLS_CC) == FAILURE) {
		/* if it failed, do not expose the possibly incomplete entry list */
		it->empty_iterator = 1;
	}
	else
		it->empty_iterator = 0;

	zend_iterator_init(&it->parent);
	ZVAL_COPY(&it->parent.data, object);
	it->parent.funcs = ce->iterator_funcs.funcs;
	_rar_entry_search_start(rar, RAR_SEARCH_TRAVERSE, &it->state TSRMLS_CC);
	it->value = NULL;
	return &it->parent;
}
/* }}} */

/* {{{ rararch_it_invalidate_current */
static void rararch_it_invalidate_current(zend_object_iterator *iter TSRMLS_DC)
{
	rararch_iterator *it = (rararch_iterator *) iter;
	if (it->value != NULL) {
		zval_ptr_dtor(it->value);
		efree(it->value);
		it->value = NULL;
	}
}
/* }}} */

/* {{{ rararch_it_dtor */
static void rararch_it_dtor(zend_object_iterator *iter TSRMLS_DC)
{
	rararch_iterator *it = (rararch_iterator *) iter;

	rararch_it_invalidate_current(iter TSRMLS_CC);

	zval_ptr_dtor(&it->parent.data); /* decrease refcount on zval object */

	_rar_entry_search_end(it->state);
}
/* }}} */

/* {{{ rararch_it_fetch - populates it->current */
static void rararch_it_fetch(rararch_iterator *it TSRMLS_DC)
{
	rar_file_t	*rar_file;
	int			res;

	assert(it->value == NULL);

	if (it->empty_iterator) {
		it->value = emalloc(sizeof(zval));
		ZVAL_FALSE(it->value);
		return;
	}

	res = _rar_get_file_resource_ex(&(it->parent.data), &rar_file, 1 TSRMLS_CC);
	if (res == FAILURE)
		php_error_docref(NULL TSRMLS_CC, E_ERROR,
			"Cannot fetch RarArchive object");

	_rar_entry_search_advance(it->state, NULL, 0, 0);
	it->value = emalloc(sizeof(zval));
	if (it->state->found) {
		_rar_entry_to_zval(&(it->parent.data), it->state->header, it->state->packed_size, it->state->position, it->value TSRMLS_CC);
	} else {
		ZVAL_FALSE(it->value);	
	}	
}
/* }}} */

/* {{{ rararch_it_valid */
static int rararch_it_valid(zend_object_iterator *iter TSRMLS_DC)
{
	zval *value = ((rararch_iterator *) iter)->value;
	assert(value != NULL);
	return Z_TYPE_P(value) == IS_FALSE ? FAILURE : SUCCESS;
}
/* }}} */

/* {{{ rararch_it_current_data */
static zval* rararch_it_current_data(zend_object_iterator *iter)
{
	zval* value = ((rararch_iterator *) iter)->value;
	assert(value != NULL);
	return value;
}
/* }}} */

/* {{{ rararch_it_move_forward */
static void rararch_it_move_forward(zend_object_iterator *iter TSRMLS_DC)
{
	rararch_iterator *it = (rararch_iterator *) iter;
	rararch_it_invalidate_current((zend_object_iterator *) it TSRMLS_CC);
	rararch_it_fetch(it TSRMLS_CC);
}
/* }}} */

/* {{{ rararch_it_rewind */
static void rararch_it_rewind(zend_object_iterator *iter TSRMLS_DC)
{
	rararch_iterator *it = (rararch_iterator *) iter;
	rararch_it_invalidate_current((zend_object_iterator *) it TSRMLS_CC);
	_rar_entry_search_rewind(it->state);
	rararch_it_fetch(it TSRMLS_CC);
}
/* }}} */

/* iterator handler table */
static zend_object_iterator_funcs rararch_it_funcs = {
	rararch_it_dtor,
	rararch_it_valid,
	rararch_it_current_data,
	NULL,
	rararch_it_move_forward,
	rararch_it_rewind,
	rararch_it_invalidate_current
};
/* }}} */

void minit_rararch(TSRMLS_D)
{
	zend_class_entry ce;

	memcpy(&rararch_object_handlers, zend_get_std_object_handlers(),
		sizeof rararch_object_handlers);
	rararch_object_handlers.count_elements  = rararch_count_elements;
	rararch_object_handlers.read_dimension  = rararch_read_dimension;
	rararch_object_handlers.write_dimension = rararch_write_dimension;
	rararch_object_handlers.has_dimension   = rararch_has_dimension;
	rararch_object_handlers.unset_dimension = rararch_unset_dimension;

	INIT_CLASS_ENTRY(ce, "RarArchive", php_rararch_class_functions);
	rararch_ce_ptr = zend_register_internal_class(&ce TSRMLS_CC);
	rararch_ce_ptr->clone = NULL;
	rararch_ce_ptr->create_object = rararch_ce_create_object;
	rararch_ce_ptr->get_iterator = rararch_it_get_iterator;
	rararch_ce_ptr->iterator_funcs.funcs = &rararch_it_funcs;
	zend_class_implements(rararch_ce_ptr TSRMLS_CC, 1, zend_ce_traversable);
}

#ifdef __cplusplus
}
#endif
