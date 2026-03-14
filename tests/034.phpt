--TEST--
RarException::(set/is)UsingExceptions() test
--FILE--
<?php
echo "Initial state: " . (RarException::isUsingExceptions()?'yes':'no').".\n";
RarException::setUsingExceptions(true);
echo "State change done.\n";
echo "Final state: " . (RarException::isUsingExceptions()?'yes':'no').".\n";
echo "Done.\n";
--EXPECTF--
Initial state: no.
State change done.
Final state: yes.
Done.
