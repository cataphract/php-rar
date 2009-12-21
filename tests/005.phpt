--TEST--
rar_comment_get() function
--SKIPIF--
<?php if(!extension_loaded("rar")) print "skip"; ?>
--FILE--
<?php

$rar_file1 = rar_open(dirname(__FILE__).'/commented.rar'); 
var_export(rar_comment_get($rar_file1));
echo "\n";
var_export(rar_comment_get($rar_file1));
echo "\n";

$rar_file2 = rar_open(dirname(__FILE__).'/linux_rar.rar'); 
var_export(rar_comment_get($rar_file2));
echo "\n";

echo "Done\n";
?>
--EXPECTF--
'This is the comment of the file commented.rar.'
'This is the comment of the file commented.rar.'
NULL
Done
