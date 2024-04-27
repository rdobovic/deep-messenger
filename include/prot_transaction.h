#ifndef _INCLUDE_PROT_TRANSACTION_H_
#define _INCLUDE_PROT_TRANSACTION_H_

#include <stdint.h>
#include <prot_main.h>
#include <constants.h>

/**
 * Transaction REQUEST message
 */

// Transaction request data structure
struct prot_txn_req {
    struct prot_recv_handler hrecv;
    struct prot_tran_handler htran;
};

// Allocate new prot transaction request handler
struct prot_txn_req * prot_txn_req_new(void);

// Free given transaction request handler
void prot_txn_req_free(struct prot_txn_req *msg);

// Prepare transsion handler for given message
struct prot_tran_handler * prot_txn_req_htran(struct prot_txn_req *msg);

// Prepare receive for given message
struct prot_recv_handler * prot_txn_req_hrecv(struct prot_txn_req *msg);


/**
 * Transaction RESPONSE message
 */

// Transaction response data structure
struct prot_txn_res {
    struct prot_recv_handler hrecv;
    struct prot_tran_handler htran;

    uint8_t txn_id[TRANSACTION_ID_LEN];
};

// Allocate new prot transaction response header
struct prot_txn_res * prot_txn_res_new(void);

// Free given transaction response header
void prot_txn_res_free(struct prot_txn_res *msg);

// Prepare transsion handler for given message
struct prot_tran_handler * prot_txn_res_htran(struct prot_txn_res *msg);

// Prepare receive for given message
struct prot_recv_handler * prot_txn_res_hrecv(struct prot_txn_res *msg);


#endif