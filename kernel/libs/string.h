#ifndef _STRING_H_
#define _STRING_H_

#include <libs/types.h>

int memcmp(const void * buf1, const void * buf2, uint32_t count);
void *memcpy(void *dest, const void *src, uint32_t count);
void *memmove(void *dest, const void *src, uint32_t count);
void memset(void *dest, uint8_t val, uint32_t len);
void bzero(void *dest, uint32_t len);
int strcmp(const char *str1, const char *str2);
char *strcpy(char *dest, const char *src);
char *strcat(char *dest, const char *src);
char *strncpy(char *dest, const char *src, uint32_t len);
char* safestrcpy(char *s, const char *t, int n);
int strlen(const char *src);

int find_last_of(const char *src, char c);
int find_first_of(const char *src, char c);

#endif  // _STRING_H_