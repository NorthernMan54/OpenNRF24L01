


## ESP32 connection to NRF24L01 Module

```
   gnd - gnd    O O vcc  - 3v3
gpio21 - ce     O O csn  - gpio5
gpio18 - sck    O O mosi - gpio23
gpio19 - miso   O O irq  - gpio4
```

## Compiler Directives

```
	'-DCE_GPIO=21'
	'-DSCK_GPIO=18'
	'-DMISO_GPIO=19'
	'-DCSN_GPIO=5'
	'-DMOSI_GPIO=23'
	'-DIRQ_GPIO=4'
```