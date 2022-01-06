/* Copyright (c) 2021 Connected Way, LLC. All rights reserved.
 * Use of this source code is governed by a Creative Commons 
 * Attribution-NoDerivatives 4.0 International license that can be
 * found in the LICENSE file.
 */
#define _GNU_SOURCE
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>

#include "ofc/types.h"
#include "ofc/handle.h"
#include "ofc/event.h"

#include "ofc/heap.h"
#include "ofc/impl/eventimpl.h"
#include "ofc/impl/waitsetimpl.h"

typedef struct
{
  OFC_EVENT_TYPE eventType ;
  OFC_BOOL signalled ;
  pthread_cond_t pthread_cond ;
  pthread_mutex_t pthread_mutex ;
  pthread_mutexattr_t pthread_mutexattr ;
} ANDROID_EVENT ;

OFC_HANDLE ofc_event_create_impl(OFC_EVENT_TYPE eventType)
{
  ANDROID_EVENT *android_event ;
  OFC_HANDLE hAndroidEvent ;
  pthread_cond_t pthread_cond_initializer = PTHREAD_COND_INITIALIZER ;
  pthread_mutex_t pthread_mutex_initializer = PTHREAD_MUTEX_INITIALIZER ;

  hAndroidEvent = OFC_HANDLE_NULL ;
  android_event = ofc_malloc(sizeof (ANDROID_EVENT)) ;
  if (android_event != OFC_NULL)
    {
      android_event->eventType = eventType ;
      android_event->signalled = OFC_FALSE ;
      android_event->pthread_cond = pthread_cond_initializer ;
      android_event->pthread_mutex = pthread_mutex_initializer ;
      pthread_mutexattr_init (&android_event->pthread_mutexattr) ;
      pthread_mutexattr_settype (&android_event->pthread_mutexattr, 
				 PTHREAD_MUTEX_ERRORCHECK) ;
      pthread_cond_init (&android_event->pthread_cond, NULL)  ;
      pthread_mutex_init (&android_event->pthread_mutex, 
			  &android_event->pthread_mutexattr) ;
      hAndroidEvent = ofc_handle_create (OFC_HANDLE_EVENT, android_event) ;
    }
  return (hAndroidEvent) ;
}

OFC_VOID ofc_event_set_impl(OFC_HANDLE hEvent)
{
  ANDROID_EVENT *androidEvent ;
  OFC_HANDLE hWaitSet ;

  androidEvent = ofc_handle_lock(hEvent) ;
  if (androidEvent != OFC_NULL)
    {
      pthread_mutex_lock (&androidEvent->pthread_mutex) ;

      androidEvent->signalled = OFC_TRUE ;
      pthread_cond_broadcast (&androidEvent->pthread_cond) ;
      
      hWaitSet = ofc_handle_get_wait_set (hEvent) ;
      if (hWaitSet != OFC_HANDLE_NULL) {
	ofc_waitset_signal_impl (hWaitSet, hEvent) ;
      }
      pthread_mutex_unlock (&androidEvent->pthread_mutex) ;

      ofc_handle_unlock(hEvent) ;
    }
}

OFC_VOID ofc_event_reset_impl(OFC_HANDLE hEvent)
{
  ANDROID_EVENT *androidEvent ;

  androidEvent = ofc_handle_lock(hEvent) ;
  if (androidEvent != OFC_NULL)
    {
      pthread_mutex_lock (&androidEvent->pthread_mutex) ;
      androidEvent->signalled = OFC_FALSE ;
      pthread_mutex_unlock (&androidEvent->pthread_mutex) ;
      ofc_handle_unlock(hEvent) ;
    }
}

OFC_EVENT_TYPE ofc_event_get_type_impl(OFC_HANDLE hEvent) 
{
    ANDROID_EVENT *android_event ;
    OFC_EVENT_TYPE eventType ;

    eventType = OFC_EVENT_AUTO ;
    android_event = ofc_handle_lock(hEvent) ;
    if (android_event != OFC_NULL) {
      eventType = android_event->eventType ;
      ofc_handle_unlock(hEvent) ;
    }
    return (eventType) ;
}

OFC_VOID ofc_event_destroy_impl(OFC_HANDLE hEvent) 
{
  ANDROID_EVENT *androidEvent ;

  androidEvent = ofc_handle_lock(hEvent) ;
  if (androidEvent != OFC_NULL)
    {
      pthread_cond_destroy (&androidEvent->pthread_cond) ;
      pthread_mutex_destroy (&androidEvent->pthread_mutex) ;
      pthread_mutexattr_destroy (&androidEvent->pthread_mutexattr) ;
      ofc_free(androidEvent) ;
      ofc_handle_destroy(hEvent) ;
      ofc_handle_unlock(hEvent) ;
    }
}

OFC_VOID ofc_event_wait_impl(OFC_HANDLE hEvent) 
{
  ANDROID_EVENT *android_event ;

  android_event = ofc_handle_lock(hEvent) ;
  if (android_event != OFC_NULL)
    {
      pthread_mutex_lock (&android_event->pthread_mutex) ;
      if (!android_event->signalled)
	pthread_cond_wait (&android_event->pthread_cond,
			   &android_event->pthread_mutex) ;
      if (android_event->eventType == OFC_EVENT_AUTO)
	android_event->signalled = OFC_FALSE ;

      pthread_mutex_unlock (&android_event->pthread_mutex) ;
      ofc_handle_unlock(hEvent) ;
    }
}

OFC_BOOL ofc_event_test_impl(OFC_HANDLE hEvent)
{
  ANDROID_EVENT *android_event ;
  OFC_BOOL ret ;

  ret = OFC_TRUE ;
  android_event = ofc_handle_lock(hEvent) ;
  if (android_event != OFC_NULL)
    {
      pthread_mutex_lock (&android_event->pthread_mutex) ;
      ret = android_event->signalled ;
      pthread_mutex_unlock (&android_event->pthread_mutex) ;
      ofc_handle_unlock(hEvent) ;
    }
  return (ret) ;
}

