--TEST--
rar_open() function
--SKIPIF--
<?php if(!extension_loaded("rar")) print "skip"; ?>
--FILE--
<?php

$rar_file1 = rar_open(dirname(__FILE__).'/linux_rar.rar'); 
var_dump($rar_file1);

$rar_file2 = rar_open(dirname(__FILE__).'/latest_winrar.rar'); 
var_dump($rar_file2);

?>
--EXPECTF--
resource(%d) of type (Rar)
resource(%d) of type (Rar)

