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
#ifdef ZEND_ENGINE_2

#include <wchar.h>

#include "php_rar.h"
#include "unrar/rartypes.hpp"

#include "php_streams.h"
/* will be needed to implement a wrapper
 * #include "ext/standard/file.h"
 * #include "ext/standard/php_string.h"
 * #include "fopen_wrappers.h"
 * #include "ext/standard/url.h"
 */

typedef struct php_rar_stream_data_t {
	struct RAROpenArchiveDataEx	open_data;
	struct RARHeaderDataEx		header_data;
	HANDLE						rar_handle;
	unsigned char				*buffer;
	size_t						buffer_size;
	size_t						buffer_cont_size;
	size_t						buffer_pos;
	uint64						cursor;
	int							no_more_data;
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

	if (close_handle) {
		if (self->open_data.ArcName != NULL) {
			efree(self->open_data.ArcName);
			self->open_data.ArcName = NULL;
		}
		if (self->buffer != NULL) {
			efree(self->buffer);
			self->buffer = NULL;
		}
		if (self->rar_handle != NULL) {
			int res = RARCloseArchive(self->rar_handle);
			if (_rar_handle_error(res TSRMLS_CC) == FAILURE) {
				; //not much we can do...
			}
			self->rar_handle = NULL;
		}
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

php_stream_ops php_stream_rario_ops = {
	php_rar_ops_write, php_rar_ops_read,
	php_rar_ops_close, php_rar_ops_flush,
	"rar",
	NULL, /* seek */
	NULL, /* cast */
	NULL, /* stat */
	NULL  /* set_option */
};

/* {{{ php_stream_rar_open */
php_stream *php_stream_rar_open(char *arc_name,
								char *utf_file_name,
								char *password,
								char *mode STREAMS_DC TSRMLS_DC)
{
	php_stream				*stream	= NULL;
	php_rar_stream_data_P	self	= NULL;
	int						result,
							found;

	if (strncmp(mode, "r", strlen("r")) != 0) {
		goto cleanup;
	}

	self = ecalloc(1, sizeof *self);
	self->open_data.ArcName		= estrdup(arc_name);
	self->open_data.OpenMode	= RAR_OM_EXTRACT;
	
	result = _rar_find_file(&self->open_data, utf_file_name, password,
		&self->rar_handle, &found, &self->header_data);

	if (_rar_handle_error(result TSRMLS_CC) == FAILURE) {
		goto cleanup;
	}

	if (!found)
		_rar_handle_ext_error("Can't find file %s in archive %s" TSRMLS_CC,
			utf_file_name, arc_name);

	{
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
#endif /* ZEND_ENGINE_2 */
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
