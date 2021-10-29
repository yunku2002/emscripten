#include <assert.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <emscripten/threading.h>

typedef int* (*sidey_data_type)();
typedef int (*func_t)();
typedef func_t (*sidey_func_type)();

static sidey_data_type p_side_data_address;
static sidey_func_type p_side_func_address;
static int* expected_data_addr;
static func_t expected_func_addr;

static pthread_cond_t ready_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t ready_mutex = PTHREAD_MUTEX_INITIALIZER;
static atomic_bool ready = false;

static void* thread_main() {
  while (!ready) {
    pthread_mutex_lock(&ready_mutex);
    pthread_cond_wait(&ready_cond, &ready_mutex);
    pthread_mutex_unlock(&ready_mutex);
  }

  printf("in thread_main\n");
  _emscripten_thread_sync_code();

  printf("calling p_side_data_address=%p\n", p_side_data_address);
  int* data_addr = p_side_data_address();
  assert(data_addr == expected_data_addr);

  printf("calling p_side_func_address=%p\n", p_side_func_address);
  func_t func_addr = p_side_func_address();
  assert(expected_func_addr == func_addr);
  assert(func_addr() == 43);

  printf("thread_main done\n");
  return 0;
}

int main() {
  printf("in main\n");

  // Start a thread before loading the shared library
  pthread_t t;
  int rc = pthread_create(&t, NULL, thread_main, NULL);
  assert(rc == 0);

  printf("loading dylib\n");

  void* handle = dlopen("liblib.so", RTLD_NOW|RTLD_GLOBAL);
  if (!handle) {
    printf("dlerror: %s\n", dlerror());
  }
  assert(handle);
  p_side_data_address = dlsym(handle, "side_data_address");
  printf("p_side_data_address=%p\n", p_side_data_address);
  p_side_func_address = dlsym(handle, "side_func_address");
  printf("p_side_func_address=%p\n", p_side_func_address);

  expected_data_addr = p_side_data_address();

  // side_func_address return the address of a function
  // internal to the side module (i.e. part of its static
  // table region).
  expected_func_addr = p_side_func_address();
  printf("p_side_func_address -> %p\n", expected_func_addr);
  assert(expected_func_addr() == 43);

  pthread_mutex_lock(&ready_mutex);
  ready = true;
  pthread_cond_signal(&ready_cond);
  pthread_mutex_unlock(&ready_mutex);

  printf("joining\n");
  rc = pthread_join(t, NULL);
  assert(rc == 0);
  printf("done join\n");

  dlclose(handle);
  return 0;
}
