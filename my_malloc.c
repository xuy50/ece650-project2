#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#include "my_malloc.h"

__thread md_t *freeList_lt = NULL; // for local thread
md_t *freeList = NULL;
size_t segSize = 0;
size_t freeSize = 0;
size_t md_tSize = sizeof(md_t);

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;    // lock for sbrk
pthread_mutex_t lock_fl = PTHREAD_MUTEX_INITIALIZER; // lock for free list

// free help
// merge next
void mergeNext(md_t *curr, md_t *newFR)
{
    newFR->size += curr->size;
    if (curr->next != NULL)
    {
        newFR->next = curr->next;
        curr->next->prev = newFR;
        curr->prev = NULL;
        curr->next = NULL;
    }
    else
    {
        newFR->next = NULL;
        curr->prev = NULL;
        curr->next = NULL;
    }
}

// no lock version
// free
void ts_free_nolock(void *ptr)
{
    if (ptr == NULL)
    {
        return;
    }

    //  get the metadata point position
    md_t *newFR;
    newFR = (md_t *)((char *)ptr - md_tSize);

    // add new free region to freed region size
    // pthread_mutex_lock(&lock);
    freeSize += newFR->size;
    // pthread_mutex_unlock(&lock);

    // change the region state to freed
    newFR->isFree = 1;

    // add the freed region to the free list
    md_t *curr = freeList_lt;
    int isAdded = 0;
    if (freeList_lt == NULL)
    {
        freeList_lt = newFR;
        newFR->next = NULL;
        newFR->prev = NULL;
        isAdded = 1;
    }
    else
    {
        // loop 3
        while (curr != NULL && isAdded == 0)
        {
            if (curr > newFR)
            {
                if (curr->prev != NULL)
                {
                    if (((md_t *)((char *)curr->prev + curr->prev->size)) == newFR)
                    {
                        // check prev
                        newFR->prev = NULL;
                        newFR->next = NULL;
                        curr->prev->size += newFR->size;
                        newFR = curr->prev;
                        // check next
                        if (((md_t *)((char *)newFR + newFR->size)) == curr)
                        {
                            mergeNext(curr, newFR);
                        }
                    }
                    else
                    {
                        newFR->prev = curr->prev;
                        curr->prev->next = newFR;

                        if (((md_t *)((char *)newFR + newFR->size)) == curr)
                        {
                            mergeNext(curr, newFR);
                        }
                        else
                        {
                            newFR->next = curr;
                            curr->prev = newFR;
                        }
                    }
                }
                else
                {
                    // check next
                    if (((md_t *)((char *)newFR + newFR->size)) == curr)
                    {
                        mergeNext(curr, newFR);
                    }
                    else
                    {
                        curr->prev = newFR;
                        newFR->next = curr;
                    }
                    freeList_lt = newFR;
                    newFR->prev = NULL;
                }
                isAdded = 1;

                break;
            }
            if (curr->next != NULL)
                curr = curr->next;
            else
                break;
        }
        if (isAdded == 0)
        {
            if (((md_t *)((char *)curr + curr->size)) == newFR)
            {
                // check prev
                newFR->prev = NULL;
                newFR->next = NULL;
                curr->size += newFR->size;
                newFR = curr;
            }
            else
            {
                curr->next = newFR;
                newFR->prev = curr;
                newFR->next = NULL;
            }
        }
    }
}

// malloc without lock
void *ts_malloc_nolock(size_t size)
{
    size_t tSize = size + md_tSize;
    // new malloc address position, which need to be return
    md_t *newMd = NULL;

    int freeRegionEnough = 0;
    md_t *target = NULL;
    md_t *curr = freeList_lt; // get free region from free list
    // loop 2
    while (curr != NULL)
    {
        if (curr->size == tSize)
        {
            target = curr;
            freeRegionEnough = 1;
            break;
        }
        else if (curr->size > tSize)
        {
            freeRegionEnough = 2;
            if (target == NULL)
            {
                target = curr;
            }
            else if (curr->size < target->size)
            {
                target = curr;
            }
        }
        curr = curr->next;
    }

    // if the free region size is enough (>=0)
    if (freeRegionEnough != 0)
    {
        size_t leftSize = target->size - tSize; // target free region size - total malloc size
        if ((freeRegionEnough == 2) && (leftSize <= md_tSize))
        {
            freeRegionEnough = 1;
        }
        newMd = target;
        newMd->isFree = 0;
        // if the left region is not enough for other one malloc use
        // all this free region space, else segement the used space
        // and left space
        if (freeRegionEnough == 1)
        {
            newMd->size = target->size;
            if (target->prev == NULL)
            {
                if (target->next != NULL)
                {
                    md_t *headTemp = target->next;
                    freeList_lt = headTemp;
                    headTemp->prev = NULL;
                }
                else
                {
                    freeList_lt = NULL;
                }
            }
            else
            {
                if (target->next != NULL)
                {
                    target->prev->next = target->next;
                    target->next->prev = target->prev;
                }
                else
                {
                    target->prev->next = NULL;
                }
            }
            newMd->next = NULL;
            newMd->prev = NULL;
            // decreace the free region size
            // pthread_mutex_lock(&lock);
            freeSize -= target->size;
            // pthread_mutex_unlock(&lock);
        }
        else if (freeRegionEnough == 2)
        {
            newMd->size = tSize;
            // get the left free region for free list
            md_t *newCurr = (md_t *)((char *)target + tSize);
            newCurr->size = leftSize;
            newCurr->isFree = 1;
            if (target->prev != NULL)
                newCurr->prev = target->prev;
            else
                newCurr->prev = NULL;
            if (target->next != NULL)
                newCurr->next = target->next;
            else
                newCurr->next = NULL;
            if (target->prev == NULL)
            {
                freeList_lt = newCurr;
                freeList_lt->prev = NULL;
            }
            else
            {
                target->prev->next = newCurr;
            }
            if (target->next != NULL)
                target->next->prev = newCurr;
            newMd->next = NULL;
            newMd->prev = NULL;
            // decreace the free region size
            // pthread_mutex_lock(&lock);
            freeSize -= tSize;
            // pthread_mutex_unlock(&lock);
        }
    }

    // if the free region is not enough,
    // use sbrk to increase heap space
    if (freeRegionEnough == 0)
    {
        pthread_mutex_lock(&lock);
        newMd = sbrk(tSize);
        if (newMd == (md_t *)((char *)(-1)))
        {
            return NULL;
        }
        segSize += tSize;
        pthread_mutex_unlock(&lock);
        newMd->size = tSize;
        newMd->isFree = 0;
        newMd->next = NULL;
        newMd->prev = NULL;
    }

    return (md_t *)((char *)newMd + md_tSize);
}

// lock version
// free
void ts_free_lock(void *ptr)
{
    if (ptr == NULL)
    {
        return;
    }

    //  get the metadata point position
    md_t *newFR;
    newFR = (md_t *)((char *)ptr - md_tSize);

    // change the region state to freed
    newFR->isFree = 1;

    // lock for free region size and free list operation
    pthread_mutex_lock(&lock_fl);
    {
        // add new free region to freed region size
        freeSize += newFR->size;

        // add the freed region to the free list
        md_t *curr = freeList;
        int isAdded = 0;
        if (freeList == NULL)
        {
            freeList = newFR;
            newFR->next = NULL;
            newFR->prev = NULL;
            isAdded = 1;
        }
        else
        {
            // loop 3
            while (curr != NULL && isAdded == 0)
            {
                if (curr > newFR)
                {
                    if (curr->prev != NULL)
                    {
                        if (((md_t *)((char *)curr->prev + curr->prev->size)) == newFR)
                        {
                            // check prev
                            newFR->prev = NULL;
                            newFR->next = NULL;
                            curr->prev->size += newFR->size;
                            newFR = curr->prev;
                            // check next
                            if (((md_t *)((char *)newFR + newFR->size)) == curr)
                            {
                                mergeNext(curr, newFR);
                            }
                        }
                        else
                        {
                            newFR->prev = curr->prev;
                            curr->prev->next = newFR;

                            if (((md_t *)((char *)newFR + newFR->size)) == curr)
                            {
                                mergeNext(curr, newFR);
                            }
                            else
                            {
                                newFR->next = curr;
                                curr->prev = newFR;
                            }
                        }
                    }
                    else
                    {
                        // check next
                        if (((md_t *)((char *)newFR + newFR->size)) == curr)
                        {
                            mergeNext(curr, newFR);
                        }
                        else
                        {
                            curr->prev = newFR;
                            newFR->next = curr;
                        }
                        freeList = newFR;
                        newFR->prev = NULL;
                    }

                    isAdded = 1;

                    break;
                }
                if (curr->next != NULL)
                    curr = curr->next;
                else
                    break;
            }
            if (isAdded == 0)
            {
                if (((md_t *)((char *)curr + curr->size)) == newFR)
                {
                    // check prev
                    newFR->prev = NULL;
                    newFR->next = NULL;
                    curr->size += newFR->size;
                    newFR = curr;
                }
                else
                {
                    curr->next = newFR;
                    newFR->prev = curr;
                    newFR->next = NULL;
                }
            }
        }
    }
    pthread_mutex_unlock(&lock_fl);
}

void *ts_malloc_lock(size_t size)
{
    size_t tSize = size + md_tSize;
    // new malloc address position, which need to be return
    md_t *newMd = NULL;

    int freeRegionEnough = 0;
    md_t *target = NULL;

    pthread_mutex_lock(&lock_fl); // lock for linked list operation
    {
        md_t *curr = freeList; // get free region from free list
        // loop 2
        while (curr != NULL)
        {
            if (curr->size == tSize)
            {
                target = curr;
                freeRegionEnough = 1;
                break;
            }
            else if (curr->size > tSize)
            {
                freeRegionEnough = 2;
                if (target == NULL)
                {
                    target = curr;
                }
                else if (curr->size < target->size)
                {
                    target = curr;
                }
            }
            curr = curr->next;
        }

        // if the free region size is enough (>=0)
        if (freeRegionEnough != 0)
        {
            size_t leftSize = target->size - tSize; // target free region size - total malloc size
            if ((freeRegionEnough == 2) && (leftSize <= md_tSize))
            {
                freeRegionEnough = 1;
            }
            newMd = target;
            newMd->isFree = 0;
            // if the left region is not enough for other one malloc use
            // all this free region space, else segement the used space
            // and left space
            if (freeRegionEnough == 1)
            {
                newMd->size = target->size;
                if (target->prev == NULL)
                {
                    if (target->next != NULL)
                    {
                        md_t *headTemp = target->next;
                        freeList = headTemp;
                        headTemp->prev = NULL;
                    }
                    else
                    {
                        freeList = NULL;
                    }
                }
                else
                {
                    if (target->next != NULL)
                    {
                        target->prev->next = target->next;
                        target->next->prev = target->prev;
                    }
                    else
                    {
                        target->prev->next = NULL;
                    }
                }
                newMd->next = NULL;
                newMd->prev = NULL;
                // decreace the free region size
                freeSize -= target->size;
            }
            else if (freeRegionEnough == 2)
            {
                newMd->size = tSize;
                // get the left free region for free list
                md_t *newCurr = (md_t *)((char *)target + tSize);
                newCurr->size = leftSize;
                newCurr->isFree = 1;
                if (target->prev != NULL)
                    newCurr->prev = target->prev;
                else
                    newCurr->prev = NULL;
                if (target->next != NULL)
                    newCurr->next = target->next;
                else
                    newCurr->next = NULL;
                if (target->prev == NULL)
                {
                    freeList = newCurr;
                    freeList->prev = NULL;
                }
                else
                {
                    target->prev->next = newCurr;
                }
                if (target->next != NULL)
                    target->next->prev = newCurr;
                newMd->next = NULL;
                newMd->prev = NULL;
                // decreace the free region size
                freeSize -= tSize;
            }
        }
    }
    pthread_mutex_unlock(&lock_fl); // unlock for linked list operation

    // if the free region is not enough,
    // use sbrk to increase heap space
    if (freeRegionEnough == 0)
    {
        pthread_mutex_lock(&lock); // lock for sbrk operation
        newMd = sbrk(tSize);
        if (newMd == (md_t *)((char *)(-1)))
        {
            return NULL;
        }
        segSize += tSize;
        pthread_mutex_unlock(&lock); // unlock for sbrk operation
        newMd->size = tSize;
        newMd->isFree = 0;
        newMd->next = NULL;
        newMd->prev = NULL;
    }

    return (md_t *)((char *)newMd + md_tSize);
}

unsigned long get_data_segment_size() // in bytes
{
    return segSize;
}

unsigned long get_data_segment_free_space_size() // in bytes
{
    return freeSize;
}