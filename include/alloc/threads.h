/*
 *   Copyright 2024-2026 Franciszek Balcerak
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#pragma once

#include <alloc/sync.h>

#include <stdint.h>
#include <pthread.h>

#define THREAD_ONCE_INIT PTHREAD_ONCE_INIT


typedef pthread_once_t thread_once_t;

typedef void (*thread_once_init_fn_t)(void);


extern void
thread_once(
	thread_once_t* once,
	thread_once_init_fn_t init_fn
	);


typedef pthread_key_t thread_key_t;

typedef void (*tls_dtor_fn_t)(void*);


extern thread_key_t
thread_key_create(
	tls_dtor_fn_t dtor
	);


extern void
thread_key_free(
	thread_key_t key
	);


extern void*
thread_key_get(
	thread_key_t key
	);


extern void
thread_key_set(
	thread_key_t key,
	void* value
	);


typedef pthread_t thread_t;

typedef void
(*thread_fn_t)(
	void* data
	);

typedef struct thread_data
{
	thread_fn_t fn;
	void* data;
}
thread_data_t;


extern void
thread_init(
	thread_t* thread,
	thread_data_t data
	);


extern void
thread_free(
	thread_t* thread
	);


extern void
thread_cancel_on(
	void
	);


extern void
thread_cancel_off(
	void
	);


extern void
thread_async_on(
	void
	);


extern void
thread_async_off(
	void
	);


extern void
thread_detach(
	thread_t thread
	);


extern void
thread_join(
	thread_t thread
	);


extern thread_t
thread_self(
	void
	);


extern bool
thread_equal(
	thread_t a,
	thread_t b
	);


extern void
thread_cancel_sync(
	thread_t thread
	);


extern void
thread_cancel_async(
	thread_t thread
	);


extern void
thread_exit(
	void
	);


extern void
thread_sleep(
	uint64_t ns
	);


typedef struct threads
{
	thread_t* threads;
	uint32_t used;
	uint32_t size;
}
threads_t;


extern void
threads_init(
	threads_t* threads
	);


extern void
threads_free(
	threads_t* threads
	);


extern void
threads_add(
	threads_t* threads,
	thread_data_t data,
	uint32_t count
	);


extern void
threads_cancel_sync(
	threads_t* threads,
	uint32_t count
	);


extern void
threads_cancel_async(
	threads_t* threads,
	uint32_t count
	);


extern void
threads_cancel_all_sync(
	threads_t* threads
	);


extern void
threads_cancel_all_async(
	threads_t* threads
	);


typedef struct thread_pool
{
	sync_sem_t sem;
	sync_mtx_t mtx;

	thread_data_t* queue;
	uint32_t used;
	uint32_t size;
}
thread_pool_t;


extern void
thread_pool_fn(
	void* data
	);


extern void
thread_pool_init(
	thread_pool_t* pool
	);


extern void
thread_pool_free(
	thread_pool_t* pool
	);


extern void
thread_pool_lock(
	thread_pool_t* pool
	);


extern void
thread_pool_unlock(
	thread_pool_t* pool
	);


extern void
thread_pool_add_u(
	thread_pool_t* pool,
	thread_data_t data
	);


extern void
thread_pool_add(
	thread_pool_t* pool,
	thread_data_t data
	);


extern bool
thread_pool_try_work_u(
	thread_pool_t* pool
	);


extern bool
thread_pool_try_work(
	thread_pool_t* pool
	);


extern void
thread_pool_work_u(
	thread_pool_t* pool
	);


extern void
thread_pool_work(
	thread_pool_t* pool
	);
