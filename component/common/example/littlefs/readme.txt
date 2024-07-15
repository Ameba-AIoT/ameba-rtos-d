This example is for littlefs-2.50 test.

[Setup Guide]
~~~~~~~~~~~
		1. Set the parameter CONFIG_EXAMPLE_LITTLEFS to 1 in platform_opts.h file.

		2. In component/common/file_system/littlefs/littlefs_adapter.h, set LFS_FLASH_BASE_ADDR and LFS_DEVICE_SIZE, which means the address and size of littlefs respectively.
		   The value of LFS_FLASH_BASE_ADDR represents the offset from 0x08000000.

		3. Make.

[Supported List]
~~~~~~~~~~
	    AmebaD