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

#include <alloc/consts.h>
#include <alloc/platform.h>

#include <io.h>
#include <stdlib.h>
#include <windows.h>


const char**
alloc_get_env_array(
	void
	)
{
	return (const char**) _environ;
}


alloc_t
alloc_read_page_size(
	void
	)
{
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwPageSize;
}


alloc_t
alloc_read_huge_page_size(
	void
	)
{
	return GetLargePageMinimum();
}


uint32_t
alloc_get_random(
	void
	)
{
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	return ft.dwHighDateTime * ft.dwLowDateTime;
}


uint64_t
alloc_read_time_ns(
	void
	)
{
	LARGE_INTEGER frequency;
	LARGE_INTEGER counter;
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&counter);
	return ((unsigned __int128) counter.QuadPart * 1000000000) / frequency.QuadPart;
}


uint16_t
alloc_get_cpus(
	void
	)
{
	SYSTEM_INFO info;
	GetSystemInfo(&info);

	uint32_t cpus = info.dwNumberOfProcessors;
	assert_le(cpus, UINT16_MAX);

	return cpus;
}


uint16_t
alloc_get_numa_nodes(
	void
	)
{
	ULONG highest_node_number = 0;
	BOOL status = GetNumaHighestNodeNumber(&highest_node_number);
	if(!status)
	{
		return 0;
	}

	assert_lt(highest_node_number, UINT16_MAX);

	return highest_node_number + 1;
}


void
alloc_get_cpu_numa_node(
	uint16_t* cpu,
	uint16_t* numa
	)
{
	PROCESSOR_NUMBER proc_number;
	GetCurrentProcessorNumberEx(&proc_number);

	if(cpu)
	{
		uint32_t c = (uint32_t) proc_number.Group * 64 + proc_number.Number;
		assert_le(c, UINT16_MAX);

		*cpu = c;
	}

	if(numa)
	{
		USHORT node;
		BOOL status = GetNumaProcessorNodeEx(&proc_number, &node);
		if(!status)
		{
			*numa = 0;
		}
		else
		{
			*numa = node;
			assert_le(node, ALLOC_MAX_NUMA_NODES);
		}
	}
}


bool
alloc_is_numa_node_valid(
	uint16_t numa
	)
{
	GROUP_AFFINITY affinity;
	BOOL status = GetNumaNodeProcessorMaskEx(numa, &affinity);
	if(!status)
	{
		return 0;
	}

	return affinity.Mask != 0;
}


bool
alloc_is_tty(
	int fd
	)
{
	HANDLE h = (HANDLE) _get_osfhandle(fd);
	if(h == INVALID_HANDLE_VALUE)
	{
		return 0;
	}

	DWORD mode;
	return GetConsoleMode(h, &mode);
}


attr_alloc_fn void*
alloc_alloc_virtual_e(
	alloc_t reserve_size,
	alloc_t commit_size,
	uint32_t numa,
	int thp
	)
{
	(void) thp;

	if(!reserve_size)
	{
		return NULL;
	}

	if(commit_size == (alloc_t) -1)
	{
		commit_size = reserve_size;
	}

	void* ptr = NULL;

	if(reserve_size == commit_size)
	{
		if(numa != (uint32_t) -1)
		{
			ptr = VirtualAllocExNuma(GetCurrentProcess(), NULL, reserve_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, numa);
		}
		else
		{
			ptr = VirtualAlloc(NULL, reserve_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		}

		return ptr;
	}

	if(numa != (uint32_t) -1)
	{
		ptr = VirtualAllocExNuma(GetCurrentProcess(), NULL, reserve_size, MEM_RESERVE, PAGE_NOACCESS, numa);
	}
	else
	{
		ptr = VirtualAlloc(NULL, reserve_size, MEM_RESERVE, PAGE_NOACCESS);
	}

	if(!ptr)
	{
		return NULL;
	}

	if(commit_size)
	{
		void* commit_ptr;

		if(numa != (uint32_t) -1)
		{
			commit_ptr = VirtualAllocExNuma(GetCurrentProcess(), ptr, commit_size, MEM_COMMIT, PAGE_READWRITE, numa);
		}
		else
		{
			commit_ptr = VirtualAlloc(ptr, commit_size, MEM_COMMIT, PAGE_READWRITE);
		}

		if(!commit_ptr)
		{
			VirtualFree(ptr, 0, MEM_RELEASE);
			return NULL;
		}
	}

	return ptr;
}


void
alloc_free_virtual_e(
	const volatile void* ptr,
	alloc_t size
	)
{
	(void) size;

	if(!ptr)
	{
		return;
	}

	BOOL status = VirtualFree((void*) ptr, 0, MEM_RELEASE);
	assert_neq(status, 0);
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
		return alloc_alloc_virtual_e(new_size, new_size, (uint32_t) -1, 0);
	}

	if(new_size <= old_size)
	{
		return (void*) ptr;
	}

	void* new_ptr = alloc_alloc_virtual_e(new_size, new_size, (uint32_t) -1, 0);
	if(!new_ptr)
	{
		return NULL;
	}

	memcpy(new_ptr, (void*) ptr, old_size);
	alloc_free_virtual_e(ptr, old_size);

	return new_ptr;
}


bool
alloc_commit_virtual_e(
	const volatile void* ptr,
	alloc_t size
	)
{
	return VirtualAlloc((void*) ptr, size, MEM_COMMIT, PAGE_READWRITE) != NULL;
}


void
alloc_decommit_virtual_e(
	const volatile void* ptr,
	alloc_t size
	)
{
	BOOL status = VirtualFree((void*) ptr, size, MEM_DECOMMIT);
	assert_neq(status, 0);
}
