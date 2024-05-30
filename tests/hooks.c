#include <hooks.h>
#include <debug.h>

void print_info(int hevent, void *data, void *cbarg) {
    debug("HOOK CB: d(%s) a(%s)", (char *)data, (char *)cbarg);
}

int main() {
    int *arr;
    struct hook_list *hooks;

    char str[] = "This is str";

    debug_set_fp(stdout);

    hooks = hook_list_new();

    hook_list_call(hooks, 2, "BEF");

    hook_add(hooks, 1, print_info, "A1");
    hook_add(hooks, 2, print_info, "A2");
    hook_add(hooks, 2, print_info, str);

    hook_list_call(hooks, 2, "MSG");
    hook_list_call(hooks, 1, "SOM");

    hook_remove(hooks, 2, print_info, str);
    hook_list_call(hooks, 2, "MSG");

    hook_list_free(hooks);
    
    return 0;
}