#include "config.h"
#include "core.h"

/* Config Management */

static const config_item desktop_items[] = {
	CFG_INT("refresh_rate", "Refresh rate in milliseconds",
		&wm.configs.desktop.refresh_rate, "800")
};

static const config_item key_items[] = {
	CFG_INT("modifier", "27 is Alt", &wm.configs.keys.modifier, "27"),
	CFG_STR("win_mv_up", "Move focused window up", &wm.configs.keys.win_mv_up, "k"),
	CFG_STR("win_mv_down", "Move focused window down", &wm.configs.keys.win_mv_down, "j"),
	CFG_STR("win_mv_right", "Move focused window right", &wm.configs.keys.win_mv_right, "l"),
	CFG_STR("win_mv_left", "Move focused window left", &wm.configs.keys.win_mv_left, "h"),
};

static const config_item color_items[] = {
	CFG_INT("desktop", "Desktop background colors", &wm.configs.colorscheme.desktop, "232"),
	CFG_INT("standard", "Standard window foreground", &wm.configs.colorscheme.standard, "7"),
	CFG_INT("standard_bg", "Standard window background", &wm.configs.colorscheme.standard_bg, "0"),
	CFG_INT("cursor", "Cursor color", &wm.configs.colorscheme.cursor, "0"),
	CFG_INT("cursor_bg", "Cursor background", &wm.configs.colorscheme.cursor_bg, "8"),
	CFG_INT("accent", "Accent color", &wm.configs.colorscheme.accent, "1"),
	CFG_INT("statusbar", "Statusbar text", &wm.configs.colorscheme.statusbar, "7"),
	CFG_INT("statusbar_bg", "Statusbar background", &wm.configs.colorscheme.statusbar_bg, "31")
};

static const config_section app_config[] = {
	{
	 "desktop",
	 "Adjust your cosh desktop environment",
	 desktop_items,
	 sizeof(desktop_items) / sizeof(desktop_items[0])
	 },
	{
	 "keys",
	 "Adjust your key shortcut here",
	 key_items,
	 sizeof(key_items) / sizeof(key_items[0])
	 },
	{
	 "colorscheme",
	 "Adjust your cosh colorscheme/theme. Based on the ncurses color id.",
	 color_items,
	 sizeof(color_items) / sizeof(color_items[0])
	 }
};

#define NUM_SECTIONS ((int)(sizeof(app_config) / sizeof(app_config[0])))

void generate_default_config(void)
{
	FILE *fp;
	fp = fopen(CONFIGFILE, "w");

	if (!fp) {
		c_log_warn("Failed to open/create config file: %s",
			   strerror(errno));
		return;
	}

	fprintf(fp, "; Check updates on: https://github.com/John-fried/Cosh.git\n\n");
	for (int i = 0; i < NUM_SECTIONS; i++) {
		const config_section *sec = &app_config[i];
		fprintf(fp, "[%s]\n", sec->name);

		//section desc
		if (strlen(sec->desc) > 0)
			fprintf(fp, "; %s\n", sec->desc);

		for (int j = 0; j < sec->item_count; j++) {
			const config_item *it = &sec->items[j];
			fprintf(fp, "%s=%s", it->key, it->def_val);

			if (strlen(it->desc) > 0)
				fprintf(fp, "\t; %s", it->desc);

			fprintf(fp, "\n");
		}

		fprintf(fp, "\n");
	}

	fclose(fp);
	c_log_info("Created default config in: %s", CONFIGFILE);
}

int c_load_config(void)
{
	if (access(CONFIGFILE, F_OK) != 0) {
		c_log_warn("Config file not found: %s", CONFIGFILE);
		return -1;
	}

	for (int i = 0; i < NUM_SECTIONS; i++) {
		const config_section *sec = &app_config[i];

		for (int j = 0; j < sec->item_count; j++) {
			const config_item *it = &sec->items[j];

			switch (it->type) {
			case TYPE_INT:
				*(int *)it->target =
				    (int)ini_getl(sec->name, it->key,
						  atol(it->def_val),
						  CONFIGFILE);
				break;

			case TYPE_STR:
				ini_gets(sec->name, it->key, it->def_val,
					 (char *)it->target, (int)it->len,
					 CONFIGFILE);
				break;

			case TYPE_BOOL:
				*(bool *)it->target =
				    ini_getbool(sec->name, it->key,
						(int)atol(it->def_val),
						CONFIGFILE);
				break;
			}
		}
	}

	c_log_info("Config loaded..");
	return 0;
}
