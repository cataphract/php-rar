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
static void _rar_dos_date_to_text(int dos_time, char *date_string);

/* }}} */

/* {{{ Functions with external linkage */
/* should be passed the last entry that corresponds to a given file
 * only that one has the correct CRC. Still, it may have a wrong packedSize */
void _rar_entry_to_zval(zval *parent, /* zval to RarArchive object, will have its refcount increased */
						struct RARHeaderDataEx *entry,
						unsigned long packed_size,
						size_t position,
						zval *object TSRMLS_DC) /* {{{ */
{
	char tmp_s [MAX_LENGTH_OF_LONG + 1];
	char time[50];
	char *filename;
	int  filename_size, filename_len;
	long unp_size; /* zval stores PHP ints as long, so use that here */

	object_init_ex(object, rar_class_entry_ptr);
	zend_update_property(rar_class_entry_ptr, object, "rarfile",
		sizeof("rararch") - 1, parent TSRMLS_CC);

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

	filename_size = sizeof(entry->FileNameW) * sizeof(wchar_t);
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
	zend_update_property_long(rar_class_entry_ptr, object, "position",
		sizeof("position") - 1, (long) position TSRMLS_CC);
	zend_update_property_stringl(rar_class_entry_ptr, object, "name",
		sizeof("name") - 1, filename, filename_len TSRMLS_CC);
	zend_update_property_long(rar_class_entry_ptr, object, "unpacked_size",
		sizeof("unpacked_size") - 1, unp_size TSRMLS_CC);
	zend_update_property_long(rar_class_entry_ptr, object, "packed_size",
		sizeof("packed_size") - 1, packed_size TSRMLS_CC);
	zend_update_property_long(rar_class_entry_ptr, object, "host_os",
		sizeof("host_os") - 1, entry->HostOS TSRMLS_CC);

	_rar_dos_date_to_text(entry->FileTime, time);
	zend_update_property_string(rar_class_entry_ptr, object, "file_time",
		sizeof("file_time") - 1, time TSRMLS_CC);

	sprintf(tmp_s, "%x", entry->FileCRC);
	zend_update_property_string(rar_class_entry_ptr, object, "crc",
		sizeof("crc") - 1, tmp_s TSRMLS_CC);

	zend_update_property_long(rar_class_entry_ptr, object, "attr",
		sizeof("attr") - 1, entry->FileAttr TSRMLS_CC);
	zend_update_property_long(rar_class_entry_ptr, object, "version",
		sizeof("version") - 1, entry->UnpVer TSRMLS_CC);
	zend_update_property_long(rar_class_entry_ptr, object, "method",
		sizeof("method") - 1, entry->Method TSRMLS_CC);
	zend_update_property_long(rar_class_entry_ptr, object, "flags",
		sizeof("flags") - 1, entry->Flags TSRMLS_CC);

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
		sizeof(const_name) - 1, (zend_long) value TSRMLS_CC)

#define REG_RAR_PROPERTY(name, comment) \
	_rar_decl_priv_prop_null(rar_class_entry_ptr, name, sizeof(name) -1, \
		comment, sizeof(comment) - 1 TSRMLS_CC)

static int _rar_decl_priv_prop_null(zend_class_entry *ce, const char *name,
									 int name_length, char *doc_comment,
									 int doc_comment_len TSRMLS_DC) /* {{{ */
{
	zval property;
	ZVAL_NULL(&property);
	zend_string *name_string = zend_string_init(name, name_length, ce->type & ZEND_INTERNAL_CLASS);
	zend_string *comment_string = zend_string_init(doc_comment, doc_comment_len, ce->type & ZEND_INTERNAL_CLASS);
	int result = zend_declare_property_ex(ce, name_string, &property, ZEND_ACC_PRIVATE, comment_string);
	zend_string_release(name_string);
	zend_string_release(comment_string);
	return result;
}
/* }}} */

static zval *_rar_entry_get_property(zval *entry_obj, char *name, int namelen TSRMLS_DC) /* {{{ */
{
	zval *tmp, rv;
	zend_class_entry *orig_scope = EG(scope);

	EG(scope) = rar_class_entry_ptr;

	tmp = zend_read_property(Z_OBJCE_P(entry_obj), entry_obj, name, namelen, 1 TSRMLS_CC, &rv);
	if (tmp == NULL) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
			"Bug: unable to find property '%s'. Please report.", name);
	}

	EG(scope) = orig_scope;

	return tmp;
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
	int						dir_len,
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
	if (_rar_get_file_resource(tmp, &rar TSRMLS_CC) == FAILURE) {
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

	RETURN_STRINGL(Z_STRVAL_P(tmp), Z_STRLEN_P(tmp));
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
   Return modification time of the entry */
PHP_METHOD(rarentry, getFileTime)
{
	zval *tmp;
	zval *entry_obj = getThis();

	RAR_RETNULL_ON_ARGS();

	RAR_GET_PROPERTY(tmp, "file_time");

	RETURN_STRINGL(Z_STRVAL_P(tmp), Z_STRLEN_P(tmp));
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

	RETURN_STRINGL(Z_STRVAL_P(tmp), Z_STRLEN_P(tmp));
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
	int					password_len; /* ignored */
	rar_cb_user_data	cb_udata = {NULL};


	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s!",
			&password, &password_len) == FAILURE ) {
		return;
	}

	RAR_GET_PROPERTY(position, "position");
	RAR_GET_PROPERTY(tmp, "rarfile");
	if (_rar_get_file_resource(tmp, &rar TSRMLS_CC) == FAILURE) {
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
	is_dir = ((flags & LHD_WINDOWMASK) == LHD_DIRECTORY);

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
	is_encrypted = (flags & 0x04);

	RETURN_BOOL(is_encrypted);
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
	int			restring_len;
	const char	format[] = "RarEntry for %s \"%s\" (%s)";

	RAR_RETNULL_ON_ARGS();

	RAR_GET_PROPERTY(flags_zval, "flags");
	flags = Z_LVAL_P(flags_zval);
	is_dir = ((flags & 0xE0) == 0xE0);

	RAR_GET_PROPERTY(name_zval, "name");
	name = Z_STRVAL_P(name_zval);

	RAR_GET_PROPERTY(crc_zval, "crc");
	crc = Z_STRVAL_P(crc_zval);

	/* 2 is size of %s, 8 is size of crc */
	restring_len = (sizeof(format)-1) - 2 * 3 + (sizeof("directory")-1) +
		strlen(name) + 8 + 1;
	restring = emalloc(restring_len);
	snprintf(restring, restring_len, format, is_dir?"directory":"file",
		name, crc);
	restring[restring_len - 1] = '\0'; /* just to be safe */

	RETVAL_STRING(restring); 
	efree(restring);
	return;
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
	PHP_ME(rarentry,		__toString,			arginfo_rar_void,	ZEND_ACC_PUBLIC)
	PHP_ME_MAPPING(__construct,	rar_bogus_ctor,	arginfo_rar_void,	ZEND_ACC_PRIVATE | ZEND_ACC_CTOR)
	{NULL, NULL, NULL}
};

void minit_rarentry(TSRMLS_D)
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "RarEntry", php_rar_class_functions);
	rar_class_entry_ptr = zend_register_internal_class(&ce TSRMLS_CC);
	rar_class_entry_ptr->clone = NULL;
	/* Custom creation currently not really needed, but you never know... */
	rar_class_entry_ptr->create_object = zend_objects_new;

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

	REG_RAR_CLASS_CONST_LONG("HOST_MSDOS",	HOST_MSDOS);
	REG_RAR_CLASS_CONST_LONG("HOST_OS2",	HOST_OS2);
	REG_RAR_CLASS_CONST_LONG("HOST_WIN32",	HOST_WIN32);
	REG_RAR_CLASS_CONST_LONG("HOST_UNIX",	HOST_UNIX);
	REG_RAR_CLASS_CONST_LONG("HOST_MACOS",	HOST_MACOS);
	REG_RAR_CLASS_CONST_LONG("HOST_BEOS",	HOST_BEOS);

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
