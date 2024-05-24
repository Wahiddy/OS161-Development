/*
 * Testing framework implementation for the cdrom problem.
 *
 * This file coordinates starting of threads that implement the problem 
 * environment, and implements request management. 
 *
 * NOTE: DO NOT RELY ON ANY CHANGES YOU MAKE TO THIS FILE, BECAUSE
 * IT WILL BE OVERWRITTEN DURING TESTING.
 */
#include "opt-synchprobs.h"
#include <types.h>  /* required by lib.h */
#include <lib.h>    /* for kprintf */
#include <synch.h>  /* for P(), V(), sem_* */
#include <thread.h> /* for thread_fork() */
#include <test.h>

#include "cdrom.h"
#include "cdrom_tester.h"


/* Semaphores which the framework uses to determine when all
 *  threads have finished.
 */
struct semaphore *cdrom_client_finished;
static struct semaphore *cdrom_handler_work;
static struct semaphore *cdrom_handler_finished;

/* type used to store an outstanding request to the cdrom */
struct cdrom_req {
        int block_num;
};

/*
* A FIFO circular buffer that stores the currently outstanding requests.
* It is of a finite size that which limits the number of concurrently
* outstanding requests. 
*/
#define REQ_BUF_SIZE  (MAX_CONCURRENT_REQ+1)
static struct cdrom_req cdrom_requests[REQ_BUF_SIZE];
static int req_head, req_tail;
static struct lock * buf_lock;

/*
* cdrom_block_read: The is the function that is provided to your code to
* request the content of disk blocks. It is the only function in this file
* that your code need to call.
*
* The function stores the request in the circular buffer of requets and 
* wakes the handler thread which services the requests.
* Observation: a lock is used to ensure updates to the buffer are mutually exclusive.
*/

void cdrom_block_request(int block_num)
{
        lock_acquire(buf_lock);
        if ((req_head + 1) % REQ_BUF_SIZE == req_tail) {
                lock_release(buf_lock);
                panic("Too many requests queued concurrently");
        } 
        else {
                cdrom_requests[req_head].block_num = block_num;
                req_head = (req_head + 1)% REQ_BUF_SIZE;
                V(cdrom_handler_work);
        }
        lock_release(buf_lock);
}

/* A flag used to indicate when the request handling thread should return */
static int cdrom_handler_shutdown;

/* 
 * cdrom_handler_thread: This thread processes requests when woken up.
 */
static void
cdrom_handler_thread(void *unused_ptr, unsigned long handler_num)
{
       
        (void)unused_ptr; /* Avoid compiler warnings */
        int block_num;
        unsigned int val;


        kprintf("Interrupt thread thread %ld started\n", handler_num);

        
        while(cdrom_handler_shutdown == 0)
        {
                P(cdrom_handler_work); /* Wait for outstanding requests*/

                lock_acquire(buf_lock);
                if (req_head == req_tail) {
                        if (cdrom_handler_shutdown != 0) {
                                /* If woken up, and the buffer is empty, and the shutdown flag
                                is set, then break out of the loop and return */
                                lock_release(buf_lock);
                                break;
                        }
                        else {
                                /* This is never happen, we should either have requests to
                                   process or are shutting down */
                                lock_release(buf_lock);
                                panic("cdrom_handler_thread woken with no work? This should never happen");
                        }
                }
                else {
                        /* Process the request in the queue */
                        block_num = cdrom_requests[req_tail].block_num;
                        req_tail = (req_tail + 1) % REQ_BUF_SIZE;
                        lock_release(buf_lock);

                        val = BLOCK_FUNC(block_num); /* Obtain the value from the block requested */
                        
                        cdrom_handler(block_num, val); /* Call the handler code to pass it to the client */
                }
        
        }
        
        V(cdrom_handler_finished); /* Signal the framework this handler is finished */
}

/* start_handler_thread: start a thread running the request handler */
static void
start_handler_thread()
{
        
        int result;

        result = thread_fork("interrupt thread", NULL,
                                     cdrom_handler_thread, NULL, 0);
        if(result) {
                panic("start_barista_threads: couldn't fork (%s)\n",
                        strerror(result));
                
        }
}

/* Wait for all the handler thread to exit.  The handler signals a semaphore
on exit, so waiting for it to finish means waiting on that semaphore. */

static void
wait_for_handler_thread()
{
        
        kprintf("Waiting for handler thread to exit...\n");
        P(cdrom_handler_finished);
        
        kprintf("Handler thread has exited.\n");
}

/* Wait for all the client threads to exit. We wait on a semaphore a number of 
   times corresponding to the number og threads.
 */
static void
wait_for_cdrom_client_threads()
{
        int i;
        
        kprintf("Waiting for tester threads to exit...\n");
        /* Now wait for all client threads to signal completion. */
        for(i = 0; i < MAX_CLIENTS; i++) {
                P(cdrom_client_finished);
        }
}

/* The main function for run the cdrom problem framework. */
int
run_cdrom(int nargs, char **args)
{
        (void) nargs; /* Avoid "unused variable" warning */
        (void) args;

        kprintf("run_cdrom: starting up\n");

        /* Initialise synch primitives used in this testing framework */
        cdrom_client_finished = sem_create("cdrom_client_finished", 0);
        if(!cdrom_client_finished) {
                panic("run_cdrom: couldn't create semaphore\n");
        }

        cdrom_handler_finished = sem_create("cdrom_handler_finished", 0);
        if(!cdrom_handler_finished ) {
                panic("run_cdrom: couldn't create semaphore\n");
        }

        cdrom_handler_work = sem_create("cdrom_handler_work", 0);
        if(!cdrom_handler_work ) {
                panic("run_cdrom: couldn't create semaphore\n");
        }

        buf_lock = lock_create("buf_lock");
        if(buf_lock == NULL ) {
                panic("run_cdrom: couldn't create semaphore\n");
        }
        
        /* Set the request buffer empty */
        req_head = req_tail = 0;

        /* set the shutdown flag to FALSE */
        cdrom_handler_shutdown = 0;

        /* Now run the student code required to initialise their implementation
        */
        cdrom_startup();

        /* Run the threads implementing the framework */
        start_cdrom_client_threads();
        start_handler_thread();

        /* Wait for all the threads to finish */

        wait_for_cdrom_client_threads();

        /* flag the handler to shut down and wake it up */
        cdrom_handler_shutdown = 1;
        V(cdrom_handler_work);
        wait_for_handler_thread();

        /* Run any student code required to clean up after the system completes */
        cdrom_shutdown();

        /* Done! */
        sem_destroy(cdrom_handler_finished);
        sem_destroy(cdrom_client_finished);
        sem_destroy(cdrom_handler_work);
        lock_destroy(buf_lock);
        return 0;
}

