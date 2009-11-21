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
  | Author: Antony Dovgal <tony@daylessday.org>                          |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_RAR_H
#define PHP_RAR_H

extern zend_module_entry rar_module_entry;
#define phpext_rar_ptr &rar_module_entry

#define PHP_RAR_VERSION "2.0.0-dev"

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
#define SKIP_WINDOWS_INCLUDES
#include "unrar/os.hpp"
#include "unrar/dll.hpp"
#include "unrar/version.hpp"
/* These are in unrar/headers.hpp, but that header depends on several other */
enum HOST_SYSTEM {
  HOST_MSDOS=0,HOST_OS2=1,HOST_WIN32=2,HOST_UNIX=3,HOST_MACOS=4,
  HOST_BEOS=5,HOST_MAX
};

PHP_MINIT_FUNCTION(rar);
PHP_MSHUTDOWN_FUNCTION(rar);
PHP_RINIT_FUNCTION(rar);
PHP_RSHUTDOWN_FUNCTION(rar);
PHP_MINFO_FUNCTION(rar);

PHP_FUNCTION(rar_open);
PHP_FUNCTION(rar_list);
PHP_FUNCTION(rar_entry_get);
PHP_FUNCTION(rar_close);

//maximum comment size if 64KB
#define RAR_MAX_COMMENT_SIZE 65536

typedef struct rar {
	int							id;
	int							entry_count; //>= number of files
	struct RARHeaderDataEx		**entries;
	struct RAROpenArchiveDataEx	*list_open_data;
	struct RAROpenArchiveDataEx	*extract_open_data;
	//archive handle opened with RAR_OM_LIST_INCSPLIT open mode
	void						*arch_handle;
	char						*password;
} rar_file_t;

int _rar_handle_error(int TSRMLS_DC);
php_stream *php_stream_rar_open(char *arc_name, char *file_name,
								char *mode STREAMS_DC TSRMLS_DC);

//PHP 5.2 compatibility
#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 3
# define OPENBASEDIR_CHECKPATH(filename) \
	(PG(safe_mode) && \
	(!php_checkuid(filename, NULL, CHECKUID_CHECK_FILE_AND_DIR))) \
	|| php_check_open_basedir(filename TSRMLS_CC)
#endif

#endif	/* PHP_RAR_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
