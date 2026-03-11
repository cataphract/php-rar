EXTRA_CFLAGS := $(EXTRA_CFLAGS) -Wall

.PHONY: replace-run-tests
replace-run-tests:
	cp run-tests-rar.php run-tests.php

test: replace-run-tests
