Example Description

This example describes how to use i2c interrupt mode by using raw api

1.In this demo code, i2c as the master, can perform tx and rx in interrupt mode. The interrupt handler and driver alternately use semaphore, which reduces cycle waiting and is simple to use.

2.The user needs to provide the semaphore of tx or rx and interface for acquiring and releasing semaphores.

3.Connect LOG-UART connector to PC
  
4.Connect 
  - Master board I2C0 SDA (_PA_25) and SCL (_PA_26) pin.

