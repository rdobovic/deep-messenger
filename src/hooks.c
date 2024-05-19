#include <hooks.h>
#include <stdlib.h>
#include <sys_memory.h>

// Create new hook list to handle hooks
struct hook_list * hook_list_new(void) {
    struct hook_list *list;

    list = safe_malloc(sizeof(struct hook_list),
        "Failed to allocate hook list");

    list->head = NULL;
    return list;
}

// Free the hook list and all it's hooks
void hook_list_free(struct hook_list *list) {
    struct hook *hk;

    // Free all hooks in the list
    hk = list->head;
    while (hk != NULL) {
        struct hook *hk_free = hk;

        hk = hk->next;
        free(hk_free);
    }

    free(list);
}

// Add new hook to the hook list
void hook_add(struct hook_list *list, int hevent, hook_callback cb, void *cbarg) {
    struct hook *hk;

    hk = safe_malloc(sizeof(struct hook), "Failed to allocate hook");
    hk->cb = cb;
    hk->cbarg = cbarg;
    hk->hook_event = hevent;
    hk->next = NULL;

    if (list->head == NULL) {
        list->head = hk;
    } else {
        struct hook *last_hk;

        // Find the last hook in the list
        for (last_hk = list->head; last_hk->next != NULL; last_hk = last_hk->next)
            /* Do nothing */;

        last_hk->next = hk;
    }
}

// Remove hook with given data from the list
void hook_remove(struct hook_list *list, int hevent, hook_callback cb, void *cbarg) {
    struct hook *hk;

    hk = list->head;
    // If the first hook is the one
    if (hk->hook_event == hevent && hk->cb == cb && hk->cbarg == cbarg) {
        list->head = hk->next;
        free(hk);
        return;
    }

    // Search for matching hook in the list
    for (; hk->next != NULL; hk = hk->next) {
        struct hook *hkn = hk->next;

        if (hkn->hook_event == hevent && hkn->cb == cb && hkn->cbarg == cbarg) {
            hk->next = hkn->next;
            free(hkn);
            return;
        }
    }
}

// Call all hooks for given event
void hook_list_call(struct hook_list *list, int hevent, void *data) {
    struct hook *hk;

    for (hk = list->head; hk != NULL; hk = hk->next) {
        if (hk->hook_event == hevent) {
            hk->cb(hevent, data, hk->cbarg);
        }
    }
}