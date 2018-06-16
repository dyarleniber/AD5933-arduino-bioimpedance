/*
   AD5933-arduino-bioimpedance

   Arduino code for a bioimpedance or bioelectrical impedance analysis (BIA) system using the AD5933.

   (c) Dyarlen Iber <dyarlen1@gmail.com>

   For the full copyright and license information, please view the LICENSE
   file that was distributed with this source code.

   https://github.com/dyarleniber/AD5933-arduino-bioimpedance
*/

#include "Wire.h"
#include <SoftwareSerial.h>

SoftwareSerial serial1(10, 11); // RX, TX

// Register locations
#define SLAVEADDR 0x0D
#define ADDRPTR 0xB0
#define STARTFREQ_R1 0x82
#define STARTFREQ_R2 0x83
#define STARTFREQ_R3 0x84
#define FREGINCRE_R1 0x85
#define FREGINCRE_R2 0x86
#define FREGINCRE_R3 0x87
#define NUMINCRE_R1 0x88
#define NUMINCRE_R2 0x89
#define NUMSCYCLES_R1 0x8A
#define NUMSCYCLES_R2 0x8B
#define REDATA_R1 0x94
#define REDATA_R2 0x95
#define IMGDATA_R1 0x96
#define IMGDATA_R2 0x97
#define CTRLREG 0x80
#define CTRLREG2 0x81
#define STATUSREG 0x8F

const float MCLK = 16.776 * pow(10, 6); // AD5933 Internal Clock Speed 16.776 MHz
const float startfreq = 50 * pow(10, 3); // Frequency Start
const float increfreq = 1 * pow(10, 3); // Frequency Increment
const int increnum = 50; // Number of increments
const double gain_factor_50khz  = 0.0894676; // Gain Factor at 50 kHz | Known Impedance: ~1 kOhm (985 Ohm)
const double gain_factor_100khz = 0.09138455; // Gain Factor at 100 kHz | Known Impedance: ~1 kOhm (985 Ohm)

// Impedance values
double impedance_50khz;
double impedance_100khz;

// States
typedef enum
{
  WAITING_START = 0,
  READING_USERDATA,
  MEASURING,
  SENDING_RESULTS
} STATES;

STATES actual_state;

String user_data[4]; // [0] = Age | [1] = sex (1=M | 2=F) | [2] = height | [3] = weight
int user_data_index;

void setup() {
  Wire.begin();
  serial1.begin(9600);

  writeData(CTRLREG, 0x0); // Clear Control Register
  writeData(CTRLREG2, 0x10); // Reset / Internal clock

  programReg();

  actual_state = WAITING_START;
}

void loop() {
  char receivedChar;

  switch (actual_state) {
    case WAITING_START:
      if (serial1.available()) {
        receivedChar = serial1.read();

        if (receivedChar == 'S') {
          for (int i = 0; i < 4; ++i) {
            user_data[i] = "";
          }

          user_data_index = 0;
          actual_state = READING_USERDATA;
        }
      }

      break;

    case READING_USERDATA:
      if (serial1.available()) {
        receivedChar = serial1.read();

        if (user_data_index < 4) {
          if (receivedChar != '-') {
            user_data[user_data_index] += receivedChar;
          } else if (receivedChar == '-') {
            user_data_index++;
          }
        } else {
          if (receivedChar == 'F') {
            actual_state = MEASURING;
          }
        }
      }

      break;

    case MEASURING:
      actual_state = runSweep();

      break;

    case SENDING_RESULTS:
      float FM;
      float FFM;
      float TBW;

      user_data[1] = (user_data[1].toInt() == 1) ? "1" : "0"; // 1 for Male and 0 for Female

      TBW = 6.69 + (0.34573 * (pow(user_data[2].toFloat(), 2) / impedance_100khz)) + (0.17065 * user_data[3].toFloat()) - (0.11 * user_data[0].toInt()) + (2.66 * user_data[1].toInt());
      FFM = TBW / 0.73;
      FM = user_data[3].toFloat() - FFM;

      FM = abs(FM);
      FFM = abs(FFM);
      TBW = abs(TBW);

      serial1.write("D");
      sendFloatBluetooth((float) FM);
      serial1.write("-");
      sendFloatBluetooth((float) FFM);
      serial1.write("-");
      sendFloatBluetooth((float) TBW);
      serial1.write("F");

      actual_state = WAITING_START;
      break;

    default:
      actual_state = WAITING_START;
      break;
  }
}

void programReg() {
  // Set Range 1, PGA gain 1
  writeData(CTRLREG, 0x01);

  // Set settling cycles
  writeData(NUMSCYCLES_R1, 0x07);
  writeData(NUMSCYCLES_R2, 0xFF);

  // Start frequency of 50 kHz
  writeData(STARTFREQ_R1, getFrequency(startfreq, 1));
  writeData(STARTFREQ_R2, getFrequency(startfreq, 2));
  writeData(STARTFREQ_R3, getFrequency(startfreq, 3));

  // Increment by 1 kHz
  writeData(FREGINCRE_R1, getFrequency(increfreq, 1));
  writeData(FREGINCRE_R2, getFrequency(increfreq, 2));
  writeData(FREGINCRE_R3, getFrequency(increfreq, 3));

  // Points in frequency sweep (100), max 511
  writeData(NUMINCRE_R1, (increnum & 0x001F00) >> 0x08);
  writeData(NUMINCRE_R2, (increnum & 0x0000FF));
}

STATES runSweep() {
  short re;
  short img;
  float freq;
  double mag;
  int i = 0;

  impedance_50khz = 0;
  impedance_100khz = 0;

  programReg();

  // 1. Standby '10110000' Mask D8-10 of avoid tampering with gains
  writeData(CTRLREG, (readData(CTRLREG) & 0x07) | 0xB0);
  // 2. Initialize sweep
  writeData(CTRLREG, (readData(CTRLREG) & 0x07) | 0x10);
  // 3. Start sweep
  writeData(CTRLREG, (readData(CTRLREG) & 0x07) | 0x20);

  while ((readData(STATUSREG) & 0x07) < 4 ) { // Check that status reg != 4, sweep not complete
    delay(100); // Delay between measurements

    int flag = readData(STATUSREG) & 2;
    if (flag == 2) {
      byte R1 = readData(REDATA_R1);
      byte R2 = readData(REDATA_R2);
      re = (R1 << 8) | R2;
      R1 = readData(IMGDATA_R1);
      R2 = readData(IMGDATA_R2);
      img = (R1 << 8) | R2;

      freq = startfreq + i * increfreq;

      mag = sqrt(pow(double(re), 2) + pow(double(img), 2));

      if (freq / 1000 == 50) { //50 kHz
        impedance_50khz = gain_factor_50khz * mag;
      }

      if (freq / 1000 == 100) { //100 kHz
        impedance_100khz = gain_factor_100khz * mag;
      }

      // Increment frequency
      if ((readData(STATUSREG) & 0x07) < 4 ) {
        writeData(CTRLREG, (readData(CTRLREG) & 0x07) | 0x30);
        i++;
      }
    }
  }

  writeData(CTRLREG, (readData(CTRLREG) & 0x07) | 0xA0); // Power down

  if (impedance_100khz != 0) {
    return SENDING_RESULTS;
  } else {
    serial1.write("ER");
    return WAITING_START;
  }
}

void writeData(int addr, int data) {
  Wire.beginTransmission(SLAVEADDR);
  Wire.write(addr);
  Wire.write(data);
  Wire.endTransmission();
  delay(1);
}

int readData(int addr) {
  int data;
  Wire.beginTransmission(SLAVEADDR);
  Wire.write(ADDRPTR);
  Wire.write(addr);
  Wire.endTransmission();
  delay(1);
  Wire.requestFrom(SLAVEADDR, 1);

  if (Wire.available() >= 1) {
    data = Wire.read();
  } else {
    data = -1;
  }

  delay(1);
  return data;
}

byte getFrequency(float freq, int n) {
  long val = long((freq / (MCLK / 4)) * pow(2, 27));
  byte code;

  switch (n) {
    case 1:
      code = (val & 0xFF0000) >> 0x10;
      break;
    case 2:
      code = (val & 0x00FF00) >> 0x08;
      break;
    case 3:
      code = (val & 0x0000FF);
      break;
    default:
      code = 0;
  }

  return code;
}

void sendFloatBluetooth(float number) {
  String str_number = (String) number;
  char char_array[str_number.length() + 1];
  str_number.toCharArray(char_array, str_number.length() + 1);
  serial1.write(char_array);
  delay(1);
}