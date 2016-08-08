#!/bin/bash
usage()
{
cat << EOF
usage: $0 [options] ADOName

Create one-to-one EPICS2ADO map from ADO for use in ado2epics and epics2ado

OPTIONS: 
   -vN  verbosity

EXAMPLE: 
   $0 simple.test > epics2ado.csv

EOF
}
# Version 01 2016-08-04 by Andrei Sukhanov
# Version 02 2016-08-08 Added space at the beginning of the output line. 
#                       Useful for manual manipulation of the generated map file

DIRECTION="<" # direction of parameter transfer
#         ">" from epics to ADO
#         "x" both directions  
# currently, only '<' directions are generated.
# TODO:  modify DIRECTION based on ADO variable property.

VERB=0
OPTIND=1 # to skip ADOName
while getopts "v:" opt; do
  case $opt in
    v) VERB=$OPTARG; echo "#VERB set to $VERB";;
    \?)
       echo "Invalid option: -$OPTARG" >&2
       usage
       exit 1
       ;;
    :)
      echo "Option -$OPTARG requires an argument." >&2
      exit 1
  esac
done
shift $(($OPTIND - 1))
if [ $# -lt 1 ]; then echo "No ADOName supplied"; usage; exit 1; fi
ADONAME=$1

process_cmd() {
  ii=0
  while read line
  do
     ((ii++))
     if [ $VERB -ne "0" ]; then echo "line $line"; fi
     WORDS=($line)
     echo " ,${WORDS[0]},$DIRECTION,${WORDS[0]}"
  done
  if [ $ii -eq "0" ]; then echo "No variables found in ADO \"$ADONAME\""; exit 1;
  else 
    echo "#Processed $ii variables"
  fi
}

CMD="adoMetaData -f $ADONAME |grep value"
echo "# epics2ado map is generated using \"$0 $1\" command"
eval $CMD | process_cmd
