#ifndef CDROM_H
#define CDROM_H

/*  This file contains constants, types, and prototypes for the
 *  cdrom problem. It is included by the test framework and the
 *  file you modify so as to share the definitions between both.
 *
 *  YOU SHOULD NOT RELY ON CHANGES TO THIS FILE
 *
 *  We will replace it in testing, so any changes will be lost.
 */

/* Some constants that determine the size of the system created. */

#define MAX_CONCURRENT_REQ 3
#define MAX_CLIENTS 3

/* The two main cdrom functions you must implement */
extern unsigned int cdrom_read(int block_num);
extern void cdrom_handler(int block_num, unsigned int value);


/* The function we provide to add a request to the queue 
   of cdrom requests */

extern void cdrom_block_request(int block_num);


/* startup and shutdown functions for your code */
extern void cdrom_startup(void);
extern void cdrom_shutdown(void);

#endif 
