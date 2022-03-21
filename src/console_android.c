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

#define LOG_TO_FILE
#define LOG_FILE "%S/connectedsmb.log.%d"

OFC_INT g_fd = STDOUT_FILENO ;
OFC_INT ix = 0 ;

#define OBUF_SIZE 200
static OFC_VOID open_log(OFC_VOID) {
#if defined(LOG_TO_FILE)
  char szPath[128] ;
  OFC_TCHAR config_dir[128];
  OFC_CHAR obuf[OBUF_SIZE];

  if (ofc_get_config_dir(config_dir, 128) == OFC_TRUE)
    {
      if (g_fd != STDOUT_FILENO)
	close (g_fd) ;

      snprintf (szPath, 128, LOG_FILE, config_dir, ix) ;
      
      g_fd = open (szPath, 
		   O_CREAT | O_TRUNC | O_WRONLY, S_IRWXU | S_IRWXG | S_IRWXO);

      if (g_fd < 0)
	g_fd = STDOUT_FILENO ;

      ofc_snprintf(obuf, OBUF_SIZE,
		   "OpenFiles (%s) %d.%d %s\n",
		   OFC_SHARE_VARIANT,
		   OFC_SHARE_MAJOR, OFC_SHARE_MINOR,
		   OFC_SHARE_TAG);
      ofc_write_console(obuf);
      ofc_process_dump_libs();
    }
#endif  
}

OFC_VOID ofc_write_stdout_impl(OFC_CCHAR *obuf, OFC_SIZET len) {
#if defined(LOG_TO_FILE)
  if (g_fd == STDOUT_FILENO)
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
  if (g_fd == STDOUT_FILENO)
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
