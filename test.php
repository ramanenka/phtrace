<?php

class A {
    public function f1($n) {
    	usleep(rand(200, 1000));
        if ($n < 40) {
             for ($i = 1; $i <= rand(1, 3); $i++) {
                $this->f1($n + rand(1, 15));
                if ($n % 4 == 0) {
                	$this->f1($n + 1);
                }
             }
        }
        usleep(rand(200, 1000));
    }
}

class B extends A {

}

$b = new B();
$b->f1(0);
