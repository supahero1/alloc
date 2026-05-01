/* skip */
/*
 *   Copyright 2026 Franciszek Balcerak
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

#include <alloc/attr.h>
#include <alloc/debug.h>
#include <alloc/macro.h>
#include <alloc/types.h>
#include <alloc/consts.h>
#include <alloc/platform.h>

#include <time.h>
#include <sched.h>
#include <stdio.h>
#include <dirent.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <linux/mempolicy.h>


extern char** environ;


const char**
alloc_get_env_array(
	void
	)
{
	return (const char**) environ;
}


alloc_t
alloc_read_page_size(
	void
	)
{
	return getpagesize();
}


alloc_t
alloc_read_huge_page_size(
	void
	)
{
	FILE* f = fopen("/proc/meminfo", "r");
	if(!f)
	{
		return 0;
	}

	alloc_t huge_page_size = 0;
	char line[256];
	char format[32];
	snprintf(format, sizeof(format), "Hugepagesize: %s kB", MACRO_FORMAT_TYPE_CONST(alloc_t));

	while(fgets(line, sizeof(line), f))
	{
		alloc_t size_kb;
		if(sscanf(line, format, &size_kb) == 1)
		{
			huge_page_size = size_kb * 1024;
			break;
		}
	}

	fclose(f);

	return huge_page_size;
}


uint32_t
alloc_get_random(
	void
	)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * tv.tv_usec;
}


uint64_t
alloc_read_time_ns(
	void
	)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
	return (uint64_t) ts.tv_sec * 1000000000 + ts.tv_nsec;
}


uint16_t
alloc_get_cpus(
	void
	)
{
	long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
	if(nprocs < 0)
	{
		return 0;
	}

	assert_le(nprocs, UINT16_MAX);

	return nprocs;
}


uint16_t
alloc_get_numa_nodes(
	void
	)
{
	DIR* dir = opendir("/sys/devices/system/node");
	if(!dir)
	{
		return 0;
	}

	struct dirent* de;
	int count = 0;

	while((de = readdir(dir)) != NULL)
	{
		if(strncmp(de->d_name, "node", 4))
		{
			continue;
		}

		int node_id = strtoul(de->d_name + 4, NULL, 0);
		if(node_id > count)
		{
			count = node_id;
		}
	}

	closedir(dir);

	assert_lt(count, UINT16_MAX);

	return count + 1;
}


void
alloc_get_cpu_numa_node(
	uint16_t* cpu,
	uint16_t* numa
	)
{
	uint32_t c, n;
	getcpu(&c, &n);

	assert_le(c, UINT16_MAX);
	assert_le(n, ALLOC_MAX_NUMA_NODES);

	if(cpu)
	{
		*cpu = c;
	}

	if(numa)
	{
		*numa = n;
	}
}


bool
alloc_is_numa_node_valid(
	uint16_t numa
	)
{
	char path[128];
	snprintf(path, sizeof(path), "/sys/devices/system/node/node%u", (unsigned) numa);
	return access(path, F_OK) == 0;
}


bool
alloc_is_tty(
	int fd
	)
{
	return isatty(fd);
}


attr_alloc_fn void*
alloc_alloc_virtual_e(
	alloc_t reserve_size,
	alloc_t commit_size,
	uint32_t numa,
	int thp
	)
{
	if(!reserve_size)
	{
		return NULL;
	}

	if(commit_size == (alloc_t) -1)
	{
		commit_size = reserve_size;
	}

	assert_le(commit_size, reserve_size);

	int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;

	int prot =
#ifdef ALLOC_RELEASE
		PROT_READ | PROT_WRITE
#else
		PROT_NONE
#endif
		;

	void* ptr = mmap(NULL, reserve_size, prot, flags, -1, 0);
	if(ptr == MAP_FAILED)
	{
		return NULL;
	}

	if(numa != (uint32_t) -1)
	{
		const uint32_t bits_per_long = sizeof(unsigned long) * 8;
		uint32_t mask_len = (numa / bits_per_long) + 1;
		unsigned long nodemask[mask_len];

		memset(nodemask, 0, sizeof(nodemask));
		nodemask[numa / bits_per_long] = (unsigned long) 1 << (numa % bits_per_long);

		syscall(SYS_mbind, ptr, reserve_size, MPOL_BIND, nodemask, mask_len * bits_per_long, 0);
	}

	madvise(ptr, reserve_size, thp ? MADV_HUGEPAGE : MADV_NOHUGEPAGE);


#ifdef ALLOC_RELEASE
	(void) commit_size;
#else
	if(commit_size)
	{
		void* commit_ptr = mmap(ptr, commit_size,
			PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
		if(commit_ptr == MAP_FAILED)
		{
			munmap(ptr, reserve_size);
			return NULL;
		}
	}
#endif

	return ptr;
}


void
alloc_free_virtual_e(
	const volatile void* ptr,
	alloc_t size
	)
{
	if(!ptr)
	{
		return;
	}

	int status = munmap((void*) ptr, size);
	assert_eq(status, 0);
}


void*
alloc_realloc_virtual_e(
	const volatile void* ptr,
	alloc_t old_size,
	alloc_t new_size
	)
{
	if(!new_size)
	{
		alloc_free_virtual_e(ptr, old_size);
		return NULL;
	}

	if(!ptr)
	{
		/* Assumes you might call alloc cross-numa, but not realloc. */
		return alloc_alloc_virtual_e(new_size, new_size, -1, 0);
	}

	void* new_ptr = mremap((void*) ptr, old_size, new_size, MREMAP_MAYMOVE);
	if(new_ptr == MAP_FAILED)
	{
		return NULL;
	}

	return new_ptr;
}


bool
alloc_commit_virtual_e(
	const volatile void* ptr,
	alloc_t size
	)
{
	assert_not_null(ptr);
	assert_eq((alloc_t) ptr & alloc_consts.page.mask, 0);

	(void) size;

	return
#ifdef ALLOC_RELEASE
		true
#else
		mprotect((void*) ptr, size, PROT_READ | PROT_WRITE) == 0
#endif
		;
}


void
alloc_decommit_virtual_e(
	const volatile void* ptr,
	alloc_t size
	)
{
	assert_not_null(ptr);
	assert_eq((alloc_t) ptr & alloc_consts.page.mask, 0);

	int status = madvise((void*) ptr, size, MADV_DONTNEED);
	assert_eq(status, 0);

#ifndef ALLOC_RELEASE
	status = mprotect((void*) ptr, size, PROT_NONE);
	assert_eq(status, 0);
#endif
}
