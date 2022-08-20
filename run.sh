#!/bin/bash

# -e numero ETCS (1, 2)
# -m numero MAPPA (1, 2)
# -c esecuzione Makefile
# -d debug mode (ETCS2 non disponibile)

etcs=1          # default   ETCS1
mappa=1         # default   MAPPA1
debug=""        # default   NO DEBUG

usage_msg="Usage: $(basename $0) [-e arg] [-m arg] [-c] [-d]"

while getopts ":e:m:cdh" flags
do
    case "$flags" in
        e) etcs=${OPTARG};;
        m) mappa=${OPTARG};;
        c) make clean && make;;
        d) debug="gdb -x gdb-commands.txt --args";;
        h)
            echo $usage_msg
            exit 0
            ;;
        :)
            echo -e "option requires an argument.\n$usage_msg"
            exit 1
            ;;
        ?)
            echo -e "Invalid command option.\n$usage_msg"
            exit 1
            ;;
    esac
done

case $etcs in
    1) $debug bin/progetto-so ETCS$etcs MAPPA$mappa;;
    2) bin/progetto-so ETCS$etcs MAPPA$mappa & bin/progetto-so ETCS$etcs MAPPA$mappa RBC &;;
    ?)
        echo "ETCS$etcs opzione non valida"
        exit 1
        ;;
esac