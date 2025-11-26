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

void *memset(void *dst, int value, size_t count)
{
    unsigned char *ptr = (unsigned char *)dst;
    while (count--)
    {
        *ptr++ = (unsigned char)value;
    }
    return dst;
}

void *memmove(void *dst, const void *src, size_t count)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;

    if (d == s || count == 0)
    {
        return dst;
    }

    if (d < s)
    {
        for (size_t i = 0; i < count; ++i)
        {
            d[i] = s[i];
        }
    }
    else
    {
        for (size_t i = count; i != 0; --i)
        {
            d[i - 1] = s[i - 1];
        }
    }
    return dst;
}
