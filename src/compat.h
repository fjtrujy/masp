#ifndef MASP_COMPAT_H
#define MASP_COMPAT_H

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);
void xmalloc_set_program_name(const char *name);

/* Convenience ctype wrappers with unsigned char casting */
#define ISALNUM(c)  (isalnum((unsigned char)(c)))
#define ISALPHA(c)  (isalpha((unsigned char)(c)))
#define ISDIGIT(c)  (isdigit((unsigned char)(c)))
#define ISUPPER(c)  (isupper((unsigned char)(c)))
#define TOLOWER(c)  (tolower((unsigned char)(c)))
#define TOUPPER(c)  (toupper((unsigned char)(c)))

#endif /* MASP_COMPAT_H */

