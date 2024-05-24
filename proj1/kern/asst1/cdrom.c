/* This file will contain your solution. Modify it as you wish. */
#include <types.h>
#include <lib.h>
#include <synch.h>
#include "cdrom.h"

/* Some variables shared between threads */
#define MAX_ARR_SIZE (MAX_CLIENTS + MAX_CONCURRENT_REQ)

// MAYBE CHANGE MAX_ARR_SIZE to MAX_CLIENT size of array. Check why livelocking.

struct cond_req {
        struct cv *req_event;
        int block_num;
        unsigned int val;
        int isTaken;
};

struct cond_req shared_data[MAX_ARR_SIZE];
struct lock *cond_lock;

/* Non shared data - Only to keep track of max_cur_req */
struct semaphore *max_req;

/*
* cdrom_read: This is the client facing interface to the cdrom that makes
* requests to the cdrom system. 
* 
* Args :-
* block_num: The block number of the cdrom that the client is requesting the
* content from.
*
* This function makes a request to the cdrom system using the cdrom_block_read()
* function. cdrom_block_read() returns immediately before the request has completed.
* cdrom_read() then puts the current thread to sleep until the request completes,
* upon which, this thread needs to the woken up, the value from the specific block
* obtained, and the value is return to the requesting client.
*/

unsigned int cdrom_read(int block_num)
{        
        // Need semaphore here with initial count to MAX_CON_REQ, 
        // to prevent more than n threads making a request.
        kprintf("Received request read block %d\n",block_num);
        P(max_req);
        //kprintf("Received request read block %d\n",block_num);

        lock_acquire(cond_lock);
        cdrom_block_request(block_num);         // Make the request

        // Go through the array and sleep on a particular cv, that
        // has not been taken yet
        for (int i = 0; i < MAX_ARR_SIZE; ++i) {
                if (shared_data[i].block_num == -1 && !shared_data[i].isTaken) {

                        // Go to sleep on i'th cv until request done
                        shared_data[i].isTaken = 1;
                        cv_wait(shared_data[i].req_event, cond_lock);
                        break;       
                }
        }
        
        // Woken up and retrieve a value corresponding to the 
        // appropriate block_num
        int req_val = 0;
        for (int i = 0; i < MAX_ARR_SIZE; ++i){
                if (shared_data[i].block_num == block_num) {
                        req_val = shared_data[i].val;

                        // Reset the values back
                        shared_data[i].block_num = -1;
                        shared_data[i].val = 0;
                        shared_data[i].isTaken = 0;
                        break;
                }
        }
        
        // Let another thread now make requests, more than 0
        V(max_req);

        // Leave critical region
        lock_release(cond_lock);
        return req_val;
}

/*
* cdrom_handler: This function is called by the system each time a cdrom block request
* has completed.
* 
* Args:-
* block_num: the number of the block originally requested from the cdrom.
* value: the content of the block, i.e. the data value retrieved by the request.
* 
* The cdrom_handler runs in its own thread. The handler needs to deliver the value 
* to the original requestor and wake the requestor waiting on the value.
*/

void cdrom_handler(int block_num, unsigned int value)
{
        lock_acquire(cond_lock);
        for (int i = 0; i < MAX_ARR_SIZE; ++i) {
                if (shared_data[i].block_num == -1 && shared_data[i].isTaken) {

                        // Set values on empty slot
                        shared_data[i].block_num = block_num;
                        shared_data[i].val = value;

                        // Signal on the particular block_num to wake up.
                        // This thread can now retrieve the value.
                        cv_signal(shared_data[i].req_event, cond_lock);
                                                
                        break;
                }
        } 
        lock_release(cond_lock);  
}

/*
* cdrom_startup: This is called at system initilisation time, 
* before the various threads are started. Use it to initialise 
* or allocate anything you need before the system starts.
*/

void cdrom_startup(void)
{
        max_req = sem_create("max-req", MAX_CONCURRENT_REQ);
        if (max_req == NULL) {
                panic("Error creating max-req");
        }
        cond_lock = lock_create("cond-lock");
        if (cond_lock == NULL) {
                panic("Error creating cond-lock");
        }

        for (int i = 0; i < MAX_ARR_SIZE; ++i) {
                struct cond_req data;
                struct cv *req_event;
                char cv_name[10];
                snprintf(cv_name, 10, "%d", i);

                req_event = cv_create(cv_name);
                
                if (req_event == NULL) {
                        panic("Error creating %d condition variable", i);
                }

                // Setting empty array
                data.req_event = req_event;
                data.block_num = -1;
                data.val = 0;
                data.isTaken = 0;
                shared_data[i] = data;
        }
}   

/*
* cdrom_shutdown: This is called after all the threads in the system
* have completed. Use this function to clean up and de-allocate anything
* you set up in cdrom_startup()
*/
void cdrom_shutdown(void)
{
        lock_destroy(cond_lock);
        sem_destroy(max_req);
        for (int i = 0; i < MAX_ARR_SIZE; ++i) {
                cv_destroy(shared_data[i].req_event);
        }
}


/* Just a sanity check to warn about including the internal header file 
   It contains nothing relevant to a correct solution and any use of 
   what is contained is likely to result in failure in our testing 
   */

#if defined(CDROM_TESTER_H) 
#error Do not include the cdrom_tester header file
#endif