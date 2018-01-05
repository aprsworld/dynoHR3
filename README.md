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


VFD communications
```
  protected static void vfdSetFreq(MbusRtuMasterProtocol mbusVFD, int address, double frequency) {
      short f = (short) (frequency * 10.0);

      try {
         mbusVFD.writeSingleRegister(address, 2332, (short) 1); /* run mode */
         mbusVFD.writeSingleRegister(address, 2333, (short) 0); /* 0=>forward 1=>reverse */
         mbusVFD.writeSingleRegister(address, 2331, f);
      } catch ( Exception e) {
         e.printStackTrace();
      }
   }

   protected static void vfdStop(MbusRtuMasterProtocol mbusVFD, int address) {

      try {
         mbusVFD.writeSingleRegister(address, 2332, (short) 0);
      } catch ( Exception e) {
         e.printStackTrace();
      }
   }

```
