# HR3 Dyno

## Compiling


```
gcc dynoHR3.c -o dynoHR3 -lmosquitto `pkg-config --cflags --libs libmodbus`
```

## Libraries needed:
```
sudo apt-get install libusb-1.0-0-dev
sudo apt-get install libhidapi-dev

sudo apt-get install libudev-dev libusb-1.0-0-dev libfox-1.6-dev
```

https://github.com/wjasper/Linux_Drivers/tree/master/USB/mcc-libusb
