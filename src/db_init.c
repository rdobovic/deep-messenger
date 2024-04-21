#include <sqlite3.h>
#include <db_init.h>
#include <stdlib.h>

sqlite3 *db_global_database = NULL;

void db_init_global(const char *db_file_path) {
    if (sqlite3_open(db_file_path, &db_global_database)) {
        sys_db_crash(db_global_database, "Unable to start database engine");
    }
}