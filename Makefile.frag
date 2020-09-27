replace-run-tests:
	@if grep 'is_numeric($$valgrind_version)' run-tests.php > /dev/null; then \
		curl -L -f https://raw.githubusercontent.com/php/php-src/PHP-7.2.33/run-tests.php > run-tests.php; \
	fi

test: replace-run-tests
