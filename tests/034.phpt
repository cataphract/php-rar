--TEST--
RarException::(set/is)UsingExceptions() test
--SKIPIF--
<?php if(!extension_loaded("rar")) print "skip"; ?>
--FILE--
<?php
echo "Initial state: " . (RarException::isUsingExceptions()?'yes':'no')."\n";
echo "State change: " . (RarException::setUsingExceptions(true)?'success':'failure')."\n";
echo "Final state: " . (RarException::isUsingExceptions()?'yes':'no')."\n";
echo "Done.\n";
--EXPECTF--
Initial state: no
State change: success
Final state: yes
Done.
