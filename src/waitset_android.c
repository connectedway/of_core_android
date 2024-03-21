/* Copyright (c) 2021 Connected Way, LLC. All rights reserved.
 * Use of this source code is governed by a Creative Commons 
 * Attribution-NoDerivatives 4.0 International license that can be
 * found in the LICENSE file.
 */
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include "ofc/config.h"
#include "ofc/types.h"
#include "ofc/handle.h"
#include "ofc/waitq.h"
#include "ofc/timer.h"
#include "ofc/process.h"
#include "ofc/queue.h"
#include "ofc/socket.h"
#include "ofc/event.h"
#include "ofc/waitset.h"
#include "ofc/impl/waitsetimpl.h"
#include "ofc/impl/socketimpl.h"
#include "ofc/impl/eventimpl.h"

#include "ofc/heap.h"

#include "ofc/fs.h"
#include "ofc/file.h"

#include "ofc_android/fs_android.h"
#if defined(OF_RESOLVER_FS)
#include <dlfcn.h>
#include "of_resolver_fs/fs_resolver.h"
#endif

/**
 * \defgroup waitset_android Android Dependent Scheduler Handling
 */

/** \{ */

typedef struct
{
  int pipe_files[2] ;
} ANDROID_WAIT_SET ;

OFC_VOID ofc_waitset_create_impl(WAIT_SET *pWaitSet)
{
  ANDROID_WAIT_SET *AndroidWaitSet ;

  AndroidWaitSet = ofc_malloc (sizeof (ANDROID_WAIT_SET)) ;
  pWaitSet->impl = AndroidWaitSet ;
  pipe (AndroidWaitSet->pipe_files) ;
  fcntl (AndroidWaitSet->pipe_files[0], F_SETFL,
	 fcntl (AndroidWaitSet->pipe_files[0], F_GETFL) | O_NONBLOCK) ;
  fcntl (AndroidWaitSet->pipe_files[1], F_SETFL,
	 fcntl (AndroidWaitSet->pipe_files[1], F_GETFL) | O_NONBLOCK) ;
}

OFC_VOID ofc_waitset_destroy_impl(WAIT_SET *pWaitSet)
{
  ANDROID_WAIT_SET *AndroidWaitSet ;

  AndroidWaitSet = pWaitSet->impl ;
  close (AndroidWaitSet->pipe_files[0]) ;
  close (AndroidWaitSet->pipe_files[1]) ;
  ofc_free(pWaitSet->impl) ;
  pWaitSet->impl = OFC_NULL;
}

typedef struct
{
  OFC_HANDLE hEvent ;
  OFC_HANDLE hAssoc ;
} EVENT_ELEMENT ;

OFC_VOID ofc_waitset_signal_impl(OFC_HANDLE handle, OFC_HANDLE hEvent)
{
  WAIT_SET *pWaitSet ;
  ANDROID_WAIT_SET *AndroidWaitSet ;

  pWaitSet = ofc_handle_lock(handle) ;

  if (pWaitSet != OFC_NULL)
    {
      AndroidWaitSet = pWaitSet->impl ;
      write (AndroidWaitSet->pipe_files[1], &hEvent, sizeof (OFC_HANDLE)) ;
      ofc_handle_unlock(handle) ;
    }
}

OFC_VOID ofc_waitset_wake_impl(OFC_HANDLE handle)
{
  ofc_waitset_signal_impl(handle, OFC_HANDLE_NULL) ;
}

OFC_HANDLE PollEvent (OFC_INT fd, OFC_HANDLE eventQueue)
{
  EVENT_ELEMENT *eventElement ;
  OFC_HANDLE hEvent ;
  OFC_HANDLE triggered_event ;
  OFC_INT size ;
  OFC_BOOL wake ;
  /*
   * Special case.  It's the pipe.  Let's read the
   * event handle
   */
  triggered_event = OFC_HANDLE_NULL ;
  wake = OFC_FALSE ;

  do
    {
      size = read (fd, &hEvent, sizeof(OFC_HANDLE)) ;
      if (size == sizeof (OFC_HANDLE))
	{
	  if (hEvent == OFC_HANDLE_NULL)
	    wake = OFC_TRUE ;
	  else
	    {
	      for (eventElement = ofc_queue_first(eventQueue) ;
		   eventElement != OFC_NULL && eventElement->hEvent != hEvent ;
		   eventElement = ofc_queue_next(eventQueue, eventElement) ) ;

	      if (eventElement != OFC_NULL)
		{
		  if (ofc_event_test(hEvent) == OFC_TRUE)
		    {
		      if (ofc_event_get_type(hEvent) == OFC_EVENT_AUTO)
			ofc_event_reset(hEvent) ;
		      triggered_event = eventElement->hAssoc ;
		    }
		}
	    }
	}
    }
  while (triggered_event == OFC_HANDLE_NULL && !wake &&
	 size == sizeof (OFC_HANDLE)) ;

  return (triggered_event);
}

#if defined(OF_RESOLVER_FS)
typedef OFC_HANDLE (*getEventHandleFunc)(OFC_HANDLE parentHandle);

/*
 * Resolver requires JNI.  It is a reverse JNI module
 * that lets us call back into the Java App.
 * The problem is, we build the android waitset into
 * the core.  It is not linked with the JNI layer.
 * So, even though we would hope we can do dynamic linking
 * the build will fail if we call the resolver here.  So,
 * let's see if we can do our own dynamic binding
 */
static OFC_HANDLE
ofc_android_get_resolver_overlapped_event(OFC_HANDLE hEventHandle)
{
  OFC_HANDLE hEvent;
  
  static getEventHandleFunc overlapped_function = OFC_NULL;

  if (overlapped_function == OFC_NULL)
    {
      /*
       * Bind to the function
       */
      overlapped_function =
        (getEventHandleFunc) dlsym(RTLD_DEFAULT,
                                   "OfcFSResolverGetOverlappedEvent");
      if (overlapped_function == OFC_NULL)
        {
          ofc_process_crash(dlerror());
        }
    }

  hEvent = overlapped_function(hEventHandle);
  return (hEvent);
}
#endif

OFC_HANDLE ofc_waitset_wait_impl(OFC_HANDLE handle)
{
  WAIT_SET *pWaitSet ;
  ANDROID_WAIT_SET *AndroidWaitSet ;

  OFC_HANDLE hEvent ;
  OFC_HANDLE hEventHandle ;
  OFC_HANDLE triggered_event ;
  OFC_HANDLE timer_event ;
  OFC_HANDLE androidHandle ;
#if defined(OFC_FS_ANDROID)
  OFC_HANDLE fsHandle ;
#endif
  struct pollfd *android_handle_list ;
  OFC_HANDLE *ofc_handle_list ;

  nfds_t wait_count ;
  int wait_index ;
  int leastWait ;

  int poll_count ;
  OFC_MSTIME wait_time ;
#if defined(OFC_FS_ANDROID)
  OFC_FST_TYPE fsType ;
#endif
  OFC_HANDLE eventQueue ;
  EVENT_ELEMENT *eventElement ;
  OFC_HANDLE hWaitQ;

  triggered_event = OFC_HANDLE_NULL ;
  pWaitSet = ofc_handle_lock(handle) ;

  if (pWaitSet != OFC_NULL)
    {
      eventQueue = ofc_queue_create() ;
      leastWait = OFC_MAX_SCHED_WAIT ;
      timer_event = OFC_HANDLE_NULL ;

      wait_count = 0 ;
      android_handle_list = ofc_malloc(sizeof (struct pollfd)) ;
      ofc_handle_list = ofc_malloc(sizeof (OFC_HANDLE)) ;

      AndroidWaitSet = pWaitSet->impl ;

      /*
       * Purge any additional queued events.  We'll get these before we
       * sleep the next time
       */
      while (read (AndroidWaitSet->pipe_files[0], &hEventHandle,
		   sizeof (OFC_HANDLE)) > 0) ;

      android_handle_list[wait_count].fd = AndroidWaitSet->pipe_files[0] ;
      android_handle_list[wait_count].events = POLLIN ;
      android_handle_list[wait_count].revents = 0 ;
      ofc_handle_list[wait_count] = OFC_HANDLE_NULL ;

      wait_count++ ;

      for (hEventHandle =
	     (OFC_HANDLE) ofc_queue_first (pWaitSet->hHandleQueue) ;
	   hEventHandle != OFC_HANDLE_NULL &&
	     triggered_event == OFC_HANDLE_NULL ;
	   hEventHandle =
	     (OFC_HANDLE) ofc_queue_next (pWaitSet->hHandleQueue,
				      (OFC_VOID *) hEventHandle) )
	{
	  switch (ofc_handle_get_type(hEventHandle))
	    {
	    default:
	    case OFC_HANDLE_WAIT_SET:
	    case OFC_HANDLE_SCHED:
	    case OFC_HANDLE_APP:
	    case OFC_HANDLE_THREAD:
	    case OFC_HANDLE_PIPE:
	    case OFC_HANDLE_MAILSLOT:
	    case OFC_HANDLE_FSWIN32_FILE:
	    case OFC_HANDLE_FSANDROID_FILE:
	    case OFC_HANDLE_QUEUE:
	      /*
	       * These are not synchronizeable.  Simple ignore
	       */
	      break ;

	    case OFC_HANDLE_WAIT_QUEUE:
	      hEvent = ofc_waitq_get_event_handle(hEventHandle) ;
	      if (!ofc_waitq_empty(hEventHandle))
		{
		  triggered_event = hEventHandle ;
		}
	      else
		{
		  eventElement = ofc_malloc(sizeof (EVENT_ELEMENT)) ;
		  eventElement->hAssoc = hEventHandle ;
		  eventElement->hEvent = hEvent ;
		  ofc_enqueue (eventQueue, eventElement) ;
		}
	      break ;

	    case OFC_HANDLE_FILE:
#if defined(OFC_FS_ANDROID)
	      fsType = OfcFileGetFSType(hEventHandle) ;

	      if (fsType == OFC_FST_ANDROID)
		{
		  android_handle_list =
		    ofc_realloc(android_handle_list,
				sizeof (struct pollfd) * (wait_count+1)) ;
		  ofc_handle_list =
		    ofc_realloc(ofc_handle_list,
				     sizeof (OFC_HANDLE) * (wait_count+1)) ;
		  fsHandle = OfcFileGetFSHandle (hEventHandle) ;
		  android_handle_list[wait_count].fd =
		    OfcFSAndroidGetFD (fsHandle) ;
		  android_handle_list[wait_count].events = 0 ;
		  android_handle_list[wait_count].revents = 0 ;
		  ofc_handle_list[wait_count] = hEventHandle ;
		  wait_count++ ;
		}
#endif
	      break ;
	    case OFC_HANDLE_SOCKET:
	      /*
	       * Wait on event
	       */
	      android_handle_list =
		ofc_realloc(android_handle_list,
				 sizeof (struct pollfd) * (wait_count+1)) ;
	      ofc_handle_list =
		ofc_realloc(ofc_handle_list,
				 sizeof (OFC_HANDLE) * (wait_count+1)) ;

	      androidHandle = ofc_socket_get_impl(hEventHandle) ;
	      android_handle_list[wait_count].fd =
		ofc_socket_impl_get_fd(androidHandle) ;
	      android_handle_list[wait_count].events =
		ofc_socket_impl_get_event(androidHandle) ;
	      android_handle_list[wait_count].revents = 0 ;
	      ofc_handle_list[wait_count] = hEventHandle ;
	      wait_count++ ;
	      break ;

	    case OFC_HANDLE_FSRESOLVER_OVERLAPPED:
#if defined(OF_RESOLVER_FS)
              hEvent =
                ofc_android_get_resolver_overlapped_event(hEventHandle);
	      if (ofc_event_test(hEvent))
		{
		  triggered_event = hEventHandle ;
		}
	      else
		{
		  eventElement = ofc_malloc(sizeof (EVENT_ELEMENT)) ;
		  eventElement->hAssoc = hEventHandle ;
		  eventElement->hEvent = hEvent ;
		  ofc_enqueue(eventQueue, eventElement) ;
		}
#endif
	      break ;

	    case OFC_HANDLE_FSANDROID_OVERLAPPED:
#if defined(OFC_FS_ANDROID)
	      hEvent = OfcFSAndroidGetOverlappedEvent (hEventHandle) ;
	      if (ofc_event_test(hEvent))
		{
		  triggered_event = hEventHandle ;
		}
	      else
		{
		  eventElement = ofc_malloc(sizeof (EVENT_ELEMENT)) ;
		  eventElement->hAssoc = hEventHandle ;
		  eventElement->hEvent = hEvent ;
		  ofc_enqueue(eventQueue, eventElement) ;
		}
#endif
	      break ;

	    case OFC_HANDLE_FSSMB_OVERLAPPED:
	      hWaitQ = OfcFileGetOverlappedWaitQ (hEventHandle) ;
	      hEvent = ofc_waitq_get_event_handle(hWaitQ) ;

	      if (!ofc_waitq_empty(hWaitQ))
		{
		  triggered_event = hEventHandle ;
		}
	      else
		{
		  eventElement = ofc_malloc(sizeof (EVENT_ELEMENT)) ;
		  eventElement->hAssoc = hEventHandle ;
		  eventElement->hEvent = hEvent ;
		  ofc_enqueue(eventQueue, eventElement) ;
		}
	      break ;

	    case OFC_HANDLE_EVENT:
	      if (ofc_event_test(hEventHandle))
		{
		  triggered_event = hEventHandle ;
		  if (ofc_event_get_type(hEventHandle) == OFC_EVENT_AUTO)
		    ofc_event_reset (hEventHandle) ;
		}
	      else
		{
		  eventElement = ofc_malloc(sizeof (EVENT_ELEMENT)) ;
		  eventElement->hAssoc = hEventHandle ;
		  eventElement->hEvent = hEventHandle ;
		  ofc_enqueue(eventQueue, eventElement) ;
		}
	      break ;

	    case OFC_HANDLE_TIMER:
	      wait_time = ofc_timer_get_wait_time(hEventHandle) ;
	      if (wait_time == 0)
		triggered_event = hEventHandle ;
	      else
		{
		  if (wait_time < leastWait)
		    {
		      leastWait = wait_time ;
		      timer_event = hEventHandle ;
		    }
		}
	      break ;

	    }
	}

      if (triggered_event == OFC_HANDLE_NULL)
	{
	  poll_count = poll (android_handle_list, wait_count, leastWait) ;
	  if (poll_count == 0 && timer_event != OFC_HANDLE_NULL)
	    triggered_event = timer_event ;
	  else if (poll_count > 0)
	    {
	      for (wait_index = 0 ;
		   (wait_index < wait_count &&
		    android_handle_list[wait_index].revents == 0) ;
		   wait_index++) ;

	      if (wait_index == 0)
		triggered_event =
		  PollEvent(AndroidWaitSet->pipe_files[0], eventQueue) ;
	      else if (wait_index < wait_count)
		{
		  ofc_socket_impl_set_event
		    (ofc_socket_get_impl(ofc_handle_list[wait_index]),
		     android_handle_list[wait_index].revents) ;

		  if (android_handle_list[wait_index].revents != 0)
		    triggered_event = ofc_handle_list[wait_index] ;
		}
	    }
	}

      for (eventElement = ofc_dequeue (eventQueue) ;
	   eventElement != OFC_NULL ;
	   eventElement = ofc_dequeue (eventQueue))
	ofc_free (eventElement) ;

      ofc_queue_destroy (eventQueue) ;

      ofc_free(android_handle_list) ;
      ofc_free(ofc_handle_list) ;

      ofc_handle_unlock(handle) ;
    }
  return (triggered_event) ;
}

OFC_VOID ofc_waitset_set_assoc_impl(OFC_HANDLE hEvent,
                                    OFC_HANDLE hApp, OFC_HANDLE hSet)
{
  OFC_HANDLE hAssoc ;

  switch (ofc_handle_get_type(hEvent))
    {
    default:
    case OFC_HANDLE_WAIT_SET:
    case OFC_HANDLE_SCHED:
    case OFC_HANDLE_APP:
    case OFC_HANDLE_THREAD:
    case OFC_HANDLE_PIPE:
    case OFC_HANDLE_MAILSLOT:
    case OFC_HANDLE_FSWIN32_FILE:
    case OFC_HANDLE_QUEUE:
      /*
       * These are not synchronizeable.  Simple ignore
       */
      break ;

    case OFC_HANDLE_WAIT_QUEUE:
      hAssoc = ofc_waitq_get_event_handle(hEvent) ;
      ofc_handle_set_app(hAssoc, hApp, hSet) ;
      break ;

    case OFC_HANDLE_FSRESOLVER_OVERLAPPED:
#if defined(OF_RESOLVER_FS)
      hAssoc = ofc_android_get_resolver_overlapped_event(hEvent);
      ofc_handle_set_app(hAssoc, hApp, hSet);
#endif
      break;

    case OFC_HANDLE_FSANDROID_OVERLAPPED:
#if defined(OFC_FS_ANDROID)
      hAssoc = OfcFSAndroidGetOverlappedEvent (hEvent) ;
      ofc_handle_set_app(hAssoc, hApp, hSet) ;
#endif
      break ;

    case OFC_HANDLE_FSSMB_OVERLAPPED:
      hAssoc = OfcFileGetOverlappedEvent (hEvent) ;
      ofc_handle_set_app (hAssoc, hApp, hSet) ;
      break ;

    case OFC_HANDLE_EVENT:
    case OFC_HANDLE_FILE:
    case OFC_HANDLE_SOCKET:
    case OFC_HANDLE_TIMER:
      /*
       * These don't need to set associated events
       */
      break ;
    }
}

OFC_VOID ofc_waitset_add_impl(OFC_HANDLE hSet, OFC_HANDLE hApp,
                              OFC_HANDLE hEvent)
{
  OFC_HANDLE hAssoc ;

  switch (ofc_handle_get_type(hEvent))
    {
    default:
    case OFC_HANDLE_WAIT_SET:
    case OFC_HANDLE_SCHED:
    case OFC_HANDLE_APP:
    case OFC_HANDLE_THREAD:
    case OFC_HANDLE_PIPE:
    case OFC_HANDLE_MAILSLOT:
    case OFC_HANDLE_FSWIN32_FILE:
    case OFC_HANDLE_QUEUE:
      /*
       * These are not synchronizeable.  Simple ignore
       */
      break ;

    case OFC_HANDLE_WAIT_QUEUE:
      hAssoc = ofc_waitq_get_event_handle(hEvent) ;
      ofc_handle_set_app(hAssoc, hApp, hSet) ;
      if (!ofc_waitq_empty(hEvent))
	ofc_waitset_signal_impl(hSet, hAssoc) ;
      break ;

    case OFC_HANDLE_EVENT:
      ofc_handle_set_app(hEvent, hApp, hSet) ;
      if (ofc_event_test(hEvent))
	ofc_waitset_signal_impl(hSet, hEvent) ;
      break ;

    case OFC_HANDLE_FSRESOLVER_OVERLAPPED:
#if defined(OF_RESOLVER_FS)
      hAssoc = ofc_android_get_resolver_overlapped_event(hEvent);
      ofc_handle_set_app(hAssoc, hApp, hSet);
      if (ofc_event_test(hAssoc))
	ofc_waitset_signal_impl(hSet, hAssoc);
#endif
      break;
    case OFC_HANDLE_FSANDROID_OVERLAPPED:
      hAssoc = OfcFSAndroidGetOverlappedEvent (hEvent) ;
      ofc_handle_set_app(hAssoc, hApp, hSet) ;
      if (ofc_event_test(hAssoc))
	{
	  ofc_waitset_signal_impl(hSet, hAssoc) ;
	}
      break ;

    case OFC_HANDLE_FSSMB_OVERLAPPED:
      hAssoc = OfcFileGetOverlappedEvent (hEvent) ;
      ofc_handle_set_app(hAssoc, hApp, hSet) ;
      if (ofc_event_test(hAssoc))
	ofc_waitset_signal_impl(hSet, hAssoc) ;
      break ;

    case OFC_HANDLE_FILE:
    case OFC_HANDLE_SOCKET:
    case OFC_HANDLE_TIMER:
      /*
       * These don't need to set associated events
       */
      break ;
    }
}

/** \} */
