# SerialFlash-manager
A simple firmware and userspace file manager for the Arduino [SerialFlash](https://github.com/PaulStoffregen/SerialFlash) library.
Uploads, download and erase SPI connected Flash memory, with progress reports.

A much friendly alternative to the [CopyFromSerial](https://github.com/PaulStoffregen/SerialFlash/tree/master/examples/CopyFromSerial) example
supplied with that library as it offers feedback: comments begin with "#" and can be ignored, the rest is machine-parsable. Flash the supplied firmware to the device (only dependency is SerialFlash), then:

```
./serialflash.js --port /dev/cu.usbmodem2300301 id
# Connected: send command or "help" at OK prompt
ID ef-40-01-00-00

$ ./serialflash.js --port /dev/cu.usbmodem2300301 erase
# Connected: send command or "help" at OK prompt
# Erasing...
# Erased

$ ./serialflash.js --port /dev/cu.usbmodem2300301 write path/to/myfile.raw path/to/otherfile.raw
# Connected: send command or "help" at OK prompt
# Writing "myfile.raw" (251900 bytes)
Sending "path/to/myfile.raw" (251900 bytes):  100% 
# Writing "otherfile.raw" (123456 bytes)
Sending "path/to/othefile.raw" (123456 bytes):  100% 

$ ./serialflash.js --port /dev/cu.usbmodem2300301 dir
# Connected: send command or "help" at OK prompt
FILE myfile.raw 251900
FILE otherfile.raw 123456

$ ./serialflash.js --port /dev/cu.usbmodem2300301 read myfile.raw
# Connected: send command or "help" at OK prompt
# Reading "myfile.raw" (251900 bytes)

$ ls -l myfile.raw
-rw-rw-r--   1 mike  staff  251900  8 Oct 15:28 myfile.raw
```
