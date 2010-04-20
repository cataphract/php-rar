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
  | Its license states, that you MUST NOT use its code to develop        |
  | a RAR (WinRAR) compatible archiver.                                  |
  | Please, read unRAR license for full information.                     |
  | unRAR & RAR copyrights are owned by Eugene Roshal                    |
  +----------------------------------------------------------------------+
  | Author: Antony Dovgal <tony@daylessday.org>                          |
  | Author: Gustavo Lopes <cataphract@php.net>                           |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_RAR_H
#define PHP_RAR_H

extern zend_module_entry rar_module_entry;
#define phpext_rar_ptr &rar_module_entry

#define PHP_RAR_VERSION "2.1.0-dev"
#define PHP_RAR_REVISION "$Revision$"

#ifdef PHP_WIN32
#define PHP_RAR_API __declspec(dllexport)
#else
#define PHP_RAR_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

/* causes linking errors (multiple definitions) in functions
   that were requested inlining but were not inlined by the compiler */
/* #include "unrar/rar.hpp */
/* only these includes are necessary anyway: */
#include "unrar/raros.hpp"
/* no need to reinclude windows.h or new.h */
#define LEAN_RAR_INCLUDES
#include "unrar/os.hpp"
#include "unrar/dll.hpp"
#include "unrar/version.hpp"
/* These are in unrar/headers.hpp, but that header depends on several other */
enum HOST_SYSTEM {
  HOST_MSDOS=0,HOST_OS2=1,HOST_WIN32=2,HOST_UNIX=3,HOST_MACOS=4,
  HOST_BEOS=5,HOST_MAX
};

//maximum comment size if 64KB
#define RAR_MAX_COMMENT_SIZE 65536

typedef struct rar {
	zend_object_handle			id;
	int							entry_count; //>= number of files
	struct RARHeaderDataEx		**entries;
	struct RAROpenArchiveDataEx	*list_open_data;
	struct RAROpenArchiveDataEx	*extract_open_data;
	//archive handle opened with RAR_OM_LIST_INCSPLIT open mode
	void						*arch_handle;
	char						*password;
} rar_file_t;

//PHP 5.2 compatibility
#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 3
# define ALLOC_PERMANENT_ZVAL(z) \
        (z) = (zval*) malloc(sizeof(zval));
# define OPENBASEDIR_CHECKPATH(filename) \
	(PG(safe_mode) && \
	(!php_checkuid(filename, NULL, CHECKUID_CHECK_FILE_AND_DIR))) \
	|| php_check_open_basedir(filename TSRMLS_CC)
# undef ZEND_BEGIN_ARG_INFO_EX
# define ZEND_BEGIN_ARG_INFO_EX(name, pass_rest_by_reference, return_reference, required_num_args) \
	static const zend_arg_info name[] = { \
		{ NULL, 0, NULL, 0, 0, 0, pass_rest_by_reference, return_reference, required_num_args },
#endif

/* rar.c */
PHP_MINIT_FUNCTION(rar);
PHP_MSHUTDOWN_FUNCTION(rar);
PHP_RINIT_FUNCTION(rar);
PHP_RSHUTDOWN_FUNCTION(rar);
PHP_MINFO_FUNCTION(rar);

PHP_FUNCTION(rar_bogus_ctor);

void _rar_wide_to_utf(const wchar_t *src, char *dest, size_t dest_size);
void _rar_utf_to_wide(const char *src, wchar_t *dest, size_t dest_size);
int _rar_find_file(struct RAROpenArchiveDataEx *open_data, /* IN */
				   const char *const utf_file_name, /* IN */
				   char *password, /* IN, can be null */
				   void **arc_handle, /* OUT: where to store rar archive handle  */
				   int *found, /* OUT */
				   struct RARHeaderDataEx *header_data /* OUT, can be null */
				   );
int CALLBACK _rar_unrar_callback(UINT msg, LPARAM UserData, LPARAM P1, LPARAM P2);

/* rar_error.c */
zend_class_entry *rarexception_ce_ptr;
int _rar_handle_error(int errcode TSRMLS_DC);
int _rar_handle_error_ex(const char *preamble, int errcode TSRMLS_DC);
void _rar_handle_ext_error(const char *format TSRMLS_DC, ...);
int _rar_using_exceptions(TSRMLS_D);
const char * _rar_error_to_string(int errcode);
void minit_rarerror(TSRMLS_D);

/* rararch.c */
int _rar_get_file_resource(zval *zval_file, rar_file_t **rar_file TSRMLS_DC);
int _rar_get_file_resource_ex(zval *zval_file, rar_file_t **rar_file, int silent TSRMLS_DC);
void minit_rararch(TSRMLS_D);

PHP_FUNCTION(rar_open);
PHP_FUNCTION(rar_list);
PHP_FUNCTION(rar_entry_get);
PHP_FUNCTION(rar_solid_is);
PHP_FUNCTION(rar_comment_get);
PHP_FUNCTION(rar_close);

/* rarentry.c */
extern zend_class_entry *rar_class_entry_ptr;
void minit_rarentry(TSRMLS_D);
void _rar_entry_to_zval(struct RARHeaderDataEx *entry, zval *object,
							   unsigned long packed_size TSRMLS_DC);

/* rar_stream.c */
php_stream *php_stream_rar_open(char *arc_name,
								char *utf_file_name,
								char *password,
								char *mode STREAMS_DC TSRMLS_DC);

#endif	/* PHP_RAR_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
