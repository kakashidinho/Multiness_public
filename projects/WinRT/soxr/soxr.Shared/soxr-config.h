/* SoX Resampler Library      Copyright (c) 2007-13 robs@users.sourceforge.net
* Licence for this file: LGPL v2.1                  See LICENCE for details. */

/* N.B. Pre-configured for typical MS-Windows systems.  However, the normal
* procedure is to use the cmake configuration and build system. See INSTALL. */

#if !defined soxr_config_included
#define soxr_config_included

#define HAVE_SINGLE_PRECISION 1
#define HAVE_DOUBLE_PRECISION 1
#define HAVE_AVFFT            0
#define HAVE_SIMD             0
#define HAVE_FENV_H           1
#define HAVE_LRINT            0
#define WORDS_BIGENDIAN       0

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

static __inline const char* getenv(const char* name) {
	return 0;//dummy function
}

#endif
