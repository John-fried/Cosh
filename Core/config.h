#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>
#include <stdbool.h>

typedef enum {
	TYPE_INT,
	TYPE_STR,
	TYPE_BOOL
} cfg_type;

typedef struct {
	const char *key;
	const char *desc;
	void *target;
	const char *def_val;
	cfg_type type;
	size_t len;
} config_item;

typedef struct {
	const char *name;
	const char *desc;
	const config_item *items;
	int item_count;
} config_section;


#define CFG_INT(k, d, t, v) { k, d, t, v, TYPE_INT, 0 }
#define CFG_STR(k, d, t, v) { k, d, t, v, TYPE_STR, sizeof(t) }
#define CFG_BOOL(k, d, t, v) { k, d, t, v, TYPE_BOOL, 0 }

void generate_default_config(void);
int c_load_config(void);

#endif /* CONFIG_H */
