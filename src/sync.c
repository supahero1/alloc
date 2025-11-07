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

#include "../include/sync.h"
#include "../include/debug.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>


void
sync_mtx_init(
	sync_mtx_t* mtx
	)
{
	assert_not_null(mtx);

	int status = pthread_mutex_init(mtx, NULL);
	hard_assert_eq(status, 0);
}


void
sync_mtx_free(
	sync_mtx_t* mtx
	)
{
	assert_not_null(mtx);

	int status = pthread_mutex_destroy(mtx);
	hard_assert_eq(status, 0);
}


void
sync_mtx_lock(
	sync_mtx_t* mtx
	)
{
	assert_not_null(mtx);

	int status = pthread_mutex_lock(mtx);
	assert_eq(status, 0);
}


bool
sync_mtx_try_lock(
	sync_mtx_t* mtx
	)
{
	assert_not_null(mtx);

	int status = pthread_mutex_trylock(mtx);
	if(status == 0)
	{
		return true;
	}

	assert_eq(status, EBUSY);
	return false;
}


void
sync_mtx_unlock(
	sync_mtx_t* mtx
	)
{
	assert_not_null(mtx);

	int status = pthread_mutex_unlock(mtx);
	assert_eq(status, 0);
}


void
sync_rwlock_init(
	sync_rwlock_t* rwlock
	)
{
	assert_not_null(rwlock);

	int status = pthread_rwlock_init(rwlock, NULL);
	hard_assert_eq(status, 0);
}


void
sync_rwlock_free(
	sync_rwlock_t* rwlock
	)
{
	assert_not_null(rwlock);

	int status = pthread_rwlock_destroy(rwlock);
	hard_assert_eq(status, 0);
}


void
sync_rwlock_rdlock(
	sync_rwlock_t* rwlock
	)
{
	assert_not_null(rwlock);

	int status = pthread_rwlock_rdlock(rwlock);
	assert_eq(status, 0);
}


bool
sync_rwlock_try_rdlock(
	sync_rwlock_t* rwlock
	)
{
	assert_not_null(rwlock);

	int status = pthread_rwlock_tryrdlock(rwlock);
	if(status == 0)
	{
		return true;
	}

	assert_eq(status, EBUSY);
	return false;
}


void
sync_rwlock_wrlock(
	sync_rwlock_t* rwlock
	)
{
	assert_not_null(rwlock);

	int status = pthread_rwlock_wrlock(rwlock);
	assert_eq(status, 0);
}


bool
sync_rwlock_try_wrlock(
	sync_rwlock_t* rwlock
	)
{
	assert_not_null(rwlock);

	int status = pthread_rwlock_trywrlock(rwlock);
	if(status == 0)
	{
		return true;
	}

	assert_eq(status, EBUSY);
	return false;
}


void
sync_rwlock_unlock(
	sync_rwlock_t* rwlock
	)
{
	assert_not_null(rwlock);

	int status = pthread_rwlock_unlock(rwlock);
	assert_eq(status, 0);
}


void
sync_cond_init(
	sync_cond_t* cond
	)
{
	assert_not_null(cond);

	int status = pthread_cond_init(cond, NULL);
	hard_assert_eq(status, 0);
}


void
sync_cond_free(
	sync_cond_t* cond
	)
{
	assert_not_null(cond);

	int status = pthread_cond_destroy(cond);
	hard_assert_eq(status, 0);
}


void
sync_cond_wait(
	sync_cond_t* cond,
	sync_mtx_t* mtx
	)
{
	assert_not_null(cond);
	assert_not_null(mtx);

	int status = pthread_cond_wait(cond, mtx);
	assert_eq(status, 0);
}


void
sync_cond_wake(
	sync_cond_t* cond
	)
{
	assert_not_null(cond);

	int status = pthread_cond_signal(cond);
	assert_eq(status, 0);
}


void
sync_sem_init(
	sync_sem_t* sem,
	uint32_t value
	)
{
	assert_not_null(sem);

	int status = sem_init(sem, 0, value);
	hard_assert_eq(status, 0);
}


void
sync_sem_free(
	sync_sem_t* sem
	)
{
	assert_not_null(sem);

	int status = sem_destroy(sem);
	hard_assert_eq(status, 0);
}


void
sync_sem_wait(
	sync_sem_t* sem
	)
{
	assert_not_null(sem);

	int status;
	while((status = sem_wait(sem)))
	{
		if(errno == EINTR)
		{
			continue;
		}

		fprintf(stderr, "sem_wait: %s\n", strerror(errno));
		hard_assert_unreachable();
	}
}


void
sync_sem_timed_wait(
	sync_sem_t* sem,
	uint64_t ns
	)
{
	assert_not_null(sem);

	struct timespec time;
	time.tv_sec = ns / 1000000000;
	time.tv_nsec = ns % 1000000000;

	int status;
	while((status = sem_timedwait(sem, &time)))
	{
		if(errno == EINTR)
		{
			continue;
		}

		if(errno == ETIMEDOUT)
		{
			break;
		}

		fprintf(stderr, "sem_timedwait: %s\n", strerror(errno));
		hard_assert_unreachable();
	}
}


void
sync_sem_post(
	sync_sem_t* sem
	)
{
	assert_not_null(sem);

	int status = sem_post(sem);
	assert_eq(status, 0);
}
