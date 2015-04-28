#ifndef _HAVE_UTIL_HEADER
#define _HAVE_UTIL_HEADER

typedef struct {
  int len;
  int total;
  char *s;
} string_t;

#define STRING_ZERO { 0, 0, (char *)0 }

char *
util_mprintf(const char *format, ...);

int 
util_ascii_to_hex(int c);

void 
util_dehttpize(char *z);

char *
util_extract_token(char *input, char **leftOver);

char *
util_htmlize(const char *in, string_t *s);

char * 
util_httpize(const char* url);
#endif
