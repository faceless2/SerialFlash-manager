#include <SerialFlash.h>

// Simple interface to read/write/manage etc the SerialFlash filesystem
// by Paul Stoffregen at https://github.com/PaulStoffregen/SerialFlash
//
// This is an alternative to the "CopyFromSerial.ino" example from that
// distribution, which I couldn't get working with a (very) legacy board,
// and I also wanted somethiing with progress feedback.
//
// Usage: connect, then run a command. "help", "dir", "id", "read", "write" or "erase"


#define MODE_INIT       0
#define MODE_READ       1
#define MODE_WRITE      2
#define MODE_WRITE2     3
#define MODE_WRITECHUNK 4

#define CONNECTED       Serial && Serial.dtr()  /* Use this if you're on an Arduino board that supports Serial.dtr */
//#define CONNECTED       Serial                  /* Use this if you're not */

void setup() {
    Serial.begin(9600);  // Baud rate ignored for USB serials
}

void loop() {
    SerialFlashFile file;
    char buf[256];
    uint32_t pos = 0, mode = 0, len = 0, chunk = 0, numchunks = 0;
    memset(buf, 0, sizeof(buf));

    while (!(CONNECTED));

    Serial.println(F("# Connected: send command or \"help\" at OK prompt"));
    if (!SerialFlash.begin(SS)) {
        Serial.println(F("ERROR Unable to access SPI Flash chip"));
        while (1) {
            delay(1000);
        }
    }
    pos = len = mode = chunk = numchunks = 0;

    Serial.println("OK");
    while (CONNECTED) {
        int v = Serial.read();
        if (v >= 0) {
            if (mode == MODE_INIT) {
                if (v == '\r') {
                    v = '\n';
                }
                buf[pos++] = v;
                if (v == ' ' || v == '\n') {
                    buf[pos - 1] = 0;
                    if (!strcmp(buf, "help") && v == '\n') {
                        Serial.println("# Commands (which should be terminated by LF not CR):");
                        Serial.println("# erase - erase the chip:");
                        Serial.println("# id - print the chip id");
                        Serial.println("# dir - print the files and sizes");
                        Serial.println("# read <filename> - read a file. Response is \"READ n\" followed by n bytes");
                        Serial.println("# write <filename> <length> - write a file; response is \"WRITE n blocknum numblocks\" where n is the number of bytes to send in the next block");
                        Serial.println("OK");
                    } else if (!strcmp(buf, "write") && v == ' ') {
                        mode = MODE_WRITE;
                    } else if (!strcmp(buf, "read") && v == ' ') {
                        mode = MODE_READ;
                    } else if (!strcmp(buf, "id") && v == '\n') {
                        SerialFlash.readID((uint8_t*)buf);
                        Serial.printf("ID %02x-%02x-%02x-%02x-%02x\r\n", buf[0], buf[1], buf[2], buf[3], buf[4]);
                        Serial.println("OK");
                    } else if (!strcmp(buf, "erase") && v == '\n') {
                        SerialFlash.eraseAll();
                        Serial.println("# Erasing... ");
                        while (!SerialFlash.ready()) {
                            delay(500);
                        }
                        Serial.println("# Erased... ");
                        Serial.println("OK");
                    } else if (!strcmp(buf, "dir") && v == '\n') {
                        SerialFlash.opendir();
                        pos = 0;
                        while (SerialFlash.readdir(buf, sizeof(buf), len)) {
                            pos++;
                            Serial.print(F("FILE "));
                            Serial.print(buf);
                            Serial.print(F(" "));
                            Serial.print(len);
                            Serial.println();
                        }
                        Serial.println("OK");
                    } else if (v != ' ' && pos > 1) {
                        Serial.println("ERROR Bad command. Type \"help\" for help");
                    }
                    pos = 0;
                } else if (pos == sizeof(buf)) {
                    Serial.println("ERROR Bad command, too long. Type \"help\" for help");
                }
            } else if (mode == MODE_READ) {
                if (v == '\r') {
                    v = '\n';
                }
                buf[pos++] = v;
                if (v == '\n') {
                    buf[pos - 1] = 0;
                    file = SerialFlash.open(buf);
                    if (file) {
                        len = file.size();
                        Serial.printf("# Reading \"%s\" (%d bytes)\r\n", buf, len);
                        Serial.print("READ ");
                        Serial.println(len);
                        pos = 0;
                        while (len) {
                            uint32_t v = len;
                            if (v > sizeof(buf)) {
                                v = sizeof(buf);
                            }
                            file.read(buf, v);
                            Serial.write(buf, v);
                            len -= v;
                        }
                        pos = len = mode = 0;
                        Serial.println("OK");
                    } else {
                        Serial.printf("ERROR can't open \"%s\"\r\n", buf);
                        pos = len = mode = 0;
                        Serial.println("OK");
                    }
                } else if (pos == sizeof(buf)) {
                    Serial.println("ERROR Bad command, too long. Type \"help\" for help");
                }
            } else if (mode == MODE_WRITE) {
                buf[pos++] = v;
                if (v == ' ') {
                    buf[pos - 1] = 0;
                    mode = MODE_WRITE2;
                } else if (!((v >= '0' && v <= '9') || (v >= 'A' && v <= 'Z') || (v >= 'a' && v <= 'z') || v == '_' || v == '-' || v == '.')) {
                    Serial.println("ERROR Bad filename, only accepts a-z, 0-9, hyphen, underscore and dot");
                    pos = len = mode = 0;
                } else if (pos == sizeof(buf)) {
                    Serial.println("ERROR Bad command, too long. Type \"help\" for help");
                    pos = len = mode = 0;
                }
            } else if (mode == MODE_WRITE2) {
                if (v >= '0' && v <= '9') {
                    len = (len * 10) + (v - '0');
                    if (len > 214748364) {
                        Serial.println("ERROR Bad command, file too large");
                        pos = len = mode = 0;
                    }
                } else if (v == '\n') {
                    if (SerialFlash.exists(buf)){
                        Serial.printf("ERROR file \"%s\" already exists\r\n", buf);
                        pos = len = mode = 0;
                    } else {
                        SerialFlash.create(buf, len);
                        file = SerialFlash.open(buf);
                        if (!file) {
                            Serial.printf("ERROR can't open \"%s\"\r\n", buf);
                            pos = len = mode = 0;
                        } else {
                            pos = 0;
                            chunk = 0;
                            numchunks = (len + sizeof(buf) - 1) / sizeof(buf);
                            Serial.printf("# Writing \"%s\" (%d bytes)\r\n", buf, len);
                            Serial.print("WRITE ");
                            Serial.print(len < sizeof(buf) ? len : sizeof(buf));
                            Serial.print(" ");
                            Serial.print(chunk);
                            Serial.print(" ");
                            Serial.println(numchunks);
                            mode = MODE_WRITECHUNK;
                        }
                    }
                } else if (v != '\r') {
                    Serial.println("ERROR Bad command, invalid length");
                    pos = len = mode = 0;
                }
            } else if (mode == MODE_WRITECHUNK) {
                buf[pos++] = v;
                len--;
                if (pos == sizeof(buf) || len == 0) {
                    file.write(buf, pos);
                    chunk++;
                    pos = 0;
                    if (len == 0) {
                        Serial.println("OK");
                        pos = len = mode = chunk = numchunks = 0;
                    } else {
                        Serial.print("WRITE ");
                        Serial.print(len < sizeof(buf) ? len : sizeof(buf));
                        Serial.print(" ");
                        Serial.print(chunk);
                        Serial.print(" ");
                        Serial.println(numchunks);
                    }
                }
            }
        }
    }
    Serial.println("RESETTING");
}
