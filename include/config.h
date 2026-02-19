#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
  char komga_url[512];
  char komga_api_key[256];
  char komga_username[128];
  char komga_password[128];
  char download_path[1024];
} AppConfig;

void config_set_defaults(AppConfig *cfg);
int config_load(AppConfig *cfg);
int config_has_komga(const AppConfig *cfg);

#endif
