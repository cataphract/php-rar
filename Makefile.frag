replace-run-tests:
	@if ! grep -q 'Minimum required PHP version: 5\.3\.0' run-tests.php; then \
		cp run-tests8.php run-tests.php; \
	fi

test: replace-run-tests
