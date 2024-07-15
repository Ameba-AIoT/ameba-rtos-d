
#include <platform_opts.h>

#if defined(CONFIG_EXAMPLE_SPI_NAND_LITTLEFS) && CONFIG_EXAMPLE_SPI_NAND_LITTLEFS

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "littlefs_nand_adapter.h"
#include "littlefs_nand_ftl.h"
#include "example_spi_nand_littlefs.h"


static int littlefs_list_all(lfs_t *lfs, char *path)
{
	lfs_dir_t dir;
	struct lfs_info info;
	int ret = 0;
	char cur_path[512] = {0};
	ret = lfs_dir_open(lfs, &dir, path);

	if (ret == LFS_ERR_OK) {
		sprintf(cur_path, path);
		for (;;) {
			ret = lfs_dir_read(lfs, &dir, &info);

			printf("%s %d %d\r\n", info.name, info.type, info.size);
			if (ret <= 0) {
				break;
			}
			if (info.name[0] == '.') {
				continue;
			}
			if (info.type == LFS_TYPE_DIR) {
				sprintf(&cur_path[strlen(path)], "/%s", info.name);
				printf("List folder %s %d %d\r\n", info.name, info.type, info.size);
				littlefs_list_all(lfs, cur_path);
				printf("cur_path=%s\r\n", cur_path);
			} else {
				printf("List file %s %s %d %d\r\n", path, info.name, info.type, info.size);
			}
		}
	}
	//Close directory
	ret = lfs_dir_close(lfs, &dir);

	if (ret < 0) {
		printf("Close directory fail: %d\r\n", ret);
	}
	return ret;
}


int littlefs_delete_dir(lfs_t *lfs, const char *path, int del_self)
{
	lfs_dir_t dir;
	struct lfs_info info;
	int ret = 0;
	ret = lfs_dir_open(lfs, &dir, path);
	printf("path=%s lfs_dir_open %d\r\n", path, ret);
	char file[512] = {0};
	if (ret == LFS_ERR_OK) {
		for (;;) {
			// read directory and store it in file info object
			ret = lfs_dir_read(lfs, &dir, &info);
			printf("lfs_dir_read %d\r\n", ret);
			printf("%s %d %d\r\n", info.name, info.type, info.size);
			if (ret <= 0) {
				break;
			}
			if (info.name[0] == '.') {
				continue;
			}

			//printf("%s %d %d\r\n",info.name,info.type,info.size);
			sprintf((char *)file, "%s/%s", path, info.name);
			if (info.type == LFS_TYPE_DIR) {
				littlefs_delete_dir(lfs, file, del_self);
				printf("file=%s littlefs_delete_dir \r\n", file);
			} else {
				printf("Delete file=%s type=%d size=%d\r\n", file, info.type, info.size);
				ret = lfs_remove(lfs, file);
			}
		}
	}

	// close directory
	ret = lfs_dir_close(lfs, &dir);

	// delete self?
	if (ret == LFS_ERR_OK) {
		if (del_self == 1) {
			ret = lfs_remove(lfs, path);
			if (ret >= 0) {
				printf("Delete folder %s\r\n", path);
			}
		}
	}
	return ret;
}

int traverse_cb(void *p, lfs_block_t block)
{
	/* To avoid gcc warnings */
	(void) block;

	uint32_t *nb = p;
	*nb += 1;
	return 0;
}


void example_spi_nand_littlefs_thread(void *param)
{
	/* To avoid gcc warnings */
	(void) param;

	lfs_t *lfs = malloc(sizeof(lfs_t));
	lfs_file_t *file = malloc(sizeof(lfs_file_t));
	lfs_dir_t *dir = malloc(sizeof(lfs_dir_t));
	struct lfs_info *info = malloc(sizeof(struct lfs_info));
	int ret = 0;
	char *str_content = "hello_world";
	char *file_name = "hello.txt";
	char *file_folder = "flash";
	char r_buf[64];
	memset(r_buf, 0, sizeof(r_buf));

	//Init Spi Nand Flash and LittleFS
	ret = NandFlash_FTL_Init();
    if(ret){
		printf("\r\n SPI Nand Flash init FAILED %d\r\n");
		goto EXIT;
    }    
	g_nand_flash_lfs_cfg.block_count = NandFlash_Device.MemInfo.BlocksPerLun*NandFlash_Device.MemInfo.LunsPerTarget*NandFlash_Device.MemInfo.Targets;
    printf("BlocksPerLun=%d, LunsPerTarget=%d, Targets=%d", NandFlash_Device.MemInfo.BlocksPerLun, NandFlash_Device.MemInfo.LunsPerTarget, NandFlash_Device.MemInfo.Targets);

	// mount the filesystem
	ret = lfs_mount(lfs, &g_nand_flash_lfs_cfg);
	printf("\r\n    lfs_mount %d\r\n", ret);
	// reformat if we can't mount the filesystem
	// this should only happen on the first boot
	if (ret) {
		ret = lfs_format(lfs, &g_nand_flash_lfs_cfg);
		if (ret) {
			printf("lfs_format fail %d\r\n", ret);
			goto EXIT;
		}
		ret = lfs_mount(lfs, &g_nand_flash_lfs_cfg);
		if (ret < 0) {
			printf("lfs_mount fail %d\r\n", ret);
			goto EXIT;
		}
	}

	rtw_mdelay_os(1 * 1000);

	printf("\r\n =================== Spi_Nand_littlefs_2.50 example start =================== \r\n");

	//Delete all file and folder
	littlefs_delete_dir(lfs, "", 1);

	ret = lfs_file_open(lfs, file, file_name, LFS_O_WRONLY | LFS_O_CREAT);
	if (ret < 0) {
		printf("lfs_file_open fail %d\r\n", ret);
		goto EXIT;
	}

	ret = lfs_file_seek(lfs, file, 0, LFS_SEEK_SET);
	if (ret < 0) {
		printf("lfs_file_seek fail %d\r\n", ret);
		goto EXIT;
	}

	ret = lfs_file_write(lfs, file, str_content, strlen(str_content));
	if (ret < 0) {
		printf("lfs_file_write fail %d\r\n", ret);
		goto EXIT;
	}

	ret = lfs_file_close(lfs, file);
	if (ret < 0) {
		printf("lfs_file_close fail %d\r\n", ret);
		goto EXIT;
	}

	ret = lfs_file_open(lfs, file, "hello.txt", LFS_O_RDONLY);
	if (ret < 0) {
		printf("lfs_file_open fail %d\r\n", ret);
		goto EXIT;
	}

	ret = lfs_file_size(lfs, file);
	if (ret < 0) {
		printf("lfs_file_size fail %d\r\n", ret);
		goto EXIT;
	}
	printf("lfs_file_size %d\r\n", ret);

	ret = lfs_file_read(lfs, file, r_buf, strlen(str_content));
	//printf("sizeof(r_buf) %d  strlen(r_buf) %d\r\n", sizeof(r_buf),strlen(r_buf));
	if (ret < 0) {
		printf("lfs_file_size fail %d\r\n", ret);
		goto EXIT;
	}
	printf("File content [%s]\r\n",  r_buf);

	ret = lfs_file_close(lfs, file);
	if (ret < 0) {
		printf("lfs_file_close fail %d\r\n", ret);
		goto EXIT;
	}

	ret = lfs_mkdir(lfs, file_folder);
	if (ret < 0) {
		printf("lfs_mkdir fail %d\r\n", ret);
		//goto EXIT;
	}
	/////////////////
	char path[64] = {0};
	sprintf(path, "%s/%s", file_folder, file_name);
	printf("open path %s\r\n", path);
	ret = lfs_file_open(lfs, file, path, LFS_O_WRONLY | LFS_O_CREAT);
	if (ret < 0) {
		printf("lfs_file_open fail %d\r\n", ret);
		goto EXIT;
	}

	ret = lfs_file_write(lfs, file, str_content, strlen(str_content));
	if (ret < 0) {
		printf("lfs_file_write fail %d\r\n", ret);
		goto EXIT;
	}

	ret = lfs_file_close(lfs, file);
	if (ret < 0) {
		printf("lfs_file_close fail %d\r\n", ret);
		goto EXIT;
	}

	char buff[256] = {0};
	strcpy(buff, "");
	littlefs_list_all(lfs, buff);

	uint32_t _df_nballocatedblock = 0;
	ret = lfs_fs_traverse(lfs, traverse_cb, &_df_nballocatedblock); //遍历文件系统正在使用的所有块
	printf("_df_nballocatedblock=%d \r\n", _df_nballocatedblock);
	if (ret < 0) {
		printf("lfs_fs_traverse fail %d\r\n", ret);
		goto EXIT;
	}
	uint32_t available = g_nand_flash_lfs_cfg.block_count * g_nand_flash_lfs_cfg.block_size - _df_nballocatedblock * g_nand_flash_lfs_cfg.block_size;
	printf("Avaliable space:%d\r\n", available);
	//printf("g_lfs_cfg.block_size %d\r\n", g_nand_flash_lfs_cfg.block_size);

	printf("lfs_fs_size %d\r\n", lfs_fs_size(lfs));

	ret = lfs_unmount(lfs);
	if (ret < 0) {
		printf("lfs_fs_traverse fail %d\r\n", ret);
		goto EXIT;
	}
	//printf("lfs_unmount %d\r\n", ret);

EXIT:
	if (lfs) {
		free(lfs);
	}
	if (file) {
		free(file);
	}
	if (dir) {
		free(dir);
	}
	if (info) {
		free(info);
	}

	vTaskDelete(NULL);
}

void example_spi_nand_littlefs(void)
{
	if (xTaskCreate(example_spi_nand_littlefs_thread, ((const char *)"example_spi_nand_littlefs_thread"), 2048, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
		printf("\n\r xTaskCreate(example_spi_nand_littlefs_thread) failed");
	}
}

#endif
