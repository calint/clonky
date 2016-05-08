#ifndef STRB_h
#define STRB_h

#include<stdio.h>

typedef struct{
	char chars[512];
	size_t index;
}strb;

/// initiates @o
void strbi(strb*o);

/// @returns remaining free chars in @o
size_t strbrem(strb*o);

/// appends @str to @o  @returns 0 if ok
int strbp(strb*o,const char*str);

/// appends @n to @o  @returns 0 if ok
int strbpl(strb*o,const long long n);

/// formats @bytes to @o  @returns 0 if ok
int strbfmtbytes(strb*o,long long bytes);

#endif
