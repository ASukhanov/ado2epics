// Simple bridge from ADO to epics

// Version v01 2016-07-11 by &RA. based 99.9% on adoIfA
// Version v02 2016-07-13 by &RA. Good. REMOVE_USELESS, DONOT_CAGET, gverb, one argument, gpv2ado_map from file
// Version v03 2016-07-15 by &RA. Good. Major cleanup. getopts, parse_epics2ado_csvmap
// Version v05 2016-07-15 by &RA. Good. extern "C" int gVerb
// Version v06 2016-08-08 by &RA. Call  adoChanged() on initial property setting
// Version v07 2016-08-08. Support for variable arrays. String and numbers treated differently

#include <iostream>
#include <iomanip>
#include <string.h>
#include <unistd.h>
#include "adoIf/adoIf.hxx"
#include "Async/AsyncHandler.hxx"
#include "rhicError/rhicError.h"
#include <string>
#include "ifHandlerLib/ConnManager.hxx" // Connection Manager

#include <cns/cnsRequest.hxx>

using namespace std;

#define STATIC
//#define PRINTRID

static int numReceived = 0;
static int stopAfter = 0;
static int pauseAfter = 0;
static int pauseState = 0;
static int retry = 0;
static bool printUnixTimestamp = false;
#define VERB_INFO 1
#define VERB_DEBUG 2
#define VERB_DETAILED 4
extern "C" int gVerb; //printout verbosity. bit[0] info messages, bit[1]: debugging messages, bit[2]: detailed debugging


//'''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
//&RA
#define REMOVE_USELESS

extern "C" int caput_string(const char* pvname, const char* value);
extern "C" int caput_numbers(const char* pvname, int nvals, const double *values);

//#define gpv2ado_map_filename "epics2ado.csv"
char *gpv2ado_map_filename = NULL;

//
//'''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
#include <stdlib.h>

#define MAXRECORDS 300
#define MINCOLS 3 // expected minimum number of columns
#define MAXTOKENS  MAXRECORDS*MINCOLS
#define MAXTOKENSIZE 100
#define EPICS2ADO_COLUMN_EPICS 0 //
#define EPICS2ADO_COLUMN_ADO 2

#undef ADO_GROUPING

// globals
char *gpv2ado_map[MAXTOKENS]; // epics-to-ado map, 3 elements per record: 0:epics_PVname, 1:flag, 2:ado_param_name
                              // the flag defines the data direction: three options: '>', '<', and 'x'
char gstorage[MAXTOKENS*MAXTOKENSIZE];
int gncols=0;
int gnPvs=0;
char *gAdoName=NULL;

// parse_epics2ado_csvmap
// read the csv file with epics-to-ado table and fill gpv2ado_map
int parse_epics2ado_csvmap(
  const char *filename,  //in:
  const int selectkey,   //in: select key for column[1]
                         // selectkey='>': select records for epics -> ado conversion
                         // selectkey='<': select records for ado -> epics conversion
                         // selectkey='x': select all records
  int *ncols,            //out: number of columns in the table
  char *tokens[],        //io: array for accepting the token pointers
  const int max_tokens,  //in: size of array of token pointers
  char *storage,         //io: storage for token strings
  const int storage_size //in: size of the storage
)
{
  #define MAX_STRING_LENGTH 100
  FILE *pFile;
  char mystring [MAX_STRING_LENGTH];
  char *instring = NULL;
  char *pch;
  int ntoks=0;
  int filled=0;
  int ii=0;
  int col=0;
  char *key = NULL;
  *ncols = 0;

  pFile = fopen (filename,"r");
  if (pFile == NULL)
  {
    sprintf(mystring,"ERROR opening file %s",filename);
    perror (mystring);
    return 1;
  }
  if(gVerb&VERB_INFO) printf("processing %s for epics %c ado records\n",filename,selectkey);
    while (fgets (mystring , MAX_STRING_LENGTH , pFile) != NULL )
    {
      ii++;
      //if(ii>10) break;
      if(gVerb&VERB_DETAILED) puts (mystring);
      if(mystring[0]=='#') continue;
      instring = mystring+1; //skip first comma
      instring = strchr(mystring,',') + 1; //skip everything before first comma
      //check if the field in the second column matches the key
      key = strchr(instring,',');
      if(key[1] != selectkey && selectkey != '*') //check if record should be dropped
        if(key[1] != 'x')
           if (!(key[1] != '.' && selectkey == 'x')) //
        {
          //printf("dropping line %i key %c, select %c\n",ii,key[1],(char)selectkey);
          continue;
        }
      if(gVerb&VERB_DEBUG) printf ("Splitting string \"%s\" into tokens:\n",instring);
      pch = strtok (instring," ,\"\n");
      col = 0;
      while (pch != NULL)
      {
        if(ntoks >= max_tokens) {printf("ERROR. too many tokens in epics2ado.csv\n"); exit(EXIT_FAILURE);}
        if(filled + (int)strlen(pch) >= storage_size) {perror("ERROR. too little storage for epics2ado.csv\n"); exit(EXIT_FAILURE);}

        tokens[ntoks] = storage + filled;
        //printf ("<%s>\n",pch);
        strcpy(storage+filled,pch);
        filled += strlen(pch)+1;
        ntoks++;
        pch = strtok (NULL, " ,\"\n");
        col++;
      }
      if(ncols[0]==0) ncols[0] = col;
      else if(ncols[0] != col)
      {
        printf("ERROR inconsistent number of columns in the epics2ado table line %i, col %i\n",ii, col);
        exit(EXIT_FAILURE);
      }
  }
  ntoks /= ncols[0];
  if(gVerb&VERB_INFO) printf("Number of records selected: %i\n",ntoks);
  //for(ii=0;ii<ntoks;ii++) printf("<%s>\n",tokens[ii]);

  fclose(pFile);
  return ntoks;
}
// param2pv
// return the ado parameter name for the epics PV using conversion table
char* param2pv(const char* pvname, char* table[], const int ncols, const int nrows)
{
	int ii;
	char* nothing = "";
	for (ii=0; ii<nrows; ii++)
	{
		if(strcmp(table[ii*ncols+EPICS2ADO_COLUMN_ADO],pvname) == 0 ) return table[ii*ncols+EPICS2ADO_COLUMN_EPICS];
	}
	return nothing;
}

void adoChanged(const char* adoName, const char* propertyID, int count, const char* aString)
{
	char* rr;
	if(gVerb&VERB_DEBUG) cout<<"ADO changed: "<<adoName<<" "<<propertyID<<":["<<count<<"] = "<<aString<<endl;
	rr = param2pv(propertyID,gpv2ado_map,gncols,gnPvs);
	if(rr!=NULL)
	{
		if(gVerb&VERB_DEBUG) cout<<"caput("<<rr<<"["<<count<<"],"<<aString<<")"<<endl;
		caput_string(rr,aString);
	}
}
//,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
//======================================================================
int errcb(AdoIf *a, int count, const char* const propertyID[],
	  const int adoStatus[], int const paramStatus[],
	  const AsyncSetup *setup, void *arg, const void *reqId)
{
  const char * const *adoNames = a->AdoNames();
  int numAdos = a->NumAdos();
  for(int ia = 0; ia < numAdos; ia++)
    for(int ip = 0; ip < count; ip++){
      int cp = ia * count + ip;
      // use ADO_FAILED ?
      int status = (paramStatus[cp]) ? paramStatus[cp] : adoStatus[ia];
      cout << "# " << adoNames[ia] << " " << propertyID[ip] << " error: <"
	   << RhicErrorNumToErrorStr(status) << ">" << endl;
    }
  return TRUE;
}

//======================================================================
int datacb(AdoIf *a, int count, const char* const propertyID[], Value *data[],
		const AsyncSetup* setup, void *arg, const void *reqId) {
	numReceived++;

	//Note: It looks like the count is always 1.
	string _timeString;
	int Time;
	if (printUnixTimestamp) {
		time_t _timestamp;
		time(&_timestamp);
		_timeString = ctime(&_timestamp);
		_timeString.erase(_timeString.length() - 1, 1);
	} else
		Time = time(0) % 3600;
	const char * const *adoNames = a->AdoNames();
	int numAdos = a->NumAdos();
	if (gVerb & VERB_DETAILED)
	  cout << "request id = " << reqId << " = " << *(int *)reqId << " received"
	       << " (" << numAdos << " ados, count = " << count << ")" << endl;
	Value *vptr = NULL;
	double *dptr = NULL;
	int nvalues=0;
	char *pvname=NULL;
	for (int ia = 0; ia < numAdos; ia++)
		for (int ip = 0; ip < count; ip++)
		{
			vptr = data[ia * count + ip];
			if (vptr == NULL) continue;
			nvalues = (int)vptr->Length();
			if(gVerb & VERB_DEBUG) {
				cout<<"datacb["<<ip<<"] "<<propertyID[ip]<<"["<<nvalues <<"]= "<<":";
				if(vptr->IsNumeric())
				{
					dptr = vptr->DoublePtr();
					for(int ii=0;ii<nvalues;ii++) cout<<dptr[ii]<<" ";
					cout<<endl;
				}
				else cout<<vptr->StringPtr()[0]<<endl;
			}
			pvname = param2pv(propertyID[ip],gpv2ado_map,gncols,gnPvs);
			if(pvname==NULL) continue;
			if(vptr->IsNumeric())
			{
				dptr = vptr->DoublePtr();
				caput_numbers(pvname,nvalues,dptr);
			}
			else caput_string(pvname,vptr->StringPtr()[0]);
			//char *aString = data[ia * count + ip]->StringVal(' ');
			//adoChanged(adoNames[ia], propertyID[ip],  , nvalues, aString);
			/*
			if (gVerb & VERB_INFO)
			{
				cout<<"adoChanged: "<< setw(5) << numReceived % 1000 << " ";
				if (printUnixTimestamp) {
					cout << setw(25) << _timeString.c_str() << "  ";
				} else {
					int Time = time(0) % 3600;
					cout << setw(5) << Time << "  ";
				}
				//&RA/char *aString = data[ia * count + ip]->StringVal(' ');
				cout << adoNames[ia] << " " << propertyID[ip] << " : " << "<"
						<< vptr->StringPtr()[0] << ">" << endl;
			}
			*/
			//&RA/ the following line was in the original adoIfA.cxx, not sure the purpose of it.
			//free(aString); // to avoid Mismatched free() / delete / delete [] //delete [] aString;
		}
	cout.flush();

	if (retry and retry <= numReceived) {
		//gsetup->SetImmediate(0); // no immediate
		void *new_reqId = (void *) reqId;
		// this does not work with many properties deliveries
		a->GetAsync(propertyID[0], setup, data, &new_reqId);
		if (*(int *) new_reqId != *(int *) reqId) {
			cout << "during re-requesting of " << adoNames[0] << " : "
					<< propertyID[0] << " request id changed, "
					<< *(int *) reqId << " -> " << *(int *) new_reqId << endl;
		} else
			cout << "re-requesting " << adoNames[0] << " : " << propertyID[0]
					<< endl;
		retry = 0;
	}
#ifndef STATIC
	for(int i = 0; i < numAdos * count; i++)
	Unref( data[i] );
#endif
	return TRUE;
}

void timerCb(void *arg, unsigned long* tid) {
	AsyncHandler *asyncHandler = (AsyncHandler *) arg;
	if (pauseState == 0) {
		cout << "Pausing for " << pauseAfter << " seconds" << endl;
		GlobalCommController()->AsyncPause();
		pauseState = 1;
	} else {
		cout << "Resuming for " << pauseAfter << " seconds" << endl;
		GlobalCommController()->AsyncResume();
		pauseState = 0;
	}

	asyncHandler->RegisterTimer(pauseAfter * 1000, timerCb, asyncHandler);
}

void connNotifyCallback(tcpClient* client, int state, void* arg) {
	string s;
	if (state)
		s += "Heartbeat re-established to ADO server ";
	else
		s += "Heartbeat LOST to ADO server ";
	s += client->getServerName();
	cout << s << endl;
}

//======================================================================
void usage(const char *progname)
{
  cout <<progname<<" <OPTIONS> ado_name epics-ado_map_file\n";
  cout << "-u N    desired ppm user N.\n";
  cout << "-i      request immediate get async.\n";
//ADO_GROUPING  cout << "-g      deliver data grouped.\n";
  cout << "-n      use non-blocking call to get data (not recommended).\n";
  cout << "-v N    printout verbosity, bit[1]: info, bit[2]: debug. Default 1.\n";
  cout << "-s N    stop after N data deliveries.\n";
  cout << "-k N    send data every Nth time.\n";
  cout << "-I N    send data no more frequently than N msec.\n";
  cout << "-c N    send data when it changes more than N.\n";
  cout << "-t      send data when it gets out of/back in tolerance.\n";
  cout << "-p N    pause/resume for N sec.\n";
  cout << "-T      print a full text unix timestamp with every callback.\n";
  cout << "-a ADO  ado name.\n";
  cout << "-m map  name of the map file (csv format) for conversion from EPICS PV to ADO variables.\n";
  cout<<"Example: "<<progname<<" -T -a simple.test -m epics2ado.csv\n";
}

//======================================================================
int main(int argc, char *argv[])
  // this is the main program adoIfA which is a command line to catch async
  // messages from a property.
  // The command takes at least two arguments <ado name> and <property name>.
  // The command will make a request for async reports from the property and
  // wait for them.  When the arrive they will be written to standard output.
{
#define MAXADOIFS 5
AdoIf *adoIfArray[ MAXADOIFS ];

  // Expect at least two arguments
  if(argc < 2){
    usage(argv[0]);
    cerr << "ADO name missing" << endl;
    exit(1);
  } // end test on the arguments
  // set up the synchronous handler to handle all the asynchronous events
  AsyncHandler *asyncHandler = new AsyncHandler();
  int argIndex = 1;

  int ppmUser = 0;
  int immediate = 0;
  int grouped = 0;
  int getasync = 1;
  ADOIF_ASYNC_FILTER_TYPE filterType = ASYNC_FILTER_NONE;
  double filterValue = 0.;

//'''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
// process arguments
//
  int opt;
  unsigned int numa = 0;
  int retval = 0;

  while ((opt = getopt(argc, argv, "ignTtv:u:s:k:I:c:a:m:h")) != -1)
  {
	 //cout<<"opt "<<(char)opt<<" ";
     switch (opt) {
     case 'i': immediate = 1; cout<<"immediate\n";break;
//ADO_GROUPING     case 'g': grouped = 1; break;
     case 'n': getasync = 0; cout<<"non-blocking call to get data\n";break;
     case 'T': printUnixTimestamp = true; cout<<"time stamped\n";break;
     case 't': filterType = ASYNC_OUT_OF_TOLERANCE; cout<<"filterType set to "<<filterType<<endl;break;
     case 's': stopAfter = atoi(optarg); cout<<"stop after "<<stopAfter<<endl; break;
     case 'k': filterType = ASYNC_SKIP_FACTOR; filterValue = atoi(optarg); cout<<"filter = skip "<<filterValue<<endl;break;
     case 'I': filterType = ASYNC_MINIMUM_INTERVAL; filterValue = atoi(optarg); cout<<"filter = min interval "<<filterValue<<endl;break;
     case 'c': filterType = ASYNC_MINIMUM_CHANGE; filterValue = atoi(optarg); cout<<"filter = change "<<filterValue<<endl;break;
     case 'v': gVerb = atoi(optarg); cout<<"verbosity set to "<<gVerb<<endl; break;
     case 'u': ppmUser = atoi(optarg); cout<<"ppmUser set to "<<ppmUser<<endl;break;
     case 'a': // additional ADO
			//cout << numa << ": new AdoIf["<<numa<<"](" <<optarg<< ")" << endl;
			if (numa >= MAXADOIFS) {cerr<<"Error. Too many ADOs > "<<MAXADOIFS<<endl; exit(25);}
			adoIfArray[numa] = new AdoIf(optarg);
			if (ppmUser) {
				cout << "SetPpmUser(" << ppmUser << ")" << endl;
				adoIfArray[numa]->SetPpmUser(ppmUser);
			}
			retval = adoIfArray[numa]->CreateOK();
			if (retval != 0) {
				cerr << "Error creating " << optarg << " ado: " << RhicErrorNumToErrorStr(retval) << endl;
				exit(22);
			}
			numa++;
			break;
     case 'm':
    	 gpv2ado_map_filename = optarg;
    	 gnPvs = parse_epics2ado_csvmap(gpv2ado_map_filename,'<',&gncols,gpv2ado_map,MAXTOKENS,gstorage,MAXTOKENS*MAXTOKENSIZE);
    	 break;
     case 'h': usage(argv[0]); return 0;
     case '?': fprintf(stderr,"Unrecognized option: '-%c'.\n", optopt);return 1;
     case ':': fprintf(stderr,"Option '-%c' requires an argument.\n",optopt);return 1;
     default : usage(argv[0]); return 1;
     }
  }
  //cout<<endl;
  if(numa==0) {cerr<<"At least one ADO name expected (option -a)\n"; return 1;}
  //if(argc - optind != 1){ cerr<<"One argument expected: csv map file\n"; return 1;}
  argIndex = optind;

  // call this here just for test
  GlobalConnManager()->addNotifyCb(connNotifyCallback, NULL);

  unsigned int numadoifs = (grouped) ? 1 : numa;
  //AdoIf *adoIfArray[ numadoifs ];
#ifdef ADO_GROUPING
  unsigned int ni = argIndex;
  if( grouped ){

    const char *anames[ numa + 1];
    for(unsigned int ia = 0; ia < numa; ia++)
      anames[ ia ] = argv[ni + ia];
    anames[ numa ] = NULL;

    adoIfArray[0] = new AdoIf(anames);
    if( ppmUser )
      adoIfArray[0]->SetPpmUser( ppmUser );
    retval = adoIfArray[0]->CreateOK();
    if( retval != 0 ){
      cerr << "Error creating " << anames[0];
      for(unsigned int ia = 1; ia < numa; ia++)
	cout << ", " << anames[ ia ];
      cerr << " ado: " << RhicErrorNumToErrorStr(retval) << endl;
      exit(22);
    }

  } else
  {
		for (unsigned int ia = 0; ia < numa; ia++) {
			cout << ia << ":new AdoIf(" << argv[ni + ia] << ")" << endl;
			adoIfArray[ia] = new AdoIf(argv[ni + ia]);
			if (ppmUser) {
				cout << "SetPpmUser(" << ppmUser << ")" << endl;
				adoIfArray[ia]->SetPpmUser(ppmUser);
			}
			retval = adoIfArray[ia]->CreateOK();
			if (retval != 0) {
				cerr << "Error creating " << argv[ni + ia] << " ado: "
						<< RhicErrorNumToErrorStr(retval) << endl;
				exit(22);
			}
		}
	}
#endif //ADO_GROUPING
  //GlobalConnManager()->addNotifyCb(connNotifyCallback, NULL);

  AsyncSetup *gsetup = NULL;
  switch(filterType){
  case ASYNC_FILTER_NONE:
    gsetup= new GetAsyncSetup(errcb, NULL); // no user arguments
    break;
  case ASYNC_SKIP_FACTOR:
    gsetup= new SkipFactorSetup((unsigned long)filterValue, errcb, NULL); // no user arguments
    break;
  case ASYNC_MINIMUM_INTERVAL:
    gsetup= new MinIntervalSetup((unsigned long)filterValue, errcb, NULL); // no user arguments
    break;
  case ASYNC_MINIMUM_CHANGE:
    gsetup= new MinimumChangeSetup(new Value(filterValue), errcb, NULL); // no user argument
    break;
  case ASYNC_OUT_OF_TOLERANCE:
    gsetup= new OutOfTolSetup(errcb, NULL); // no user argument
    break;
  default:
    cerr << "Unknown filter" << endl;
    exit(23);
  }
  gsetup->SetDataCallback(datacb, NULL); // no user arguments
  if( grouped )
    gsetup->SetDeliveryType(ASYNC_ARRIVAL);
  // set the SetImmediate mode
  if( immediate )
    gsetup->SetImmediate();

#ifndef REMOVE_USELESS
  // OK, now ask for the async data using adoIfCb as the callback routine.
  // Do this for as many arguments as there are
  cout<<"argc="<<argc<<". ind="<<argIndex<<endl;
  if( argIndex >= argc ){
    usage();
    cerr << "Property name missing" << endl;
    exit(24);
  }
#endif //REMOVE_USELESS
  // all arguments after ado name(s) are properties
  Value **dataArray = NULL;
  Value *data = NULL;
  void *reqId = NULL;
  int ip = 0;
  int numProp;
  //gpv2ado_map_filename = argv[argIndex];
  if(gnPvs==0) {cerr << "No epics2ado conversion map supplied, this is not implementded (yet)." << endl; exit(26);}
  numProp = gnPvs;
  ReqStruct r[ numProp ];
  MultiReqStruct mr[ numProp ];
  if(gVerb&VERB_DEBUG) cout<<"numProp="<<numProp<<endl;
  char* paramName="";
  int nPropsRegistered = 0;
  Value val(0.0);
  char *initial_value;
  int stat;
  for (ip=0; ip<numProp; ip++)
  {
	  paramName = gpv2ado_map[ip*gncols+EPICS2ADO_COLUMN_ADO];
	  if(gVerb&VERB_INFO) {cout<<"Monitor "<<paramName<<", update "<<gpv2ado_map[ip*gncols]<<endl;}
	  r[ ip ].propertyID = paramName;
	  r[ ip ].status = 0;
#ifdef STATIC
	  // create Value object to receive data
	  r[ ip ].data = new Value("No data received yet");
#else
	  r[ ip ].data = NULL;
#endif
	  if(gVerb&VERB_DEBUG) cout<<"param["<<ip<<"]:"<<r[ ip ].propertyID<<", gsetup:"<<gsetup<<",data:"<<data<<endl;
	  reqId = NULL;
	  if( getasync )
		  retval = adoIfArray[ 0 ]->GetAsync(paramName, gsetup, data, &reqId);
	  else
		  retval = adoIfArray[ 0 ]->GetNoBlock(paramName, gsetup, data, &reqId);
	  if( retval ){
		  cout << "Error requesting async data for property " << paramName
				  << ": " << RhicErrorNumToErrorStr(retval) << endl;
		  continue;
	  }
	  //
	  nPropsRegistered++;
	  stat = adoIfArray[0]->Get(paramName,&val);
	  if(stat!=0) { // check the status
	      cout << "ERROR getting initial value for property " << paramName
				  << ": " << RhicErrorNumToErrorStr(retval) << endl;
		  continue;
	  }
	  initial_value = val.StringVal(' ');
	  if(gVerb&VERB_DEBUG) cout <<"Initial setting of "<<paramName<<" = "<<initial_value<<endl;
	  adoChanged(adoIfArray[0]->AdoName(), paramName, 1, initial_value);
  }
  if(nPropsRegistered==0) {cout<<"nPropsRegistered=0\n";exit(27);}
  if(gVerb&VERB_DEBUG) cout<<"nPropsRegistered="<<nPropsRegistered<<endl;
  //if (retval == 0)
  {
	  if (getasync) {
		  if (!grouped)
			  stopAfter *= nPropsRegistered * numa;
	  } else {
		  if (grouped)
			  stopAfter = 1;
		  else
			  stopAfter = nPropsRegistered * numa;
	  }

	  if (pauseAfter)
		  asyncHandler->RegisterTimer(pauseAfter * 1000, timerCb,
				  asyncHandler);

#ifdef PRINTRID
	  cout << "request id = " << reqId << " = " << *(int *)reqId << " requested" << endl;
#endif
	  //asyncHandler->HandleEvents();
	  while (!stopAfter || stopAfter > numReceived)
		  asyncHandler->HandleNextEvent();
  }
  //,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
  // some of this cleanup is for valgrind

  // clean up even if there is an error
  delete gsetup; gsetup = 0;

#if 1
  for( unsigned int i = 0; i < numadoifs; i++ )
    adoIfArray[ i ]->StopGetAsync(reqId);
#endif
  for( unsigned int i = 0; i < numadoifs; i++ ){
    delete adoIfArray[ i ];
    adoIfArray[ i ] = 0;
  }

#ifdef STATIC
  if( nPropsRegistered > 1 && grouped ){
    if( numa == 1 ){
      for(int ip = 0 ; ip < nPropsRegistered; ip++)
	delete r[ ip ].data;
    } else {
      for(int ip = 0 ; ip < nPropsRegistered; ip++){
	for(unsigned int ia = 0; ia < numa; ia++)
	  delete mr[ ip ].data[ia];
	delete [] mr[ ip ].data;
	mr[ ip ].data = 0;
      }
    }
  } else {
    if( numa == 1 ){
      for(int ip = 0 ; ip < nPropsRegistered; ip++)
	delete dataArray[ip];
      delete [] dataArray;
    } else {
      for(unsigned int ia = 0; ia < numa * nPropsRegistered; ia++)
	delete dataArray[ia];
      delete [] dataArray;
    }
    dataArray = 0;
  }
#endif

  // call this here just for test
  GlobalConnManager()->deleteNotifyCb(connNotifyCallback, NULL);

#if 1
  // all of this is to reduce valgrind's report
  delete GlobalRhicDataMgr();
  delete asyncHandler; asyncHandler = 0;
  CnsRequest::clear();
  CnsRequest::disconnect();
  void clean_adoIf_stuff();
  clean_adoIf_stuff();

  //delete GlobalAsyncHandler();
  delete GlobalCommController();
  delete GlobalRpcFdStore();

  // hope this does not cause the crash
  delete cdevGlobalTagTable::tagTable();
#endif

  return 0;
}
