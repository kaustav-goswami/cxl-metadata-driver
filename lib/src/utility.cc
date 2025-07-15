#include "utility.hh"


void
warn(const char* what, ...) {
    va_list args;
    va_start(args, what);
    printf("warn: ");
    while (*what) {
        if (*what == '%') {
            what++;
            if (*what == 'd') {
                int i = va_arg(args, int);
                printf("%d", i);
            }
            else if (*what == 'p') {
                // Print a pointer
                void *p = va_arg(args, void *);
                printf("%p", p);
            }
            else if 
            (*what == 'l' && *(what + 1) == 'u' && *(what + 2) == 's') {
                what += 2;
                unsigned long ul = va_arg(args, unsigned long);
                char *s = va_arg(args, char *);
                printf("%lu%s", ul, s);
            }
            else {
                putchar('%');
                putchar(*what);
            }
        }
        else {
            putchar(*what);
        }
        what++;
    }
    printf("\n");
}

void
fatal(const char* reason, ...) {
    va_list args;
    va_start(args, reason);
    printf("fatal: ");
    while (*reason) {
        if (*reason == '%') {
            reason++;
            if (*reason == 'd') {
                int i = va_arg(args, int);
                printf("%d", i);
            }
            else if 
            (*reason == 'l' && *(reason + 1) == 'u' && *(reason + 2) == 's') {
                reason += 2;
                unsigned long ul = va_arg(args, unsigned long);
                char *s = va_arg(args, char *);
                printf("%lu%s", ul, s);
            }
            else {
                putchar('%');
                putchar(*reason);
            }
        }
        else {
            putchar(*reason);
        }
        reason++;
    }
    printf("\n");
    // This function does not return anything and simply exist the program.
    exit(-1);
}

void
info_who_locked(int host_id) {
    printf("info: head locked by host %d\n", host_id);
}

void
info_who_unlocked(int host_id) {
    printf("info: head unlocked by host %d\n", host_id);
}

void
info(const char* what, ...) {
    va_list args;
    va_start(args, what);
    printf("info: ");
    while (*what) {
        if (*what == '%') {
            what++;
            if (*what == 'd') {
                int i = va_arg(args, int);
                printf("%d", i);
            }
            else if (*what == 'p') {
                // Print a pointer
                void *p = va_arg(args, void *);
                printf("%p", p);
            }
            else if 
            (*what == 'l' && *(what + 1) == 'u' && *(what + 2) == 's') {
                what += 2;
                unsigned long ul = va_arg(args, unsigned long);
                char *s = va_arg(args, char *);
                printf("%lu%s", ul, s);
            }
            else {
                putchar('%');
                putchar(*what);
            }
        }
        else {
            putchar(*what);
        }
        what++;
    }
    printf("\n");
}