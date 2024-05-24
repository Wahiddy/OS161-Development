#ifndef CDROM_TESTER_H
#define CDROM_TESTER_H

/*  This file contains constants, types, and prototypes for the
 *  cdrom testing. It is included by the test framework only and 
 *  is not needed by cdrom.c (the file you put your implemenation in).
 *
 *  YOU SHOULD NOT RELY ON CHANGES TO THIS FILE

 *  We will replace it in testing, so any changes will be lost.
 */


/* 'magic' functions that determines what is stored in a block on the cdrom 
   so returned values can be checked.
*/
#define RANDOM_VALUE 0x12345678
#define BLOCK_FUNC(b) ((unsigned int) ((b) ^ RANDOM_VALUE))
#define BLOCK_VAL_CHECK(b,v) (BLOCK_FUNC(b) == v)

/* Some prototypes used by the main cdrom testing files to coordinate
   starting and stopping the testing client threads. */
   
extern struct semaphore *cdrom_client_finished;
extern void start_cdrom_client_threads(void);

extern int run_cdrom(int, char**);

#endif 
