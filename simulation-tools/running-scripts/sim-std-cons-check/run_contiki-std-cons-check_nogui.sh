#!/bin/bash
stack_to_run=std
csc_to_run=std-single-node-cons-check.csc


echo "Bash version ${BASH_VERSION}..."
for i in {1..10}
  do 
     echo "Running sim $i"
     java -mx1024m -jar /home/vinicius/Doutorado/contiki/tools/cooja/dist/cooja.jar -nogui="/home/vinicius/Documents/$csc_to_run" -contiki=/home/vinicius/Doutorado/contiki
     mv COOJA.testlog "COOJA-$stack_to_run-cons-check-$i.txt"
 done