replace-run-tests:
	@if grep 'is_numeric($$valgrind_version)' run-tests.php > /dev/null; then \
		curl -L -f https://raw.githubusercontent.com/php/php-src/PHP-7.2.33/run-tests.php > run-tests.php; \
	fi

fixup-run-tests: replace-run-tests
	@if ! grep -q leak-check=full run-tests.php; then \
		sed -i 's/--trace-children=yes/--trace-children=no/' run-tests.php && \
		    if grep -q 'tool={$$this->tool}' run-tests.php; then \
				sed -i "s@--tool={\$$this->tool}@\0 --leak-check=full --gen-suppressions=all --suppressions='$(top_srcdir)/valgrind.supp' --num-callers=16 --run-libc-freeres=no@" run-tests.php; \
			elif grep -q wrapCommand run-tests.php; then \
				sed -i 's@--tool=memcheck@\0 --leak-check=full --gen-suppressions=all --suppressions="$(top_srcdir)/valgrind.supp" --run-libc-freeres=no@' run-tests.php; \
			else \
				sed -i "s@--tool=memcheck@\0 --leak-check=full --gen-suppressions=all --suppressions='$(top_srcdir)/valgrind.supp' --run-libc-freeres=no@" run-tests.php; \
			fi \
	fi

test: fixup-run-tests
