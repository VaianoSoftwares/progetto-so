#!/bin/bash

# -e numero ETCS (1, 2)
# -m numero MAPPA (1, 2)
# -c esecuzione makeFile

etcs=1      # default   ETCS1
mappa=1     # default   MAPPA1

while getopts ":e:m:ch" flags
do
    case "$flags" in
        e) etcs=${OPTARG};;
        m) mappa=${OPTARG};;
        c) make;;
        h)
            echo "Usage: $(basename $0) [-e arg] [-m arg] [-c]"
            exit 0
            ;;
        :)
            echo -e "option requires an argument.\nUsage: $(basename $0) [-e arg] [-m arg] [-c]"
            exit 1
            ;;
        ?)
            echo -e "Invalid command option.\nUsage: $(basename $0) [-e arg] [-m arg] [-c]"
            exit 1
            ;;
    esac
done

case $etcs in
    1) bin/progetto-so ETCS$etcs MAPPA$mappa;;
    2) bin/progetto-so ETCS$etcs MAPPA$mappa RBC & bin/progetto-so ETCS$etcs MAPPA$mappa &;;
    ?) echo "ETCS$etcs opzione non valida";;
esac