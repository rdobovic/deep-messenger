#include <sqlite3.h>
#include <db_options.h>
#include <stdio.h>
#include <debug.h>
#include <sys_crash.h>

int main(void) {
    sqlite3 *db;
    char arr[10] = "addr", arr2[10];

    debug_init();
    db_init_global("deep_messenger.db");

    dbd_init_schema(db_global_database);

    debug("Something");

    //sys_crash("main", "something %d :)", 11, 0, 11);

    /*if (sqlite3_open("deep_messenger.db", &db)) {
        printf("Fail...");
        return 1;
    };*/

    //printf("Option defined: %d\n", db_options_is_defined("a", DB_OPTIONS_INT));

    db_options_set_int("test_int", 976);
    db_options_set_bin("test_bin", "RNDBIN12", 8);
    db_options_set_text("test_txt", "This is some text", -1);


    
    int a;
    char b[25], c[25];

    a = db_options_get_int("test_int");
    db_options_get_bin("test_bin", b, 25);
    db_options_get_text("test_txt", c, 25);

    b[8] = '\0';

    printf("%d %s %s\n", a, b, c);

    //db_options_set_bin("onion_public_key", arr, 5);
    //int a = db_options_get_bin("onion_public_key", arr2, 10);
    //arr2[a] = '\0';

    //printf("TEST: %s %d\n", arr2, a);

    sqlite3_close(db);
}