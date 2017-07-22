#ifdef __cplusplus
extern "C" {
#endif

#include <php.h>
#include "php_rar.h"

void rar_time_convert(unsigned low, unsigned high, time_t *to) /* {{{ */
{
	time_t default_ = (time_t) 0,
		   local_time;
	struct tm tm = {0};
	TSRMLS_FETCH();

	if (high == 0U && low == 0U) {
		*to = default_;
		return;
	}

	/* 11644473600000000000 - number of ns between 01-01-1601 and 01-01-1970. */
	uint64 ushift=INT32TO64(0xA1997B0B,0x4C6A0000);

	/* value is in 10^-7 seconds since 01-01-1601 */
	/* convert to nanoseconds, shift to 01-01-1970 and convert to seconds */
	local_time = (time_t) ((INT32TO64(high, low) * 100 - ushift) / 1000000000);

	/* now we have the time in... I don't know what. It gives UTC - tz offset */
	/* we need to try and convert it to UTC */
	/* abuse gmtime, which is supposed to work with UTC */
	if (php_gmtime_r(&local_time, &tm) == NULL) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
			"Could not convert time to UTC, using local time");
		*to = local_time;
	}

	tm.tm_isdst = -1;
	*to = local_time + (local_time - mktime(&tm));
}
/* }}} */

int rar_dos_time_convert(unsigned dos_time, time_t *to) /* {{{ */
{
	struct tm time_s = {0};

	time_s.tm_sec  = (dos_time & 0x1f)*2;
	time_s.tm_min  = (dos_time>>5) & 0x3f;
	time_s.tm_hour = (dos_time>>11) & 0x1f;
	time_s.tm_mday = (dos_time>>16) & 0x1f;
	time_s.tm_mon  = ((dos_time>>21) & 0x0f) - 1;
	time_s.tm_year = (dos_time>>25) + 80;
	/* the dos times that unrar gives out seem to be already in UTC.
	 * Or at least they don't depend on TZ */
	if ((*to = timegm(&time_s)) == (time_t) -1) {
		return FAILURE;
	}

	return SUCCESS;
}
/* }}} */

#ifdef __cplusplus
}
#endif
