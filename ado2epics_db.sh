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
# Version 02 2016-08-09 Support for arrays, all stringType converted to 'stringout' records, 
#                       the rest, by default, are the 'ai' records.
# Version 03 2016-08-09 default NELM=2000

#''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
#        Parse arguments
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
#,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
#''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
echo "# EPICS database is generated on `date`"
echo "# from live ADO using following command:" 
echo "#    $0 $1 command"
echo "# NOTE, the variable-length arrays are transformed to waveform records with NELM=2000,"
echo "#       the NELM could be manually adjusted in this file." 
ADONAME=$1
CMD="adoMetaData -f $ADONAME"
#''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
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
       #'''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
       #                     close previous record and start new one
       ((NUMBER_OF_RECORDS++))
       VARIABLE=${WORDS[0]}
       #
       # the first record is 'value' (is that always correct?), set the record type according to property type
       RECTYPE="ai" # default record type is most generic: analog input, hold one double
       
       # is it stringout record?
       if [ ${WORDS[2]} == "StringType" ]; then RECTYPE="stringout"; fi
       
       # is it array?
       if [ ${WORDS[3]} != "1" ]; then # it is array
         RECTYPE="waveform"; 
         if [ ${WORDS[3]} == "0" ]; then # it is variable length array, not fully supported by EPICS,
           NELM="2000" # fix max length for variable array to 2, this can be changed in the output db file manually
           else NELM=${WORDS[3]}
         fi
       fi
       #
       # TODO: handle other types
       #
       printf "}\nrecord($RECTYPE, \"%s\")\n{\n" $VARIABLE
     fi
     #,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
     #'''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
     #                       Generate 'fields'
     if [ ${WORDS[1]} == "desc" ]; then
       printf "    field(DESC,"
       DESC_CMD="adoIf -ns -voncr $ADONAME $VARIABLE:desc"
       DESC=$($DESC_CMD)
       echo "\"${DESC:0:$EPICS_MAX_STRING_LENGTH}\")"
     else
       #'''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
       #                     VAL field
       if [ ${WORDS[1]} == "value" ]; then
         if [ $RECTYPE == "waveform" ]; then # for array the VAL should be omitted, and FVTL present.
           printf "    field(FTVL,\"DOUBLE\")\n" # DOUBLE should work for any type
           printf "    field(NELM,\"$NELM\")\n"  # number of elements
         else
           printf "    field(VAL,\"0\")\n"
         fi
       fi
     fi
     #,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
  done
  printf "}\n"
  echo "# Generated $NUMBER_OF_RECORDS EPICS records"
}
#,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
eval $CMD | process_cmd
#,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,


