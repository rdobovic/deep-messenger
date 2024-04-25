#include <sqlite3.h>
#include <db_contact.h>
#include <db_options.h>
#include <stdio.h>
#include <debug.h>
#include <sys_crash.h>
#include <string.h>

int main(void) {
    sqlite3 *db;
    struct db_contact *cont;

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

    //cont = db_contact_new();

    cont = db_contact_get_by_pk(dbg, 10);

    debug("nick: %s", cont->nickname);

    //cont->status = DB_CONTACT_ACTIVE;
    //strcpy(cont->nickname, "rdobovic");
    //strcpy(cont->onion_address, "i4mcwgorejxtforxrd7dsf73hsiiphhlgxxz3aeuef3hixdcv4vg3bid.onion");
    //db_contact_save(dbg, cont);

    strcpy(cont->nickname, "rdobovic122");
    db_contact_save(dbg, cont);

    sqlite3_close(dbg);
}