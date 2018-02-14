# HR3 Dyno

Commands VFD to sweep RPM from high RPM to low RPM. Measure analog values using USB-1608FS DAQ at each step. Record raw and summarized data.

## Compiling


```
gcc -std=c99 -g -Wall -I. -o dynoHR3 dynoHR3.c -L. -lmccusb  -lm -L/usr/local/lib -lhidapi-libusb -lusb-1.0 `pkg-config --cflags --libs libmodbus`
```

## Libraries needed:
```
sudo apt-get install libusb-1.0-0-dev
sudo apt-get install libhidapi-dev

sudo apt-get install libudev-dev libusb-1.0-0-dev libfox-1.6-dev
```

https://github.com/wjasper/Linux_Drivers/tree/master/USB/mcc-libusb


