#ifndef _BSP_SD_H_
#define _BSP_SD_H_

/*————————————————————————————————————————Header file declaration————————————————————————————————————————*/
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
/*——————————————————————————————————————Header file declaration end——————————————————————————————————————*/

/*——————————————————————————————————————————Variable declaration—————————————————————————————————————————*/

#define SD_TAG "SD_CARD"
#define SD_INFO(fmt, ...)  ESP_LOGI(SD_TAG, fmt, ##__VA_ARGS__)
#define SD_DEBUG(fmt, ...) ESP_LOGD(SD_TAG, fmt, ##__VA_ARGS__)
#define SD_ERROR(fmt, ...) ESP_LOGE(SD_TAG, fmt, ##__VA_ARGS__)

#define SD_MOUNT_POINT "/sdcard"

/**
 * @brief Initialize and mount the SD card (1-wire SDIO mode).
 *
 * GPIO43=CLK, GPIO44=CMD, GPIO39=D0.
 *
 * @return ESP_OK on success
 */
esp_err_t sd_init(void);

/**
 * @brief Append a string to a file (creates if not existing).
 *
 * @param path  Full path including mount point, e.g. "/sdcard/log.csv"
 * @param data  Null-terminated string to append
 * @return ESP_OK on success
 */
esp_err_t sd_append_string(const char *path, const char *data);

/**
 * @brief Check whether a file exists.
 *
 * @param path  Full path to the file
 * @return true if file exists
 */
bool sd_file_exists(const char *path);

/*———————————————————————————————————————Variable declaration end——————————————-—————————————————————————*/
#endif
