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

int main(void) {
    sqlite3 *db;
    int conts_n, i;
    struct db_contact *cont;
    struct db_contact **conts;
    struct db_message *msg;
    struct db_mb_account *acc;

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

    sqlite3_close(dbg);
}