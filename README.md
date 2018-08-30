# A lab rat training platform with 3-arm Escalator

This training platform use MCU [EFM8LB12F64E](https://www.silabs.com/products/mcu/8-bit/efm8-laser-bee/device.efm8lb12f64e-qfn24) from Silicon Labs, we use its ADC peripheral to receive analog signal from IR sensor(**Sharp 2Y0A21**), which is distance to the rat, then transfer the distance with wifi module(**esp8266**) to PC, then PC reply a user-specified speed(12-bit DAC table) which correspond to the current rat location to the MCU , and then MCU apply the received DAC data to each arm's servo motor, this opration is performed once IR sensored the rat's location changed.
## Misc
This project use [Semantic Versioning](https://semver.org/).