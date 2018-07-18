#ifndef _LOGGER_H_
#define _LOGGER_H_

extern int logger_get_category(char *category);
extern int logger_add_category(char *category);
extern char **logger_get_categories();
extern void logger_enable_category(int cat, int enable);
extern int  logger_set_file(char *fn);
extern void logger_close_file();
extern void logger_log(int cat, char *fmt, ...);

#endif

