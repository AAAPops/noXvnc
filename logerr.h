

/* Define this at the top of a file to add a prefix to debug messages */
/*
#ifndef pr_fmt
#define pr_fmt(fmt) fmt
#endif
*/


/*
 * Output a debug text when condition "cond" is met. The "cond" should be
 * computed by a preprocessor in the best case, allowing for the best
 * optimization.
 */
#define debug_cond(cond, fmt, args...)                  \
        do {                                            \
                if (cond)                               \
                        printf(pr_fmt(fmt), ##args);    \
        } while (0)



void err_exit();

void err_exit_2(char *);