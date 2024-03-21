/* Copyright (c) 2021 Connected Way, LLC. All rights reserved.
 * Use of this source code is governed by a Creative Commons 
 * Attribution-NoDerivatives 4.0 International license that can be
 * found in the LICENSE file.
 */

#include <execinfo.h>


#include "ofc/types.h"
#include "ofc/impl/backtraceimpl.h"

OFC_VOID ofc_backtrace_impl(OFC_VOID ***trace, OFC_SIZET len)
{
#if (defined(__GNUC__) || defined(__clang__)) && defined(OFC_STACK_TRACE)
#if defined(__cyg_profile)
  if (len > 0)
    trace[0] = __cyg_profile_return_address(0) ;
  if (len > 1)
    trace[1] = __cyg_profile_return_address(1) ;
  if (len > 2)
    trace[2] = __cyg_profile_return_address(2) ;
  if (len > 3)
    trace[3] = __cyg_profile_return_address(3) ;
  if (len > 4)
    trace[4] = __cyg_profile_return_address(4) ;
#else
  if (len > 0)
    (*trace)[0] = __builtin_return_address(0);
  if (len > 1)
    trace[1] = __builtin_return_address(1);
  if (len > 2)
    trace[2] = __builtin_return_address(2);
  if (len > 3)
    trace[3] = __builtin_return_address(3);
  if (len > 4)
    trace[4] = __builtin_return_address(4);
#endif
#else
  if (len > 0)
    trace[0] = __builtin_return_address(0);
  if (len > 1)
    trace[1] = __builtin_return_address(1);
  if (len > 2)
    trace[2] = __builtin_return_address(2);
  if (len > 3)
    trace[3] = __builtin_return_address(3);
  if (len > 4)
    trace[4] = __builtin_return_address(4);
#endif
}
