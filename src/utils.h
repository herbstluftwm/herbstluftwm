
#ifndef __HERBST_UTILS_H_
#define __HERBST_UTILS_H_

/// print a printf-like message to stderr and exit
void die(const char *errstr, ...);

// get X11 color from color string
unsigned long getcolor(const char *colstr);

#define ATOM(A) XInternAtom(g_display, (A), False)


#endif


