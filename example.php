<?

$rar = rar_open('/www/obj.rar');
$entries = rar_list($rar);

var_dump($entries[0]->getName());
var_dump($entries[0]->extract('/www/test/',''));
var_dump($entries[0]->extract('','/www/test/2.php'));
var_dump($entries[0]->extract('','/www/test/1.php'));

?>
