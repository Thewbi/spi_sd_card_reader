# spi_sd_card_reader

Windows Software to read and write a SD card via SPI




## Credit and Links

Source code inspired by:

* https://www.rjhcoding.com/avrc-sd-interface-1.php
* https://github.com/hpaluch/ch341-spi-shift-reg
* https://www.rjhcoding.com/avrc-sd-interface-1.php

* https://tad-electronics.com/2019/03/10/ch341a-mini-programmer-schematic-and-drivers/
* https://www.wch.cn/download/CH341EVT_ZIP.html
* https://www.wch.cn/download/CH341PAR_EXE.html
* https://github.com/hpaluch/ch341-i2c-24c01c?tab=readme-ov-file
* https://github.com/hpaluch/ch341-i2c-24c01c
* https://github.com/hpaluch/hpaluch.github.io/wiki/Getting-started-with-LC-CH341A-USB-conversion-module
* https://github.com/hpaluch/ch341-spi-shift-reg

http://www.chinalctech.com/cpzx/Programmer/Serial_Module/2019/0124/266.html




## Drivers on Windows

Install the driver: (This is the multiprotocol driver)
https://www.wch.cn/download/CH341PAR_EXE.html

Do not install CH341SER.EXE, it is only for USB to serial.

CH341SER.EXE – self installing archive with USB to serial driver.
CH341PAR.EXE – self installing archive with multiprotocol interface driver (this one is for the programmer mode)

In the Device Manager, there will be a new node, when plugging in the device.
The node is called Interface. Withint the Interface node, you will have USB-EPP/I2C... CH341A




## Source Code for Applications

https://github.com/hpaluch/ch341-i2c-24c01c?tab=readme-ov-file

Download and install CH341PAR.ZIP (https://www.wch.cn/downloads/CH341PAR_ZIP.html) - USB driver for CH341 chip in Parallel mode (EPP, MEM) + SourceCode!!!!!!!!!!!!
This driver is valid also for I2C mode and SPI mode (yes - even when it is marked parallel).

C:\Users\U5353\Downloads\CH341PAR.ZIP




## Overview

This repository is about accessing a SD card via a small SD card reader breakout board:

![image info](DEBO_MICROSD2_01.jpg)

The SD card reader breakout board is accessed using the SPI procotol. To be able to talk
to the breakout board from Windows, a SPI debugger is used:

![image info](s-l500.jpg)

This specific debugger uses a windows driver which has to be installed (CH341PAR.ZIP).
The manufacturer does provide a windows library to talk to the debugger from a C++ windows application.
This library is able to communicate over USB and wraps all the USB traffic within the provided API.




## Using the CH341PAR library

The CH341PAR library provides several functions out of which the functions 

* CH341GetVersion()
* CH341GetDeviceName()
* CH341OpenDevice()
* CH341SetStream()
* CH341StreamSPI4()
* CH341CloseDevice() 

are used.

### CH341GetVersion()

CH341GetVersion() returns the version number of the CH341PAR library release used.

### CH341GetDeviceName()

CH341GetDeviceName() returns the human readable name of the USB SPI debugger.

### CH341OpenDevice()

CH341OpenDevice() does scan the USB bus for all devices of the specific vendor and the specific device id. In USB
programming, all plugged in devices will be iterated over initially. Each device identifies using a vendor and a device
id. If you plug in two or more of the SPI debuggers into your PC, severeal devices will be iterated having the exact same
vendor id and device id and the CH341PAR library has to be told which of the devices you want to access.

To specify the device, a variable is defined:

```
ULONG iDevIndex = 0;
```

This will select the first device that is encountered on the USB bus having the specific device id and vendor id.

### CH341SetStream()

Once the device is opened, CH341SetStream() is used to select the operating mode of the USB debugger.
The debugger does support SPI, UART and I2C. In order to make the USB adapter talk SPI, 

```
CH341SetStream(iDevIndex, 0x81)
```

is used.

### CH341StreamSPI4()

CH341StreamSPI4() is used to send and receive bytes. The function actually does perform a send operation
and also a receive operation at the same time! There is no specific read and write operation for SPI in the API!

If you want to send a byte of data, cann CH341StreamSPI4() with a buffer size of one. The USB SPI debugger will answer
with a byte for each byte it receives! If the SD card plugged into the SD SPI card reader breakout board receives the
byte, processes it and produces an response byte fast enough, then this byte is returned. CH341StreamSPI4() will place
the returned byte into the buffer that is used to initially provide the request! This means that the same buffer is 
used to contain the request and the request is then overriden to contain the response!

If the SD card is not fast enough to produce a response byte, it will not pull the lines low and the SD Card reader breakout 
board will read from the lines anyways. It will then read all high values, since the SD card does not pull any line low.
This means that if the SD card has not produced any output yet, you will read 0xFF (all lines high).

If specific protocol spoken over SPI (in this case, the SD SPI card protocol) defines a response of 0x01 but you read 0xFF,
then this most likely means that the card has not yet processed the request! The solution to this situation is to keep sending
dummy data to the SD card over SPI in order to poll the SD card until it actually answers with the defined 0x01 byte!

So the take away is:

1. To talk to the SPI SD card, use CH341StreamSPI4()
1. CH341StreamSPI4() does perform sending and receiving at the same time
1. The data in the buffer of bytes used to store the request send via CH341StreamSPI4() is replaced with response bytes.
1. For each byte send, a byte is read. If your buffer has a length of 5 and you send all 5 bytes, then the buffer is manipulated
and 5 bytes of response are stored in the buffer

Here is an implementation of a function that sends and receives a single byte.

```
uint8_t SPI_transfer(uint8_t data) 
{
	//printf(">> %x\r\n", data);
	uint8_t ioBuf[1];
	ioBuf[0] = data;

	if (!CH341StreamSPI4(iDevIndex, chip_select, 1, ioBuf)) {
		printf("CH341StreamSPI4 failed\r\n");
		return 0x00;
	}

	//printf("<< %x\r\n", ioBuf[0]);
	return ioBuf[0];
}
```

### CH341CloseDevice()

CH341CloseDevice(iDevIndex); closes the USB device

