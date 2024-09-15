#ifndef CUSTOM_STRING_H
#define CUSTOM_STRING_H

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define strchr strchr_lvgl
#define strcat strcat_lvgl
#define strcmp strcmp_lvgl

char *strcat_lvgl (char *__restrict, const char *__restrict);
char *strchr_lvgl (const char *, int);
int strcmp_lvgl (const char *, const char *);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*CUSTOM_STRING_H*/
