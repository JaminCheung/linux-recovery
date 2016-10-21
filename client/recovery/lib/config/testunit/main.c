#include <limits.h>
#include <string.h>

#include <utils/log.h>
#include <lib/config/libconfig.h>
#include <version.h>

#define LOG_TAG "test_libconfig"

#define CONFIG_FILE "recovery.cfg"

struct config_file {
    const char *version;
    const char *ip;
    const char *url;
};

struct config_file app_config;

int main(void) {
    config_t cfg;
    config_setting_t *setting;

    config_init(&cfg);

    /* Read the file. If there is an error, report it and exit. */
    if(!config_read_file(&cfg, CONFIG_FILE))
    {
      LOGE("%s:%d - %s\n", config_error_file(&cfg),
              config_error_line(&cfg), config_error_text(&cfg));
      config_destroy(&cfg);
      return -1;
    }

    /* Get the store name. */
    if(!config_lookup_string(&cfg, "Version", &app_config.version)) {
        LOGE("%s:%d - %s\n", config_error_file(&cfg),
                config_error_line(&cfg), config_error_text(&cfg));
        config_destroy(&cfg);
        return -1;
    }

    /*
     * check version
     */
    if (strncmp(app_config.version, VERSION, sizeof(VERSION))) {
        LOGE("version mismatch - %s : %s", app_config.version, VERSION);
        return -1;
    }

    setting = config_lookup(&cfg, "Application.Server");
    if (setting != NULL) {
        int count = config_setting_length(setting);
        int i;

        for(i = 0; i < count; ++i) {
            if(!(config_setting_lookup_string(setting, "ip", &app_config.ip)
                 && config_setting_lookup_string(setting, "url", &app_config.url)))
              continue;
        }
    }

    LOGI("=====================\n");
    LOGI("Dump %s\n", CONFIG_FILE);
    LOGI("version: %s\n", app_config.version);
    LOGI("ip: %s\n", app_config.ip);
    LOGI("url: %s\n", app_config.url);
    LOGI("=====================\n");

    config_destroy(&cfg);

    return 0;
}
