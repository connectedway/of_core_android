/* Copyright (c) 2021 Connected Way, LLC. All rights reserved.
 * Use of this source code is governed by a Creative Commons 
 * Attribution-NoDerivatives 4.0 International license that can be
 * found in the LICENSE file.
 */
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

#include "ofc/types.h"
#include "ofc/impl/consoleimpl.h"
#include "ofc/libc.h"
#include "ofc/heap.h"
#include "ofc/framework.h"
#include "ofc/version.h"
#include "ofc/process.h"

#include <android/log.h>

/**
 * \defgroup console_android Android Console Interface
 */

/** \{ */

OFC_VOID ofc_write_stdout_impl(OFC_CCHAR *obuf, OFC_SIZET len) {
  OFC_CHAR *p ;

  p = ofc_malloc (len+1) ;
  ofc_strncpy (p, obuf, len) ;
  p[len] = '\0' ;

  __android_log_print (ANDROID_LOG_DEBUG, "gridlock-app", "%s", p) ;
  ofc_free (p) ;
}

OFC_VOID ofc_write_log_impl(OFC_LOG_LEVEL level,
			    OFC_CCHAR *obuf, OFC_SIZET len)
{
  ofc_write_stdout_impl(obuf, len);
}

OFC_VOID ofc_write_console_impl(OFC_CCHAR *obuf)
{
  __android_log_print (ANDROID_LOG_DEBUG, "gridlock-app", "%s", obuf) ;
}

OFC_VOID ofc_read_stdin_impl(OFC_CHAR *inbuf, OFC_SIZET len) {
  fgets (inbuf, len, stdin) ;
  if (ofc_strlen (inbuf) < len)
    len = ofc_strlen (inbuf) ;
  inbuf[len-1] = '\0' ;
}

OFC_VOID ofc_read_password_impl(OFC_CHAR *inbuf, OFC_SIZET len)
{
  ofc_log (OFC_LOG_WARN,
	   "Attempt to Read Password on Android.  Need Method\n") ;

  inbuf[0] = '\0' ;
}

OFC_VOID
ofc_console_set_log_file_impl(OFC_CHAR *log_file,
			      OFC_LARGE_INTEGER rollover_size,
			      OFC_UINT max_instance)
{
}

OFC_VOID ofc_console_init_impl(OFC_VOID)
{
}

OFC_VOID ofc_console_destroy_impl(OFC_VOID)
{
}

/** \} */
