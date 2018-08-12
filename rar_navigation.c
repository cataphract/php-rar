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

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <php.h>
#include <wchar.h>
#include "php_rar.h"

#if HAVE_RAR

/* {{{ Structure definitions */

typedef struct _rar_find_state {
	rar_find_output			out;
	rar_file_t				*rar;
	size_t					index; /* next unread in entries_array or entries_array_s */
} rar_find_state;

struct _rar_unique_entry {
	size_t					id;				/* position in the entries_array */
	struct RARHeaderDataEx	entry;			/* last entry */
	unsigned long			packed_size;
	int						depth;			/* number of directory separators */
	size_t					name_wlen;		/* excluding L'\0' terminator */
};

/* last_accessed has the index of the last accessed entry. Its purpose is to make
 * more efficient the situation wherein the user traverses a directory and
 * stats each the gotten entry in each iteration. This gives 100% cache hits in
 * directory traversal tests 064 and 065 for exact name searches */
struct _rar_entries {
	size_t						num_entries;
	struct _rar_unique_entry	**entries_array; /* shoud not be NULL */
	struct _rar_unique_entry	**entries_array_s; /* sorted version for bsearch */
	struct _rar_unique_entry	*last_accessed;
	int							list_result; /* tell whether the archive's broken */
};
/* }}} */


/* {{{ Function prototypes for functions with internal linkage */
static void _rar_nav_get_depth_and_length(wchar_t *filenamew, const size_t file_size,
										  int *depth_out, size_t *wlen_out TSRMLS_DC);
static int _rar_nav_get_depth(const wchar_t *filenamew, const size_t file_size);
static int _rar_nav_compare_entries(const void *op1, const void *op2 TSRMLS_DC);
#if PHP_MAJOR_VERSION >= 7
static void _rar_nav_swap_entries(void *op1, void *op2);
#endif
static int _rar_nav_compare_entries_std(const void *op1, const void *op2);
static inline int _rar_nav_compare_values(const wchar_t *str1, const int depth1,
								   const wchar_t *str2, const int depth2,
								   const size_t max_size);
static int _rar_nav_directory_match(const wchar_t *dir, const size_t dir_len,
									const wchar_t *entry, const size_t entry_len);
static size_t _rar_nav_position_on_dir_start(const wchar_t *dir_name,
											 int dir_depth,
											 size_t dir_size,
											 struct _rar_unique_entry **entries,
											 size_t low, size_t high);
/* }}} */


/* {{{ Functions with external linkage */

/* {{{ _rar_entry_count */
size_t _rar_entry_count(rar_file_t *rar) {
	return rar->entries->num_entries;
}
/* }}} */

/* {{{ _rar_entry_search_start */
void _rar_entry_search_start(rar_file_t *rar,
							 unsigned mode,
							 rar_find_output **state TSRMLS_DC)
{
	rar_find_state **out = (rar_find_state **) state;
	assert(out != NULL);
	*out = ecalloc(1, sizeof **out);
	(*out)->rar = rar;
	(*out)->out.position = -1;
	assert(rar->entries != NULL);
	assert(rar->entries->num_entries == 0 || rar->entries->entries_array != NULL);
	if ((mode & 0x02U) && (rar->entries->num_entries > 0) &&
			(rar->entries->entries_array_s == NULL)) {
		rar->entries->entries_array_s = emalloc(rar->entries->num_entries *
			sizeof rar->entries->entries_array_s[0]);
		memcpy(rar->entries->entries_array_s, rar->entries->entries_array,
			rar->entries->num_entries * sizeof rar->entries->entries_array[0]);
#if PHP_MAJOR_VERSION < 7
		zend_qsort(rar->entries->entries_array_s, rar->entries->num_entries,
			sizeof *rar->entries->entries_array_s, _rar_nav_compare_entries
			TSRMLS_CC);
#else
		zend_qsort(rar->entries->entries_array_s, rar->entries->num_entries,
			sizeof *rar->entries->entries_array_s, _rar_nav_compare_entries,
			_rar_nav_swap_entries);
#endif
	}
}
/* }}} */

/* {{{ _rar_entry_search_seek */
void _rar_entry_search_seek(rar_find_output *state, size_t pos)
{
	rar_find_state *rstate	= (rar_find_state *) state;
	assert(pos >= 0);
	rstate->out.eof = 0;
	rstate->out.found = 0;
	rstate->out.position = -1;
	rstate->out.header = NULL;
	rstate->out.packed_size = 0;
	rstate->index = pos;
}
/* }}} */

/* {{{ _rar_entry_search_end */
void _rar_entry_search_end(rar_find_output *state)
{
	if (state) {
		/* may not have been initialized due to error conditions
		 * in rararch_it_get_iterator that jumped out of the function */
		efree(state);
	}
}
/* }}} */

/* {{{ _rar_entry_search_rewind */
void _rar_entry_search_rewind(rar_find_output *state)
{
	rar_find_state *rstate	= (rar_find_state *) state;
	rstate->out.eof = 0;
	rstate->out.found = 0;
	rstate->out.position = -1;
	rstate->out.header = NULL;
	rstate->out.packed_size = 0;
	rstate->index = 0;
}
/* }}} */

/* {{{ _rar_entry_search_advance */
void _rar_entry_search_advance(rar_find_output *state,
							   const wchar_t * const file, /* NULL = give next */
							   size_t file_size, /* length + 1; 0 if unknown */
							   int directory_match)
{
	rar_find_state		*rstate = (rar_find_state *) state;
	struct _rar_entries *entries;
	int					found = FALSE;
	int					in_sorted;
	size_t				filenamewsize;

	assert(state != NULL);
	assert(file == NULL || file_size == 0 || file[file_size - 1] == L'\0');

	entries = rstate->rar->entries;
	assert(entries != NULL);

	if ((file != NULL) && (file_size == 0))
		file_size = wcslen(file) + 1;

	/* reset output */
	memset(&rstate->out, 0, sizeof rstate->out);

	filenamewsize = sizeof(entries->entries_array[0]->entry.FileNameW) /
		sizeof(entries->entries_array[0]->entry.FileNameW[0]); /* = 1024 */
	if (rstate->out.eof || (rstate->index >= entries->num_entries) ||
			(file_size > filenamewsize)) {
		rstate->out.found = 0;
		rstate->out.eof = 1;
		return;
	}

	/* three different cases:
	 * (1) ask next
	 * (2) ask by name
	 * (3) ask next directory child */

	if (!directory_match && (file == NULL)) {
		/* ask next */
		in_sorted = FALSE;
		found = TRUE;
		/* populate cache for exact name access */
		entries->last_accessed = entries->entries_array[rstate->index];
	}
	else if (!directory_match) {
		/* ask by exact name */
		struct _rar_unique_entry temp_entry,
								 *temp_entry_ptr = &temp_entry,
								 **found_entry;
		/* try to hit cache */
		if (entries->last_accessed != NULL) {
			if ((entries->last_accessed->name_wlen == file_size - 1) &&
					wmemcmp(entries->last_accessed->entry.FileNameW, file,
					file_size) == 0) {
				/* cache hit */
				in_sorted = FALSE;
				found = TRUE;
				rstate->index = entries->last_accessed->id;
				/*php_printf("cache hit\n", entries);*/
			}
			else {
				entries->last_accessed = NULL;
				/*php_printf("cache miss\n", entries);*/
			}
		}
		/*else
			php_printf("cache miss (empty)\n", entries);*/

		if (!found) { /* the cache didn't do; use binary search */
			wmemcpy(temp_entry.entry.FileNameW, file, file_size);
			temp_entry.depth = _rar_nav_get_depth(file, file_size);
			found_entry = bsearch(&temp_entry_ptr,
				&entries->entries_array_s[rstate->index],
				entries->num_entries - rstate->index,
				sizeof entries->entries_array_s[0],
				_rar_nav_compare_entries_std);
			if (found_entry != NULL) {
				in_sorted = TRUE;
				found = TRUE;
				rstate->index = found_entry - entries->entries_array_s;
			}
		}
	}
	else {
		/* ask by next directory child */
		struct _rar_unique_entry *cur = entries->entries_array_s[rstate->index];
		in_sorted = TRUE;
		assert(file != NULL);
		if (_rar_nav_directory_match(file, file_size - 1,
				cur->entry.FileNameW, cur->name_wlen)) {
			found = TRUE;
			/* populate cache for exact name access */
			entries->last_accessed = cur;
		}
		else {
			/* no directory match for current */
			int comp, dir_depth;
			dir_depth = _rar_nav_get_depth(file, file_size);
			comp = _rar_nav_compare_values(cur->entry.FileNameW, cur->depth,
				file, dir_depth + 1, file_size); /* guaranteed file_size <= 1024 */
			assert(comp != 0); /* because + 1 was summed to the depth */
			if (comp > 0) {
				/* past the entries of the directory */
				/* do nothing */
			}
			else {
				int pos = _rar_nav_position_on_dir_start(file, dir_depth,
					file_size, entries->entries_array_s, rstate->index,
					entries->num_entries);
				if (pos != -1) {
					found = TRUE;
					rstate->index = pos;
					/* populate cache for exact name access */
					entries->last_accessed = entries->entries_array_s[pos];
				}
			}
		}
	}

	if (found == FALSE) {
		rstate->out.found = 0;
		rstate->out.eof = 1;
	}
	else {
		struct _rar_unique_entry *cur;
		if (in_sorted)
			cur = entries->entries_array_s[rstate->index];
		else
			cur = entries->entries_array[rstate->index];
		rstate->out.found = 1;
		rstate->out.position = cur->id;
		rstate->out.header = &cur->entry;
		rstate->out.packed_size = cur->packed_size;
		rstate->index++;
	}
}
/* }}} */

/* {{{ _rar_delete_entries - accepts an allocated entries list */
void _rar_delete_entries(rar_file_t *rar TSRMLS_DC)
{
	if (rar->entries != NULL) {
		if (rar->entries->entries_array != NULL) {
			size_t i;
			for (i = 0; i < rar->entries->num_entries; i++) {
				if (rar->entries->entries_array[i]->entry.RedirName != NULL) {
					efree(rar->entries->entries_array[i]->entry.RedirName);
				}
				efree(rar->entries->entries_array[i]);
			}
			efree(rar->entries->entries_array);

			if (rar->entries->entries_array_s != NULL)
				efree(rar->entries->entries_array_s);
		}
		efree(rar->entries);
	}
}
/* }}} */

/* guarantees correct initialization of rar->entries on failure
 * If the passed rar_file_t structure has the allow_broken option, it
 * always returns success (ERAR_END_ARCHIVE) */
int _rar_list_files(rar_file_t *rar TSRMLS_DC) /* {{{ */
{
	int result = 0;
	size_t capacity = 0;
	int first_file_check = TRUE;
	unsigned long packed_size = 0UL;
	struct _rar_entries *ents;

	if (rar->entries != NULL) {
		/* we've already listed this file's entries */
		if (rar->allow_broken)
			return ERAR_END_ARCHIVE;
		else
			return rar->entries->list_result;
	}

	assert(rar->entries == NULL);
	rar->entries = emalloc(sizeof *rar->entries);
	ents = rar->entries;
	ents->num_entries = 0;
	ents->entries_array = NULL;
	ents->entries_array_s = NULL;
	ents->last_accessed = NULL;

	while (result == 0) {
		struct _rar_unique_entry *ue;
		struct RARHeaderDataEx entry = {0};
		wchar_t redir_name[1024] = L"";
		entry.RedirName = redir_name;
		entry.RedirNameSize = sizeof(redir_name) / sizeof(redir_name[0]);
		result = RARReadHeaderEx(rar->arch_handle, &entry);
		/* value of 2nd argument is irrelevant in RAR_OM_LIST_[SPLIT] mode */
		if (result == 0) {
			result = RARProcessFile(rar->arch_handle, RAR_SKIP, NULL, NULL);
		}
		if (result != 0)
			break;

		if (first_file_check) {
			if (entry.Flags & RHDF_SPLITBEFORE)
				continue;
			else
				first_file_check = FALSE;
		}

		/* reset packed size if not split before */
		if ((entry.Flags & RHDF_SPLITBEFORE) == 0)
			packed_size = 0UL;

		/* we would exceed size of ulong. cap at ulong_max
		 * equivalent to packed_size + entry.PackSize > ULONG_MAX,
		 * but without overflowing */
		if (ULONG_MAX - packed_size < entry.PackSize)
			packed_size = ULONG_MAX;
		else {
			packed_size += entry.PackSize;
			if (entry.PackSizeHigh != 0) {
#if ULONG_MAX > 0xffffffffUL
				packed_size += ((unsigned long) entry.PackSizeHigh) << 32;
#else
				packed_size = ULONG_MAX; /* cap */
#endif
			}
		}

		if (entry.Flags & RHDF_SPLITAFTER) /* do not commit */
			continue;

		/* commit the entry */
		assert(capacity >= ents->num_entries);
		if (capacity == ents->num_entries) { /* 0, 2, 6, 14, 30... */
			capacity = (capacity + 1) * 2;
			ents->entries_array = safe_erealloc(ents->entries_array, capacity,
				sizeof(*ents->entries_array), 0);
		}
		assert(capacity > ents->num_entries);

		ents->entries_array[ents->num_entries] = ue =
			emalloc(sizeof *ents->entries_array[0]);
		memcpy(&ue->entry, &entry, sizeof ents->entries_array[0]->entry);
		ue->id = ents->num_entries;
		ue->packed_size = packed_size;
		_rar_nav_get_depth_and_length(entry.FileNameW,
			sizeof(entry.FileNameW) / sizeof(entry.FileNameW[0]), /* = 1024 */
			&ue->depth, &ue->name_wlen TSRMLS_CC);
		if (redir_name[0] != L'\0') {
			size_t size = (wcslen(redir_name) + 1) * sizeof(redir_name[0]);
			ue->entry.RedirName = emalloc(size);
			memcpy(ue->entry.RedirName, redir_name, size);
		} else {
			ue->entry.RedirName = NULL;
			ue->entry.RedirNameSize = 0;
		}
		ents->num_entries++;
	}

	rar->entries->list_result = result;

	return rar->allow_broken ? ERAR_END_ARCHIVE : result;
}
/* }}} */

/* end functions with external linkage }}} */


/* {{{ Functions with internal linkage */

static void _rar_nav_get_depth_and_length(wchar_t *filenamew, const size_t file_size,
										  int *depth_out, size_t *wlen_out TSRMLS_DC) /* {{{ */
{
	size_t	i;
	int		depth = 0;

	assert(file_size >= 1);

	for (i = 0; i < file_size; i++) {
		if (filenamew[i] == L'\0')
			break;
		if (filenamew[i] == SPATHDIVIDER[0])
			depth++;
	}

	if (i == file_size) { /* should not happen */
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
			"The library gave an unterminated file name. "
			"This is a bug, please report it.");
		i--;
		filenamew[i] = L'\0';
	}

	if ((i >= 1) && (filenamew[i-1] == SPATHDIVIDER[0])) {
		/* entry name ended in path divider. shouldn't happen */
		i--;
		filenamew[i] = L'\0';
		depth--;
	}

	*depth_out = depth;
	if (wlen_out != NULL)
		*wlen_out = (size_t) i;
}
/* }}} */

static int _rar_nav_get_depth(const wchar_t *filenamew, const size_t file_size) /* {{{ */
{
	size_t	i;
	int		depth = 0;

	for (i = 0; i < file_size; i++) {
		if (filenamew[i] == L'\0')
			break;
		if (filenamew[i] == SPATHDIVIDER[0])
			depth++;
	}
	assert(i < file_size);

	return depth;
}
/* }}} */

static int _rar_nav_compare_entries(const void *op1, const void *op2 TSRMLS_DC) /* {{{ */
{
	const struct _rar_unique_entry *a = *((struct _rar_unique_entry **) op1),
								   *b = *((struct _rar_unique_entry **) op2);

	return _rar_nav_compare_values(a->entry.FileNameW, a->depth,
		b->entry.FileNameW, b->depth,
		sizeof(a->entry.FileNameW) / sizeof(a->entry.FileNameW[0]) /*1024*/);
}
/* }}} */

#if PHP_MAJOR_VERSION >= 7
static void _rar_nav_swap_entries(void *op1, void *op2) /* {{{ */
{
	/* just swaps two pointer values */
	struct _rar_unique_entry **a = op1,
							 **b = op2,
							 *tmp;
	tmp = *a;
	*a = *b;
	*b = tmp;

}
/* }}} */
#endif

static int _rar_nav_compare_entries_std(const void *op1, const void *op2) /* {{{ */
{
	const struct _rar_unique_entry *a = *((struct _rar_unique_entry **) op1),
								   *b = *((struct _rar_unique_entry **) op2);

	return _rar_nav_compare_values(a->entry.FileNameW, a->depth,
		b->entry.FileNameW, b->depth,
		sizeof(a->entry.FileNameW) / sizeof(a->entry.FileNameW[0]) /*1024*/);
}
/* }}} */

static inline int _rar_nav_compare_values(const wchar_t *str1, const int depth1,
								   const wchar_t *str2, const int depth2,
								   const size_t max_size) /* {{{ */
{
	if (depth1 == depth2) {
		return wcsncmp(str1, str2, max_size);
	}
	else {
		return depth1 > depth2 ? 1 : -1;
	}
}
/* }}} */

/* does not assume null termination */
static int _rar_nav_directory_match(const wchar_t *dir, const size_t dir_len,
									const wchar_t *entry, const size_t entry_len) /* {{{ */
{
	const wchar_t *chr,
				  *entry_rem;
	size_t		  entry_rem_len;

	/* dir does not end with the path separator */

	if (dir_len > 0) {
		if (entry_len <= dir_len) /* don't match the dir itself */
			return FALSE;
		/* assert(entry_len > dir_len > 0) */
		if (wmemcmp(dir, entry, dir_len) != 0)
			return FALSE;
		/* directory name does not follow path sep or path sep ends the name */
		if (entry[dir_len] != SPATHDIVIDER[0] || entry_len == dir_len + 1)
			return FALSE;
		/* assert(entry_len > dir_len + 1) */
		entry_rem = &entry[dir_len + 1];
		entry_rem_len = entry_len - (dir_len + 1);
	}
	else {
		entry_rem = entry;
		entry_rem_len = entry_len;
	}

	chr = wmemchr(entry_rem, SPATHDIVIDER[0], entry_rem_len);
	/* must have no / after the directory */
	return (chr == NULL);
}
/* }}} */

static size_t _rar_nav_position_on_dir_start(const wchar_t *dir_name,
											 int dir_depth,
											 size_t dir_size,
											 struct _rar_unique_entry **entries,
											 size_t low, size_t high) /* {{{ */
{
	size_t	mid;
	int		comp;
	size_t	orig_high = high;

	if (dir_size == 1) { /* root */
		if (low >= high)
			return -1;

		if (entries[low]->depth == 0)
			return low;
		else
			return -1;
	}

	while (low < high) {
		mid = low + (high - low) / 2;
		comp = _rar_nav_compare_values(dir_name, dir_depth + 1,
			entries[mid]->entry.FileNameW, entries[mid]->depth,
			dir_size);
		if (comp > 0)
			low = mid + 1;
		else
			high = mid;
	}

	if (low >= orig_high)
		return -1;

	if (_rar_nav_directory_match(dir_name, dir_size - 1,
			entries[low]->entry.FileNameW, entries[low]->name_wlen))
		return low;
	else
		return -1;
}
/* }}} */


/* end functions with internal linkage */

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


