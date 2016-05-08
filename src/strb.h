#ifndef STRB_h
#define STRB_h

#include<stdio.h>

typedef struct _strb{
	char chars[512];
	size_t index;
}strb;

/// initiates strb
void strbi(strb*o);

/// @returns remaining free chars
size_t strbrem(strb*o);

/// appends @str to @o  @returns 0 if ok
int strbp(strb*o,const char*str);

/// appends @n  @returns 0 if ok
int strbpl(strb*o,const long long n);

/// formats @bytes  @returns 0 if ok
int strbfmtbytes(strb*o,long long bytes);

#endif
