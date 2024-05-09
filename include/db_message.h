#ifndef _INCLUDE_DB_MESSAGE_H_
#define _INCLUDE_DB_MESSAGE_H_

#include <onion.h>
#include <db_contact.h>
#include <stdint.h>
#include <constants.h>

#define DB_MESSAGE_TEXT_CHUNK 32

enum db_message_types {
    DB_MESSAGE_TEXT = 0x01,
    DB_MESSAGE_NICK = 0x02,
    DB_MESSAGE_MBOX = 0x03,
    
    // Virtual type, it is only sent but never saved
    // to the database
    DB_MESSAGE_RECV = 0x04,
};

enum db_message_status {
    // Messages waiting to me sent
    DB_MESSAGE_STATUS_UNDELIVERED,
    // For messages I sent
    DB_MESSAGE_STATUS_SENT,
    DB_MESSAGE_STATUS_SENT_CONFIRMED,
    // For messages I received
    DB_MESSAGE_STATUS_RECV,
    DB_MESSAGE_STATUS_RECV_CONFIRMED,
};

enum db_message_sender {
    DB_MESSAGE_SENDER_ME,
    DB_MESSAGE_SENDER_FRIEND,
};

struct db_message {
    int id;
    int contact_id;
    enum db_message_sender sender;
    enum db_message_status status;

    enum db_message_types type;
    uint8_t global_id[MESSAGE_ID_LEN];

    char *body_text;
    int body_text_len;
    int body_text_n_chunks;

    int body_nick_len;
    char body_nick[CLIENT_NICK_MAX_LEN];

    uint8_t body_recv_id[MESSAGE_ID_LEN];

    uint8_t body_mbox_id[MAILBOX_ID_LEN];
    uint8_t body_mbox_onion[ONION_ADDRESS_LEN + 1];
};

// Create new empty message object
struct db_message * db_message_new(void);
// Free given message object, note that if you want to save changes you
// made to the object you must first save it
void db_message_free(struct db_message *msg);

// Save all message data into database
void db_message_save(sqlite3 *db, struct db_message *msg);

// Delete given message
void db_message_delete(sqlite3 *db, struct db_message *msg);

// Pull new data from the database
void db_message_refresh(sqlite3 *db, struct db_message *msg);

// Write text_len characters of text into message text body
void db_message_set_text(struct db_message *msg, const char *text, int text_len);

// Generate random global message ID
void db_message_gen_id(struct db_message *msg);

// They return NULL when not found, if dest is not NULL functions will copy
// data from the database into dest, otherwise they will allocate new structure

// Get message by local id
struct db_message * db_message_get_by_pk(sqlite3 *db, int id, struct db_message *dest);
// Get message by global message id
struct db_message * db_message_get_by_gid(sqlite3 *db, uint8_t *gid, struct db_message *dest);
// Get last message for given contact
struct db_message * db_message_get_last(sqlite3 *db, struct db_contact *cont, struct db_message *dest);
// Get message before the given message for the same contact
struct db_message * db_message_get_before(sqlite3 *db, struct db_message *current_msg, struct db_message *dest);

#endif