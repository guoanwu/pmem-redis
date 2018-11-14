/*
 * Copyright (c) 2018, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __NVM_COW_H
#define __NVM_COW_H
#include "stddef.h"
#include "dict.h"
#include "zmalloc.h"

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#define SHARED_MEMORY_KEY 24
#define BUFFER_ENTRY_NUMBER 10000
#define BUFFER_PTR_START 2
#define SHARED_BUF_SIZE (BUFFER_ENTRY_NUMBER<<3)

#ifdef __cplusplus
extern "C" {
#endif

void * creatsharememory();
// called by the main process
void readandprocessaddress(void *shmptr,dict *newdict, dict * cowdict);

// called by the forked child process
void writeaddress(void * shmptr, void * addr);

int cow_isnvmaddrindict(dict *dict, void * addr);
void * cow_createforknvmdict();
void * cow_createcownvmdict();
void cow_addaddressindict(dict *dict, void *addr);
void cow_remaddressindict(dict *dict, void *addr);
#ifdef __cplusplus
}
#endif
#endif

