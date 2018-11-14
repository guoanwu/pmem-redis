/***************************************************************************/
/*
 * Copyright (c) 2018, Intel Corporation
 * Copyright (c) 2018, Dennis, Wu <dennis.wu@intel.com>
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
/***************************************************************************/
#include "nvm_cow.h"
#include "stdio.h"

/*compare the two address*/
static int dictAddrCompare(void *privdata, const void *addr1, const void * addr2) {
    DICT_NOTUSED(privdata);
    return addr1==addr2;
}

static void dictAddrDestructor (void *privdata, void *addr) {
    DICT_NOTUSED(privdata);
    zfree(addr);
}

static uint64_t dictaddrHash(const void *addr) {
    return dictGenHashFunction((unsigned char*)&addr, sizeof(void *));
}


static dictType dbforkType = {
    dictaddrHash,                /* hash function */
    NULL,                       /* key dup */
    NULL,                       /* val dup */
    dictAddrCompare,            /* key compare */
    NULL,                       /* key destructor */
    NULL                        /* val destructor */
};

/*DB->dict, COW dict in which the NVM adress are freeed or duplicated then 
after BGSAVE, the object must be freeded.*/
static dictType dbcowType = {
    dictaddrHash,                /* hash function */
    NULL,                       /* key dup */
    NULL,                       /* val dup */
    dictAddrCompare,            /* key compare */
    dictAddrDestructor,         /* key destructor */
    NULL                        /* val destructor */
};

/***************************************************************************/
/*NVM COW APIs*/
/***************************************************************************/
int cow_isnvmaddrindict(dict *dict, void * addr) {
    dictEntry *de;
    de = dictFind(dict,addr);
    if (de) {
        return 1;
    }
    return 0;
}

void * cow_createforknvmdict() {
    return dictCreate(&dbforkType, NULL);
}

void * cow_createcownvmdict() {
    return dictCreate(&dbcowType,NULL);
}

/* Low level key lookup API, not actually called directly from commands
 * implementations that should instead rely on lookupKeyRead(),
 * lookupKeyWrite() and lookupKeyReadWithFlags(). */
void cow_addaddressindict(dict * dict, void *addr) {   
    //fprintf(stdout,"cowaddaddr in dict=%p, addr=%p\n", dict, addr);
    dictAddRaw(dict, addr, NULL);
}

void cow_remaddressindict(dict *dict, void *addr) {
    dictDelete(dict,addr);
}

//called for both main and child process
void * creatsharememory() {
	int shmid;
	void * shmptr;
    shmid=shmget(SHARED_MEMORY_KEY, SHARED_BUF_SIZE, IPC_CREAT);
    
    if(shmid == -1)
	{
		fprintf(stderr, "error, get the share memory failed\n");
		exit(1);
	}
	//printf("shmid=%d\n",shmid);
    shmptr=shmat(shmid,0,0);
    if(shmptr ==(void *)-1) {
		fprintf(stderr, "shmat error\n");
		exit(1);
	}
    //printf("shmptr=%p\n",shmptr);
	*(int64_t *)shmptr=BUFFER_PTR_START;
    *((int64_t *)shmptr+1)=BUFFER_PTR_START;
    return shmptr;
}

// called by the main process
void readandprocessaddress(void *shmptr,dict *newdict, dict * cowdict) {
	int i=0;
	int write_position;
	int read_position;
	int64_t * ptr;
    void * addr;
	
    //position informaiton
	write_position= *(int64_t *)shmptr;
	read_position=*((int64_t *)shmptr+1);
	int size=write_position-read_position;
    //printf("read...., write position=%d, read_position=%d\n",write_position, read_position);
	ptr=(int64_t *)shmptr+BUFFER_PTR_START;
	for(i=0;i<size;i++) {
		addr=(void *)(*(int64_t *)ptr);
		if(!cow_isnvmaddrindict(cowdict,addr)) {
			cow_addaddressindict(newdict,addr);
            //fprintf(stdout, "add new dict address=%p\n",addr);
		}else {
            //fprintf(stdout, "free address=%p in cow dict=%p\n",addr,cowdict);
            cow_remaddressindict(cowdict,addr);
            zfree(addr);
        }
		ptr+=1;
	}
	
    if(size !=0) {
        read_position+=size;
	    if(read_position == BUFFER_ENTRY_NUMBER) {
		    //reset the postion to 2
	        *(int64_t *)shmptr=BUFFER_PTR_START;
            *((int64_t *)shmptr+1)=BUFFER_PTR_START;
            //fprintf(stdout, "reset the write,read pos\n");	
        } else {
		    *((int64_t *)shmptr+1)=read_position;
            //fprintf(stdout,"update the read pos=%d\n",read_position);
	    }
    }
}

// called by the forked child process
void writeaddress(void * shmptr, void * addr) {
	int write_position;
	//position informaiton
	write_position=*(int64_t*)shmptr;
	
    //if the postion reached to the end of the buffer, not write any more
    //fprintf(stdout,"write addr=%p,writepositon=%d\n", addr,write_position);
    if(write_position!=BUFFER_ENTRY_NUMBER) {
		*((int64_t *)shmptr+write_position)=(int64_t )addr;
        write_position++;
        *(int64_t *)shmptr = write_position;
	} else if(write_position == *((int64_t *)shmptr+1)) {
		//reset the postion to 2
	    *(int64_t *)shmptr=BUFFER_PTR_START;
        *((int64_t *)shmptr+1)=BUFFER_PTR_START;
        //printf("reset read,write pos\n");
    }
}


