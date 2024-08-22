
This example is for key-value module which wraps littlefs.

[Setup Guide]
~~~~~~~~~~~
		1. Set the parameter CONFIG_EXAMPLE_KV to 1 in platform_opts.h file.
		2. Set USE_ENCRYPT to 1 in example_kv.c if encryptation is needed.
		3. make
		

[Supported List]
~~~~~~~~~~
		AMEBAD

Result description
~~~~~~~~~~~~~~~~~~
	   Example Steps:
			1. Set key-value pair (key = TEST_FILE, value = KV module works normally !!!)
			2. Get key-value pair
			3. Delete key-value pair

	   Expected Results:
			Step 1: Set key-value pair success with value length = 28
			Step 2: Get key-value pair success with value length = 28
			Step 3: Delete Success