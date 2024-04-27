#include <sqlite3.h>
#include <db_contact.h>
#include <db_options.h>
#include <stdio.h>
#include <debug.h>
#include <sys_crash.h>
#include <string.h>
#include <stdint.h>
#include <db_message.h>
#include <db_mb_account.h>
#include <db_mb_key.h>
#include <base32.h>
#include <db_mb_contact.h>
#include <db_mb_message.h>

int main(void) {
    sqlite3 *db;
    int conts_n, i, keys_n;
    struct db_contact *cont;
    struct db_contact **conts;
    struct db_message *msg;
    struct db_mb_key *key;
    struct db_mb_key **keys;

    struct db_mb_account *acc;
    struct db_mb_contact *mcont;
    struct db_mb_message *mmsg;

    uint8_t gid[] = { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, };
    

    char arr[10] = "addr", arr2[10];

    //debug_init();
    debug_set_fp(stdout);
    db_init_global("deep_messenger.db");
    db_init_schema(dbg);

    debug("Testing options: ");

    db_options_set_int(dbg, "test_int", 976);
    db_options_set_bin(dbg, "test_bin", "RNDBIN12", 8);
    db_options_set_text(dbg, "test_txt", "This is some text", -1);

    int a;
    char b[25], c[25];

    a = db_options_get_int(dbg, "test_int");
    db_options_get_bin(dbg, "test_bin", b, 25);
    db_options_get_text(dbg, "test_txt", c, 25);

    b[8] = '\0';
    debug("%d %s %s\n", a, b, c);

    cont = db_contact_new();

    //cont = db_contact_get_by_pk(dbg, 10);

    cont->status = DB_CONTACT_ACTIVE;
    cont->has_mailbox = 1;
    strcpy(cont->nickname, "rdobovic");
    strcpy(cont->onion_address, "i4mcwgorejxtforxrd7dsf73hsiiphhlgxxz3aeuef3hixdcv4vg3bid.onion");
    strcpy(cont->mailbox_onion, "i4mcwgorejxtforxrd7dsf73hsiiphhlgxxz3aeuef3hixdcv4vg3bid.onion");
    db_contact_save(dbg, cont);

    //strcpy(cont->nickname, "rdobovic122");
    //db_contact_save(dbg, cont);

    debug("nick: %s", cont->nickname);

    conts = db_contact_get_all(dbg, &conts_n);

    debug("Contacts:");
    for (i = 0; i < conts_n; i++) {
        debug("- [%d] %s", conts[i]->id, conts[i]->nickname);
        //db_contact_delete(dbg, conts[i]);
    }
    debug("Done");

    msg = db_message_new();

    msg->contact_id = cont->id;
    msg->type = DB_MESSAGE_TEXT;
    msg->status = DB_MESSAGE_RECV_CONFIRMED;
    msg->sender = DB_MESSAGE_SENDER_FRIEND;
    db_message_set_text(msg, "Ola", -1);
    db_message_save(dbg, msg);
    db_message_free(msg);

    msg = db_message_get_by_gid(dbg, gid, NULL);
    debug("MSG: %s", msg == NULL ? "NONE" : msg->body_text);
    db_message_free(msg);
    db_contact_free(cont);

    cont = db_contact_get_by_pk(dbg, 3, NULL);
    msg = db_message_get_last(dbg, cont, NULL);

    if (cont && msg)
        debug("[%s] %s", cont->nickname, msg->body_text);

    if (cont)
        db_contact_delete(dbg, cont);

    db_contact_free_all(conts, conts_n);

    acc = db_mb_account_new();

    acc->mailbox_id[0] = 0x25;
    db_mb_account_save(dbg, acc);
    db_mb_account_refresh(dbg, acc);
    db_mb_account_free(acc);

    acc = db_mb_account_get_by_pk(dbg, 3, NULL);
    if (acc) {
        debug("Some MBID: %x", acc->mailbox_id[0]);
        db_mb_account_save(dbg, acc);
        db_mb_account_delete(dbg, acc);
    }

    key = db_mb_key_new();

    key->uses_left = 10;
    key->key[0] = 0xBB;
    key->key[DB_MB_KEY_LEN - 1] = 0xBB;

    db_mb_key_save(dbg, key);

    keys = db_mb_key_get_all(dbg, &keys_n);

    debug("Key list: ");
    for (i = 0; i < keys_n; i++) {
        int len;
        char key_encoded[BASE32_ENCODED_LEN(DB_MB_KEY_LEN) + 1];
        len = base32_encode(keys[i]->key, DB_MB_KEY_LEN, key_encoded, 0);
        key_encoded[len] = '\0';

        debug("- [%03d] key(%s) uses_left(%d)", keys[i]->id, key_encoded, keys[i]->uses_left);
    }

    acc = db_mb_account_new();
    acc->mailbox_id[0] = 0xAA;
    acc->mailbox_id[15] = 0xBB;
    db_mb_account_save(dbg, acc);

    mcont = db_mb_contact_new();
    mcont->account_id = acc->id;
    mcont->signing_pub_key[0] = 0xCC;
    mcont->signing_pub_key[31] = 0xDD;
    db_mb_contact_save(dbg, mcont);

    mmsg = db_mb_message_new();
    mmsg->account_id = acc->id;
    mmsg->contact_id = mcont->id;
    mmsg->global_id[0] = 0x11;
    mmsg->global_id[15] = 0x22;
    db_mb_message_set_data(mmsg, "This is text", 12);
    db_mb_message_save(dbg, mmsg);

    db_mb_account_free(acc);
    db_mb_contact_free(mcont);
    db_mb_message_free(mmsg);

    sqlite3_close(dbg);
}