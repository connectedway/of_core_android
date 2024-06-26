/* Copyright (c) 2021 Connected Way, LLC. All rights reserved.
 * Use of this source code is governed by a Creative Commons 
 * Attribution-NoDerivatives 4.0 International license that can be
 * found in the LICENSE file.
 */
#define __USE_UNIX98
#include <pthread.h>

#include "ofc/types.h"
#include "ofc/lock.h"
#include "ofc/heap.h"
#include "ofc/process.h"

typedef struct
{
  OFC_VOID *caller ;
  OFC_UINT32 thread ;
  pthread_mutexattr_t mutex_attr ;
  pthread_mutex_t mutex_lock ;
} OFC_LOCK_IMPL ;

OFC_VOID ofc_lock_destroy_impl(OFC_LOCK_IMPL *lock)
{
  pthread_mutex_destroy (&lock->mutex_lock) ;
  pthread_mutexattr_destroy (&lock->mutex_attr) ;
  ofc_free(lock);
}

OFC_VOID *ofc_lock_init_impl(OFC_VOID)
{
    OFC_LOCK_IMPL *lock;

    lock = ofc_malloc(sizeof(OFC_LOCK_IMPL));
    pthread_mutexattr_init(&lock->mutex_attr);
    pthread_mutexattr_settype(&lock->mutex_attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init (&lock->mutex_lock, &lock->mutex_attr) ;
    lock->caller = OFC_NULL;
    return (lock);
}

OFC_BOOL ofc_lock_try_impl(OFC_LOCK_IMPL *lock)
{
  OFC_BOOL ret ;

  ret = OFC_FALSE ;
  if (pthread_mutex_trylock (&lock->mutex_lock) == 0)
    {
#if defined(OFC_STACK_TRACE)
      lock->caller = ofc_process_relative_addr(__builtin_return_address(1));
#endif
      ret = OFC_TRUE ;
    }

  return (ret) ;
}

OFC_VOID ofc_lock_impl(OFC_LOCK_IMPL *lock)
{
    pthread_mutex_lock(&lock->mutex_lock);
#if defined(OFC_STACK_TRACE)
    lock->caller = ofc_process_relative_addr(__builtin_return_address(1));
#endif
}

OFC_VOID ofc_unlock_impl(OFC_LOCK_IMPL *lock)
{
#if defined(OFC_STACK_TRACE)
    lock->caller = ofc_process_relative_addr(__builtin_return_address(1));
#endif
    pthread_mutex_unlock(&lock->mutex_lock);
}

