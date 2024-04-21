#include <helpers.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <debug.h>

enum divide_units {
    FIX, PER
};

struct divide_field {
    char type;
    int size;
    int real_size;
    enum divide_units unit;
};

int divide(const char *format, int full_length, ...) {
    int i, n;                        // Helper variables
    int field = -1, n_fields;        // Field index and number of fields
    int fixed_sum = 0, per_sum = 0;  // Sum of all fixed fields and all percentages
    int per_space;                   // Size of all percentages together
    int last_per = -1;               // Index of last percentage field
    int per_sum_space = 0;           // Sum of all percentage sizes

    va_list args;

    // Field structures 
    struct divide_field fields[DIVIDE_HELPER_MAX_DIVS];

    n = strlen(format);
    va_start(args, full_length);

    for (i = 0; i < n; i++) {
        if (format[i] == 'f' || format[i] == 'p') {
            ++field;
            fields[field].type = format[i];
            fields[field].size = 0;
            fields[field].real_size = 0;
            continue;
        }
        if (isdigit(format[i]) && field >= 0) {
            fields[field].size *= 10;
            fields[field].size += format[i] - '0';

            if (fields[field].type == 'f')
                fields[field].real_size = fields[field].size;
            continue;
        }

        return 1;
    }

    n_fields = field + 1;

    for (i = 0; i < n_fields; i++) {
        if (fields[i].type == 'f') {
            fixed_sum += fields[i].size;
        } else {
            per_sum += fields[i].size;
        }
    }

    if (per_sum > 100 || fixed_sum > full_length)
        return 1;

    per_space = full_length - fixed_sum;

    for (i = 0; i < n_fields; i++) {
        if (fields[i].type == 'p') {
            fields[i].real_size = per_space * fields[i].size / 100;
            last_per = i;
            per_sum_space += fields[i].real_size;
        }
    }

    if (per_sum_space > per_space || per_sum == 100)
        fields[last_per].real_size += per_space - per_sum_space;

    for (i = 0; i < n_fields; i++) {
        int *pot = va_arg(args, int*);
        
        *pot = fields[i].real_size;
    }

    va_end(args);
    return 0;
}

int str_ends_with(const char *str, const char *end) {
    int is, ie;
    size_t str_len = strlen(str);
    size_t end_len = strlen(end);

    if (str_len < end_len)
        return 0;

    is = str_len - 1;
    ie = end_len - 1;

    while (ie >= 0 && str[is] == end[ie]) {
        --is; --ie;
    }

    if (ie >= 0)
        return 0;

    return 1;
}