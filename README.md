# Brose 22-044-502-020 with VM-IIC controller
I recently got hold of a Brose flipdot display. Naturally I wanted to find out how it works and how to drive it. My efforts of understanding it will be documented in this repo.

The `Alcatel MT 1028 I` ICs seem to be a Brose clone of the FP2800A flipdot drivers.

## Protocol
The Brose Vollmatrix Control with VM-IIC controller has quite a cursed hardware layer.

It uses two RS422 transceivers to transmit the SDA and SCL signal of an I²C bus to all connected displays. Because this method of transmission is inherently unidirictional, it means that you somehow have to ignore the missing ACK from the I²C slaves.

3 bytes are sent via the bus to update 3 PCF8574 I²C port expanders. Connected to those are two FP2800 row drivers located on the VM-IIC controller card and level shifters for signals on the 60 pin header. Those signals are for the FP2800 column drivers and their module select enable signal located on the flipdot modules. The mapping for the port expanders is listed [below](#ic-port-expander-mapping). 

For the correct method of controlling the FP2800 flipdot drivers, look into its [datasheet](doc/fp2800-datasheet.pdf) or at my implementation. Maybe I'll go into more depth in the future™.

### Schematic 
I reverse engineered the most important aspects of the VM-IIC and documented it [here](Brose_VM-IIC_schematic/Brose_VM-IIC_schematic.pdf).

### Plug Pinout
| Pin | Color  | Function |
| --- | ------ | -------- |
| 1   | White  | SDA A    |
| 2   | Brown  | SDA B    |
| 3   | Green  | SCL A    |
| 4   | Yellow | SCL B    |

### I²C Port Expander Mapping
| Address | I²C Bit | Function               |
| ------- | ------- | ---------------------- |
| 0x40    | 0       | Module Select 8        |
|         | 1       | Module Select 7        |
|         | 2       | Module Select 6        |
|         | 3       | Module Select 5        |
|         | 4       | Module Select 4        |
|         | 5       | Module Select 3        |
|         | 6       | Module Select 2        |
|         | 7       | Module Select 1        |
| 0x42    | 0       | ???                    |
|         | 1       | Column FP2800 B1       |
|         | 2       | Column FP2800 B0       |
|         | 3       | Column FP2800 A2       |
|         | 4       | Column FP2800 A1       |
|         | 5       | Column FP2800 A0       |
|         | 6       | Column FP2800 DATA     |
|         | 7       | Row FP2800 A2          |
| 0x44    | 0       | Row FP2800 A2          |
|         | 1       | Row FP2800 A1          |
|         | 2       | Row FP2800 A0          |
|         | 3       | Row FP2800 B1          |
|         | 4       | Row FP2800 B0          |
|         | 5       | Row FP2800 Low DATA    |
|         | 5       | Row FP2800 Low ENABLE  |
|         | 5       | Row FP2800 High DATA   |
|         | 5       | Row FP2800 High ENABLE |

Row Low: 0-13  
Row High: 14-19