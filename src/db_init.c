#include <sqlite3.h>
#include <db_init.h>
#include <stdlib.h>

sqlite3 *dbg = NULL;

// Try to open database file on global connection
void db_init_global(const char *db_file_path) {
    if (sqlite3_open(db_file_path, &dbg)) {
        sys_db_crash(dbg, "Unable to start database engine");
    }
}

// Create database schema
void db_init_schema(sqlite3 *db) {

    const char sql[] = 
        "CREATE TABLE IF NOT EXISTS options ("
            "id INTEGER,"
            "key TEXT UNIQUE NOT NULL,"
            "int_value INTEGER,"
            "bin_value BLOB,"
            "text_value TEXT,"
            "PRIMARY KEY(id AUTOINCREMENT)"
        ");"
        "CREATE TABLE IF NOT EXISTS client_contacts ("
            "id INTEGER,"
            "status INTEGER,"
            "nickname TEXT,"
            "onion_address TEXT,"
            "onion_pub_key BLOB,"
            "has_mailbox INTEGER,"
            "mailbox_id BLOB,"
            "mailbox_onion TEXT,"
            "local_sig_key_pub BLOB,"
            "local_sig_key_priv BLOB,"
            "local_enc_key_pub BLOB,"
            "local_enc_key_priv BLOB,"
            "remote_sig_key_pub BLOB,"
            "remote_enc_key_pub BLOB,"
            "PRIMARY KEY(id AUTOINCREMENT)"
        ");"
        "CREATE TABLE IF NOT EXISTS client_messages ("
            "id INTEGER,"
            "global_id BLOB,"
            "contact_id INTEGER,"
            "sender INTEGER,"
            "status INTEGER,"
            "type INTEGER,"
            "body_text TEXT,"
            "body_nick TEXT,"
            "body_mbox_id BLOB,"
            "body_mbox_onion TEXT,"
            "PRIMARY KEY(id AUTOINCREMENT),"
            "FOREIGN KEY(contact_id) REFERENCES client_contacts(id) ON DELETE CASCADE"
        ");"
        "CREATE TABLE IF NOT EXISTS mailbox_keys ("
            "id INTEGER,"
            "key TEXT,"
            "uses_left INTEGER,"
            "PRIMARY KEY(id AUTOINCREMENT)"
        ");"
        "CREATE TABLE IF NOT EXISTS mailbox_accounts ("
            "id INTEGER,"
            "mailbox_id BLOB,"
            "signing_pub_key BLOB,"
            "PRIMARY KEY(id AUTOINCREMENT)"
        ");"
        "CREATE TABLE IF NOT EXISTS mailbox_contacts ("
            "id INTEGER,"
            "account_id INTEGER,"
            "signing_pub_key BLOB,"
            "PRIMARY KEY(id AUTOINCREMENT),"
            "FOREIGN KEY(account_id) REFERENCES mailbox_accounts(id) ON DELETE CASCADE"
        ");"
        "CREATE TABLE IF NOT EXISTS mailbox_messages ("
            "id INTEGER,"
            "account_id INTEGER,"
            "contact_id INTEGER,"
            "global_id BLOB,"
            "data BLOB,"
            "PRIMARY KEY(id AUTOINCREMENT),"
            "FOREIGN KEY(account_id) REFERENCES mailbox_accounts(id) ON DELETE CASCADE,"
            "FOREIGN KEY(contact_id) REFERENCES mailbox_contacts(id) ON DELETE CASCADE"
        ");"

        "PRAGMA foreign_keys = ON;"
    ;

    if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to init database schema");
}