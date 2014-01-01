#pragma once 

#ifndef SUBGETOPTNOSHORT
#define sgopt subgetopt
#define sgoptarg subgetoptarg
#define sgoptind subgetoptind
#define sgoptpos subgetoptpos
#define sgoptproblem subgetoptproblem
#define sgoptprogname subgetoptprogname
#define sgoptdone subgetoptdone
#endif

#define SUBGETOPTDONE -1

extern int subgetopt(int,char **,const char *);
extern char *subgetoptarg;
extern int subgetoptind;
extern int subgetoptpos;
extern int subgetoptproblem;
extern char *subgetoptprogname;
extern int subgetoptdone;
