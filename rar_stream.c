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
  | Author: Gustavo Lopes <cataphract@php.net>                           |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "php.h"

#if HAVE_RAR

#include <wchar.h>

#include "php_rar.h"
#include "unrar/rartypes.hpp"

#include "php_streams.h"
#include "ext/standard/url.h"
#include "ext/standard/php_string.h"

typedef struct php_rar_stream_data_t {
	struct RAROpenArchiveDataEx	open_data;
	struct RARHeaderDataEx		header_data;
	HANDLE						rar_handle;
	/* TODO: consider encapsulating a php memory/tmpfile stream */
	unsigned char				*buffer;
	size_t						buffer_size;
	size_t						buffer_cont_size; /* content size */
	size_t						buffer_pos;
	uint64						cursor;
	int							no_more_data;
	rar_cb_user_data			cb_userdata;
	php_stream					*stream;
} php_rar_stream_data, *php_rar_stream_data_P;

#define STREAM_DATA_FROM_STREAM \
	php_rar_stream_data_P self = (php_rar_stream_data_P) stream->abstract;

/* {{{ php_rar_ops_read */
static size_t php_rar_ops_read(php_stream *stream, char *buf, size_t count TSRMLS_DC)
{
	int n = 0;
	STREAM_DATA_FROM_STREAM
	size_t left = count;

	if (count == 0)
		return 0;

	if (self->buffer != NULL && self->rar_handle != NULL) {
		while (left > 0) {
			size_t this_read_size;
			//if nothing in the buffer or buffer already read, fill buffer
			if (/*self->buffer_cont_size == 0 || > condition not necessary */
				self->buffer_pos == self->buffer_cont_size)
			{
				int res;
				self->buffer_pos = 0;
				self->buffer_cont_size = 0;
				/* Note: this condition is important, you cannot rely on
				 * having a call to RARProcessFileChunk return no data and
				 * break on the condition self->buffer_cont_size == 0 because
				 * calling RARProcessFileChunk when there's no more data to
				 * be read may cause an integer division by 0 in
				 * RangeCoder::GetCurrentCount() */
				if (self->no_more_data)
					break;
				res = RARProcessFileChunk(self->rar_handle, self->buffer,
					self->buffer_size, &self->buffer_cont_size,
					&self->no_more_data);
				if (_rar_handle_error(res TSRMLS_CC) == FAILURE) {
					break; //finish in case of failure
				}
				assert(self->buffer_cont_size <= self->buffer_size);
				//we did not read anything. no need to continue
				if (self->buffer_cont_size == 0)
					break;
			}
			//if we get here we have data to be read in the buffer
			this_read_size = MIN(left,
				self->buffer_cont_size - self->buffer_pos);
			assert(this_read_size > 0);
			memcpy(&buf[count-left], &self->buffer[self->buffer_pos],
				this_read_size);
			left				-= this_read_size;
			n					+= this_read_size;
			self->buffer_pos	+= this_read_size;
			assert(left >= 0);
		}

		self->cursor += n;
	}

	//no more data upstream and buffer already read
	if (self->no_more_data && self->buffer_pos == self->buffer_cont_size)
		stream->eof = 1;

	if (!self->no_more_data && n == 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
			"Extraction reported as unfinished but no data read. Please report"
			" this, as this is a bug.");
		stream->eof = 1;
	}

	return n<1 ? 0 : n;
}
/* }}} */

/* {{{ php_rar_ops_write */
static size_t php_rar_ops_write(php_stream *stream, const char *buf, size_t count TSRMLS_DC)
{
	php_error_docref(NULL TSRMLS_CC, E_WARNING,
		"Write operation not supported for RAR streams.");
	if (!stream) {
		return 0;
	}

	return count;
}
/* }}} */

/* {{{ php_rar_ops_close */
static int php_rar_ops_close(php_stream *stream, int close_handle TSRMLS_DC)
{
	STREAM_DATA_FROM_STREAM

	if (self->open_data.ArcName != NULL) {
		efree(self->open_data.ArcName);
		self->open_data.ArcName = NULL;
	}
	_rar_destroy_userdata(&self->cb_userdata);
	if (self->buffer != NULL) {
		efree(self->buffer);
		self->buffer = NULL;
	}
	if (self->rar_handle != NULL) {
		if (close_handle) {
			int res = RARCloseArchive(self->rar_handle);
			if (_rar_handle_error(res TSRMLS_CC) == FAILURE) {
				; //not much we can do...
			}
		}
		self->rar_handle = NULL;
	}
	efree(self);
	stream->abstract = NULL;
	return EOF;
}
/* }}} */

/* {{{ php_rar_ops_flush */
static int php_rar_ops_flush(php_stream *stream TSRMLS_DC)
{
	return 0;
}
/* }}} */

/* {{{ php_rar_ops_flush */
/* Fill ssb, return non-zero value on failure */
static int php_rar_ops_stat(php_stream *stream, php_stream_statbuf *ssb TSRMLS_DC)
{
	STREAM_DATA_FROM_STREAM

	uint64 unp_size = INT32TO64(self->header_data.UnpSizeHigh,
		self->header_data.UnpSize);

	ssb->sb.st_dev = 0;
	ssb->sb.st_ino = 0;
	ssb->sb.st_mode = (self->header_data.FileAttr & 0xffff);
	ssb->sb.st_nlink = 1;
	/* RAR stores owner/group information (header type NEWSUB_HEAD and subtype
	 * SUBHEAD_TYPE_UOWNER), but it is not exposed in unRAR */
	ssb->sb.st_uid = 0;
	ssb->sb.st_gid = 0;
#ifdef HAVE_ST_RDEV
	ssb->sb.st_rdev = 0;
#endif
	/* never mind signedness, we'll never get sizes big enough for that to
	 * matter */
	if (sizeof(ssb->sb.st_size) == sizeof(unp_size))
		ssb->sb.st_size = (int64) unp_size;
	else {
		assert(sizeof(ssb->sb.st_size) == sizeof(long));
		if (unp_size > ((uint64) MAXLONG))
			ssb->sb.st_size = MAXLONG;
		else
			ssb->sb.st_size = (long) unp_size;
	}

	/* Creation/access time are also available in (recent) versions of RAR,
	 * but unexposed */
	{
		struct tm time_s = {0};
		time_t time;
		unsigned dos_time = self->header_data.FileTime;

		time_s.tm_mday = 1; //this one starts on 1, not 0
		time_s.tm_year = 70; /* default to 1970-01-01 00:00 */
		if ((time = mktime(&time_s)) == -1)
			return FAILURE;

		ssb->sb.st_atime = time;
		ssb->sb.st_ctime = time;

		time_s.tm_sec  = (dos_time & 0x1f)*2;
		time_s.tm_min  = (dos_time>>5) & 0x3f;
		time_s.tm_hour = (dos_time>>11) & 0x1f;
		time_s.tm_mday = (dos_time>>16) & 0x1f;
		time_s.tm_mon  = ((dos_time>>21) & 0x0f) - 1;
		time_s.tm_year = (dos_time>>25) + 80;
		if ((time = mktime(&time_s)) == -1)
			return FAILURE;
		ssb->sb.st_mtime = time;
	}

#ifdef HAVE_ST_BLKSIZE
	ssb->sb.st_blksize = 0;
#endif
#ifdef HAVE_ST_BLOCKS
	ssb->sb.st_blocks = 0;
#endif
	/* php_stat in filestat.c doesn't check this one, so don't touch it */
	//ssb->sb.st_attr = ;

	return SUCCESS;
}
/* }}} */

static php_stream_ops php_stream_rario_ops = {
	php_rar_ops_write, php_rar_ops_read,
	php_rar_ops_close, php_rar_ops_flush,
	"rar",
	NULL, /* seek */
	NULL, /* cast */
	php_rar_ops_stat, /* stat */
	NULL  /* set_option */
};

/* {{{ php_stream_rar_open */
/* callback user data does NOT need to be managed outside
 * No openbasedir etc checks; this is called from RarEntry::getStream and
 * RarEntry objects cannot be instantiation or tampered with; the check
 * was already done in RarArchive::open */
php_stream *php_stream_rar_open(char *arc_name,
								char *utf_file_name,
								rar_cb_user_data *cb_udata_ptr, /* will be copied */
								char *mode STREAMS_DC TSRMLS_DC)
{
	php_stream				*stream	= NULL;
	php_rar_stream_data_P	self	= NULL;
	int						result,
							found;

	//mode must be exactly "r"
	if (strncmp(mode, "r", sizeof("r")) != 0) {
		goto cleanup;
	}

	self = ecalloc(1, sizeof *self);
	self->open_data.ArcName		= estrdup(arc_name);
	self->open_data.OpenMode	= RAR_OM_EXTRACT;
	/* deep copy the callback userdata */
	if (cb_udata_ptr->password != NULL)
		self->cb_userdata.password = estrdup(cb_udata_ptr->password);
	if (cb_udata_ptr->callable != NULL) {
		self->cb_userdata.callable = cb_udata_ptr->callable;
		zval_add_ref(&self->cb_userdata.callable);
	}
	
	result = _rar_find_file(&self->open_data, utf_file_name, &self->cb_userdata,
		&self->rar_handle, &found, &self->header_data);

	if (_rar_handle_error(result TSRMLS_CC) == FAILURE) {
		goto cleanup;
	}

	if (!found)
		_rar_handle_ext_error("Can't find file %s in archive %s" TSRMLS_CC,
			utf_file_name, arc_name);
	else {
		//no need to allocate a buffer bigger than the file uncomp size
		size_t buffer_size = (size_t)
			MIN((uint64) RAR_CHUNK_BUFFER_SIZE,
			INT32TO64(self->header_data.UnpSizeHigh,
			self->header_data.UnpSize));
		int process_result = RARProcessFileChunkInit(self->rar_handle);

		if (_rar_handle_error(process_result TSRMLS_CC) == FAILURE) {
			goto cleanup;
		}

		self->buffer = emalloc(buffer_size);
		self->buffer_size = buffer_size;
		stream = php_stream_alloc(&php_stream_rario_ops, self, NULL, mode);
	}

cleanup:
	if (stream == NULL) { //failed
		if (self != NULL) {
			if (self->open_data.ArcName != NULL)
				efree(self->open_data.ArcName);
			_rar_destroy_userdata(&self->cb_userdata);
			if (self->buffer != NULL)
				efree(self->buffer);
			if (self->rar_handle != NULL)
				RARCloseArchive(self->rar_handle);
			efree(self);
		}
	}

	return stream;
}
/* }}} */

/* {{{ Wrapper stuff */

#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 3
/* PHP 5.2 has no zend_resolve_path. Adapted from 5.3's php_resolve_path */
static char *zend_resolve_path(const char *filename,
							  int filename_length TSRMLS_DC) /* {{{ */
{
	const char *path = PG(include_path);
	char resolved_path[MAXPATHLEN];
	char trypath[MAXPATHLEN];
	const char *ptr, *end;
	char *actual_path;

	if (filename == NULL || filename[0] == '\0') {
		return NULL;
	}

	/* do not use the include path in these circumstances */
	if ((*filename == '.' && (IS_SLASH(filename[1]) || 
			((filename[1] == '.') && IS_SLASH(filename[2])))) ||
			IS_ABSOLUTE_PATH(filename, filename_length) ||
			path == NULL || path[0] == '\0') {
		if (tsrm_realpath(filename, resolved_path TSRMLS_CC)) {
			return estrdup(resolved_path);
		} else {
			return NULL;
		}
	}

	ptr = path;
	while (ptr && *ptr) {
		end = strchr(ptr, DEFAULT_DIR_SEPARATOR);
		if (end) {
			if ((end-ptr) + 1 + filename_length + 1 >= MAXPATHLEN) {
				ptr = end + 1;
				continue;
			}
			memcpy(trypath, ptr, end-ptr);
			trypath[end-ptr] = '/';
			memcpy(trypath+(end-ptr)+1, filename, filename_length+1);
			ptr = end+1;
		} else {
			int len = strlen(ptr);

			if (len + 1 + filename_length + 1 >= MAXPATHLEN) {
				break;
			}
			memcpy(trypath, ptr, len);
			trypath[len] = '/';
			memcpy(trypath+len+1, filename, filename_length+1);
			ptr = NULL;
		}
		actual_path = trypath;
		if (tsrm_realpath(actual_path, resolved_path TSRMLS_CC)) {
			return estrdup(resolved_path);
		}
	} /* end provided path */

	return NULL;
}
/* }}} */
#endif

/* {{{ php_rar_process_context */
/* memory is to be managed externally */
static void php_rar_process_context(php_stream_context *context,
									php_stream_wrapper *wrapper,
									int options,
									char **open_password,
									char **file_password,
									zval **volume_cb TSRMLS_DC)
{
	zval **ctx_opt = NULL;

	assert(context != NULL);
	assert(open_password != NULL);
	assert(file_password != NULL);
	assert(volume_cb != NULL);

	/* TODO: don't know if I can log errors and not fail. check that */

	if (php_stream_context_get_option(context, "rar", "open_password", &ctx_opt) ==
			SUCCESS) {
		if (Z_TYPE_PP(ctx_opt) != IS_STRING)
			php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
				"RAR open password was provided, but not a string.");
		else
			*open_password = Z_STRVAL_PP(ctx_opt);
	}

	if (php_stream_context_get_option(context, "rar", "file_password", &ctx_opt) ==
			SUCCESS) {
		if (Z_TYPE_PP(ctx_opt) != IS_STRING)
			php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
				"RAR file password was provided, but not a string.");
		else
			*file_password = Z_STRVAL_PP(ctx_opt);
	}

	if (php_stream_context_get_option(context, "rar", "volume_callback",
			&ctx_opt) == SUCCESS) {
#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION == 2
		if (zend_is_callable(*ctx_opt, IS_CALLABLE_STRICT, NULL)) {
#else
		if (zend_is_callable(*ctx_opt, IS_CALLABLE_STRICT, NULL TSRMLS_CC)) {
#endif
			*volume_cb = *ctx_opt;
		}
		else
			php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
				"RAR volume find callback was provided, but invalid.");
	}
}
/* }}} */

/* _rar_get_archive_and_fragment {{{ */
/* calculate fragment and archive from url
 * *archive and *fragment should be free'd by the parent, even on failure */
static int _rar_get_archive_and_fragment(php_stream_wrapper *wrapper,
										 char *filename,
										 int options,
										 char **archive,
										 char **fragment TSRMLS_DC)
{
	char *tmp_fragment,
		 *tmp_archive = NULL;
	int  tmp_arch_len;
	int  ret = FAILURE;

	/* php_stream_open_wrapper_ex calls php_stream_locate_url_wrapper,
	 * which strips the prefix in path_for_open, but check anyway */
	if (strncmp(filename, "rar://", sizeof("rar://") - 1) == 0) {
		filename += sizeof("rar://") - 1;
	}

	tmp_fragment = strchr(filename, '#');
	if (tmp_fragment == NULL || strlen(tmp_fragment) == 1 ||
			tmp_fragment == filename) {
		php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
			"The url must contain a path and a non-empty fragment; it must be "
			"must in the form \"rar://<urlencoded path to RAR archive>#"
			"<urlencoded entry name>\"");
		goto cleanup;
	}
	tmp_arch_len = tmp_fragment - filename;
	tmp_archive = emalloc(tmp_arch_len + 1);
	strlcpy(tmp_archive, filename, tmp_arch_len + 1);
	*fragment = estrdup(tmp_fragment + 1); //+ 1 to skip # character
	php_raw_url_decode(tmp_archive, tmp_arch_len);
	php_raw_url_decode(*fragment, strlen(*fragment));

	if (!(options & STREAM_ASSUME_REALPATH)) {
		if (options & USE_PATH) {
			*archive = zend_resolve_path(tmp_archive, tmp_arch_len TSRMLS_CC);
		}
		if (*archive == NULL) {
			if ((*archive = expand_filepath(tmp_archive, NULL TSRMLS_CC))
					== NULL) {
				php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
					"Could not expand the path %s", archive);
				goto cleanup;
			}
		}
	}

	if (!(options & STREAM_DISABLE_OPEN_BASEDIR) &&
			php_check_open_basedir(*archive TSRMLS_CC)) {
		//php_check_open_basedir already emits the error
		goto cleanup;
	}

	if ((options & ENFORCE_SAFE_MODE) && PG(safe_mode) &&
			(!php_checkuid(*archive, "r", CHECKUID_CHECK_MODE_PARAM))) {
		goto cleanup;
	}

	ret = SUCCESS;
cleanup:
	if (tmp_archive != NULL)
		efree(tmp_archive);
	return ret;
}
/* }}} */

/* {{{ php_stream_rar_opener */
php_stream *php_stream_rar_opener(php_stream_wrapper *wrapper,
								  char *filename,
								  char *mode,
								  int options,
								  char **opened_path,
								  php_stream_context *context STREAMS_DC TSRMLS_DC)
{
	char *fragment = NULL,
		 /* used to hold the pointer that may be copied to opened_path */
		 *tmp_open_path = NULL,
		 *open_passwd = NULL,
		 *file_passwd = NULL;
	char const *rar_error;
	int	 rar_result,
		 file_found;
	zval *volume_cb = NULL;
	php_rar_stream_data_P self = NULL;
	php_stream *stream = NULL;

	/* {{{ preliminaries */
	if (options & STREAM_OPEN_PERSISTENT) {
		/* TODO: add support for opening RAR files in a persisten fashion */
		php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
			"No support for opening RAR files persistently yet");
		return NULL;
	}

	//mode must be exactly "r"
	if (strncmp(mode, "r", sizeof("r")) != 0) {
		php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
			"Only the \"r\" open mode is permitted, given %s", mode);
		return NULL;
	}

	if (_rar_get_archive_and_fragment(wrapper, filename, options,
			&tmp_open_path, &fragment TSRMLS_CC) == FAILURE) {
		goto cleanup;
	}

	if (context != NULL) {
		php_rar_process_context(context, wrapper, options, &open_passwd,
			&file_passwd, &volume_cb TSRMLS_CC);
	}

	self = ecalloc(1, sizeof *self);
	self->open_data.ArcName	= estrdup(tmp_open_path);
	self->open_data.OpenMode = RAR_OM_EXTRACT;
	if (open_passwd != NULL)
		self->cb_userdata.password = estrdup(open_passwd);
	if (volume_cb != NULL) {
		self->cb_userdata.callable = volume_cb;
		zval_add_ref(&self->cb_userdata.callable);
		SEPARATE_ZVAL(&self->cb_userdata.callable);
	}
	
	rar_result = _rar_find_file(&self->open_data, fragment, &self->cb_userdata,
		&self->rar_handle, &file_found, &self->header_data);

	if ((rar_error = _rar_error_to_string(rar_result)) != NULL) {
		php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
			"Error opening RAR archive %s: %s", tmp_open_path, rar_error);
		goto cleanup;
	}

	if (!file_found)  {
		php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
			"Can't file %s in RAR archive %s", fragment, tmp_open_path);
		goto cleanup;
	}
	
	/* once found, the password that matters is the file level password.
	 * we will NOT default on the open password if no file level password is
	 * given, but an open password is. This behaviour is differs from that of
	 * RarEntry::extract() */
	if (self->cb_userdata.password != NULL)
		efree(self->cb_userdata.password);

	if (file_passwd == NULL)
		self->cb_userdata.password = NULL;
	else
		self->cb_userdata.password = estrdup(file_passwd);


	{
		//no need to allocate a buffer bigger than the file uncomp size
		size_t buffer_size = (size_t)
			MIN((uint64) RAR_CHUNK_BUFFER_SIZE,
			INT32TO64(self->header_data.UnpSizeHigh,
			self->header_data.UnpSize));
		rar_result = RARProcessFileChunkInit(self->rar_handle);

		if ((rar_error = _rar_error_to_string(rar_result)) != NULL) {
			php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
				"Error opening file %s inside RAR archive %s: %s",
				fragment, tmp_open_path, rar_error);
			goto cleanup;
		}

		self->buffer = emalloc(buffer_size);
		self->buffer_size = buffer_size;
		stream = php_stream_alloc(&php_stream_rario_ops, self, NULL, mode);
	}

cleanup:

	if (tmp_open_path != NULL) {
		if (opened_path != NULL)
			*opened_path = tmp_open_path;
		else
			efree(tmp_open_path);
	}
	if (fragment != NULL)
		efree(fragment);

	if (stream == NULL) { //failed
		if (self != NULL) {
			if (self->open_data.ArcName != NULL)
				efree(self->open_data.ArcName);
			_rar_destroy_userdata(&self->cb_userdata);
			if (self->buffer != NULL)
				efree(self->buffer);
			if (self->rar_handle != NULL)
				RARCloseArchive(self->rar_handle);
			efree(self);
		}
	}

	return stream; /* may be null */
}
/* }}} */

static php_stream_wrapper_ops rar_stream_wops = {
	php_stream_rar_opener,
	NULL,	/* close */
	NULL,	/* fstat */
	NULL,	/* stat */
	NULL,	/* opendir */
	"rar wrapper",
	NULL,	/* unlink */
	NULL,	/* rename */
	NULL,	/* mkdir */
	NULL	/* rmdir */
};

extern php_stream_wrapper php_stream_rar_wrapper = {
	&rar_stream_wops,
	NULL,
	0 /* is_url */
};

/* end wrapper stuff }}} */

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
