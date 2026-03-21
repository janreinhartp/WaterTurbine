#include "bsp_sd.h"

static sdmmc_card_t *s_card = NULL;
static const char s_mount_point[] = SD_MOUNT_POINT;

/* ------------------------------------------------------------------ */
esp_err_t sd_init(void)
{
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;
    host.max_freq_khz = 10000;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.clk   = GPIO_NUM_43;
    slot_config.cmd   = GPIO_NUM_44;
    slot_config.d0    = GPIO_NUM_39;
    slot_config.width = 1;  /* 1-wire SDIO */
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    SD_INFO("Mounting filesystem");
    esp_err_t err = esp_vfs_fat_sdmmc_mount(s_mount_point, &host, &slot_config,
                                            &mount_config, &s_card);
    if (err != ESP_OK) {
        SD_ERROR("Failed to mount SD card (%s)", esp_err_to_name(err));
        return err;
    }

    SD_INFO("Filesystem mounted");
    sdmmc_card_print_info(stdout, s_card);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
esp_err_t sd_append_string(const char *path, const char *data)
{
    if (path == NULL || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    FILE *f = fopen(path, "a");
    if (f == NULL) {
        SD_ERROR("Failed to open %s for appending", path);
        return ESP_FAIL;
    }

    int written = fprintf(f, "%s", data);
    fclose(f);

    if (written < 0) {
        SD_ERROR("fprintf failed on %s", path);
        return ESP_FAIL;
    }

    return ESP_OK;
}

/* ------------------------------------------------------------------ */
bool sd_file_exists(const char *path)
{
    struct stat st;
    return (stat(path, &st) == 0);
}
