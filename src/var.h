#ifndef __VAR_H__
#define __VAR_H__


void yed_init_vars(void);

void yed_set_default_vars(void);

void yed_set_var(char *var, char *val);
char *yed_get_var(char *var);
void yed_unset_var(char *var);

#define DEFAULT_TABW 4
int yed_get_tab_width(void);

int yed_var_is_truthy(char *var);

#endif
