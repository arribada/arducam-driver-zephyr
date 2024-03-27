#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/fs/fs.h>
#include <ff.h>

#include "sd.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sd_helper);

static FATFS fat_fs;
/* mounting info */
static struct fs_mount_t mp = {
	.type = FS_FATFS,
	.fs_data = &fat_fs,
};

/*
 *  Note the fatfs library is able to mount only strings inside _VOLUME_STRS
 *  in ffconf.h
 */
static const char *disk_mount_pt = "/SD:";

int lsdir(const char *path)
{
	int res;
	struct fs_dir_t dirp;
	static struct fs_dirent entry;

	fs_dir_t_init(&dirp);

	/* Verify fs_opendir() */
	res = fs_opendir(&dirp, path);
	if (res) {
		printk("Error opening dir %s [%d]\n", path, res);
		return res;
	}

	printk("\nListing dir %s ...\n", path);
	for (;;) {
		/* Verify fs_readdir() */
		res = fs_readdir(&dirp, &entry);

		/* entry.name[0] == 0 means end-of-dir */
		if (res || entry.name[0] == 0) {
			break;
		}

		if (entry.type == FS_DIR_ENTRY_DIR) {
			printk("[DIR ] %s\n", entry.name);
		} else {
			printk("[FILE] %s (size = %zu)\n", entry.name, entry.size);
		}
	}

	/* Verify fs_closedir() */
	fs_closedir(&dirp);

	return res;
}

int mount_sd_card(void)
{
	/* raw disk i/o */
	static const char *disk_pdrv = "SD";
	uint64_t memory_size_mb;
	uint32_t block_count;
	uint32_t block_size;

	if (disk_access_init(disk_pdrv) != 0) {
		LOG_ERR("Storage init ERROR!");
		return -1;
	}

	if (disk_access_ioctl(disk_pdrv, DISK_IOCTL_GET_SECTOR_COUNT, &block_count)) {
		LOG_ERR("Unable to get sector count");
		return -1;
	}
	LOG_INF("Block count %u", block_count);

	if (disk_access_ioctl(disk_pdrv, DISK_IOCTL_GET_SECTOR_SIZE, &block_size)) {
		LOG_ERR("Unable to get sector size");
		return -1;
	}
	printk("Sector size %u\n", block_size);

	memory_size_mb = (uint64_t)block_count * block_size;
	printk("Memory Size(MB) %u\n", (uint32_t)(memory_size_mb >> 20));

	mp.mnt_point = disk_mount_pt;

	int res = fs_mount(&mp);

	if (res == FR_OK) {
		printk("Disk mounted.\n");
		lsdir(disk_mount_pt);
	} else {
		printk("Failed to mount disk - trying one more time\n");
		res = fs_mount(&mp);
		if (res != FR_OK) {
			printk("Error mounting disk.\n");
			return -1;
		}
	}

	return 0;
}

int write_to_file(const char *path, const char *file_name, size_t file_size, const char *data)
{
	int ret;
	struct fs_file_t data_filp;
	fs_file_t_init(&data_filp);
	char abs_path[100];

	const char data_fake[10] = "1234567890";
	size_t data_fake_size = sizeof(data_fake);

	char file_ch;
	printk("file_name: %s\n", file_name);
	printk("path: %s\n", path);

	sprintf(abs_path, "%s%s", path, file_name);
	printk("abs_path: %s\n", abs_path);

	ret = fs_unlink(abs_path);

	ret = fs_open(&data_filp, abs_path, FS_O_WRITE | FS_O_CREATE);
	if (ret) {
		printk("%s -- failed to create file (err = %d)\n", __func__, ret);
		return -2;
	} else {
		printk("%s - successfully created file\n", __func__);
	}

	printk("size of data: %d\n", file_size);

	ret = fs_write(&data_filp, data, file_size);
	fs_close(&data_filp);

	return 0;
}
