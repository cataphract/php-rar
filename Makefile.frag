.PHONY: replace-run-tests
replace-run-tests:
	cp run-tests-rar.php run-tests.php

test: replace-run-tests
