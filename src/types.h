#ifndef HERBSTLUFT_TYPES_H
#define HERBSTLUFT_TYPES_H

#include "arglist.h"

/* A path in the object tree */
using Path = ArgList;

/* Types for I/O with the user */
using Input = ArgList;
using Output = std::ostream&;

#endif
