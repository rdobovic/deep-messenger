#ifndef _INCLUDE_HELPERS_H_
#define _INCLUDE_HELPERS_H_

// Return smaller number
#define min(a, b) (((a) < (b)) ? (a) : (b))

// Return larger number
#define max(a, b) (((a) > (b)) ? (a) : (b))

// Draw box (border) on the window using and apply given
// attributes to the border
#define box_attr(win, attr) \
    wborder((win), \
        ACS_VLINE    | (attr), \
        ACS_VLINE    | (attr), \
        ACS_HLINE    | (attr), \
        ACS_HLINE    | (attr), \
        ACS_ULCORNER | (attr), \
        ACS_URCORNER | (attr), \
        ACS_LLCORNER | (attr), \
        ACS_LRCORNER | (attr)  \
    )

// Macro to calculate value of Ctrl + key combination
#define KEY_CTRL(ch) ((ch) & 0x1F)

// Maximum number of divisions supported by divide function
#define DIVIDE_HELPER_MAX_DIVS 50

// Divide given width to percentages and fixed widths
int divide(const char *format, int full_length, ...);

// Check if given string ends with given end (string)
int str_ends_with(const char *str, const char *end);

#endif