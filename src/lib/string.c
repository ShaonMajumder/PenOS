#include <stddef.h>

size_t strlen(const char *str)
{
    size_t len = 0;
    while (str[len])
    {
        len++;
    }
    return len;
}

int strcmp(const char *a, const char *b)
{
    while (*a && (*a == *b))
    {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    size_t i = 0;
    while (i < n && a[i] && a[i] == b[i])
    {
        i++;
    }
    if (i == n)
    {
        return 0;
    }
    return (unsigned char)a[i] - (unsigned char)b[i];
}

char *strncpy(char *dst, const char *src, size_t n)
{
    size_t i = 0;
    for (; i < n && src[i]; ++i)
    {
        dst[i] = src[i];
    }
    for (; i < n; ++i)
    {
        dst[i] = '\0';
    }
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    for (size_t i = 0; i < n; ++i)
    {
        d[i] = s[i];
    }
    return dst;
}

void *memset(void *s, int c, size_t n)
{
    unsigned char *p = (unsigned char *)s;
    for (size_t i = 0; i < n; ++i)
    {
        p[i] = (unsigned char)c;
    }
    return s;
}
