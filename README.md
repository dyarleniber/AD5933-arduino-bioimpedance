# Arduino code for a bioimpedance or bioelectrical impedance analysis (BIA) system for body composition analysis using the AD5933.

The code was developed to be part of a system that uses a mobile application in a Bluetooth device as user interface.

Besides controlling the AD5933, the Arduino is used to perform the body composition equations, using as parameters the impedance values measured by the AD5933, and the patient's personal data provided by the mobile app, the results of the body composition equations, fat mass (FM), fat free mass (FFM) and total body water (TBW), are sent to the mobile application again by Bluetooth.

The source code has four states, in the initial state, the software waits for a Bluetooth command to start reading the user data, after that, in the second state, the software waits for reading the age, sex, height and weight of the users and stores the data in an array. In the third state, the measurement is initiated, and the electrical signal is generated on the Vout pin of AD5933. After the frequency sweep is completed, and the impedance magnitude values at 50 KHz and 100 KHz are collected, the software then switches to the fourth and last state. In the last state, the bioimpedance calculations are performed, and the results are sent to the mobile application by Bluetooth.

#### AD5933 configuration

 - Internal oscillator: MCLK = 16.776 MHz
 - Start frequency = 50 KHz
 - Frequency increment (∆f) = 1 KHz
 - Number of increments = 50
 - PGA gain = ×1
 - Output range = 1

For more details of AD5933 configuration, and the AD5933 operation mode, access the [component datasheet](http://www.analog.com/media/en/technical-documentation/data-sheets/AD5933.pdf).

> #### This source code is part of my Bachelor's degree in Computer Engineering final paper. To download the full final paper [click here](https://raw.githubusercontent.com/dyarleniber/AD5933-arduino-bioimpedance/master/final-paper-dyarleniber.pdf).

License
----

GNU General Public License v3.0
