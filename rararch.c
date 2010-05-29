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
	zend_object	parent;
	rar_file_t  *rar_file;
} ze_rararch_object;

typedef struct _rararch_iterator {
	zend_object_iterator	parent;
	rar_find_output			*state;
	zval					*value;	
} rararch_iterator;
/* }}} */

/* {{{ Globals with internal linkage */
static zend_class_entry *rararch_ce_ptr;
static zend_object_handlers rararch_object_handlers;
/* }}} */

/* {{{ Function prototypes for functions with internal linkage */
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

/* Creates a RarArchive object, all three in args will be dupped */
int _rar_create_rararch_obj(const char* resolved_path,
							const char* open_password,
							zval *volume_callback, //must be callable or NULL
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
	rar->extract_open_data->CmtBuf = NULL; //not interested in it again
	rar->cb_userdata.password = NULL;
	rar->cb_userdata.callable = NULL;
	rar->entries = NULL;

	rar->arch_handle = RAROpenArchiveEx(rar->list_open_data);
	if (rar->arch_handle != NULL && rar->list_open_data->OpenResult == 0) {
		ze_rararch_object *zobj;

		if (open_password != NULL) {
			rar->cb_userdata.password = estrdup(open_password);
		}
		if (volume_callback != NULL) {
			rar->cb_userdata.callable = volume_callback;
			zval_add_ref(&rar->cb_userdata.callable);
			SEPARATE_ZVAL(&rar->cb_userdata.callable);
		}
		
		object_init_ex(object, rararch_ce_ptr);
		zobj = zend_object_store_get_object(object TSRMLS_CC);
		zobj->rar_file = rar;
		rar->id = Z_OBJ_HANDLE_P(object);

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
int _rar_get_file_resource_ex(zval *zval_file, rar_file_t **rar_file, int silent TSRMLS_DC) /* {{{ */
{
	ze_rararch_object *zobj;
	zobj = zend_object_store_get_object(zval_file TSRMLS_CC);
	if (zobj == NULL) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
			"Could not find object in the store. This is a bug, please report it.");
		return FAILURE;
	}

	*rar_file = zobj->rar_file;
	if ((*rar_file)->arch_handle == NULL && !silent) { //rar_close was called
		_rar_handle_ext_error("The archive is already closed." TSRMLS_CC);
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
	zval			*rararch_obj;

	/* create zval to point to the RarArchive object) */
	MAKE_STD_ZVAL(rararch_obj);
	Z_TYPE_P(rararch_obj) = IS_OBJECT;
	Z_OBJ_HANDLE_P(rararch_obj) = rar->id;
	Z_OBJ_HT_P(rararch_obj) = &rararch_object_handlers;
	/* object has a new reference; if not incremented, the object would be
	 * be destroyed when this new zval we created was destroyed */
	zend_objects_store_add_ref_by_handle(rar->id TSRMLS_CC);

	_rar_entry_search_start(rar, RAR_SEARCH_TRAVERSE, &state TSRMLS_CC);
	do {
		_rar_entry_search_advance(state, NULL, 0, 0);
		if (state->found) {
			zval *entry_obj;

			MAKE_STD_ZVAL(entry_obj);
			_rar_entry_to_zval(rararch_obj, state->header, state->packed_size,
				state->position, entry_obj TSRMLS_CC);

			add_next_index_zval(target, entry_obj);
		}
	} while (state->eof == 0);
	_rar_entry_search_end(state);

	/* it was created with refcount=1 and incremented for each RarEntry object
	 * created, so we must decrease by one (this will also destroy it if
	 * there were no entries */
	zval_ptr_dtor(&rararch_obj);
}
/* }}} */

static zend_object_value rararch_ce_create_object(zend_class_entry *class_type TSRMLS_DC) /* {{{ */
{
	zend_object_value	zov;
	ze_rararch_object	*zobj;

	zobj = emalloc(sizeof *zobj);
	/* rararch_ce_free_object_storage will attempt to access it otherwise */
	zobj->rar_file = NULL;
	zend_object_std_init((zend_object*) zobj, class_type TSRMLS_CC);

#if PHP_VERSION_ID < 50399
	zend_hash_copy(((zend_object*)zobj)->properties,
		&(class_type->default_properties),
		(copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval*));
#else
	object_properties_init((zend_object*)zobj, class_type);
#endif
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

	/* can safely assume rar != NULL here. This function is not called
	 * if object construction fails */
	assert(rar != NULL);

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
	
	/* could call zend_objects_free_object_storage here (not before!), but
	 * instead I'll mimic its behaviour */
	zend_object_std_dtor((zend_object*) object TSRMLS_CC);
	efree(object);
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

	if (callable != NULL) { //given volume resolver callback
#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION == 2
		if (!zend_is_callable(callable, IS_CALLABLE_STRICT, NULL)) {
#else
		if (!zend_is_callable(callable, IS_CALLABLE_STRICT, NULL TSRMLS_CC)) {
#endif
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
	int result;

	if (file == NULL && zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O",
		&file, rararch_ce_ptr) == FAILURE) {
		return;
	}

	if (_rar_get_file_resource(file, &rar TSRMLS_CC) == FAILURE) {
		RETURN_FALSE;
	}

	if (rar->entries == NULL) {
		result = _rar_list_files(rar TSRMLS_CC); 
		if (_rar_handle_error(result TSRMLS_CC) == FAILURE) {
			RETURN_FALSE;
		}
	}
	
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
	int result;
	int filename_len;
	wchar_t *filename_c = NULL;
	rar_find_output *sstate;

	if (file == NULL) {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Os",
			&file, rararch_ce_ptr, &filename, &filename_len) == FAILURE) {
			return;
		}
	}
	else {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s",
			&filename, &filename_len) == FAILURE) {
			return;
		}
	}

	if (_rar_get_file_resource(file, &rar TSRMLS_CC) == FAILURE) {
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

	_rar_entry_search_start(rar, RAR_SEARCH_NAME, &sstate TSRMLS_CC);
	_rar_entry_search_advance(sstate, filename_c, 0, 0);
	if (sstate->found) {
		_rar_entry_to_zval(file, sstate->header, sstate->packed_size,
			sstate->position, return_value TSRMLS_CC);
	}
	else {
		_rar_handle_ext_error(
			"cannot find file \"%s\" in Rar archive \"%s\"."
			TSRMLS_CC, filename, rar->list_open_data->ArcName);
		RETVAL_FALSE;	
	}
	_rar_entry_search_end(sstate);

	efree(filename_c);
}
/* }}} */

/* {{{ proto string rar_solid_is(RarArchive rarfile)
   Return whether RAR archive is solid */
PHP_FUNCTION(rar_solid_is)
{
	zval *file = getThis();
	rar_file_t *rar = NULL;

	if (file == NULL && zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O",
		&file, rararch_ce_ptr) == FAILURE) {
		return;
	}

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

	if (file == NULL && zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O",
		&file, rararch_ce_ptr) == FAILURE) {
		return;
	}

	if (_rar_get_file_resource(file, &rar TSRMLS_CC) == FAILURE) {
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

	if (_rar_get_file_resource_ex(arch_obj, &rar, TRUE TSRMLS_CC) == FAILURE) {
		RETURN_FALSE;
	}

	is_closed = (rar->arch_handle == NULL);

	//2 is size of %s, 1 is terminating 0
	restring_size = (sizeof(format) - 1) - 2 * 2 + 1;
	restring_size += strlen(rar->list_open_data->ArcName);
	if (is_closed)
		restring_size += sizeof(closed) - 1;

	restring = emalloc(restring_size);
	snprintf(restring, restring_size, format, rar->list_open_data->ArcName,
		is_closed?closed:"");
	restring[restring_size - 1] = '\0'; //just to be safe
	
	RETURN_STRINGL(restring, (int) restring_size - 1, 0);
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

ZEND_BEGIN_ARG_INFO(arginfo_rararchive_void, 0)
ZEND_END_ARG_INFO()
/* }}} */

static zend_function_entry php_rararch_class_functions[] = {
	PHP_ME_MAPPING(open,		rar_open,			arginfo_rararchive_open,		ZEND_ACC_STATIC | ZEND_ACC_PUBLIC)
	PHP_ME_MAPPING(getEntries,	rar_list,			arginfo_rararchive_void,		ZEND_ACC_PUBLIC)
	PHP_ME_MAPPING(getEntry,	rar_entry_get,		arginfo_rararchive_getentry,	ZEND_ACC_PUBLIC)
#ifdef RAR_ARCHIVE_LIST_ALIAS
	PHP_ME_MAPPING(list,		rar_list,			arginfo_rararchive_void,		ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
#endif
	PHP_ME_MAPPING(isSolid,		rar_solid_is,		arginfo_rararchive_void,		ZEND_ACC_PUBLIC)
	PHP_ME_MAPPING(getComment,	rar_comment_get,	arginfo_rararchive_void,		ZEND_ACC_PUBLIC)
	PHP_ME_MAPPING(close,		rar_close,			arginfo_rararchive_void,		ZEND_ACC_PUBLIC)
	PHP_ME(rararch,				__toString,			arginfo_rararchive_void,		ZEND_ACC_PUBLIC)
	PHP_ME_MAPPING(__construct,	rar_bogus_ctor,		arginfo_rararchive_void,		ZEND_ACC_PRIVATE | ZEND_ACC_CTOR)
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
static void rararch_it_current_data(zend_object_iterator *iter,
									zval ***data TSRMLS_DC);
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
		zend_error(E_ERROR,
			"An iterator cannot be used with foreach by reference");
	}

	it = emalloc(sizeof *it);

	res = _rar_get_file_resource_ex(object, &rar, 1 TSRMLS_CC);
	if (res == FAILURE)
		zend_error(E_ERROR, "Cannot fetch RarArchive object");
	if (rar->arch_handle == NULL)
		zend_error(E_ERROR, "The archive is already closed, "
		"cannot give an iterator");
	if (rar->entries == NULL) {
		res = _rar_list_files(rar TSRMLS_CC); 
		if (_rar_handle_error(res TSRMLS_CC) == FAILURE) {
			/* do nothing, notice was already emitted */
		}
	}

	zval_add_ref(&object);
	it->parent.data = object;
	it->parent.funcs = ce->iterator_funcs.funcs;
	_rar_entry_search_start(rar, RAR_SEARCH_TRAVERSE, &it->state TSRMLS_CC);
	it->value = NULL;
	return (zend_object_iterator*) it;
}
/* }}} */

/* {{{ rararch_it_invalidate_current */
static void rararch_it_invalidate_current(zend_object_iterator *iter TSRMLS_DC)
{
	rararch_iterator *it = (rararch_iterator *) iter;
	if (it->value != NULL) {
		zval_ptr_dtor(&it->value);
		it->value = NULL;
	}
}
/* }}} */

/* {{{ rararch_it_dtor */
static void rararch_it_dtor(zend_object_iterator *iter TSRMLS_DC)
{
	rararch_iterator *it = (rararch_iterator *) iter;
	
	rararch_it_invalidate_current((zend_object_iterator *) it TSRMLS_CC);
	
	zval_ptr_dtor((zval**) &it->parent.data); /* decrease refcount on zval object */
	
	_rar_entry_search_end(it->state);
	efree(it);
}
/* }}} */

/* {{{ rararch_it_fetch - populates it->current */
static void rararch_it_fetch(rararch_iterator *it TSRMLS_DC)
{
	rar_file_t	*rar_file;
	int			res;

	assert(it->value == NULL);

	res = _rar_get_file_resource_ex(it->parent.data, &rar_file, 1 TSRMLS_CC);
	if (res == FAILURE)
		zend_error(E_ERROR, "Cannot fetch RarArchive object");

	_rar_entry_search_advance(it->state, NULL, 0, 0);
	MAKE_STD_ZVAL(it->value);
	if (it->state->found)
		_rar_entry_to_zval(it->parent.data, it->state->header,
			it->state->packed_size, it->state->position, it->value TSRMLS_CC);
	else
		ZVAL_FALSE(it->value);
}
/* }}} */

/* {{{ rararch_it_valid */
static int rararch_it_valid(zend_object_iterator *iter TSRMLS_DC)
{
	zval *value = ((rararch_iterator *) iter)->value;
	assert(value != NULL);
	return (Z_TYPE_P(value) != IS_BOOL)?SUCCESS:FAILURE;
}
/* }}} */

/* {{{ rararch_it_current_data */
static void rararch_it_current_data(zend_object_iterator *iter,
									zval ***data TSRMLS_DC)
{
	zval **value = &(((rararch_iterator *) iter)->value);
	assert(*value != NULL);
	*data = value;
}
/* }}} */

/* {{{ rararch_it_move_forward */
static void rararch_it_move_forward(zend_object_iterator *iter TSRMLS_DC)
{
	rararch_iterator *it = (rararch_iterator *) iter;
	rararch_it_invalidate_current((zend_object_iterator *) it TSRMLS_CC);
	it->value = NULL;
	rararch_it_fetch(it TSRMLS_CC);
}
/* }}} */

/* {{{ rararch_it_rewind */
static void rararch_it_rewind(zend_object_iterator *iter TSRMLS_DC)
{
	rararch_iterator *it = (rararch_iterator *) iter;
	rararch_it_invalidate_current((zend_object_iterator *) it TSRMLS_CC);
	_rar_entry_search_rewind(it->state);
	it->value = NULL;
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

	INIT_CLASS_ENTRY(ce, "RarArchive", php_rararch_class_functions);
	rararch_ce_ptr = zend_register_internal_class(&ce TSRMLS_CC);
	rararch_ce_ptr->ce_flags |= ZEND_ACC_FINAL_CLASS;
	rararch_ce_ptr->clone = NULL;
	rararch_ce_ptr->create_object = &rararch_ce_create_object;
	rararch_ce_ptr->get_iterator = rararch_it_get_iterator;
	rararch_ce_ptr->iterator_funcs.funcs = &rararch_it_funcs;
	zend_class_implements(rararch_ce_ptr TSRMLS_CC, 1, zend_ce_traversable);
}

#ifdef __cplusplus
}
#endif
