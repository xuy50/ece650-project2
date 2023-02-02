# ece650-project2

 - Author: Yang Xu
 - netID: yx248
 - Data: 02/02/2023
 - Instructor: Rabih Younes
 - Course: ECE650

## The Thread-Safe Malloc Implementation:

&emsp;&emsp;For this project, we should implement the Thread-Safe Malloc. We should implement `4 methods`:<br>
&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;Thread Safe malloc/free: locking version:<br>
&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;`void *ts_malloc_lock(size_t size);`<br>
&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;`void ts_free_lock(void *ptr);`<br>

&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;Thread Safe malloc/free: non-locking version:<br>
&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;`void *ts_malloc_nolock(size_t size);`<br>
&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;`void ts_free_nolock(void *ptr);`<br>
<br>

&emsp;&emsp;For this project, we should let our best-fit version allocation policy become to thread-save. For multithreading, our policy can work correctly.<br>
&emsp;&emsp;Because the code based on the allocation policy code I have completed, I only need to change a little variable name to seprate lock and non-lock version and add two locks into the original code without changing logical and the other codes.

The locking version methods:<br>
&emsp;&emsp;For the locking version, I add two locks. One is for `sbrk` function, the other one is for free linked list operation. I think this two locks can let the `sbrk` function and list operation do not interact at the same time, when an thread is doing `sbrk`, the other thread cannot do `sbrk` but it can do other free list operations, so I think it can be faster than only one lock.<br>

**ts_malloc_lock**<br>
&emsp;&emsp;I add a lock `lock_fl` for all free list operations to let the free list operations at the same time. And add the `sbrk` lock for `sbrk`.<br>

**ts_free_lock**<br>
&emsp;&emsp;I add a lock `lock_fl` for free list operation, this lock is for all free list operations, find the correct position and write in the free list.<br>

The non-locking version methods:<br>
&emsp;&emsp;I use the local thread key words to implement this version. I add a new free list, `__thread md_t *freeList_lt = NULL; // for local thread`, for each thread. <br>

**ts_malloc_nolock**<br>
&emsp;&emsp;Because there are separate free list for each thead, so I only changed the original free list to `freelist_lt`, and add the `sbrk` lock for `sbrk`. All freelist operations work in each independent thread, so do not need to care about the other logical and codes.<br>

**ts_free_nolock**<br>
&emsp;&emsp;I didn't change anything for original best-fit free, because all free list operations are work in each independent thread.<br>
<br>

## Results from My Performance Experiments
Because my results are so close and multithreading causes the results to fluctuate, I ran them each ten times and got the average of the results.<br>
<img src="https://github.com/xuy50/ece650-project2/blob/main/result.png" width = "708" height = "527" alt="result"/>

<br>

## Analysis of The Results

**Execution Time**<br>
&emsp;&emsp;For the execution time, the lock version is slower than non-lock version. For the lock version, there are two locks, these locks make the other threads will pause, which will have a longer time. The non-lock version only has one lock for `sbrk`, so all threads run without any stop except running `sbkr` at the same time. Therefore, the non-locking version will be faster than locking version.<br>

**Data Segment Size**<br>
&emsp;&emsp;For the data segment size, the lock version is less than non-lock version. For the lock version, there is only one main free list, so the free list has all free region in the heap, so when do `malloc`, it can read all free region. However, the non-locking version will separate the free list for each independent thread, so when do the `malloc` operation, the thread only can read its own free list whose size is much smaller than the total free list, so it will do more times `sbrk` operation for segment. Therefore, the locking version's data segment size will smaller than non-locking version.<br>

<br>
&emsp;&emsp;The locking verion sacrifices execution time and reduces segment size, and the non-locking version sacrifices less segment size and reduces execution time.<br>
<br>




