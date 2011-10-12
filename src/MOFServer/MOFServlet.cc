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

#include <dirent.h>

#include "IOUtility.h"
#include "MOFServlet.h"

using namespace std;

/* Parse param into a shuffle_req_t */
shuffle_req_t* get_shuffle_req(const string &param)
{
    size_t start, end;
    shuffle_req_t *sreq;
    
    start = end = 0;
    sreq = new shuffle_req_t();

    end = param.find(':');
    sreq->m_jobid = param.substr(0, end);

    start = ++end;
    end = param.find(':', start);
    sreq->m_map = param.substr(start, end - start);

    start = ++end;
    end = param.find(':', start);
    sreq->map_offset = atoi(param.substr(start, end - start).c_str());

    start = ++end;
    end = param.find(':', start);
    sreq->reduceID = atoi(param.substr(start, end - start).c_str());

    start = ++end;
    end = param.find(':', start);
    sreq->remote_addr = atoll(param.substr(start, end - start).c_str());

    return sreq;
}


OutputServer::OutputServer(int data_port, int mode, 
                           supplier_state_t *state)
{
    this->data_port = data_port;
    this->rdma = NULL; 
    this->tcp  = NULL;
    this->state = state;
    INIT_LIST_HEAD(&this->incoming_req_list);

    pthread_mutex_init(&this->in_lock, NULL);
    pthread_mutex_init(&this->out_lock, NULL);
    pthread_cond_init(&this->in_cond, NULL);
    /* if (mode == STANDALONE) {
        list_fet_req(path);
    } */
}

OutputServer::~OutputServer()
{
	output_stdout("OutputServer: D'tor");
    pthread_mutex_destroy(&this->in_lock);
    pthread_mutex_destroy(&this->out_lock);
    pthread_cond_destroy(&this->in_cond);
}


void OutputServer::start_server()
{
    this->rdma = new RdmaServer(this->data_port, this->state);
    this->rdma->start_server();
}

void OutputServer::stop_server()
{
    this->rdma->stop_server();
    delete this->rdma;
}

void OutputServer::insert_incoming_req(shuffle_req_t *req)
{
    pthread_mutex_lock(&in_lock);

    /* testing section */
    /* if (req->map_offset == 0) {
        int reduceid = req->reduceID;
        map<int, int>::iterator iter =
            recv_stat.find(reduceid);
        if (iter != recv_stat.end()) {
            recv_stat[reduceid] = (*iter).second + 1;
        } else {
            recv_stat[reduceid] = 1;
        }
        output_stdout("reducer : %d, First mop recv: %d",
                      reduceid, recv_stat[reduceid]);
    } */

    list_add_tail(&req->list, &incoming_req_list);

    pthread_cond_broadcast(&in_cond);
    pthread_mutex_unlock(&in_lock);
}

void OutputServer::start_outgoing_req(shuffle_req_t *req, index_record_t* record,  chunk_t *chunk, uint64_t length, int offsetAligment)
{
    int send_bytes, len;
    char ack[NETLEV_FETCH_REQSIZE];
    uint64_t req_size;
    uintptr_t local_addr;

    req_size   = length;
    local_addr = (uintptr_t)(chunk->buff + offsetAligment);

    send_bytes = this->rdma->rdma_write_mof(req->conn, 
                                            local_addr, 
                                            req_size, 
                                            req->remote_addr, (void*)chunk);

    len = sprintf(ack, "%ld:%ld:%d:", 
                  record->rawLength,
                  record->partLength,
                  send_bytes); 

    /* bool prefetch = req_size > send_bytes; */
    shuffle_req_t *prefetch_req = NULL;
    /* if (prefetch) {
        prefetch_req = req;
        prefetch_req->prefetch = true;
    } */

    this->rdma->send_msg(ack, len + 1, (uint64_t)req->peer_wqe,
                         (void *)chunk, req->conn, prefetch_req);
    
    /* testing section */
    /* if (req->map_offset == 0) {
        int reduceid = req->reduceID;
        map<int, int>::iterator iter =
            out_stat.find(reduceid);
        if (iter != out_stat.end()) {
            out_stat[reduceid] = (*iter).second + 1;
        } else {
            out_stat[reduceid] = 1;
        }
        output_stdout("reducer : %d, First mop return: %d",
                      reduceid, out_stat[reduceid]);
    } */
}

void OutputServer::clean_job()
{
    recv_stat.erase(recv_stat.begin(), recv_stat.end());
    out_stat.erase (out_stat.begin(),  out_stat.end());
    output_stdout("JOB OVER *****************************");
}


/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ts=4 sw=4 hlsearch cindent expandtab 
 */