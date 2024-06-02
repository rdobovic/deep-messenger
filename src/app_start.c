#include <array.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <constants.h>
#include <openssl/rand.h>
#include <base32.h>
#include <db_mb_key.h>
#include <sqlite3.h>
#include <onion.h>
#include <sys/stat.h>
#include <db_init.h>
#include <debug.h>
#include <helpers_crypto.h>
#include <stdint.h>
#include <db_options.h>
#include <ui_stack.h>
#include <ui_logger.h>
#include <limits.h>

#include <app.h>

// Allocate new array and and store path/subpath in it
static char * allocate_add_path(const char *path, const char *subpath);

// Start application with given cmd arguments
void app_start(struct app_data *app, int argc, char **argv) {
    int len;
    struct stat st;

    static struct option long_options[] = {
        {"help",         no_argument,       0, 'h'},
        {"mailbox",      no_argument,       0, 'm'},
        {"dir",          required_argument, 0, 'd'},
        {"port",         required_argument, 0, 'p'},
        {"mailbox-port", required_argument, 0, 'P'},
        {"tor",          required_argument, 0, 't'},
        {"manual",       no_argument,       0, 'u'},
        {"keygen",       required_argument, 0, 'g'},
        {"keys",         no_argument,       0, 'k'},
        {"keydel",       required_argument, 0, 'r'},
        {"version",      no_argument,       0, 'v'},
        {0,              0,                 0,  0 },
    };

    const char short_options[] = "hmd:p:P:t:ug:kr:v";

    int opt;
    int option_index = 0;

    int port;
    char path[PATH_MAX];
    char *access_key;
    int key_operation, key_uses;
    int custom_app_port = 0;

    uint8_t onion_pub_key[ONION_PUB_KEY_LEN];
    uint8_t onion_priv_key[ONION_PRIV_KEY_LEN];

    debug_set_fp(stdout);

    // Set default config dir path
    app->path.data_dir = array(char);
    array_strcpy(app->path.data_dir, APP_DEFAULT_DIR_PATH, -1);

    // Set default tor binary path
    app->path.tor_bin = array(char);
    array_strcpy(app->path.tor_bin, APP_DEFAULT_TOR_PATH, -1);

    // Set default application port
    app->cf.app_port = array(char);
    array_strcpy(app->cf.app_port, DEEP_MESSENGER_PORT, -1);

    // Set default mailbox port
    app->cf.mailbox_port = array(char);
    array_strcpy(app->cf.mailbox_port, DEEP_MESSENGER_MAILBOX_PORT, -1);

    while ((opt = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                // Print help message
                printf("Usage: %s [options]\n", argv[0]);
                printf("  -h, --help                Show this help message\n");
                printf("  -m, --mailbox             Run mailbox service\n");
                printf("  -d, --dir <dir>           Set config directory path\n");
                printf("  -p, --port <port>         Listen for incomming connections on this port\n");
                printf("  -P, --mailbox-port <port> This is the port mailboxes should listen on\n");
                printf("  -t, --tor <file>          Path to tor binary (default: /bin/tor)\n");
                printf("  -u, --manual              Run messenger in manual mode (for debugging)\n");
                printf("  -g, --keygen <uses>       Generate new mailbox access key\n");
                printf("  -k, --keys                Show list of all available mailbox access keys\n");
                printf("  -r, --keydel <key>        Delete given mailbox access key\n");
                printf("  -v, --version             Show application version\n");
                exit(EXIT_SUCCESS);
                break;

            case 'm':
                // App is running as mailbox service
                app->cf.is_mailbox = 1;
                break;

            case 'd':
                // Set config dir path
                array_strcpy(app->path.data_dir, optarg, -1);
                break;

            case 'p':
                // Set app listening port
                if (sscanf(optarg, "%d", &port) != 1 || (port <= 0 || port >= 65535)) {
                    printf("Invalid port number provided\n");
                    exit(EXIT_FAILURE);
                }
                array_strcpy(app->cf.app_port, optarg, -1);
                break;

            case 'P':
                // Set mailbox listening port
                if (sscanf(optarg, "%d", &port) != 1 || (port <= 0 || port >= 65535)) {
                    printf("Invalid port number provided\n");
                    exit(EXIT_FAILURE);
                }
                array_strcpy(app->cf.mailbox_port, optarg, -1);
                break;

            case 't':
                // Set tor binary path
                array_strcpy(app->path.tor_bin, optarg, -1);
                break;

            case 'u':
                // Run app in manual mode
                app->cf.manual_mode = 1;
                break;

            case 'g':
                // Generate new mailbox access key
                if (sscanf(optarg, "%d", &key_uses) != 1 || key_uses <= 0) {
                    printf("Failed to generate access key, invalid number of uses provided\n");
                    exit(EXIT_FAILURE);
                };
                key_operation = opt;
                break;
            
            case 'k':
                // List all mailbox access
                key_operation = opt;
                break;
            
            case 'r':
                // Delete (remove) given mailbox access key
                key_operation = opt;
                access_key = array(char);
                array_strcpy(access_key, optarg, -1);
                break;
            
            case 'v':
                // Print app version
                printf("Deep Messenger version %s (protocol v%d)\n", 
                    DEEP_MESSENGER_APP_VERSION, DEEP_MESSENGER_PROTOCOL_VER);
                printf("Made by rdobovic\n");
                exit(EXIT_SUCCESS);
                break;

            case '?':
                exit(EXIT_FAILURE);
                break;
            
            default:
                exit(EXIT_FAILURE);
        }
    }

    // Generate all needed paths
    app->path.torrc = allocate_add_path(app->path.data_dir, APP_TORRC_FILE);
    app->path.onion_dir = allocate_add_path(app->path.data_dir, APP_ONION_DIR);
    app->path.db_file = allocate_add_path(app->path.data_dir, APP_DATABASE_FILE);

    // Resolve data directory path
    realpath(app->path.data_dir, path);
    array_strcpy(app->path.data_dir, path, -1);

    // Create data directory if not existing
    if (stat(app->path.data_dir, &st) == -1 && mkdir(app->path.data_dir, 0751)) {
        printf("Failed to create config directory %s\n", app->path.data_dir);
        perror("mkdir");
        exit(EXIT_FAILURE);
    }

    // Resolve other paths
    realpath(app->path.torrc, path);
    array_strcpy(app->path.torrc, path, -1);

    realpath(app->path.db_file, path);
    array_strcpy(app->path.db_file, path, -1);
    
    realpath(app->path.onion_dir, path);
    array_strcpy(app->path.onion_dir, path, -1);

    realpath(app->path.tor_bin, path);
    array_strcpy(app->path.tor_bin, path, -1);

    // Create hidden service directory if not existing
    if (stat(app->path.onion_dir, &st) == -1 && mkdir(app->path.onion_dir, 0700)) {
        printf("Failed to create hidden service directory %s\n", app->path.onion_dir);
        perror("mkdir");
        exit(EXIT_FAILURE);
    }

    // Try to open the database
    if (sqlite3_open(app->path.db_file, &(app->db))) {
        sys_db_crash(app->db, "Unable to start database engine");
    }
    // Setup database tables
    db_init_schema(app->db);

    // List all available mailbox access keys
    if (key_operation == 'k') {
        int i, n;
        struct db_mb_key **keys;

        printf("All available mailbox keys:\n");
        keys = db_mb_key_get_all(app->db, &n);
        for (i = 0; i < n; i++) {
            char encoded_key[BASE32_ENCODED_LEN(MAILBOX_ACCESS_KEY_LEN)];
            len = base32_encode(keys[i]->key, MAILBOX_ACCESS_KEY_LEN, encoded_key, 0);
            encoded_key[len] = '\0';
            printf("  - %s (uses left: %d)\n", encoded_key, keys[i]->uses_left);
        }
        printf("\nTotal %d\n", n);
        exit(EXIT_SUCCESS);
    }

    // Generate new mailbox access key
    if (key_operation == 'g') {
        struct db_mb_key *key;
        char encoded_key[BASE32_ENCODED_LEN(MAILBOX_ACCESS_KEY_LEN)];

        key = db_mb_key_new();
        printf("Generating new mailbox service access key (length: %d)\n", MAILBOX_ACCESS_KEY_LEN);
        RAND_bytes(key->key, MAILBOX_ACCESS_KEY_LEN);
        key->uses_left = key_uses;
        db_mb_key_save(app->db, key);
        len = base32_encode(key->key, MAILBOX_ACCESS_KEY_LEN, encoded_key, 0);
        encoded_key[len] = '\0';

        printf("access key: %s (uses: %d)\n\n", encoded_key, key->uses_left);
        printf("Key saved in config directory: %s\n", app->path.data_dir);
        db_mb_key_free(key);
        exit(EXIT_SUCCESS);
    }

    // Delete given mailbox access key
    if (key_operation == 'r') {
        uint8_t *raw_key;
        struct db_mb_key *key;

        len = strlen(access_key);
        if (!base32_valid(access_key, len)) {
            printf("Failed to decode, given key is invalid\n");
            exit(EXIT_FAILURE);
        }

        raw_key = array(uint8_t);
        array_expand(raw_key, BASE32_DECODED_LEN(len));

        len = base32_decode(access_key, len, raw_key);
        if (len != MAILBOX_ACCESS_KEY_LEN) {
            printf("When decoded, given key does not have valid key length (%d)\n", 
                MAILBOX_ACCESS_KEY_LEN);
            exit(EXIT_FAILURE);
        }

        key = db_mb_key_get_by_key(app->db, raw_key, NULL);
        if (!key) {
            printf("Given key does not exist in the mailbox database\n");
            exit(EXIT_FAILURE);
        }
        db_mb_key_delete(app->db, key);

        printf("Successfully removed following mailbox access key from the database:\n");
        printf("  - %s (uses left: %d)\n", access_key, key->uses_left);
        db_mb_key_free(key);
        array_free(raw_key);
        array_free(access_key);
        exit(EXIT_SUCCESS);
    }

    // Get or generate onion keys
    if (!db_options_is_defined(app->db, "onion_public_key", DB_OPTIONS_BIN) || 
        !db_options_is_defined(app->db, "onion_private_key", DB_OPTIONS_BIN)
    ) {
        ed25519_keygen(onion_pub_key, onion_priv_key);
        db_options_set_bin(app->db, "onion_public_key", onion_pub_key, ONION_PUB_KEY_LEN);
        db_options_set_bin(app->db, "onion_private_key", onion_priv_key, ONION_PRIV_KEY_LEN);
    } else {
        db_options_get_bin(app->db, "onion_public_key", onion_pub_key, ONION_PUB_KEY_LEN);
        db_options_get_bin(app->db, "onion_private_key", onion_priv_key, ONION_PRIV_KEY_LEN);
    }

    // Set default nickname if not defined
    if (!db_options_is_defined(app->db, "client_nickname", DB_OPTIONS_TEXT)) {
        strcpy(app->nickname, APP_DEFAULT_NICKNAME);
        db_options_set_text(app->db, "client_nickname", APP_DEFAULT_NICKNAME, -1);
    } else {
        db_options_get_text(app->db, "client_nickname", app->nickname, CLIENT_NICK_MAX_LEN);
    }

    // Generate onion domain from onion keys
    onion_address_from_pub_key(onion_pub_key, app->onion_address);
    app->onion_address[ONION_ADDRESS_LEN] = '\0';
    db_options_set_text(app->db, "onion_address", app->onion_address, ONION_ADDRESS_LEN);

    // Generate files used by hidden service
    if (!onion_init_dir(app->path.onion_dir, onion_pub_key, onion_priv_key)) {
        printf("Failed to initilize hidden service directory");
        exit(EXIT_FAILURE);
    }

    // If this is not mailbox start UI
    if (!app->cf.is_mailbox) {
        debug_init();

        app_ui_init(app);
        app_ui_define(app);
        app_ui_add_titles(app);
        ui_stack_redraw(app->ui.stack);

        ui_logger_log(app->ui.shell, " ____                    __  __");
        ui_logger_log(app->ui.shell, "|  _ \\  ___  ___ _ __   |  \\/  | ___  ___ ___  ___ _ __   __ _  ___ _ __");
        ui_logger_log(app->ui.shell, "| | | |/ _ \\/ _ \\ '_ \\  | |\\/| |/ _ \\/ __/ __|/ _ \\ '_ \\ / _` |/ _ \\ '__|");
        ui_logger_log(app->ui.shell, "| |_| |  __/  __/ |_) | | |  | |  __/\\__ \\__ \\  __/ | | | (_| |  __/ |");
        ui_logger_log(app->ui.shell, "|____/ \\___|\\___| .__/  |_|  |_|\\___||___/___/\\___|_| |_|\\__, |\\___|_|");
        ui_logger_log(app->ui.shell, "                |_|                                      |___/");
        ui_logger_printf(app->ui.shell, "\nVersion %s\nMade by rdobovic\n", DEEP_MESSENGER_APP_VERSION);
        ui_logger_printf(app->ui.shell, "Type help for the list of commands\n");
    } else {
        printf("Starting Deep Messenger mailbox service\n");
        printf("  Version:         %s\n", DEEP_MESSENGER_APP_VERSION);
        printf("  Protocol:        Deep Messenger protocol %d\n", DEEP_MESSENGER_PROTOCOL_VER);
        printf("  Mailbox address: %s\n", app->onion_address);
        printf("  Public port:     %s\n\n", app->cf.mailbox_port);
    }

    // Init libevent and eventloop
    app_event_init(app);

    if (!app->cf.manual_mode) {
        app_tor_start(app);
    }

    //
    // START TOR, DO SOMETHING...
    //

    // Start event loop
    app_event_start(app);
}

// Exit application (do needed exit procedures)
void app_end(struct app_data *app) {
    if (!app->cf.is_mailbox)
        app_ui_end(app);

    app_tor_end(app);
    app_event_end(app);
    printf("\nStopped Deep Messenger\n");
    exit(EXIT_SUCCESS);
}

// Allocate new array and and store path/subpath in it
static char * allocate_add_path(const char *path, const char *subpath) {
    int len;
    char *result;

    len = snprintf(NULL, 0, "%s/%s", path, subpath) + 1;
    result = array(char);
    array_expand(result, len);
    snprintf(result, len, "%s/%s", path, subpath);
    return result;
}