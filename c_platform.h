#pragma once

//we should have separate headers but have only a single .c file 
// with all platform specific things

//memory.h
//Reserve address space
//Commit address space
//uncommit address space
//unreserve? addresss space
//aligned malloc
//STACK_ALLOCATE macro

//thread.h
//making thread
//destroying thread
//mutex
//semaphore
//yield 
 
//lock_free.h
//PAUSE
//atomics as functions => load/store
//reentrant spin lock
//read_write_barrier

//utf.h
//utf conversion 

//file.h
//List contents of a directory
//Create directory
//Remove directory
//Get file stats

//window.h
//Make blocking popup
//Make blocking error
//Initi window
//Deinit window
//Play sound

//time.h 
// * good as is *

//@TODO: pool allocator
//       random
//       hash tables
//       debug allocator
//       VM allocator
//       stack allocator