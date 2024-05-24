/*
 * An example cdrom testing client for the cdrom problem.
 *
 * This starts up a number of client threads which request blocks from the cdrom
 * using the cdrom_read function which you will provide in cdrom.c
 *
 * NOTE: DO NOT RELY ON ANY CHANGES YOU MAKE TO THIS FILE, BECAUSE
 * IT WILL BE OVERWRITTEN DURING OUR AUTOTESTING.
 */
#include "opt-synchprobs.h"
#include <types.h>  /* required by lib.h */
#include <lib.h>    /* for kprintf */
#include <synch.h>  /* for P(), V(), sem_* */
#include <thread.h> /* for thread_fork() */
#include <test.h>

#include "cdrom.h"
#include "cdrom_tester.h"

static void
cdrom_client_thread(void *unused_ptr, unsigned long client_num)
{
        unsigned int v;
        int i;
        (void)unused_ptr;

        kprintf("CDROM client thread %ld started\n", client_num);

        /* We read 10 blocks from the cdrom numbered from 1 to 10 to fetch the value stored 
        at that block */

        for (i = 0; i < 5; i++) {
                
                v = cdrom_read(i);

                kprintf("Thread %ld requested block %d an received value %d\n",client_num, i, v);

                /* We sanity the value to see if it is what we expect based on the block number requested*/
                if (!BLOCK_VAL_CHECK(i,v)) {
                        /* This should never happen in a correct implementation */
                        panic("Got an unexpected value from the requested block");
                }
        }
        
        /* After we have finished, we signal that we're done. */
        V(cdrom_client_finished);
}


/* Start a number of client threads to test the cdrom system. */

void start_cdrom_client_threads()
{
        int i;
        int result;

        for(i = 0; i < MAX_CLIENTS; i++) {
                result = thread_fork("cdrom client thread", NULL,
                                     cdrom_client_thread, NULL, i);
                if(result) {
                        panic("start_cdrom_client_threads: couldn't fork (%s)\n",
                              strerror(result));
                }
        }
}

