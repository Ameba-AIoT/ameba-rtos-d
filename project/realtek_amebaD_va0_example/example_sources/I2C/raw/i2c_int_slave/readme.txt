# Example Description

1. This example describes how to use i2c as the slave by using raw api.
2. In this demo code, i2c as the slave, can perform rx and tx 3 times in interrupt mode. 

# HW Configuration

slave's SDA connect to master's SDA 
slave's SCL connect to master's SCL 
slave's GND connect to master's GND 

* On AMEBAD
  - slave's`SDA (_PB_25)` connect to master's `SDA`
  - slave's `SCL (_PB_26)` connect to master's `SCL` 
  - slave's `GND` connect to master's `GND` 

# SW Configuration

1. How to compile :
   		(1) copy main.c under folder src to project\realtek_amebaxxx_va0_example\src\src_xxx
   		(2) make and download new image.

2. How to modify gpio pin:
   you can also modify pinmux group settings according to correct pinmux table.

# Expected Result

1. i2c has successfully transmitted the data.

# Note

1. This example can be tested with the i2c_int_master. Please start slave example first, then start master example. 

# Supported IC

* AmebaD