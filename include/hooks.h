#ifndef _INCLUDE_HOOKS_H_
#define _INCLUDE_HOOKS_H_

// Callback called when event occurs
typedef void (*hook_callback)(int event_type, void *data, void *cbarg);

// Hook data (used internally)
struct hook {
    void *cbarg;        // User provided callback argument
    hook_callback cb;   // Callback function
    int hook_event;     // Event for which callback should be called
    struct hook *next;  // Next hook in the list
};

// List of hooks handled by an object
struct hook_list {
    struct hook *head;
};

// Create new hook list to handle hooks
struct hook_list * hook_list_new(void);

// Free the hook list and all it's hooks
void hook_list_free(struct hook_list *list);

// Add new hook to the hook list
void hook_add(struct hook_list *list, int hevent, hook_callback cb, void *cbarg);

// Remove hook with given data from the list
void hook_remove(struct hook_list *list, int hevent, hook_callback cb, void *cbarg);

// Call all hooks for given event
void hook_list_call(struct hook_list *list, int hevent, void *data);

#endif