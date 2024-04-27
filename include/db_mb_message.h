#ifndef _INCLUDE_DB_MB_MESSAGE_H_
#define _INCLUDE_DB_MB_MESSAGE_H_

#include <stdint.h>
#include <sqlite3.h>
#include <db_message.h>
#include <db_mb_account.h>
#include <constants.h>

#define DB_MB_MESSAGE_CHUNK_SIZE 63

struct db_mb_message {
    int id;
    int account_id;
    int contact_id;
    uint8_t global_id[MESSAGE_ID_LEN];

    uint8_t *data;
    int data_len;
    int data_n_chunks;
};

// Create new empty mailbox message object
struct db_mb_message * db_mb_message_new(void);

// Free given mailbox message object, if you want to save it
// call the save function first
void db_mb_message_free(struct db_mb_message *msg);

// Set message content
void db_mb_message_set_data(struct db_mb_message *msg, const uint8_t *data, int data_len);

// Save changes on given object to database
void db_mb_message_save(sqlite3 *db, struct db_mb_message *msg);

// Delete given message from the database
void db_mb_message_delete(sqlite3 *db, struct db_mb_message *msg);

// Pull new data from the database
void db_mb_message_refresh(sqlite3 *db, struct db_mb_message *msg);

// Get mailbox message by id
struct db_mb_message * db_mb_message_get_by_pk(sqlite3 *db, int id, struct db_mb_message *dest);

// Get mailbox message by associated account and global message id
struct db_mb_message * db_mb_message_get_by_acc_and_gid(
    sqlite3 *db, struct db_mb_account *acc, uint8_t *gid, struct db_mb_message *dest
);

// Get all mailbox messages for given account
struct db_mb_message ** db_mb_message_get_all(sqlite3 *db, struct db_mb_account *acc, int *n);

// Free list of messages returned by get_all
void db_mb_message_free_all(struct db_mb_message **msgs, int n);

#endif