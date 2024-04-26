#ifndef _INCLUDE_DB_MB_ACCOUNT_
#define _INCLUDE_DB_MB_ACCOUNT_

#include <db_contact.h>

struct db_mb_account {
    int id;
    uint8_t mailbox_id[DB_CONTACT_MAILBOX_ID_LEN];
    uint8_t signing_pub_key[DB_CONTACT_SIG_KEY_PUB_LEN];
};

#endif