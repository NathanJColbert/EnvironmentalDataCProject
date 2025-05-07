# Environmental Data Project

## Description
A C-core project made to run on a Raspberry Pi, Raspbian/Linux. Uses I2C LCD and DHT11 temp/hum sensor. Stores data in MariaDB.

I use make for the build system. This is because Linux is the only system this project was built for.

## Table of Contents

- [Dependencies](#dependencies)
- [Hardware Requirements](#hardware-requirements)
- [Installation & Setup](#installation--setup)
- [Usage](#usage)

## Dependencies
The external linked libraries are,
```bash
-lwiringPi -lm -lmysqlclient
```
The math library (lm) should come with C tools

The wiring pi library is an open source repo at https://github.com/WiringPi/WiringPi. Creating the debian-package is the easiest way. Current instructions (subject to change)
```bash
# Fetch the source
sudo apt install git
git clone https://github.com/WiringPi/WiringPi.git
cd WiringPi

# Build the package
./build debian
mv debian-template/wiringpi-3.0-1.deb .

# Install it
sudo apt install ./wiringpi-3.0-1.deb
```

MySQL is a common package install through apt. 
```bash
# Using apt
sudo apt install libmysqlclient-dev
# Check the install ->
dpkg -l | grep libmysqlclient
```

Optionally install I2C tools to interface the LCD panel easier
```bash
# Install the tools
sudo apt-get install i2c-tools

# Reboot the system

# Retrieve the list of I2C connections
i2cdetect -y 1
```

## Hardware Requirements
I am using a DHT11 Temperature and Humidity sensor and a 16x2 LCD panel with an I2C interface. (I am not a real man using the I2C I KNOW!)

TODO -> Fritzing Diagram, DHT11 model number and sensor type, LCD panel model number

## Installation & Setup
Before installing. Make sure your [Dependencies](#dependencies) are up to date

This project requires you to set up a SQL user, database and table outside of the program.
The table data should look like this
```bash
data(TempLHS int, TempRHS int, HumLHS int, HumRHS int, time timestamp default current_timestamp);
```
If you are familiar with SQL you can skip the next part.

```bash
# Run SQL as admin
sudo mysql

# Create the database
CREATE DATABASE environmental_data;

# Create a user for the program
CREATE USER 'server'@'localhost' IDENTIFIED BY 'default';
GRANT ALL PRIVILEGES ON environmental_data.* TO 'server'@'localhost';

# Setup the database
USE environmental_data
CREATE table data_main(TempLHS int, TempRHS int, HumLHS int, HumRHS int, time timestamp default current_timestamp);

QUIT
```

```bash
# Clone the repository
git clone https://github.com/NathanJColbert/EnvironmentalDataCProject.git

# Navigate into the directory
cd EnvironmentalDataCProject

# (Optional) Adjust LCD address, DHT11 pin and sample rate
# Open main.c and edit these values if needed:
nano src/main.c

// Inside main.c, look for:
const int LCD_ADDRESS = 0x27;
const int DHT11_PIN = 7;
const size_t RATE_SECONDS = 600;

# (Optional) set environment variables for database access
# The program will prompt you if they are not set
export EN_SERVER='localhost'
export EN_USER='server'
export EN_PASSWORD='default'
export EN_DATABASE='environmental_data'
export EN_TABLE='data_main'

# I use make for the build system.
mkdir build
make run
```

## Usage
Once built, the program will attempt to read data from the DHT11 sensor and write it to your SQL database. It will display temperature, humidity, and time on the LCD screen using the Linux system time via the C standard library.

```bash
# Build
make

# Run
make run

# Clean build
make clean
```
