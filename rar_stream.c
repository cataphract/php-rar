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

#include <php.h>

#if HAVE_RAR

#include <wchar.h>

#include "php_rar.h"

#include <php_streams.h>
#include <ext/standard/url.h>
#include <ext/standard/php_string.h>
#include <ext/standard/file.h>

typedef struct php_rar_stream_data_t {
	struct RAROpenArchiveDataEx	open_data;
	struct RARHeaderDataEx		header_data;
	HANDLE						rar_handle;
	size_t						file_size;
	/* TODO: consider encapsulating a php memory/tmpfile stream */
	unsigned char				*buffer;
	size_t						buffer_size;
	size_t						buffer_read_size; /* content size */
	size_t						buffer_pos;
	uint64						cursor;
	int							no_more_data;
	rar_cb_user_data			cb_userdata;
	php_stream					*stream;
} php_rar_stream_data, *php_rar_stream_data_P;

typedef struct php_rar_dir_stream_data_t {
	zval						rar_obj;
	rar_find_output				*state;
	struct RARHeaderDataEx		*self_header; /* NULL for root */
	wchar_t						*directory;
	size_t						dir_size; /* length + 1 */
	int							cur_offset;
	int							no_encode; /* do not urlencode entry names */
	php_stream					*stream;
} php_rar_dir_stream_data, *php_rar_dir_stream_data_P;

#define STREAM_DATA_FROM_STREAM \
	php_rar_stream_data_P self = (php_rar_stream_data_P) stream->abstract;

#define STREAM_DIR_DATA_FROM_STREAM \
	php_rar_dir_stream_data_P self = (php_rar_dir_stream_data_P) stream->abstract;

/* len can be -1 (calculate) */
static char *_rar_wide_to_utf_with_alloc(const wchar_t *wide, int len)
{
	size_t size;
	char *ret;

	if (len != -1)
		size = ((size_t) len + 1) * sizeof(wchar_t);
	else
		size = (wcslen(wide) + 1) * sizeof(wchar_t);

	ret = emalloc(size);
	_rar_wide_to_utf(wide, ret, size);
	return ret;
}

/* {{{ RAR file streams */

/* {{{ php_rar_ops_read */
#if PHP_VERSION_ID < 70400
static size_t php_rar_ops_read(php_stream *stream, char *buf, size_t count TSRMLS_DC)
#else
static ssize_t php_rar_ops_read(php_stream *stream, char *buf, size_t count)
#endif
{
	size_t n = 0;
	STREAM_DATA_FROM_STREAM
	size_t left = count;

	/* never return EOF as README.STREAMS says; _php_stream_read doesn't
	 * expect that nowadays */
	if (count == 0)
		return 0;

	if (self->buffer != NULL && self->rar_handle != NULL) {
		while (left > 0) {
			size_t this_read_size;
			/* if nothing in the buffer or buffer already read, fill buffer */
			if (/*self->buffer_read_size == 0 || > condition not necessary */
					self->buffer_pos == self->buffer_read_size)
			{
				int res;
				self->buffer_pos = 0;
				self->buffer_read_size = 0;
				/* Note: this condition is important, you cannot rely on
				 * having a call to RARProcessFileChunk return no data and
				 * break on the condition self->buffer_cont_size == 0 because
				 * calling RARProcessFileChunk when there's no more data to
				 * be read may cause an integer division by 0 in
				 * RangeCoder::GetCurrentCount() */
				if (self->no_more_data)
					break;
				res = RARProcessFileChunk(self->rar_handle, self->buffer,
						self->buffer_size, &self->buffer_read_size,
					&self->no_more_data);
				if (_rar_handle_error(res TSRMLS_CC) == FAILURE) {
					break; /* finish in case of failure */
				}
				assert(self->buffer_read_size <= self->buffer_size);
				/* we did not read anything. no need to continue */
				if (self->buffer_read_size == 0)
					break;
			}
			/* if we get here we have data to be read in the buffer */
			this_read_size = MIN(left,
					self->buffer_read_size - self->buffer_pos);
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

	/* no more data upstream (for sure), buffer already read and
	 * caller asked for more data than we're giving */
	if (self->no_more_data && self->buffer_pos == self->buffer_read_size &&
			n < count && stream->eof != 1) {
		stream->eof = 1;
		if (self->cursor > self->file_size) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING,
					"The file size is supposed to be %lu bytes, but "
					"we read more: %lu bytes (corruption/wrong pwd)",
					self->file_size, self->cursor);
		}
	}

	/* we should only give no data if we have no more */
	if (!self->no_more_data && n == 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
			"Extraction reported as unfinished but no data read. Please report"
			" this, as this is a bug.");
		stream->eof = 1;
	}

#if PHP_VERSION_ID < 50400
	return n;
#else
	return (ssize_t) n;
#endif
}
/* }}} */

/* {{{ php_rar_ops_write */
#if PHP_VERSION_ID < 70400
static size_t php_rar_ops_write(php_stream *stream, const char *buf, size_t count TSRMLS_DC)
#else
static ssize_t php_rar_ops_write(php_stream *stream, const char *buf, size_t count TSRMLS_DC)
#endif
{
	php_error_docref(NULL TSRMLS_CC, E_WARNING,
		"Write operation not supported for RAR streams.");
	if (!stream) {
#if PHP_VERSION_ID < 70400
		return 0;
#else
		return -1;
#endif
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
				; /* not much we can do... */
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

/* This function is consistent with Archive::ConvertAttributes() */
static mode_t _rar_convert_file_attrs(unsigned os_attrs,
									  unsigned flags,
									  unsigned host) /* {{{ */
{
	static int mask = -1;
	mode_t ret;

	if (mask == -1) {
		mask = umask(0022);
		umask(mask);
	}

	switch (host) {
		case HOST_MSDOS:
		case HOST_OS2:
		case HOST_WIN32:
			/* we don't set file type 0xA000 (sym link because in windows sym
			 * links are implemented with reparse points, and its contents
			 * (or at least flags) would have to be analyzed to determine
			 * if they're directory junctions or symbolic links */
			/* Mapping of MS/DOS OS/2 and Windows file attributes */
			if (os_attrs & 0x10) { /* FILE_ATTRIBUTE_DIRECTORY */
				ret = S_IFDIR;
				ret |= 0777;
				ret &= ~mask;
			}
			else {
				ret = S_IFREG;
				if (os_attrs & 1) /* FILE_ATTRIBUTE_READONLY */
					ret |= 0444;
				else
					ret |= 0666;

				ret &= ~mask;
			}

			break;
		case HOST_UNIX:
		case HOST_BEOS:
			/* leave as is */
			ret = (mode_t) (os_attrs & 0xffff);
			break;

		default:
			if (flags & RHDF_DIRECTORY)
				ret = S_IFDIR;
			else
				ret = S_IFREG;

			ret |= 0777;
			ret &= ~mask;
			break;
	}

	return ret;
}
/* }}} */

static int _rar_stat_from_header(struct RARHeaderDataEx *header,
								 php_stream_statbuf *ssb) /* {{{ */
{
	uint64 unp_size = INT32TO64(header->UnpSizeHigh, header->UnpSize);

	ssb->sb.st_dev = 0;
	ssb->sb.st_ino = 0;
	ssb->sb.st_mode = _rar_convert_file_attrs(header->FileAttr, header->Flags,
		header->HostOS);
	ssb->sb.st_nlink = 1;
	/* RAR stores owner/group information (header type NEWSUB_HEAD and subtype
	 * SUBHEAD_TYPE_UOWNER), but it is not exposed in unRAR */
	ssb->sb.st_uid = 0;
	ssb->sb.st_gid = 0;
#if defined(HAVE_ST_RDEV) || defined(HAVE_STRUCT_STAT_ST_RDEV)
	ssb->sb.st_rdev = 0;
#endif
	/* never mind signedness, we'll never get sizes big enough for that to
	 * matter */
	if (sizeof(ssb->sb.st_size) == sizeof(unp_size)) /* 64-bit st_size */
		ssb->sb.st_size = (int64) unp_size;
	else {
		assert(sizeof(ssb->sb.st_size) == sizeof(long));
		if (unp_size > ((uint64) LONG_MAX))
			ssb->sb.st_size = LONG_MAX;
		else
			ssb->sb.st_size = (long) (unsigned long) (int64) unp_size;
	}

	rar_time_convert(header->AtimeLow, header->AtimeHigh, &ssb->sb.st_atime);
	rar_time_convert(header->CtimeLow, header->CtimeHigh, &ssb->sb.st_ctime);

	if (header->MtimeLow == 0 && header->MtimeHigh == 0) {
		/* high precision mod time undefined */
		time_t time;
		if (rar_dos_time_convert(header->FileTime, &time) == FAILURE) {
			return FAILURE;
		}
		ssb->sb.st_mtime = time;
	}
	else {
		rar_time_convert(header->MtimeLow, header->MtimeHigh,
			&ssb->sb.st_mtime);
	}

#if defined(HAVE_ST_BLKSIZE) || defined(HAVE_STRUCT_STAT_ST_BLKSIZE)
	ssb->sb.st_blksize = 0;
#endif
#if defined(HAVE_ST_BLOCKS) || defined (HAVE_STRUCT_STAT_ST_BLOCKS)
	ssb->sb.st_blocks = 0;
#endif
	/* php_stat in filestat.c doesn't check this one, so don't touch it */
	/* ssb->sb.st_attr = ; */

	return SUCCESS;
}

/* {{{ php_rar_ops_stat */
/* Fill ssb, return non-zero value on failure */
static int php_rar_ops_stat(php_stream *stream, php_stream_statbuf *ssb TSRMLS_DC)
{
	STREAM_DATA_FROM_STREAM

	return _rar_stat_from_header(&self->header_data, ssb);
}
/* }}} */

static php_stream_ops php_stream_rario_ops = {
	php_rar_ops_write,
	php_rar_ops_read,
	php_rar_ops_close,
	php_rar_ops_flush,
	"rar",
	NULL, /* seek */
	NULL, /* cast */
	php_rar_ops_stat, /* stat */
	NULL  /* set_option */
};

/* end RAR file streams }}} */

/* {{{ RAR directory streams */

/* {{{ php_rar_dir_ops_read */
#if PHP_VERSION_ID < 70400
static size_t php_rar_dir_ops_read(php_stream *stream, char *buf, size_t count TSRMLS_DC)
#else
static ssize_t php_rar_dir_ops_read(php_stream *stream, char *buf, size_t count TSRMLS_DC)
#endif
{
	php_stream_dirent entry;
	size_t offset;
	STREAM_DIR_DATA_FROM_STREAM

	if (count != sizeof(entry)) {
#if PHP_VERSION_ID < 70400
		return 0;
#else
		return -1;
#endif
	}

	_rar_entry_search_advance(self->state, self->directory, self->dir_size, 1);
	if (!self->state->found) {
		stream->eof = 1;
#if PHP_VERSION_ID < 70400
		return 0;
#else
		return -1;
#endif
	}

	if (self->dir_size == 1) /* root */
		offset = 0;
	else
		offset = self->dir_size;

	_rar_wide_to_utf(&self->state->header->FileNameW[offset],
		entry.d_name, sizeof entry.d_name);

	if (!self->no_encode) { /* urlencode entry */
#if PHP_MAJOR_VERSION < 7
		int new_len;
		char *encoded_name;
		encoded_name = php_url_encode(entry.d_name, strlen(entry.d_name),
			&new_len);
		strlcpy(entry.d_name, encoded_name, sizeof entry.d_name);
		efree(encoded_name);
#else
		zend_string *encoded_name =
				php_url_encode(entry.d_name, strlen(entry.d_name));
		strlcpy(entry.d_name, encoded_name->val, sizeof entry.d_name);
		zend_string_release(encoded_name);
#endif
	}


	self->cur_offset++;

	memcpy(buf, &entry, sizeof entry);
	return sizeof entry;
}
/* }}} */

/* {{{ php_rar_dir_ops_close */
static int php_rar_dir_ops_close(php_stream *stream, int close_handle TSRMLS_DC)
{
	STREAM_DIR_DATA_FROM_STREAM

#if PHP_MAJOR_VERSION < 7
	zval_dtor(&self->rar_obj);
#else
	zval_ptr_dtor(&self->rar_obj);
#endif
	efree(self->directory);
	efree(self->state);
	efree(self);
	stream->abstract = NULL;

	/* 0 because that's what php_plain_files_dirstream_close returns... */
	return 0;
}
/* }}} */

/* {{{ php_rar_dir_ops_rewind */
#if PHP_VERSION_ID < 70400
static int php_rar_dir_ops_rewind(php_stream *stream, off_t offset, int whence, off_t *newoffset TSRMLS_DC)
#else
static int php_rar_dir_ops_rewind(php_stream *stream, zend_off_t offset, int whence, zend_off_t *newoffset)
#endif
{
	STREAM_DIR_DATA_FROM_STREAM

	_rar_entry_search_rewind(self->state);
	return 0;
}
/* }}} */

/* {{{ php_rar_dir_ops_stat */
static int php_rar_dir_ops_stat(php_stream *stream, php_stream_statbuf *ssb TSRMLS_DC)
{
	STREAM_DIR_DATA_FROM_STREAM

	if (self->self_header == NULL) { /* root */
		/* RAR root has no entry, so we make something up.
		 * We could use the RAR archive itself instead, but I think that would
		 * not be very consistent */
		struct RARHeaderDataEx t = {""};
		t.FileAttr = S_IFDIR | 0777;
		return _rar_stat_from_header(&t, ssb);
	}

	return _rar_stat_from_header(self->self_header, ssb);
}
/* }}} */

static php_stream_ops php_stream_rar_dirio_ops = {
	NULL,
	php_rar_dir_ops_read,
	php_rar_dir_ops_close,
	NULL,
	"rar directory",
	php_rar_dir_ops_rewind, /* rewind */
	NULL, /* cast */
	php_rar_dir_ops_stat, /* stat */
	NULL  /* set_option */
};
/* end RAR directory streams }}} */

/* {{{ php_stream_rar_open */
/* callback user data does NOT need to be managed outside
 * No openbasedir etc checks; this is called from RarEntry::getStream and
 * RarEntry objects cannot be instantiation or tampered with; the check
 * was already done in RarArchive::open */
php_stream *php_stream_rar_open(char *arc_name,
								size_t position,
								rar_cb_user_data *cb_udata_ptr /* will be copied */
								STREAMS_DC TSRMLS_DC)
{
	php_stream				*stream	= NULL;
	php_rar_stream_data_P	self	= NULL;
	int						result,
							found;

	self = ecalloc(1, sizeof *self);
	self->open_data.ArcName		= estrdup(arc_name);
	self->open_data.OpenMode		= RAR_OM_EXTRACT;
	/* deep copy the callback userdata */
	if (cb_udata_ptr->password != NULL)
		self->cb_userdata.password = estrdup(cb_udata_ptr->password);
	if (cb_udata_ptr->callable != NULL) {
		ZVAL_ALLOC_DUP(self->cb_userdata.callable, cb_udata_ptr->callable);
	}

	result = _rar_find_file_p(&self->open_data, position, &self->cb_userdata,
		&self->rar_handle, &found, &self->header_data);

	if (_rar_handle_error(result TSRMLS_CC) == FAILURE) {
		goto cleanup;
	}

	if (!found) /* position is size_t; should be %Zu but it doesn't work */
		_rar_handle_ext_error("Can't find file with index %u in archive %s"
			TSRMLS_CC, position, arc_name);
	else {
		/* no need to allocate a buffer bigger than the file uncomp size */
		size_t file_size = INT32TO64(self->header_data.UnpSizeHigh,
				self->header_data.UnpSize);
		size_t buffer_size =  MIN(
				MAX(RAR_CHUNK_BUFFER_SIZE, self->header_data.WinSize),
				file_size);
		int process_result = RARProcessFileChunkInit(self->rar_handle);

		if (_rar_handle_error(process_result TSRMLS_CC) == FAILURE) {
			goto cleanup;
		}

		self->file_size = file_size;
		self->buffer = emalloc(buffer_size);
		self->buffer_size = buffer_size;
		stream = php_stream_alloc(&php_stream_rario_ops, self, NULL, "rb");
		stream->flags |= PHP_STREAM_FLAG_NO_BUFFER;
	}

cleanup:
	if (stream == NULL) { /* failed */
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
									char **file_password, /* can be NULL */
									zval **volume_cb TSRMLS_DC)
{
	zval *ctx_opt;
#if PHP_MAJOR_VERSION < 7
	zval **ctx_opt_p = NULL;
#endif

	assert(context != NULL);
	assert(open_password != NULL);
	*open_password = NULL;
	assert(volume_cb != NULL);
	*volume_cb = NULL;

	/* TODO: don't know if I can log errors and not fail. check that */

#if PHP_MAJOR_VERSION < 7
	if (php_stream_context_get_option(
				context, "rar", "open_password", &ctx_opt_p) == SUCCESS) {
	ctx_opt = *ctx_opt_p;
#else
	if ((ctx_opt = php_stream_context_get_option(
			 context, "rar", "open_password"))) {
#endif
		if (Z_TYPE_P(ctx_opt) != IS_STRING)
			php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
				"RAR open password was provided, but not a string.");
		else
			*open_password = Z_STRVAL_P(ctx_opt);
	}

#if PHP_MAJOR_VERSION < 7
	if (file_password != NULL && php_stream_context_get_option(context, "rar",
			"file_password", &ctx_opt_p) == SUCCESS) {
		ctx_opt = *ctx_opt_p;
#else
	if (file_password != NULL && (ctx_opt = php_stream_context_get_option(
			context, "rar", "file_password"))) {
#endif
		if (Z_TYPE_P(ctx_opt) != IS_STRING)
			php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
				"RAR file password was provided, but not a string.");
		else
			*file_password = Z_STRVAL_P(ctx_opt);
	}

#if PHP_MAJOR_VERSION < 7
	if (php_stream_context_get_option(context, "rar", "volume_callback",
			&ctx_opt_p) == SUCCESS) {
		ctx_opt = *ctx_opt_p;
#else
	if ((ctx_opt = php_stream_context_get_option(
			 context, "rar", "volume_callback"))) {
#endif
#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION == 2
		if (zend_is_callable(ctx_opt, IS_CALLABLE_STRICT, NULL)) {
#else
		if (zend_is_callable(ctx_opt, IS_CALLABLE_STRICT, NULL TSRMLS_CC)) {
#endif
			*volume_cb = ctx_opt;
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
										 const char *filename,
										 int options,
										 int allow_no_frag,
										 char **archive,
										 wchar_t **fragment,
										 int *no_encode TSRMLS_DC)
{
	char *tmp_fragment,
		 *tmp_archive = NULL;
	int  tmp_arch_len,
		 tmp_frag_len;
	int  ret = FAILURE;

	/* php_stream_open_wrapper_ex calls php_stream_locate_url_wrapper,
	 * which strips the prefix in path_for_open, but check anyway */
	if (strncmp(filename, "rar://", sizeof("rar://") - 1) == 0) {
		filename += sizeof("rar://") - 1;
	}

	tmp_fragment = strchr(filename, '#');
	if (!allow_no_frag && (tmp_fragment == NULL || strlen(tmp_fragment) == 1 ||
			tmp_fragment == filename)) {
		php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
			"The url must contain a path and a non-empty fragment; it must be "
			"in the form \"rar://<urlencoded path to RAR archive>[*]#"
			"<urlencoded entry name>\"");
		goto cleanup;
	}
	if (allow_no_frag && (tmp_fragment == filename || filename[0] == '\0')) {
		php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
			"The url must contain a path and an optional fragment; it must be "
			"in the form \"rar://<urlencoded path to RAR archive>[*][#["
			"<urlencoded entry name>]]\"");
		goto cleanup;
	}

	tmp_arch_len = (tmp_fragment != NULL)?
		(tmp_fragment - filename) : (strlen(filename));
	tmp_archive = emalloc(tmp_arch_len + 1);
	strlcpy(tmp_archive, filename, tmp_arch_len + 1);
	php_raw_url_decode(tmp_archive, tmp_arch_len);

	/* process no urlencode escape entry modifier */
	if (tmp_arch_len > 1 && tmp_archive[tmp_arch_len - 1] == '*') {
		if (no_encode != NULL)
			*no_encode = TRUE;
		tmp_archive[tmp_arch_len-- - 1] = '\0';
	}
	else if (no_encode != NULL) {
		*no_encode = FALSE;
	}


	if (!(options & STREAM_ASSUME_REALPATH)) {
		if (options & USE_PATH) {
#if PHP_MAJOR_VERSION < 7
			*archive = zend_resolve_path(tmp_archive, tmp_arch_len TSRMLS_CC);
#else
			zend_string *arc_str = zend_resolve_path(tmp_archive, tmp_arch_len);
			if (arc_str != NULL) {
				*archive = estrndup(arc_str->val, arc_str->len);
			} else {
				*archive = NULL;
			}
			zend_string_release(arc_str);
#endif
		}
		if (*archive == NULL) {
			if ((*archive = expand_filepath(tmp_archive, NULL TSRMLS_CC))
					== NULL) {
				php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
					"Could not expand the path %s", tmp_archive);
				goto cleanup;
			}
		}
	}

	if (!(options & STREAM_DISABLE_OPEN_BASEDIR) &&
			php_check_open_basedir(*archive TSRMLS_CC)) {
		/* php_check_open_basedir already emits the error */
		goto cleanup;
	}

#if PHP_API_VERSION < 20100412
	if ((options & ENFORCE_SAFE_MODE) && PG(safe_mode) &&
			(!php_checkuid(*archive, "r", CHECKUID_CHECK_MODE_PARAM))) {
		goto cleanup;
	}
#endif

	if (tmp_fragment == NULL) {
		*fragment = emalloc(sizeof **fragment);
		(*fragment)[0] = L'\0';
	}
	else {
		char *frag_dup;
		assert(tmp_fragment[0] == '#');
		tmp_fragment++; /* remove # */

		/* remove initial path divider, if given */
		if (tmp_fragment[0] == '\\' || tmp_fragment[0] == '/')
			tmp_fragment++;

		tmp_frag_len = strlen(tmp_fragment);
		frag_dup = estrndup(tmp_fragment, tmp_frag_len);
		php_raw_url_decode(frag_dup, tmp_frag_len);

		*fragment = safe_emalloc(tmp_frag_len + 1, sizeof **fragment, 0);
		_rar_utf_to_wide(frag_dup, *fragment, tmp_frag_len + 1);
		efree(frag_dup);
	}

	/* Note that RAR treats \ and / the same way.
	 * If it finds any of them, it replaces it with PATHDIVIDER.
	 * Do the same for the user-supplied fragment */
	{
		wchar_t *ptr;
		for (ptr = *fragment; *ptr != L'\0'; ptr++) {
			if (*ptr == L'\\' || *ptr == L'/')
				*ptr = SPATHDIVIDER[0];
		}
	}

	ret = SUCCESS;
cleanup:
	if (tmp_archive != NULL)
		efree(tmp_archive);
	return ret;
}
/* }}} */

/* {{{ php_stream_rar_opener */
static php_stream *php_stream_rar_opener(php_stream_wrapper *wrapper,
#if PHP_MAJOR_VERSION < 7
										 char *filename,
										 char *mode,
										 int options,
										 char **opened_path,
#else
										 const char *filename,
										 const char *mode,
										 int options,
										 zend_string **opened_path,
#endif
										 php_stream_context *context
										 STREAMS_DC TSRMLS_DC)
{
	char *tmp_open_path = NULL, /* used to hold the pointer that may be copied to opened_path */
		 *open_passwd = NULL,
		 *file_passwd = NULL;
	wchar_t *fragment = NULL;
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

	/* mode must be "r" or "rb", which, for BC reasons, are treated identically */
	if (mode[0] != 'r' || (mode[1] != '\0' && mode[1] != 'b') || strlen(mode) > 2) {
		php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
			"Only the \"r\" and \"rb\" open modes are permitted, given %s", mode);
		return NULL;
	}

	if (_rar_get_archive_and_fragment(wrapper, filename, options, 0,
			&tmp_open_path, &fragment, NULL TSRMLS_CC) == FAILURE) {
		goto cleanup;
	}

	if (context != NULL) {
		php_rar_process_context(context, wrapper, options, &open_passwd,
			&file_passwd, &volume_cb TSRMLS_CC);
	}
	/* end preliminaries }}} */

	self = ecalloc(1, sizeof *self);
	self->open_data.ArcName	= estrdup(tmp_open_path);
	self->open_data.OpenMode = RAR_OM_EXTRACT;
	if (open_passwd != NULL)
		self->cb_userdata.password = estrdup(open_passwd);
	if (volume_cb != NULL) {
		ZVAL_ALLOC_DUP(self->cb_userdata.callable, volume_cb);
	}

	rar_result = _rar_find_file_w(&self->open_data, fragment,
		&self->cb_userdata, &self->rar_handle, &file_found,
		&self->header_data);

	if ((rar_error = _rar_error_to_string(rar_result)) != NULL) {
		php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
			"Error opening RAR archive %s: %s", tmp_open_path, rar_error);
		goto cleanup;
	}

	if (!file_found)  {
		char *mb_fragment = _rar_wide_to_utf_with_alloc(fragment, -1);
		php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
			"Can't file %s in RAR archive %s", mb_fragment, tmp_open_path);
		efree(mb_fragment);
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
		/* no need to allocate a buffer bigger than the file uncomp size */
		size_t file_size = INT32TO64(self->header_data.UnpSizeHigh,
				self->header_data.UnpSize);
		size_t buffer_size =  MIN(
				MAX(RAR_CHUNK_BUFFER_SIZE, self->header_data.WinSize),
				file_size);
		rar_result = RARProcessFileChunkInit(self->rar_handle);

		if ((rar_error = _rar_error_to_string(rar_result)) != NULL) {
			char *mb_entry = _rar_wide_to_utf_with_alloc(fragment, -1);
			php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
					"Error opening file %s inside RAR archive %s: %s",
					mb_entry, tmp_open_path, rar_error);
			efree(mb_entry);
			goto cleanup;
		}

		self->file_size = file_size;
		self->buffer = emalloc(buffer_size);
		self->buffer_size = buffer_size;
		stream = php_stream_alloc(&php_stream_rario_ops, self, NULL, mode);
		/* we have buffering ourselves, thank you: */
		stream->flags |= PHP_STREAM_FLAG_NO_BUFFER;
		/* stream->flags |= PHP_STREAM_FLAG_NO_SEEK; unnecessary */
	}

cleanup:

	if (tmp_open_path != NULL) {
		if (opened_path != NULL) {
#if PHP_MAJOR_VERSION < 7
			*opened_path = tmp_open_path;
#else
			*opened_path =
				zend_string_init(tmp_open_path, strlen(tmp_open_path), 0);
#endif
		} else {
			efree(tmp_open_path);
		}
	}
	if (fragment != NULL)
		efree(fragment);

	if (stream == NULL) { /* failed */
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

static void _rar_arch_cache_get_key(const char *full_path,
									const char* open_passwd,
									zval *volume_cb,
									char **key,
									uint *key_len) /* {{{ */
{
	assert(key != NULL);
	assert(key_len != NULL);
	*key_len = strlen(full_path);
	/* TODO: stat the file to get last mod time and (maybe) use the context */
	*key = estrndup(full_path, *key_len);
}
/* }}} */

/* If return is success, the caller should eventually call zval_ptr_dtor on
 * rar_obj */
static int _rar_get_cachable_rararch(php_stream_wrapper *wrapper,
									 int options,
									 const char* arch_path,
									 const char* open_passwd,
									 zval *volume_cb,
									 zval *rar_obj, /* output */
									 rar_file_t **rar TSRMLS_DC) /* {{{ */
{
	char		*cache_key = NULL;
	uint		cache_key_len;
	int			err_code,
				ret = FAILURE;
	zval		*cache_zv;

	assert(rar_obj != NULL);
#if PHP_MAJOR_VERSION < 7
	INIT_ZVAL(*rar_obj);
#else
	ZVAL_UNDEF(rar_obj);
#endif

	_rar_arch_cache_get_key(arch_path, open_passwd, volume_cb, &cache_key,
		&cache_key_len);
	cache_zv = RAR_G(contents_cache).get(
				cache_key, cache_key_len, rar_obj TSRMLS_CC);

	if (cache_zv == NULL) { /* cache miss */
		if (_rar_create_rararch_obj(arch_path, open_passwd, volume_cb,
				rar_obj, &err_code TSRMLS_CC) == FAILURE) {
			const char *err_str = _rar_error_to_string(err_code);
			if (err_str == NULL)
				php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
					"%s", "Archive opened failed (returned NULL handle), but "
					"did not return an error. Should not happen.");
			else {
				php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
					"Failed to open %s: %s", arch_path, err_str);
			}

			goto cleanup;
		}
		else { /* Success opening the RAR archive */
			int res;
			const char *err_str;

			if (_rar_get_file_resource_zv_ex(rar_obj, rar, 1 TSRMLS_CC)
					== FAILURE) {
				php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
					"Bug: could not retrieve RarArchive object from zval");
				goto cleanup;
			}

			res = _rar_list_files(*rar TSRMLS_CC);

			/* we don't cache incomplete archives */
			if ((err_str = _rar_error_to_string(res)) != NULL) {
				php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
					"Error reading entries of archive %s: %s", arch_path,
					err_str);
				goto cleanup;
			}
			RAR_G(contents_cache).put(cache_key, cache_key_len, rar_obj
				TSRMLS_CC);
			_rar_close_file_resource(*rar);
		}
	}
	else { /* cache hit */
		/* cache get already put the value in rar_obj and incremented the
		 * refcount of the object */
		if (_rar_get_file_resource_zv_ex(rar_obj, rar, 1 TSRMLS_CC) == FAILURE) {
			php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
				"Bug: could not retrieve RarArchive object from zval");
			goto cleanup;
		}
	}

	ret = SUCCESS;
cleanup:
	if (cache_key != NULL)
		efree(cache_key);

	if (ret != SUCCESS && Z_TYPE_P(rar_obj) == IS_OBJECT) {
#if PHP_MAJOR_VERSION < 7
		zval_dtor(rar_obj);
		Z_TYPE_P(rar_obj) = IS_NULL;
#else
		zval_ptr_dtor(rar_obj);
		ZVAL_UNDEF(rar_obj);
#endif
	}

	return ret;
}
/* }}} */

/* {{{ _rar_stream_tidy_wrapper_error_log
 *     These two different versions are because of PHP commit 7166298 */
#if PHP_VERSION_ID <= 50310 || PHP_VERSION_ID == 50400
/* copied from main/streams/streams.c because it's an internal function */
static void _rar_stream_tidy_wrapper_error_log(php_stream_wrapper *wrapper TSRMLS_DC)
{
	if (wrapper) {
		/* tidy up the error stack */
		int i;

		for (i = 0; i < wrapper->err_count; i++) {
			efree(wrapper->err_stack[i]);
		}
		if (wrapper->err_stack) {
			efree(wrapper->err_stack);
		}
		wrapper->err_stack = NULL;
		wrapper->err_count = 0;
	}
}
#else
static void _rar_stream_tidy_wrapper_error_log(php_stream_wrapper *wrapper TSRMLS_DC)
{
	if (wrapper && FG(wrapper_errors)) {
		zend_hash_str_del(FG(wrapper_errors), (const char*)&wrapper, sizeof wrapper);
	}
}
#endif
/* }}} */

/* {{{ php_stream_rar_stater */
static int php_stream_rar_stater(php_stream_wrapper *wrapper,
#if PHP_MAJOR_VERSION < 7
								 char *url,
#else
								 const char *url,
#endif
								 int flags,
								 php_stream_statbuf *ssb,
								 php_stream_context *context TSRMLS_DC)
{
	/* flags includes PHP_STREAM_URL_STAT_LINK and PHP_STREAM_URL_STAT_QUIET
	 * We only respect the second */
	char *open_path = NULL,
		 *open_passwd = NULL;
	wchar_t *fragment = NULL;
	int  options = (flags & PHP_STREAM_URL_STAT_QUIET) ? 0 : REPORT_ERRORS;
	zval *volume_cb = NULL;
	size_t fragment_len;
	rar_file_t *rar;
	zval rararch;
	rar_find_output *state = NULL;
	int ret = FAILURE;

	/* {{{ preliminaries */
#if PHP_MAJOR_VERSION < 7
	Z_TYPE(rararch) = IS_NULL;
#else
	ZVAL_UNDEF(&rararch);
#endif

	if (_rar_get_archive_and_fragment(wrapper, url, options, 1,
			&open_path, &fragment, NULL TSRMLS_CC) == FAILURE) {
		goto cleanup;
	}

	if (context != NULL) {
		php_rar_process_context(context, wrapper, options, &open_passwd,
			NULL, &volume_cb TSRMLS_CC);
	}
	/* end preliminaries }}} */

	if (_rar_get_cachable_rararch(wrapper, options, open_path, open_passwd,
			volume_cb, &rararch, &rar TSRMLS_CC) == FAILURE)
		goto cleanup;

	if (fragment[0] == L'\0') {
		/* make sth up */
		struct RARHeaderDataEx t = {""};
		t.FileAttr = S_IFDIR | 0777;
		ret = _rar_stat_from_header(&t, ssb);
		goto cleanup;
	}

	fragment_len = wcslen(fragment);

	_rar_entry_search_start(rar, RAR_SEARCH_NAME, &state TSRMLS_CC);
	_rar_entry_search_advance(state, fragment, fragment_len + 1, 0);
	if (!state->found) {
		char *mb_entry = _rar_wide_to_utf_with_alloc(fragment,
			(int) fragment_len);
		php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
			"Found no entry %s in archive %s", mb_entry, open_path);
		efree(mb_entry);
		goto cleanup;
	}
	else
		ret	= _rar_stat_from_header(state->header, ssb);

	ret = SUCCESS;
cleanup:
	if (open_path != NULL) {
		efree(open_path);
	}

	if (fragment != NULL) {
		efree(fragment);
	}

	if (Z_TYPE(rararch) == IS_OBJECT) {
#if PHP_MAJOR_VERSION < 7
		zval_dtor(&rararch);
#else
		zval_ptr_dtor(&rararch);
#endif
	}
	if (state != NULL) {
		_rar_entry_search_end(state);
	}

	/* note PHP_STREAM_URL_STAT_QUIET is not equivalent to ~REPORT_ERRORS.
	 * ~REPORT_ERRORS instead of emitting a notice, stores the error in the
	 * wrapper, while with PHP_STREAM_URL_STAT_QUIET the error should not be
	 * put in the wrapper because the caller won't clean it up. For
	 * consistency, I treat both the same way but clean the wrapper in the end
	 * if necessary
	 */
	if (flags & PHP_STREAM_URL_STAT_QUIET) {
		_rar_stream_tidy_wrapper_error_log(wrapper TSRMLS_CC);
	}

	return ret;
}
/* }}} */

/* {{{ php_stream_rar_dir_opener */
static php_stream *php_stream_rar_dir_opener(php_stream_wrapper *wrapper,
#if PHP_MAJOR_VERSION < 7
											 char *filename,
											 char *mode,
											 int options,
											 char **opened_path,
#else
											 const char *filename,
											 const char *mode,
											 int options,
											 zend_string **opened_path,
#endif
											 php_stream_context *context
											 STREAMS_DC TSRMLS_DC)
{
	wchar_t *fragment;
	char *tmp_open_path = NULL, /* used to hold the pointer that may be copied to opened_path */
		 *open_passwd = NULL;
	int no_encode;
	zval *volume_cb = NULL;
	size_t fragment_len;
	rar_file_t *rar;
	php_rar_dir_stream_data_P self = NULL;
	php_stream *stream = NULL;

	/* {{{ preliminaries */
	if (options & STREAM_OPEN_PERSISTENT) {
		/* TODO: add support for opening RAR files in a persisten fashion */
		php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
			"No support for opening RAR files persistently yet");
		return NULL;
	}

	/* mode must be "r" or "rb", which, for BC reasons, are treated identically */
	if (mode[0] != 'r' || (mode[1] != '\0' && mode[1] != 'b') || strlen(mode) > 2) {
		php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
			"Only the \"r\" and \"rb\" open modes are permitted, given %s", mode);
		return NULL;
	}

	if (_rar_get_archive_and_fragment(wrapper, filename, options, 1,
			&tmp_open_path, &fragment, &no_encode TSRMLS_CC) == FAILURE) {
		goto cleanup;
	}

	if (context != NULL) {
		php_rar_process_context(context, wrapper, options, &open_passwd,
			NULL, &volume_cb TSRMLS_CC);
	}
	/* end preliminaries }}} */

	self = ecalloc(1, sizeof *self);

	if (_rar_get_cachable_rararch(wrapper, options, tmp_open_path, open_passwd,
			volume_cb, &self->rar_obj, &rar TSRMLS_CC) == FAILURE)
		goto cleanup;

	fragment_len = wcslen(fragment);
	self->directory = ecalloc(fragment_len + 1, sizeof *self->directory);
	wmemcpy(self->directory, fragment, fragment_len + 1);

	/* Remove the ending in the path separator */
	if (fragment_len > 0 &&
			self->directory[fragment_len - 1] == SPATHDIVIDER[0]) {
		self->directory[fragment_len - 1] = L'\0';
		self->dir_size = fragment_len;
	}
	else
		self->dir_size = fragment_len + 1;

	_rar_entry_search_start(rar, RAR_SEARCH_DIRECTORY | RAR_SEARCH_NAME,
		&self->state TSRMLS_CC);
	if (self->dir_size != 1) { /* skip if asked for root */
		/* try to find directory. only works if the directory itself was added
		 * to the archive, which I'll assume occurs in all good archives */
		_rar_entry_search_advance(
			self->state, self->directory, self->dir_size, 0);
		if (!self->state->found || !(self->state->header->Flags &
				RHDF_DIRECTORY)) {
			const char *message;
			char *mb_entry = _rar_wide_to_utf_with_alloc(self->directory,
				(size_t) self->dir_size - 1);

			if (!self->state->found)
				message = "Found no entry in archive %s for directory %s";
			else
				message = "Archive %s has an entry named %s, but it is not a "
					"directory";

			php_stream_wrapper_log_error(wrapper, options TSRMLS_CC,
				message, tmp_open_path, mb_entry);
			efree(mb_entry);
			goto cleanup;
		}
		self->self_header = self->state->header;
		/* we wouldn't have to rewind were it not for the last access cache,
		 * because otherwise the exact search for the directory and the
		 * directory traversal would both use the sorted list and the
		 * directory always appears before its entries there */
		_rar_entry_search_rewind(self->state);
	}

	self->no_encode = no_encode;

	stream = php_stream_alloc(&php_stream_rar_dirio_ops, self, NULL, mode);

cleanup:

	if (tmp_open_path != NULL) {
		if (opened_path != NULL) {
#if PHP_MAJOR_VERSION < 7
			*opened_path = tmp_open_path;
#else
			*opened_path =
					zend_string_init(tmp_open_path, strlen(tmp_open_path), 0);
#endif
		} else {
			efree(tmp_open_path);
		}
	}
	if (fragment != NULL)
		efree(fragment);

	if (stream == NULL) { /* failed */
		if (self != NULL) {
			if (Z_TYPE(self->rar_obj) == IS_OBJECT) {
#if PHP_MAJOR_VERSION < 7
				zval_dtor(&self->rar_obj);
#else
				zval_ptr_dtor(&self->rar_obj);
#endif
			}
			if (self->directory != NULL) {
				efree(self->directory);
			}
			if (self->state != NULL) {
				_rar_entry_search_end(self->state);
			}
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
	php_stream_rar_stater,
	php_stream_rar_dir_opener,	/* opendir */
	"rar wrapper",
	NULL,	/* unlink */
	NULL,	/* rename */
	NULL,	/* mkdir */
	NULL	/* rmdir */
};

php_stream_wrapper php_stream_rar_wrapper = {
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
