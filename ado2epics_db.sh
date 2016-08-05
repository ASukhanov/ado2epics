#!/bin/bash
usage()
{
cat << EOF
usage: $0 [options] ADOName

Convert ADO metadata to EPICS records

OPTIONS: 
   -vN  verbosity

EXAMPLE: 
   $0 simple.test >! simple.db
   softIoc -d simple.db

EOF
}
# Version 01 2016-08-04 by Andrei Sukhanov

VERB=0
OPTIND=1 # to skip ADOName
while getopts "v:" opt; do
  case $opt in
    v) VERB=$OPTARG; echo "#verbosity=$VERB";;
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

EPICS_MAX_STRING_LENGTH=40 # EPICS has limited field sting length 

process_cmd()
{
  local VARIABLE="?"
  local NUMBER_OF_RECORDS=0
  local ii=0
  printf "#" # do not remove it!
  while read line
  do
     ((ii++))
     if [ $ii -le "2" ]; then continue; fi
     if [ $VERB -ne "0" ]; then echo "line $line"; fi
     WORDS=($line)
     if [ ${WORDS[0]} != $VARIABLE ]; then
       # close previous record and start new one
       ((NUMBER_OF_RECORDS++))
       VARIABLE=${WORDS[0]}
       printf "}\nrecord(ai, \"%s\")\n{\n" $VARIABLE
     fi
     if [ ${WORDS[1]} == "desc" ]; then
       printf "    field(DESC,"
       DESC_CMD="adoIf -ns -voncr $ADONAME $VARIABLE:desc"
       DESC=$($DESC_CMD)
       echo "\"${DESC:0:$EPICS_MAX_STRING_LENGTH}\")"
     else 
       if [ ${WORDS[1]} == "value" ]; then
         printf "    field(VAL,\"0\")\n"
       fi
     fi
  done
  printf "}\n"
  echo "# Generated $NUMBER_OF_RECORDS EPICS records"
}

echo "# EPICS database is generated using \"$0 $1\" command"
ADONAME=$1
CMD="adoMetaData -f $ADONAME"
eval $CMD | process_cmd


