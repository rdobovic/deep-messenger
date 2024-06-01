#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <app.h>

int main(int argc, char **argv) {
    struct app_data app;
    memset(&app, 0, sizeof(app));

    app_start(&app, argc, argv);
    return 0;
}