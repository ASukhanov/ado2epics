# ado2epics

ADO to EPICS half-lane bridge

## Dependencies

- EPICS
- ADO
- ECLIPS
- ClearCase

## Usage

ado2epics program  monitors the ADO variables, defined in the supplied map file and modifies the corresponding EPICS PVs.

Example for simple.test ADO, assuming the softIOC is already running with proper db (see below):
:ado2epics -a simple.test -m epics2ado_simple.csv -v1

To monitor the EPICS PV 'sinM', served by ado2epics:
:camonitor sinM
sinM                           2016-08-05 11:40:00.730355 -0.809017  
sinM                           2016-08-05 11:40:01.231654 -0.819152  
...

The script ado2epics_map.sh generates map file from live ADO. 
Example for simple.test ADO:
:./ado2epics_map.sh >! epics2ado_simple.csv

The script ado2epics_db.sh generates EPICS database from live ADO. 
The generated map is csv file and it defines all ADO variables to be exported to the EPICS PVs with the same name.
Example for simple.test ADO:
:./ado2epics_db.sh simple.test >! simple.db

Example of running softIOC with the simple.db:
:softIoc -d simple.db

## Compilation
 '''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
 It was not straightforward how to setup the cmake to build with the EPICS.
 To setup the building process in Eclipse platform was much more easy.
 To build C/C++ programs the CDT plugin should be installed in Eclipse.
 
- Set environment variables for compiling ADO components in Eclipse:

setenv CMAKE_COMPILE "/vobs/libs/makefiles/g++447_64 -m32 -Wall -I/usr/include  -I/usr/include/c++/4.4.7/ -I/usr/include/c++/4.4.7/x86_64-redhat-linux -I/usr/include/c++/4.4.7/backward  -I/usr/local/share -I/usr/local/share/include -B/usr/lib/gcc/x86_64-redhat-linux/4.4.7/ -B/usr/libexec/gcc/x86_64-redhat-linux/4.4.7/  -D__X86 -D__WS6  -Wno-write-strings -g -pthread -I/vobs/store/X86/include -I/vobs/store/X86/include/cdev -I/usr/local/share/sybase150/OCS-15_0/include  -c"

setenv CMAKE_LIBS "-ladoIf -lsetHist -ldbtools -lcns -lifHandlerLib -lsvcHandlerLib -lado -lnotifLib -lcommToolsLib -lddf -ldb++2 -lMsgLog -lutils -lAsync -lbasics -lGenData -lcdevData -lUIDummy -lsmtp -lrhicError -lsybcs -lsybblk -lsybct -lsybcomn -lsybintl -lsybtcl -lXt -lX11 -lSM -lICE -lm -ldl -lrt -lpthread"

setenv CMAKE_LINK "/vobs/libs/makefiles/g++447_64 -m32 -Wall -L/vobs/store/X86/lib -Xlinker -rpath -Xlinker /usr/local/share/sybase150/OCS-15_0/lib -L/usr/local/share/sybase150/OCS-15_0/lib  -L/usr/local/X11R6/lib -L/usr/X11R6/lib -L/usr/lib -L/usr/local/lib -L/usr/local/share -L/usr/local/share/lib -rdynamic"
 ,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

:cleartool setview YOUR_VIEW
:git clone https://github.com/ASukhanov/ado2epics

 '''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
                            In Eclipse 
:eclipse&
open workspace of your choice
select: File/Import/General/Existing Project into Workspace
Select root directory: enter the directory, you cloned from github

The project ado2epics should appear in the 'C/C++ Projects' pane
Right click on ado2epics/Build Project
The program should be built in Release or Debug subdirectory.
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,



