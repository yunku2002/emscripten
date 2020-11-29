/*
 * Copyright 2021 The Emscripten Authors.  All rights reserved.
 * Emscripten is available under two separate licenses, the MIT license and the
 * University of Illinois/NCSA Open Source License.  Both these licenses can be
 * found in the LICENSE file.
 */

#define _GNU_SOURCE
#include "pthread_impl.h"
#include <pthread.h>
#include <stdbool.h>
#include <threads.h>

// See musl's pthread_create.c

extern int __cxa_thread_atexit(void (*)(void *), void *, void *);
extern int __pthread_create_js(struct pthread **thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg);
extern _Noreturn void __pthread_exit_js(void* status);
extern int8_t __dso_handle;

void __run_cleanup_handlers(void* _unused) {
  pthread_t self = __pthread_self();
  while (self->cancelbuf) {
    void (*f)(void *) = self->cancelbuf->__f;
    void *x = self->cancelbuf->__x;
    self->cancelbuf = self->cancelbuf->__next;
    f(x);
  }
}

void __do_cleanup_push(struct __ptcb *cb) {
  struct pthread *self = __pthread_self();
  cb->__next = self->cancelbuf;
  self->cancelbuf = cb;
  static thread_local bool registered = false;
  if (!registered) {
    __cxa_thread_atexit(__run_cleanup_handlers, NULL, &__dso_handle);
    registered = true;
  }
}

void __do_cleanup_pop(struct __ptcb *cb) {
  __pthread_self()->cancelbuf = cb->__next;
}

static int tl_lock_count;
static int tl_lock_waiters;

volatile int __thread_list_lock;

void __tl_lock(void) {
  int tid = __pthread_self()->tid;
  int val = __thread_list_lock;
  if (val == tid) {
    tl_lock_count++;
    return;
  }
  while ((val = a_cas(&__thread_list_lock, 0, tid)))
    __wait(&__thread_list_lock, &tl_lock_waiters, val, 0);
}

void __tl_unlock(void) {
  if (tl_lock_count) {
    tl_lock_count--;
    return;
  }
  a_store(&__thread_list_lock, 0);
  if (tl_lock_waiters) __wake(&__thread_list_lock, 1, 0);
}

int __pthread_create(pthread_t *res, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg) {
  struct pthread *self, *new;
  self = __pthread_self();

  //printf("start __pthread_create: %p\n", self);
  int rtn = __pthread_create_js(&new, attr, start_routine, arg);
  if (rtn != 0)
    return rtn;

  //printf("done __pthread_create self=%p next=%p prev=%p new=%p\n", self, self->next, self->prev, new);

  __tl_lock();

  new->next = self->next;
  new->prev = self;
  new->next->prev = new;
  new->prev->next = new;

  __tl_unlock();

  *res = new;
  return 0;
}

_Noreturn void __pthread_exit(void* retval) {
  pthread_t self = __pthread_self();
  //printf("emscripten_thread_exit self=%p prev=%p next=%p\n", self, self->prev, self->next);

  __tl_lock();

  self->next->prev = self->prev;
  self->prev->next = self->next;
  self->prev = self->next = self;

  __tl_unlock();

  __pthread_exit_js(retval);
}

weak_alias(__pthread_create, emscripten_builtin_pthread_create);
weak_alias(__pthread_create, pthread_create);
weak_alias(__pthread_exit, pthread_exit);
