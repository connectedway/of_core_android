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

#include "ofc/types.h"
#include "ofc/impl/consoleimpl.h"
#include "ofc/libc.h"
#include "ofc/heap.h"

#include <android/log.h>

/**
 * \defgroup console_android Android Console Interface
 */

/** \{ */

#undef LOG_TO_FILE
#define LOG_FILE "/data/user/0/com.connectedway.connectedsmb/files/connectedsmb.log.%d"

OFC_INT g_fd = -1 ;
OFC_INT ix = 0 ;

static OFC_VOID open_log(OFC_VOID) {
#if defined(LOG_TO_FILE)
  char szPath[128] ;

  snprintf (szPath, 128, LOG_FILE, ix) ;
  if (g_fd != -1)
    close (g_fd) ;
  g_fd = open (szPath, 
	       O_CREAT | O_TRUNC | O_WRONLY, S_IRWXU | S_IRWXG | S_IRWXO);
#else
  g_fd = STDOUT_FILENO ;
#endif  
}

OFC_VOID ofc_write_stdout_impl(OFC_CCHAR *obuf, OFC_SIZET len) {
#if defined(LOG_TO_FILE)
  if (g_fd == -1)
    open_log() ;
  else
    {
      struct stat xstat ;
      if (fstat (g_fd, &xstat) > 0)
	{
	  if (xstat.st_size > (2*1024*1024))
	    {
	      close(g_fd);
	      ix = (ix + 1) % 2 ;
	      open_log() ;
	    }
	}
    }
  write (g_fd, obuf, len) ;
  fsync (g_fd) ;
#else
  OFC_CHAR *p ;

  p = ofc_malloc (len+1) ;
  ofc_strncpy (p, obuf, len) ;
  p[len] = '\0' ;

#if 0
  __android_log_print (ANDROID_LOG_DEBUG, "OpenFiles", "%s", p) ;
#else
  printf ("%s", p);
#endif
  ofc_free (p) ;
#endif
}

OFC_VOID ofc_write_console_impl(OFC_CCHAR *obuf)
{
#if defined(LOG_TO_FILE)
  if (g_fd == -1)
    open_log();
  write (g_fd, obuf, ofc_strlen(obuf)) ;
  fsync (g_fd) ;
#else
#if 0
  __android_log_print (ANDROID_LOG_DEBUG, "OpenFiles", "%s", obuf) ;
#else
  printf ("%s", obuf);
#endif
#endif
}

OFC_VOID ofc_read_stdin_impl(OFC_CHAR *inbuf, OFC_SIZET len) {
  fgets (inbuf, len, stdin) ;
  if (ofc_strlen (inbuf) < len)
    len = ofc_strlen (inbuf) ;
  inbuf[len-1] = '\0' ;
}

OFC_VOID ofc_read_password_impl(OFC_CHAR *inbuf, OFC_SIZET len)
{
  ofc_printf ("Attempt to Read Password on Android.  Need Method\n") ;

  inbuf[0] = '\0' ;
}

/** \} */