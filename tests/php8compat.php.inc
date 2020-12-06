<?php

ini_set('pcre.jit', '0'); // avoid some valgrind errors

function argerr($cl) {
    try {
        return $cl();
    } catch (TypeError $err) {
        $msg = $err->getMessage();
        $msg = "Warning: $msg in {$err->getFile()} on line {$err->getLine()}";
        $msg = preg_replace('/: Argument #(\d) \(\$\S+\) must be of type/',
                            ' expects parameter \1 to be', $msg);
        $msg = preg_replace('/expects (.+) (\d) argument/',
                            'expects \1 \2 parameter', $msg);
        echo "\n", $msg, "\n";
    }
}
