/*
** Copyright (C) Mellanox Technologies Ltd. 2001-2011.  ALL RIGHTS RESERVED.
**
** This software product is a proprietary product of Mellanox Technologies Ltd.
** (the "Company") and all right, title, and interest in and to the software product,
** including all associated intellectual property rights, are and shall
** remain exclusively with the Company.
**
** This software product is governed by the End User License Agreement
** provided with the software product.
**
*/

#ifndef PRIORITYQUEUE_H_
#define PRIORITYQUEUE_H_

#include <vector>
#include <list>
#include <string>

#include <LinkList.h>
#include <NetlevComm.h>

#include "IOUtility.h"

class RawKeyValueIterator;

typedef struct mem_desc {
    struct list_head     list;
    char                *buff;
    int32_t              buf_len;
    int32_t              act_len;
    volatile int         status; /* available or invalid*/
    struct memory_pool  *owner;  /* owner pool */
    pthread_mutex_t      lock;
    pthread_cond_t       cond;
} mem_desc_t;


//#include "StreamRW.h"
//#include "MergeManager.h"

/****************************************************************************
 * A PriorityQueue maintains a partial ordering of its elements such that the
 * least element can always be found in constant time.  Put()'s and pop()'s
 * require log(size) time. 
 ****************************************************************************/
template <class T>
class PriorityQueue
{
private:
    std::vector<T> m_heap;
    int            m_size;
    int            m_maxSize;

public:
    /** 
     * Determines the ordering of objects in this priority queue.  
     * Subclasses must define this one method. 
     */
    virtual bool lessThan(T a, T b) {
        DataStream *key1 = &a->key;
        DataStream *key2 = &b->key;
        return memcmp(key1->getData(), key2->getData(), key1->getLength()) < 0;
    }

    void initialize(int maxSize) {
        m_size = 0;
        int heapSize = maxSize + 1;
        m_maxSize = maxSize;
        m_heap.resize(heapSize);
        for (int i = 0; i < heapSize; ++i) {
            m_heap[i] = NULL;
        }
    }
    
    PriorityQueue<T>() {}
    virtual ~PriorityQueue() {}
  
    /**
     * Right now, there is no extra exception handling in thi part
     * Please avoid putting too mand objects into the priority queue
     * so that the total number exceeds the maxSize
     */
    void put(T element) {
        m_size++;
        m_heap[m_size] = element;
        upHeap();
    }

    /**
     * Adds element to the PriorityQueue in log(size) time if either
     * the PriorityQueue is not full, or not lessThan(element, top()).
     * @param element
     * @return true if element is added, false otherwise.
     */
    bool insert(T element) {
        if (m_size < m_maxSize) {
            put(element);
            return true;
        }
        else if (m_size > 0 && !lessThan(element, top())) {
            m_heap[1] = element;
            adjustTop();
            return true;
        }
        else {
            return false;
        }
    }

    /** 
     * Returns the least element of the PriorityQueue in constant time. 
     */
    T top() {
        if (m_size > 0)
            return m_heap[1];
        else
            return NULL;
    }

    /**
     * (1) Pop the element on the top of the priority queue, which should be
     *     the least among current objects in queue.
     * (2) Move the last object in the queue to the first place
     * (3) Call downHeap() to find the least object and put it on the first
     *     position.
     */
    T pop() {
        if (m_size > 0) {
            T result = m_heap[1];      /* save first value*/
            m_heap[1] = m_heap[m_size];/* move last to first*/
            m_heap[m_size] = NULL;	   /* permit GC of objects*/
            m_size--;
            downHeap();		 /* adjust heap*/
            return result;
        } else {
            return NULL;
        }
    }

    /* Be called when the object at top changes values.*/
    void adjustTop() {
        downHeap();
    }

    /*get the total number of objects stording in priority queue.*/
    int size() { 
        return m_size; 
    }

    /*reset the priority queue*/
    void clear() {
        for (int i = 0; i <= m_size; i++)
            m_heap[i] = NULL;
        m_size = 0;
    }

private:
    
    void upHeap() {
        int i = m_size;
        T node = m_heap[i];			  /* save bottom node*/
        int j = i >> 1;
        while (j > 0 && lessThan(node, m_heap[j])) {
            m_heap[i] = m_heap[j];	  /* shift parents down*/
            i = j;
            j = j >> 1;				 
        }
        m_heap[i] = node;			  /* install saved node*/
    }

    void downHeap() {
        int i = 1;
        T node = m_heap[i];			  /* save top node*/
        int j = i << 1;				  /* find smaller child*/
        int k = j + 1;
        if (k <= m_size && lessThan(m_heap[k], m_heap[j])) {
            j = k;
        }

        while (j <= m_size && lessThan(m_heap[j], node)) {
            m_heap[i] = m_heap[j];	  /* shift up child*/
            i = j;
            j = i << 1;
            k = j + 1;
            if (k <= m_size && lessThan(m_heap[k], m_heap[j])) {
                j = k;
            }
        }
        m_heap[i] = node;			  /* install saved node*/
    }
};


/****************************************************************************
 * The implementation of PriorityQueue and RawKeyValueIterator
 ****************************************************************************/
template <class T>
class MergeQueue 
{
private:
    std::list<T> *mSegments;
    T min_segment;
    DataStream *key;
    DataStream *val;
public:
    const std::string filename;

public: 

    virtual ~MergeQueue(){}
    int        mergeq_flag;  /* flag to check the former k,v */
    RawKeyValueIterator* merge(int factor, int inMem, std::string &tmpDir);
    DataStream* getKey() { return this->key; }
    DataStream* getVal() { return this->val; }
    bool next() {
        if(this->mergeq_flag) {
            return true;
        }

        if (core_queue.size() == 0) {
        	return false;
        }

        if (this->min_segment != NULL) {
            this->adjustPriorityQueue(this->min_segment);
            if (core_queue.size() == 0) {
                this->min_segment = NULL;
                return false;
            }
        }
        this->min_segment = core_queue.top();
        this->key = &this->min_segment->key;
        this->val = &this->min_segment->val;
        return true;
    }

    bool insert(T segment){
        int ret = segment->nextKV();
        switch (ret) {
            case 0: { /*end of the map output*/
                delete segment;
                break;
            }
            case 1: { /*next keyVal exist*/
                core_queue.put(segment);
                break;
            }
            case -1: { /*break in the middle of the data*/
                output_stderr("MergeQueue:break in the first KV pair");
                segment->switch_mem();
                break;
            }
            default:
                output_stderr("MergeQueue: Error in insert");
                break;
        }

        write_log(segment->get_task()->reduce_log,
                  DBG_CLIENT, "MergeQueue: current size %d", core_queue.size());

        return true;
    }

    int32_t get_key_len() {return this->min_segment->cur_key_len;}
    int32_t get_val_len() {return this->min_segment->cur_val_len;}
    int32_t get_key_bytes(){return this->min_segment->kbytes;}
    int32_t get_val_bytes() {return this->min_segment->vbytes;}

      MergeQueue(int numMaps, const char*fname = "") : filename(fname){
        this->mSegments = NULL;
        this->min_segment = NULL;
        this->key = NULL;
        this->val = NULL;
        this->mergeq_flag = 0;
        this->staging_bufs[0] = NULL;
        this->staging_bufs[1] = NULL;
        core_queue.initialize(numMaps);
        core_queue.clear();
    }

    MergeQueue(std::list<T> *segments){
        this->mSegments = segments;
        this->min_segment = NULL;
    }

    mem_desc_t  *staging_bufs[2]; 
    PriorityQueue<T> core_queue;

protected:

    bool lessThan(T a, T b);
    void adjustPriorityQueue(T segment){
        int ret = segment->nextKV();

        switch (ret) {
            case 0: { /*no more data for this segment*/
                T s = core_queue.pop();
                delete s;
                break;
            }
            case 1: { /*next KV pair exist*/
                core_queue.adjustTop();
                break;
            }
            case -1: { /*break in the middle*/
                if (segment->switch_mem() ){
                    /* DBGPRINT(DBG_CLIENT, "adjust priority queue\n"); */
                    core_queue.adjustTop();
                } else {
                    T s = core_queue.pop();
                    delete s;
                }
                break;
            }
        }
    }


    int  getPassFactor(int factor, int passNo, int numSegments);
    void getSegmentDescriptors(std::list<T> &inputs,
                               std::list<T> &outputs,
                               int numDescriptors);

};

#endif

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ts=4 sw=4 hlsearch cindent expandtab 
 */