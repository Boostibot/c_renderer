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
//COMPILER_MEMORY_FENCE
//PROCESSOR_MEMORY_FENCE
//DEBUG_BREAK
//PAUSE
//interlocked_exchange
//atomics as functions => load/store
//reentrant spin lock

//utf.h
//utf conversion 

//file.h
//List contents of a directory
//Create directory
//Remove directory
//Get file stats

//window.h
//Make blocking popup
//Input struct

//time.h 
// * good as is *

//@TODO: pool allocator
//       random
//       hash tables
//       debug allocator
//       VM allocator
//       stack allocator