#pragma once 

extern unsigned int str_len (const char *);
extern unsigned int str_chr (const char *, int);
extern unsigned int str_rchr (const char *, int);
extern unsigned int str_copy (char *, const char *);

extern int str_diff (const char *, const char *);
extern int str_start (const char *, const char *);
extern int str_diffn (const char *, const char *, unsigned int);

#define str_equal(s,t) (!str_diff ((s), (t)))
