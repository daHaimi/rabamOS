#include <kernel/mem.h>
#include <common/stdlib.h>
#include <stdint.h>
#include <stdarg.h>

__inline__ uint32_t div(uint32_t dividend, uint32_t divisor) {
    uint32_t denom=divisor;
    uint32_t current = 1;
    uint32_t answer=0;

    if ( denom > dividend)
        return 0;

    if ( denom == dividend)
        return 1;

    while (denom <= dividend) {
        denom <<= 1;
        current <<= 1;
    }

    denom >>= 1;
    current >>= 1;

    while (current!=0) {
        if ( dividend >= denom) {
            dividend -= denom;
            answer |= current;
        }
        current >>= 1;
        denom >>= 1;
    }
    return answer;
}

__inline__ divmod_t divmod(uint32_t dividend, uint32_t divisor) {
    divmod_t res;
    res.div = div(dividend, divisor);
    res.mod = dividend - res.div*divisor;
    return res;
}

void memcpy(void * dest, const void * src, int bytes) {
    char * d = dest;
    const char * s = src;
    while (bytes--) {
        *d++ = *s++;
    }
}

void bzero(void * dest, int bytes) {
    memset(dest, 0, bytes);
}

void memset(void * dest, uint8_t c, int bytes) {
    uint8_t * d = dest;
    while (bytes--) {
        *d++ = c;
    }
}

char * strcat(char * out, const char * str) {
    char* cur = out;
    while (*cur != '\0')
        cur++;
    while (*str != '\0')
        *(cur++) = *(str++);
    *cur = '\0';
    return out;
}

char * strncat(char * out, const char * str, uint32_t len) {
    char* cur = out;
    while (*cur != '\0')
        cur++;
    while (*str != '\0' && (len--) > 0)
        *(cur++) = *(str++);
    *cur = '\0';
    return out;
}

uint32_t strlen(const char * str) {
    uint32_t size = 0;
    while (*str != 0) {
        str++;
        size++;
    }
    return size;
}

char * itoa(int num, int base) {
    static char intbuf[33];
    uint32_t j = 0, isneg = 0, i;
    divmod_t divmod_res;

    if (num == 0) {
        intbuf[0] = '0';
        intbuf[1] = '\0';
        return intbuf;
    }

    if (base == 10 && num < 0) {
        isneg = 1;
        num = -num;
    }

    i = (uint32_t) num;

    while (i != 0) {
       divmod_res = divmod(i,base);
       intbuf[j++] = (divmod_res.mod) < 10 ? '0' + (divmod_res.mod) : 'a' + (divmod_res.mod) - 10;
       i = divmod_res.div;
    }

    if (isneg)
        intbuf[j++] = '-';

    if (base == 16) {
        intbuf[j++] = 'x';
        intbuf[j++] = '0';
    } else if(base == 8) {
        intbuf[j++] = '0';
    } else if(base == 2) {
        intbuf[j++] = 'b';
        intbuf[j++] = '0';
    }

    intbuf[j] = '\0';
    j--;
    i = 0;
    while (i < j) {
        isneg = intbuf[i];
        intbuf[i] = intbuf[j];
        intbuf[j] = isneg;
        i++;
        j--;
    }

    return intbuf;
}

int atoi(char * num) {
    int res = 0, power = 0, digit, i;
    char * start = num;

    // Find the end
    while (*num >= '0' && *num <= '9') {
        num++;
    }

    num--;

    while (num != start) {
        digit = *num - '0';
        for (i = 0; i < power; i++) {
            digit *= 10;
        }
        res += digit;
        power++;
        num--;
    }

    return res;
}

uint32_t ob_puts(char ** ob, uint32_t obCur, char * str) {
    while (*(str++) != '\0') {
        (*ob)[obCur++] = *(str++);
    }
    return obCur;
}

char * sprintf(const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char * out = "";

    for (; *fmt != '\0'; fmt++) {
        if (*fmt == '%') {
            switch (*(++fmt)) {
                case '%':
                    strcat(out, "%");
                    break;
                case 'd':
                    strcat(out, itoa(va_arg(args, int), 10));
                    break;
                case 'x':
                    strcat(out, itoa(va_arg(args, int), 16));
                    break;
                case 's':
                    strcat(out, va_arg(args, char *));
                    break;
            }
        } else strncat(out, fmt, 1);
    }

    va_end(args);
    return out;
}

int strcmp(const char * s1, const char * s2) {
    while(*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char * s1, const char * s2, uint32_t n) {
    uint32_t i = 0;
    while(*s1 && (*s1 == *s2) && i < n) {
        s1++;
        s2++;
        i++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

char * strcpy(char * dest, const char * src) {
    uint32_t i = 0;
    while ((dest[i] = src[i]) != '\0')
        i++;
    return dest;
}

char * strncpy(char * dest, const char * src, uint32_t n) {
    uint32_t i = 0;
    while (i < n && (dest[i] = src[i]) != '\0')
        i++;
    if (i < n && dest[i] != '\0')
        dest[++i] = '\0';
    return dest;
}