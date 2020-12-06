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

#include <php.h>
#include "php_rar.h"

/* {{{ Globals with external linkage */
zend_class_entry *rar_class_entry_ptr;
/* }}} */

/* {{{ Function prototypes for functions with internal linkage */
static int _rar_decl_priv_prop_null(zend_class_entry *ce, const char *name,
									 int name_length, char *doc_comment,
									 int doc_comment_len TSRMLS_DC);
static zval *_rar_entry_get_property(zval *entry_obj, char *name, int namelen TSRMLS_DC);
static void _rar_dos_date_to_text(unsigned dos_time, char *date_string);
/* }}} */

/* {{{ Functions with external linkage */
/* should be passed the last entry that corresponds to a given file
 * only that one has the correct CRC. Still, it may have a wrong packedSize */
/* parent is zval to RarArchive object. The object (not the zval, in PHP 5.x)
 * will have its refcount increased */
void _rar_entry_to_zval(zval *parent,
						struct RARHeaderDataEx *entry,
						unsigned long packed_size,
						size_t position,
						zval *object TSRMLS_DC)
/* {{{ */
{
	char tmp_s [MAX_LENGTH_OF_LONG + 1];
	char time[50];
	char *filename;
	int  filename_size,
		 filename_len;
	long unp_size; /* zval stores PHP ints as long, so use that here */
	zval *parent_copy = parent;
#if PHP_MAJOR_VERSION < 7
	/* allocate zval on the heap */
	zval_addref_p(parent_copy);
	SEPARATE_ZVAL(&parent_copy);
	/* set refcount to 0; zend_update_property will increase it */
	Z_DELREF_P(parent_copy);
#endif

	object_init_ex(object, rar_class_entry_ptr);
#if PHP_MAJOR_VERSION >= 8
	zend_object *obj = Z_OBJ_P(object);
#else
	zval *obj = object;
#endif

	zend_update_property(rar_class_entry_ptr, obj, "rarfile",
		sizeof("rararch") - 1, parent_copy TSRMLS_CC);

#if ULONG_MAX > 0xffffffffUL
	unp_size = ((long) entry->UnpSize) + (((long) entry->UnpSizeHigh) << 32);
#else
	/* for 32-bit long, at least don't give negative values */
	if ((unsigned long) entry->UnpSize > (unsigned long) LONG_MAX
			|| entry->UnpSizeHigh != 0)
		unp_size = LONG_MAX;
	else
		unp_size = (long) entry->UnpSize;
#endif

	filename_size = sizeof(entry->FileNameW) * 4;
	filename = (char*) emalloc(filename_size);

	if (packed_size > (unsigned long) LONG_MAX)
		packed_size = LONG_MAX;
	_rar_wide_to_utf(entry->FileNameW, filename, filename_size);
	/* OK; safe usage below: */
	filename_len = _rar_strnlen(filename, filename_size);
	/* we're not in class scope, so we cannot change the class private
	 * properties from here with add_property_x, or
	 * direct call to rarentry_object_handlers.write_property
	 * zend_update_property_x updates the scope accordingly */
	zend_update_property_long(rar_class_entry_ptr, obj, "position",
		sizeof("position") - 1, (long) position TSRMLS_CC);
	zend_update_property_stringl(rar_class_entry_ptr, obj, "name",
		sizeof("name") - 1, filename, filename_len TSRMLS_CC);
	zend_update_property_long(rar_class_entry_ptr, obj, "unpacked_size",
		sizeof("unpacked_size") - 1, unp_size TSRMLS_CC);
	zend_update_property_long(rar_class_entry_ptr, obj, "packed_size",
		sizeof("packed_size") - 1, packed_size TSRMLS_CC);
	zend_update_property_long(rar_class_entry_ptr, obj, "host_os",
		sizeof("host_os") - 1, entry->HostOS TSRMLS_CC);

	_rar_dos_date_to_text(entry->FileTime, time);
	zend_update_property_string(rar_class_entry_ptr, obj, "file_time",
		sizeof("file_time") - 1, time TSRMLS_CC);

	sprintf(tmp_s, "%x", entry->FileCRC);
	zend_update_property_string(rar_class_entry_ptr, obj, "crc",
		sizeof("crc") - 1, tmp_s TSRMLS_CC);

	zend_update_property_long(rar_class_entry_ptr, obj, "attr",
		sizeof("attr") - 1, entry->FileAttr TSRMLS_CC);
	zend_update_property_long(rar_class_entry_ptr, obj, "version",
		sizeof("version") - 1, entry->UnpVer TSRMLS_CC);
	zend_update_property_long(rar_class_entry_ptr, obj, "method",
		sizeof("method") - 1, entry->Method TSRMLS_CC);
	zend_update_property_long(rar_class_entry_ptr, obj, "flags",
		sizeof("flags") - 1, entry->Flags TSRMLS_CC);

	zend_update_property_long(rar_class_entry_ptr, obj, "redir_type",
		sizeof("redir_type") - 1, entry->RedirType TSRMLS_CC);

	if (entry->RedirName) {
		char *redir_target = NULL;
		size_t redir_target_size;

		zend_update_property_bool(rar_class_entry_ptr, obj,
			"redir_to_directory", sizeof("redir_to_directory") - 1,
			!!entry->DirTarget TSRMLS_CC);

		redir_target_size = entry->RedirNameSize * 4;
		redir_target =  emalloc(redir_target_size);
		assert(redir_target_size > 0);
		_rar_wide_to_utf(entry->RedirName, redir_target, redir_target_size);

		zend_update_property_string(rar_class_entry_ptr, obj, "redir_target",
			sizeof("redir_target") - 1, redir_target TSRMLS_CC);

		efree(redir_target);
	}

	efree(filename);
}
/* }}} */
/* }}} */

/* {{{ Helper functions and preprocessor definitions */
#define RAR_GET_PROPERTY(var, prop_name) \
	if (!entry_obj) { \
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "this method cannot be called statically"); \
		RETURN_FALSE; \
	} \
	if ((var = _rar_entry_get_property(entry_obj, prop_name, \
			sizeof(prop_name) - 1 TSRMLS_CC)) == NULL) { \
		RETURN_FALSE; \
	}

#define REG_RAR_CLASS_CONST_LONG(const_name, value) \
	zend_declare_class_constant_long(rar_class_entry_ptr, const_name, \
		sizeof(const_name) - 1, (long) value TSRMLS_CC)

#define REG_RAR_PROPERTY(name, comment) \
	_rar_decl_priv_prop_null(rar_class_entry_ptr, name, sizeof(name) -1, \
		comment, sizeof(comment) - 1 TSRMLS_CC)

static int _rar_decl_priv_prop_null(zend_class_entry *ce, const char *name,
									 int name_length, char *doc_comment,
									 int doc_comment_len TSRMLS_DC) /* {{{ */
{
#if PHP_MAJOR_VERSION < 7
	zval *property;
	ALLOC_PERMANENT_ZVAL(property);
	INIT_ZVAL(*property);
	return zend_declare_property_ex(ce, name, name_length, property,
		ZEND_ACC_PRIVATE, doc_comment, doc_comment_len TSRMLS_CC);
#else
	zval property;
	zend_string *name_str,
				*doc_str;
	int ret;

	ZVAL_NULL(&property);
	name_str = zend_string_init(name, (size_t) name_length, 1);
	doc_str = zend_string_init(doc_comment, (size_t) doc_comment_len, 1);
# if PHP_MAJOR_VERSION >= 8
	zend_declare_property_ex(ce, name_str, &property, ZEND_ACC_PRIVATE,
							 doc_str);
	ret = SUCCESS;
# else
	ret = zend_declare_property_ex(ce, name_str, &property, ZEND_ACC_PRIVATE,
								   doc_str);
#endif
	zend_string_release(name_str);
	zend_string_release(doc_str);
	return ret;
#endif
}
/* }}} */

static zval *_rar_entry_get_property(zval *entry_obj, char *name, int namelen TSRMLS_DC) /* {{{ */
{
	zval *tmp;
#if PHP_MAJOR_VERSION >= 7
	zval zv;
#endif
#if PHP_VERSION_ID < 70100
	zend_class_entry *orig_scope = EG(scope);

	EG(scope) = rar_class_entry_ptr;
#endif

#if PHP_MAJOR_VERSION >= 8
	tmp = zend_read_property(Z_OBJCE_P(entry_obj), Z_OBJ_P(entry_obj), name, namelen, 1, &zv);
#elif PHP_MAJOR_VERSION >= 7
	tmp = zend_read_property(Z_OBJCE_P(entry_obj), entry_obj, name, namelen, 1, &zv);
#else
	tmp = zend_read_property(Z_OBJCE_P(entry_obj), entry_obj, name, namelen, 1 TSRMLS_CC);
#endif
	if (tmp == NULL) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
			"Bug: unable to find property '%s'. Please report.", name);
	}

#if PHP_VERSION_ID < 70100
	EG(scope) = orig_scope;
#endif

	return tmp;
}
/* }}} */

static void _rar_dos_date_to_text(unsigned dos_time, char *date_string) /* {{{ */
{
	time_t time = 0;
	struct tm tm = {0};
	int res;

	res = rar_dos_time_convert(dos_time, &time) != FAILURE &&
		php_gmtime_r(&time, &tm) != NULL;

	if (!res) {
		sprintf(date_string, "%s", "time conversion failure");
	}

	sprintf(date_string, "%u-%02u-%02u %02u:%02u:%02u",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min,
		tm.tm_sec);
}
/* }}} */
/* }}} */

/* {{{ Methods */
/* {{{ proto bool RarEntry::extract(string dir [, string filepath = ''
       [, string password = NULL [, bool extended_data  = FALSE]])
   Extract file from the archive */
PHP_METHOD(rarentry, extract)
{ /* lots of variables, but no need to be intimidated */
	char					*dir,
							*filepath = NULL,
							*password = NULL;
	zpp_s_size_t			dir_len,
							filepath_len = 0,
							password_len = 0;
	char					*considered_path;
	char					considered_path_res[MAXPATHLEN];
	int						with_second_arg;
	zend_bool				process_ed = 0;

	zval					*tmp,
							*tmp_position;
	rar_file_t				*rar = NULL;
	zval					*entry_obj = getThis();
	struct RARHeaderDataEx	entry;
	HANDLE					extract_handle = NULL;
	int						result;
	int						found;
	/* gotta have a new copy (shallow is enough) because we may want to use a
	 * password that's different from the one stored in the rar_file_t object*/
	rar_cb_user_data		cb_udata = {NULL};

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|ss!b", &dir,
			&dir_len, &filepath, &filepath_len, &password, &password_len,
			&process_ed) == FAILURE ) {
		return;
	}

	RAR_GET_PROPERTY(tmp, "rarfile");
	if (_rar_get_file_resource_zv(tmp, &rar TSRMLS_CC) == FAILURE) {
		RETURN_FALSE;
	}

	/* Decide where to extract */
	with_second_arg = (filepath_len != 0);

	/* the arguments are mutually exclusive.
	 * If the second is specified, we ignore the first */
	if (!with_second_arg) {
		if (dir_len == 0) /* both params empty */
			dir = ".";
		considered_path = dir;
	}
	else {
		considered_path = filepath;
	}

	if (OPENBASEDIR_CHECKPATH(considered_path)) {
		RETURN_FALSE;
	}
	if (!expand_filepath(considered_path, considered_path_res TSRMLS_CC)) {
		RETURN_FALSE;
	}

	/* Find file inside archive */
	RAR_GET_PROPERTY(tmp_position, "position");

	/* don't set the new password now because maybe the headers are
	 * encrypted with a password different from this file's (though WinRAR
	 * does not support that: if you encrypt the headers, you must encrypt
	 * the files with the same password). By not replacing the password
	 * now, we're using the password given to rar_open, if any (which must
	 * have enabled decrypting the headers or else we wouldn't be here) */
	memcpy(&cb_udata, &rar->cb_userdata, sizeof cb_udata);

	result = _rar_find_file_p(rar->extract_open_data,
		(size_t) Z_LVAL_P(tmp_position), &cb_udata, &extract_handle, &found,
		&entry);

	if (_rar_handle_error(result TSRMLS_CC) == FAILURE) {
		RETVAL_FALSE;
		goto cleanup;
	}

	if (!found) {
		_rar_handle_ext_error("Can't find file with index %d in archive %s"
			TSRMLS_CC, Z_LVAL_P(tmp_position),
			rar->extract_open_data->ArcName);
		RETVAL_FALSE;
		goto cleanup;
	}

	RARSetProcessExtendedData(extract_handle, (int) process_ed);

	/* now use the password given to this method. If none was given, we're
	 * still stuck with the password given to rar_open, if any */
	if (password != NULL)
		cb_udata.password = password;

	/* Do extraction */
	if (!with_second_arg)
		result = RARProcessFile(extract_handle, RAR_EXTRACT,
			considered_path_res, NULL);
	else
		result = RARProcessFile(extract_handle, RAR_EXTRACT,
			NULL, considered_path_res);

	if (_rar_handle_error(result TSRMLS_CC) == FAILURE) {
		RETVAL_FALSE;
	}
	else {
		RETVAL_TRUE;
	}

cleanup:
	if (extract_handle != NULL)
		RARCloseArchive(extract_handle);
}
/* }}} */

/* {{{ proto int RarEntry::getPosiiton()
   Return position for the entry */
PHP_METHOD(rarentry, getPosition)
{
	zval *tmp;
	zval *entry_obj = getThis();

	RAR_RETNULL_ON_ARGS();

	RAR_GET_PROPERTY(tmp, "position");

	RETURN_LONG(Z_LVAL_P(tmp));
}
/* }}} */

/* {{{ proto string RarEntry::getName()
   Return entry name */
PHP_METHOD(rarentry, getName)
{
	zval *tmp;
	zval *entry_obj = getThis();

	RAR_RETNULL_ON_ARGS();

	RAR_GET_PROPERTY(tmp, "name");

	RAR_RETURN_STRINGL(Z_STRVAL_P(tmp), Z_STRLEN_P(tmp), 1);
}
/* }}} */

/* {{{ proto int RarEntry::getUnpackedSize()
   Return unpacked size of the entry */
PHP_METHOD(rarentry, getUnpackedSize)
{
	zval *tmp;
	zval *entry_obj = getThis();

	RAR_RETNULL_ON_ARGS();

	RAR_GET_PROPERTY(tmp, "unpacked_size");

	RETURN_LONG(Z_LVAL_P(tmp));
}
/* }}} */

/* {{{ proto int RarEntry::getPackedSize()
   Return packed size of the entry */
PHP_METHOD(rarentry, getPackedSize)
{
	zval *tmp;
	zval *entry_obj = getThis();

	RAR_RETNULL_ON_ARGS();

	RAR_GET_PROPERTY(tmp, "packed_size");

	RETURN_LONG(Z_LVAL_P(tmp));
}
/* }}} */

/* {{{ proto int RarEntry::getHostOs()
   Return host OS of the entry */
PHP_METHOD(rarentry, getHostOs)
{
	zval *tmp;
	zval *entry_obj = getThis();

	RAR_RETNULL_ON_ARGS();

	RAR_GET_PROPERTY(tmp, "host_os");

	RETURN_LONG(Z_LVAL_P(tmp));
}
/* }}} */

/* {{{ proto string RarEntry::getFileTime()
   Return modification time of the entry.
   Due to the way the unrar library returns this time, this is in the
   system's timezone. */
PHP_METHOD(rarentry, getFileTime)
{
	zval *tmp;
	zval *entry_obj = getThis();

	RAR_RETNULL_ON_ARGS();

	RAR_GET_PROPERTY(tmp, "file_time");

	RAR_RETURN_STRINGL(Z_STRVAL_P(tmp), Z_STRLEN_P(tmp), 1);
}
/* }}} */

/* {{{ proto string RarEntry::getCrc()
   Return CRC of the entry */
PHP_METHOD(rarentry, getCrc)
{
	zval *tmp;
	zval *entry_obj = getThis();

	RAR_RETNULL_ON_ARGS();

	RAR_GET_PROPERTY(tmp, "crc");

	RAR_RETURN_STRINGL(Z_STRVAL_P(tmp), Z_STRLEN_P(tmp), 1);
}
/* }}} */

/* {{{ proto int RarEntry::getAttr()
   Return attributes of the entry */
PHP_METHOD(rarentry, getAttr)
{
	zval *tmp;
	zval *entry_obj = getThis();

	RAR_RETNULL_ON_ARGS();

	RAR_GET_PROPERTY(tmp, "attr");

	RETURN_LONG(Z_LVAL_P(tmp));
}
/* }}} */

/* {{{ proto int RarEntry::getVersion()
   Return version of the archiver, used to create this entry */
PHP_METHOD(rarentry, getVersion)
{
	zval *tmp;
	zval *entry_obj = getThis();

	RAR_RETNULL_ON_ARGS();

	RAR_GET_PROPERTY(tmp, "version");

	RETURN_LONG(Z_LVAL_P(tmp));
}
/* }}} */

/* {{{ proto int RarEntry::getMethod()
   Return packing method */
PHP_METHOD(rarentry, getMethod)
{
	zval *tmp;
	zval *entry_obj = getThis();

	RAR_RETNULL_ON_ARGS();

	RAR_GET_PROPERTY(tmp, "method");

	RETURN_LONG(Z_LVAL_P(tmp));
}
/* }}} */

/* {{{ proto resource RarEntry::getStream([string password = NULL])
   Return stream for current entry */
PHP_METHOD(rarentry, getStream)
{
	zval				*tmp,
						*position;
	rar_file_t			*rar = NULL;
	zval				*entry_obj = getThis();
	php_stream			*stream = NULL;
	char				*password = NULL;
	zpp_s_size_t		password_len; /* ignored */
	rar_cb_user_data	cb_udata = {NULL};


	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s!",
			&password, &password_len) == FAILURE ) {
		return;
	}

	RAR_GET_PROPERTY(position, "position");
	RAR_GET_PROPERTY(tmp, "rarfile");
	if (_rar_get_file_resource_zv(tmp, &rar TSRMLS_CC) == FAILURE) {
		RETURN_FALSE;
	}

	/* use rar_open password (stored in rar->cb_userdata) by default */
	memcpy(&cb_udata, &rar->cb_userdata, sizeof cb_udata);
	if (password != NULL)
		cb_udata.password = password;

	/* doesn't matter that cb_udata is stack allocated, it will be copied */
	stream = php_stream_rar_open(rar->extract_open_data->ArcName,
		Z_LVAL_P(position), &cb_udata STREAMS_CC TSRMLS_CC);

	if (stream != NULL) {
		php_stream_to_zval(stream, return_value);
	}
  else
    RETVAL_FALSE;
}
/* }}} */

/* {{{ proto int RarEntry::isDirectory()
   Return whether the entry represents a directory */
PHP_METHOD(rarentry, isDirectory)
{
	zval *tmp;
	zval *entry_obj = getThis();
	long flags;
	int is_dir;

	RAR_RETNULL_ON_ARGS();

	RAR_GET_PROPERTY(tmp, "flags");
	flags = Z_LVAL_P(tmp);
	is_dir = (flags & RHDF_DIRECTORY) != 0;

	RETURN_BOOL(is_dir);
}
/* }}} */

/* {{{ proto int RarEntry::isEncrypted()
   Return whether the entry is encrypted and needs a password */
PHP_METHOD(rarentry, isEncrypted)
{
	zval *tmp;
	zval *entry_obj = getThis();
	long flags;
	int is_encrypted;

	RAR_RETNULL_ON_ARGS();

	RAR_GET_PROPERTY(tmp, "flags");
	flags = Z_LVAL_P(tmp);
	is_encrypted = (flags & RHDF_ENCRYPTED) != 0;

	RETURN_BOOL(is_encrypted);
}
/* }}} */

/* {{{ proto int RarEntry::getRedirType()
   Returns the redirection type, or NULL if there's none */
PHP_METHOD(rarentry, getRedirType)
{
	zval *tmp;
	zval *entry_obj = getThis();

	RAR_RETNULL_ON_ARGS();

	RAR_GET_PROPERTY(tmp, "redir_type");
	if (Z_TYPE_P(tmp) != IS_LONG) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "bad redir type stored");
		RETURN_FALSE;
	}

	if (Z_LVAL_P(tmp) == FSREDIR_NONE) {
		RETURN_NULL();
	}

	RETURN_LONG(Z_LVAL_P(tmp));
}
/* }}} */

/* {{{ proto bool RarEntry::isRedirectToDirectory()
   Returns true if there is redirection and the target is a directory,
   null if there is no redirection, false otherwise */
PHP_METHOD(rarentry, isRedirectToDirectory)
{
	zval *tmp;
	zval *entry_obj = getThis();

	RAR_RETNULL_ON_ARGS();

	RAR_GET_PROPERTY(tmp, "redir_to_directory");

	RETURN_ZVAL(tmp, 1, 0);
}
/* }}} */

/* {{{ proto bool RarEntry::getRedirTarget()
   Returns the redirection target, encoded as UTF-8, or NULL */
PHP_METHOD(rarentry, getRedirTarget)
{
	zval *tmp;
	zval *entry_obj = getThis();

	RAR_RETNULL_ON_ARGS();

	RAR_GET_PROPERTY(tmp, "redir_target");

	RETURN_ZVAL(tmp, 1, 0);
}
/* }}} */

/* {{{ proto string RarEntry::__toString()
   Return string representation for entry */
PHP_METHOD(rarentry, __toString)
{
	zval		*flags_zval,
				*name_zval,
				*crc_zval;
	zval		*entry_obj = getThis();
	long		flags;
	int			is_dir;
	char		*name,
				*crc;
	char		*restring;
	int			restring_size;
	const char	format[] = "RarEntry for %s \"%s\" (%s)";

	RAR_RETNULL_ON_ARGS();

	RAR_GET_PROPERTY(flags_zval, "flags");
	flags = Z_LVAL_P(flags_zval);
	is_dir = flags & RHDF_DIRECTORY;

	RAR_GET_PROPERTY(name_zval, "name");
	name = Z_STRVAL_P(name_zval);

	RAR_GET_PROPERTY(crc_zval, "crc");
	crc = Z_STRVAL_P(crc_zval);

	/* 2 is size of %s, 8 is size of crc */
	restring_size = (sizeof(format)-1) - 2 * 3 + (sizeof("directory")-1) +
		strlen(name) + 8 + 1;
	restring = emalloc(restring_size);
	snprintf(restring, restring_size, format, is_dir?"directory":"file",
		name, crc);
	restring[restring_size - 1] = '\0'; /* just to be safe */

	RAR_RETURN_STRINGL(restring, strlen(restring), 0);
}
/* }}} */
/* }}} */

/* {{{ arginfo */
ZEND_BEGIN_ARG_INFO_EX(arginfo_rarentry_extract, 0, 0, 1)
	ZEND_ARG_INFO(0, path)
	ZEND_ARG_INFO(0, filename)
	ZEND_ARG_INFO(0, password)
	ZEND_ARG_INFO(0, extended_data)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_rarentry_getstream, 0, 0, 0)
	ZEND_ARG_INFO(0, password)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_rar_void, 0)
ZEND_END_ARG_INFO()
/* }}} */

static zend_function_entry php_rar_class_functions[] = {
	PHP_ME(rarentry,		extract,			arginfo_rarentry_extract,	ZEND_ACC_PUBLIC)
	PHP_ME(rarentry,		getPosition,		arginfo_rar_void,	ZEND_ACC_PUBLIC)
	PHP_ME(rarentry,		getName,			arginfo_rar_void,	ZEND_ACC_PUBLIC)
	PHP_ME(rarentry,		getUnpackedSize,	arginfo_rar_void,	ZEND_ACC_PUBLIC)
	PHP_ME(rarentry,		getPackedSize,		arginfo_rar_void,	ZEND_ACC_PUBLIC)
	PHP_ME(rarentry,		getHostOs,			arginfo_rar_void,	ZEND_ACC_PUBLIC)
	PHP_ME(rarentry,		getFileTime,		arginfo_rar_void,	ZEND_ACC_PUBLIC)
	PHP_ME(rarentry,		getCrc,				arginfo_rar_void,	ZEND_ACC_PUBLIC)
	PHP_ME(rarentry,		getAttr,			arginfo_rar_void,	ZEND_ACC_PUBLIC)
	PHP_ME(rarentry,		getVersion,			arginfo_rar_void,	ZEND_ACC_PUBLIC)
	PHP_ME(rarentry,		getMethod,			arginfo_rar_void,	ZEND_ACC_PUBLIC)
	PHP_ME(rarentry,		getStream,			arginfo_rarentry_getstream,	ZEND_ACC_PUBLIC)
	PHP_ME(rarentry,		isDirectory,		arginfo_rar_void,	ZEND_ACC_PUBLIC)
	PHP_ME(rarentry,		isEncrypted,		arginfo_rar_void,	ZEND_ACC_PUBLIC)
	PHP_ME(rarentry,		getRedirType,		arginfo_rar_void,	ZEND_ACC_PUBLIC)
	PHP_ME(rarentry,		isRedirectToDirectory,	arginfo_rar_void,	ZEND_ACC_PUBLIC)
	PHP_ME(rarentry,		getRedirTarget,	arginfo_rar_void,	ZEND_ACC_PUBLIC)
	PHP_ME(rarentry,		__toString,			arginfo_rar_void,	ZEND_ACC_PUBLIC)
	PHP_ME_MAPPING(__construct,	rar_bogus_ctor,	arginfo_rar_void,	ZEND_ACC_PRIVATE | ZEND_ACC_CTOR)
	{NULL, NULL, NULL}
};

void minit_rarentry(TSRMLS_D)
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "RarEntry", php_rar_class_functions);
	rar_class_entry_ptr = zend_register_internal_class(&ce TSRMLS_CC);
	rar_class_entry_ptr->ce_flags |= ZEND_ACC_FINAL_CLASS;
	rar_class_entry_ptr->clone = NULL;

	REG_RAR_PROPERTY("rarfile", "Associated RAR archive");
	REG_RAR_PROPERTY("position", "Position inside the RAR archive");
	REG_RAR_PROPERTY("name", "File or directory name with path");
	REG_RAR_PROPERTY("unpacked_size", "Size of file when unpacked");
	REG_RAR_PROPERTY("packed_size", "Size of the packed file inside the archive");
	REG_RAR_PROPERTY("host_os", "OS used to pack the file");
	REG_RAR_PROPERTY("file_time", "Entry's time of last modification");
	REG_RAR_PROPERTY("crc", "CRC checksum for the unpacked file");
	REG_RAR_PROPERTY("attr", "OS-dependent file attributes");
	REG_RAR_PROPERTY("version", "RAR version needed to extract entry");
	REG_RAR_PROPERTY("method", "Identifier for packing method");
	REG_RAR_PROPERTY("flags", "Entry header flags");
	REG_RAR_PROPERTY("redir_type", "The type of redirection or NULL");
	REG_RAR_PROPERTY("redir_to_directory", "Whether the redirection target is a directory");
	REG_RAR_PROPERTY("redir_target", "Target of the redirectory");

	REG_RAR_CLASS_CONST_LONG("HOST_MSDOS",	HOST_MSDOS);
	REG_RAR_CLASS_CONST_LONG("HOST_OS2",	HOST_OS2);
	REG_RAR_CLASS_CONST_LONG("HOST_WIN32",	HOST_WIN32);
	REG_RAR_CLASS_CONST_LONG("HOST_UNIX",	HOST_UNIX);
	REG_RAR_CLASS_CONST_LONG("HOST_MACOS",	HOST_MACOS);
	REG_RAR_CLASS_CONST_LONG("HOST_BEOS",	HOST_BEOS);

	REG_RAR_CLASS_CONST_LONG("FSREDIR_UNIXSYMLINK",	FSREDIR_UNIXSYMLINK);
	REG_RAR_CLASS_CONST_LONG("FSREDIR_WINSYMLINK",	FSREDIR_WINSYMLINK);
	REG_RAR_CLASS_CONST_LONG("FSREDIR_JUNCTION",	FSREDIR_JUNCTION);
	REG_RAR_CLASS_CONST_LONG("FSREDIR_HARDLINK",	FSREDIR_HARDLINK);
	REG_RAR_CLASS_CONST_LONG("FSREDIR_FILECOPY",	FSREDIR_FILECOPY);

	/* see WinNT.h */
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_WIN_READONLY",				0x00001L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_WIN_HIDDEN",				0x00002L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_WIN_SYSTEM",				0x00004L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_WIN_DIRECTORY",				0x00010L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_WIN_ARCHIVE",				0x00020L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_WIN_DEVICE",				0x00040L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_WIN_NORMAL",				0x00080L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_WIN_TEMPORARY",				0x00100L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_WIN_SPARSE_FILE",			0x00200L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_WIN_REPARSE_POINT",			0x00400L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_WIN_COMPRESSED",			0x00800L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_WIN_OFFLINE",				0x01000L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_WIN_NOT_CONTENT_INDEXED",	0x02000L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_WIN_ENCRYPTED",				0x04000L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_WIN_VIRTUAL",				0x10000L);

	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_UNIX_WORLD_EXECUTE",		0x00001L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_UNIX_WORLD_WRITE",			0x00002L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_UNIX_WORLD_READ",			0x00004L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_UNIX_GROUP_EXECUTE",		0x00008L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_UNIX_GROUP_WRITE",			0x00010L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_UNIX_GROUP_READ",			0x00020L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_UNIX_OWNER_EXECUTE",		0x00040L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_UNIX_OWNER_WRITE",			0x00080L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_UNIX_OWNER_READ",			0x00100L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_UNIX_STICKY",				0x00200L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_UNIX_SETGID",				0x00400L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_UNIX_SETUID",				0x00800L);

	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_UNIX_FINAL_QUARTET",		0x0F000L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_UNIX_FIFO",					0x01000L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_UNIX_CHAR_DEV",				0x02000L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_UNIX_DIRECTORY",			0x04000L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_UNIX_BLOCK_DEV",			0x06000L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_UNIX_REGULAR_FILE",			0x08000L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_UNIX_SYM_LINK",				0x0A000L);
	REG_RAR_CLASS_CONST_LONG("ATTRIBUTE_UNIX_SOCKET",				0x0C000L);
}

#ifdef __cplusplus
}
#endif
