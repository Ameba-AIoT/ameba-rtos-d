#ifndef	_TIME64_DEF_H
#define _TIME64_DEF_H

#if defined(__GNUC__)
#define __time_t_defined
#define time_t long long
#include <time.h>
#elif defined(__ICCARM__)
#include "c/time64.h"
#endif

#endif