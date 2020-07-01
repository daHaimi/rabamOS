#include <stdint.h>
#ifndef STDLIB_H
#define STDLIB_H

#define MIN(x,y) ((x < y ? x : y))
#define MAX(x,y) ((x < y ? y : x))

typedef struct divmod_result {
    uint32_t div;
    uint32_t mod;
} divmod_t;

divmod_t divmod(uint32_t dividend, uint32_t divisor);
uint32_t div(uint32_t dividend, uint32_t divisor);
void memcpy(void * dest, const void * src, int bytes);

void bzero(void * dest, int bytes);
void memset(void * dest, uint8_t c, int bytes);

// String functions
char * strcat(char * out, const char * str);
char * strncat(char * out, const char * str, uint32_t len);
uint32_t strlen(const char * str);
char * itoa(int i, int base);
int atoi(char * num);
uint32_t ob_puts(char ** ob, uint32_t obCur, char * str);
char * sprintf(const char * fmt, ...);

#endif
