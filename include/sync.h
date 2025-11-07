/*
 *   Copyright 2024-2025 Franciszek Balcerak
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

#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>


typedef pthread_mutex_t sync_mtx_t;


extern void
sync_mtx_init(
	sync_mtx_t* mtx
	);


extern void
sync_mtx_free(
	sync_mtx_t* mtx
	);


extern void
sync_mtx_lock(
	sync_mtx_t* mtx
	);


extern bool
sync_mtx_try_lock(
	sync_mtx_t* mtx
	);


extern void
sync_mtx_unlock(
	sync_mtx_t* mtx
	);


typedef pthread_rwlock_t sync_rwlock_t;


extern void
sync_rwlock_init(
	sync_rwlock_t* rwlock
	);


extern void
sync_rwlock_free(
	sync_rwlock_t* rwlock
	);


extern void
sync_rwlock_rdlock(
	sync_rwlock_t* rwlock
	);


extern bool
sync_rwlock_try_rdlock(
	sync_rwlock_t* rwlock
	);


extern void
sync_rwlock_wrlock(
	sync_rwlock_t* rwlock
	);


extern bool
sync_rwlock_try_wrlock(
	sync_rwlock_t* rwlock
	);


extern void
sync_rwlock_unlock(
	sync_rwlock_t* rwlock
	);


typedef pthread_cond_t sync_cond_t;


extern void
sync_cond_init(
	sync_cond_t* cond
	);


extern void
sync_cond_free(
	sync_cond_t* cond
	);


extern void
sync_cond_wait(
	sync_cond_t* cond,
	sync_mtx_t* mtx
	);


extern void
sync_cond_wake(
	sync_cond_t* cond
	);


typedef sem_t sync_sem_t;


extern void
sync_sem_init(
	sync_sem_t* sem,
	uint32_t value
	);


extern void
sync_sem_free(
	sync_sem_t* sem
	);


extern void
sync_sem_wait(
	sync_sem_t* sem
	);


extern void
sync_sem_timed_wait(
	sync_sem_t* sem,
	uint64_t ns
	);


extern void
sync_sem_post(
	sync_sem_t* sem
	);
