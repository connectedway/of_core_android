/* Copyright (c) 2021 Connected Way, LLC. All rights reserved.
 * Use of this source code is governed by a Creative Commons 
 * Attribution-NoDerivatives 4.0 International license that can be
 * found in the LICENSE file.
 */
#include <pthread.h>
#include <signal.h>
#define __USE_XOPEN
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#include "ofc/core.h"
#include "ofc/types.h"
#include "ofc/handle.h"
#include "ofc/thread.h"
#include "ofc/sched.h"
#include "ofc/impl/threadimpl.h"
#include "ofc/libc.h"
#include "ofc/waitset.h"
#include "ofc/event.h"
#include "ofc/heap.h"

/** \{ */

#if defined(__cyg_profile)
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>

static pthread_key_t frame_var ;

#define FRAME_SYMBOL_LEN 80
struct __frame
{
  void *this_fn ;
  void *call_site ;
} ;

void __cyg_profile_func_enter (void *this_fn, void *call_site) 
{
  struct __frame *__frame_ptr ;

  if (frame_var != -1)
    {
      __frame_ptr = pthread_getspecific (frame_var) ;
      if (__frame_ptr != OFC_NULL)
	{
	  __frame_ptr++ ;
	  pthread_setspecific (frame_var, __frame_ptr) ;
	  __frame_ptr->this_fn = this_fn ;
	  __frame_ptr->call_site = call_site ;

	}
    }
}

void __cyg_profile_func_exit (void *this_fn, void *call_site)
{
  struct __frame *__frame_ptr ;

  if (frame_var != -1)
    {
      __frame_ptr = pthread_getspecific (frame_var) ;
      if (__frame_ptr != OFC_NULL)
	{
	  __frame_ptr-- ;
	  pthread_setspecific (frame_var, __frame_ptr) ;
	}
    }
}

void *__cyg_profile_return_address(int level)
{
  struct __frame *__frame_ptr ;
  void *address ;

  address = OFC_NULL ;
  if (frame_var != -1)
    {
      __frame_ptr = pthread_getspecific (frame_var) ;
      if (__frame_ptr != OFC_NULL)
	{
	  address = (__frame_ptr-level)->call_site ;
	}
    }
  return (address) ;
}

const char *__cyg_profile_addr2sym(void *address)
{
  const char *symbol ;
  Dl_info info ;

  symbol = "unknown" ;
  if (dladdr(address, &info) && info.dli_sname)
    {
      symbol = info.dli_sname ;
    }
  return (symbol) ;
}

#endif

typedef struct
{
  pthread_t thread ;
  OFC_DWORD (*scheduler)(OFC_HANDLE hThread, OFC_VOID *context)  ;
  OFC_VOID *context ;
  OFC_DWORD ret ;
  OFC_BOOL deleteMe ;
  OFC_HANDLE handle ;
  OFC_THREAD_DETACHSTATE detachstate ;
  OFC_HANDLE wait_set ;
  OFC_HANDLE hNotify ;
} ANDROID_THREAD ;

static void *ofc_thread_launch(void *arg) 
  __attribute__((no_instrument_function)) ;

static void *ofc_thread_launch(void *arg)
{
  ANDROID_THREAD *androidThread ;

  androidThread = arg ;

#if defined(__cyg_profile)
  if (frame_var != -1)
    {
      pthread_setspecific (frame_var, androidThread->__frame_stack) ;
    }
#endif
  androidThread->ret = (androidThread->scheduler)(androidThread->handle,
						androidThread->context) ;

  if (androidThread->hNotify != OFC_HANDLE_NULL)
    ofc_event_set(androidThread->hNotify) ;

  if (androidThread->detachstate == OFC_THREAD_DETACH)
    {
      ofc_handle_destroy(androidThread->handle) ;
      ofc_free(androidThread) ;
    }
  return (OFC_NULL) ;
}

OFC_HANDLE ofc_thread_create_impl(OFC_DWORD(scheduler)(OFC_HANDLE hThread,
                                                       OFC_VOID *context),
                                  OFC_CCHAR *thread_name,
                                  OFC_INT thread_instance,
                                  OFC_VOID *context,
                                  OFC_THREAD_DETACHSTATE detachstate,
                                  OFC_HANDLE hNotify)
{
  ANDROID_THREAD *androidThread ;
  OFC_HANDLE ret ;
  pthread_attr_t attr ;

  ret = OFC_HANDLE_NULL ;
  androidThread = ofc_malloc(sizeof (ANDROID_THREAD)) ;
  if (androidThread != OFC_NULL)
    {
      androidThread->wait_set = OFC_HANDLE_NULL ;
      androidThread->deleteMe = OFC_FALSE ;
      androidThread->scheduler = scheduler ;
      androidThread->context = context ;
      androidThread->hNotify = hNotify ;
      androidThread->handle =
	ofc_handle_create (OFC_HANDLE_THREAD, androidThread) ;
      androidThread->detachstate = detachstate ;
      pthread_attr_init (&attr) ;

      if (androidThread->detachstate == OFC_THREAD_DETACH)
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) ;
      else if (androidThread->detachstate == OFC_THREAD_JOIN)
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE) ;
#if 0
      /* if we need to have a big stack.  Don't think so though
       */
      pthread_attr_setstacksize(&attr, 8 * 1024 * 1024);
#endif
      if (pthread_create (&androidThread->thread, &attr,
			  ofc_thread_launch, androidThread) != 0)
	{
	  ofc_handle_destroy(androidThread->handle) ;
	  ofc_free(androidThread) ;
	}
      else
	ret = androidThread->handle ;
    }
  return (ret) ;
}

OFC_VOID
ofc_thread_set_waitset_impl(OFC_HANDLE hThread, OFC_HANDLE wait_set)
{
  ANDROID_THREAD *androidThread ;

  androidThread = ofc_handle_lock(hThread) ;
  if (androidThread != OFC_NULL)
    {
      androidThread->wait_set = wait_set ;
      ofc_handle_unlock(hThread) ;
    }
}

OFC_VOID ofc_thread_delete_impl(OFC_HANDLE hThread)
{
  ANDROID_THREAD *androidThread ;

  androidThread = ofc_handle_lock(hThread) ;
  if (androidThread != OFC_NULL)
    {
      androidThread->deleteMe = OFC_TRUE ;
      if (androidThread->wait_set != OFC_HANDLE_NULL)
	ofc_waitset_wake(androidThread->wait_set) ;
      ofc_handle_unlock(hThread) ;
    }
}

OFC_VOID ofc_thread_wait_impl(OFC_HANDLE hThread)
{
  ANDROID_THREAD *androidThread ;
  int ret ;

  androidThread = ofc_handle_lock(hThread) ;
  if (androidThread != OFC_NULL)
    {
      if (androidThread->detachstate == OFC_THREAD_JOIN)
	{
	  ret = pthread_join (androidThread->thread, OFC_NULL) ;
	  ofc_handle_destroy(androidThread->handle) ;
	  ofc_free(androidThread) ;
	}
      ofc_handle_unlock(hThread) ;
    }
}

OFC_BOOL ofc_thread_is_deleting_impl(OFC_HANDLE hThread)
{
  ANDROID_THREAD *androidThread ;
  OFC_BOOL ret ;

  ret = OFC_FALSE ;
  androidThread = ofc_handle_lock (hThread) ;
  if (androidThread != OFC_NULL)
    {
      if (androidThread->deleteMe)
	ret = OFC_TRUE ;
      ofc_handle_unlock(hThread) ;
    }
  return (ret) ;
}

OFC_VOID ofc_sleep_impl(OFC_DWORD milliseconds)
{
  useconds_t useconds ;

  if (milliseconds == OFC_INFINITE)
    {
      for (;1;)
	/* Sleep for a day, then more */
	sleep (60*60*24) ;
    }
  else
    {
      useconds = milliseconds * 1000 ;
      usleep (useconds) ;
    }
}

OFC_DWORD ofc_thread_create_variable_impl(OFC_VOID)
{
  pthread_key_t key ;

  pthread_key_create (&key, NULL) ;
  return ((OFC_DWORD) key) ;
}

OFC_VOID ofc_thread_destroy_variable_impl(OFC_DWORD dkey)
{
  pthread_key_t key ;
  key = (pthread_key_t) dkey ;

  pthread_key_delete (key);
}

OFC_DWORD_PTR ofc_thread_get_variable_impl(OFC_DWORD var)
{
  return ((OFC_DWORD_PTR) pthread_getspecific ((pthread_key_t) var)) ;
}

OFC_VOID ofc_thread_set_variable_impl(OFC_DWORD var, OFC_DWORD_PTR val)
{
  pthread_setspecific ((pthread_key_t) var, (OFC_LPVOID) val) ;
}

/*
 * These routines are noops on platforms that support TLS
 */
OFC_CORE_LIB OFC_VOID
ofc_thread_create_local_storage_impl(OFC_VOID)
{
}

OFC_CORE_LIB OFC_VOID
ofc_thread_destroy_local_storage_impl(OFC_VOID)
{
}

OFC_CORE_LIB OFC_VOID
ofc_thread_init_impl(OFC_VOID)
{
#if defined(__cyg_profile)
  pthread_key_create (&frame_var, NULL) ;
#endif
}

OFC_CORE_LIB OFC_VOID
ofc_thread_destroy_impl(OFC_VOID)
{
#if defined(__cyg_profile)
  pthread_key_delete (frame_var) ;
#endif
}

OFC_CORE_LIB OFC_VOID
ofc_thread_detach_impl(OFC_HANDLE hThread)
{
  ANDROID_THREAD *androidThread ;

  androidThread = ofc_handle_lock (hThread) ;
  if (androidThread != OFC_NULL)
    {
      androidThread->detachstate = OFC_THREAD_DETACH;
      pthread_detach(androidThread->thread);
      ofc_handle_unlock(hThread) ;
    }
}

/** \} */
