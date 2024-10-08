#!/usr/bin/env node

// Requires "npm install serialport" first

const { SerialPort } = require("serialport");
const path = require("path");
const fs = require("fs");
const maxlength = 1024;

let command, portname, filenames = [];
for (let i=2;i<process.argv.length;i++) {
    let v = process.argv[i];
    if (v == "--port" && i + 1 < process.argv.length && !portname) {
        portname = process.argv[++i];
    } else if (!command && (v == "write" || v == "erase" || v == "id" || v == "read" || v == "dir" || v == "help")) {
        command = v;
    } else if (command == "write") {
        filenames.push(v);
    } else if (command == "read" && filenames.length == 0) {
        filenames.push(v);
    } else {
        command = null;
        break;
    }
}
if (!command) {
    console.log("Usage: serialflash.js [--port <portname>] <command> <params...>");
    console.log(" Where command is id, dir, erase read, write");
    console.log(" * read takes one filename parameter, and downloads that file");
    console.log(" * write takes one or more filename parameters, and uploads that file (dropping directory from stored name)");
    process.exit(0);
}
const port = new SerialPort({ path: portname, baudRate: 9600 })

let outfile, outfilesize, outfilepos;   // For "read" command
let inprefix, inbuffer;   // For "write" command
let line = "";

port.on("data", data => {
    let pos = 0;
    while (pos < data.length) {
        if (outfile) {
            let l = Math.min(data.length - pos, outfilesize - outfilepos);
            fs.writeSync(outfile, data, pos, l);
            outfilepos += l;
            pos += l;
            if (outfilepos == outfilesize) {
                fs.closeSync(outfile);
                outfile = 0;
                console.log();
                line = "";
            }
        } else {
            let c = String.fromCharCode(data[pos++]);
            if (c == '\n') {
                line = line.replace(/\r$/, "");
                // OK - send next commend to upload, eg "write filename length\n"
                // WRITE n chunkix numchunks - a request to send "n" bytes (chunkix and numchunks are informative).
                // READ n - followed by "n" bytes to write to file
                // ERROR msg - print the msg
                // Anything else, just print it out
//                console.log("READ \"" + line + "\"");
                if (line == "OK") {
                    if (command == null) {
                        port.close();
                    } else if (command == "id") {
                        port.write("id\n");
                        command = null;
                    } else if (command == "erase") {
                        port.write("erase\n");
                        command = null;
                    } else if (command == "dir") {
                        port.write("dir\n");
                        command = null;
                    } else if (command == "read") {
                        port.write("read " + filenames[0] + "\n");
                        command = null;
                    } else if (command == "write") {
                        let filename = filenames.shift();
                        if (filename) {
                            inbuffer = fs.readFileSync(filename);
                            inprefix = "Sending \"" + filename + "\" (" + inbuffer.length + " bytes): ";
                            filename = path.basename(filename);
                            port.write("write " + filename + " " + inbuffer.length + "\r\n");
                        } else {
                            command = null;
                            port.close();
                        }
                    }
                } else if (line.startsWith("WRITE ")) {
                    let vals = line.split(/ /);
                    let blocksize = parseInt(vals[1]);
                    let chunk = parseInt(vals[2]) + 1;
                    let numchunks = parseInt(vals[3]);
                    let buf0 = inbuffer.subarray(0, blocksize);
                    inbuffer = inbuffer.length == blocksize ? null : inbuffer.subarray(blocksize);
                    port.write(buf0);
                    process.stdout.write("\r" + inprefix  + " " + (chunk * 100 / numchunks).toFixed(0) + "%   ");
                    if (!inbuffer) {
                        process.stdout.write("\n");
                    }
                } else if (line.startsWith("READ ")) {
                    let vals = line.split(/ /);
                    let filename = filenames[0];
                    outfilepos = 0;
                    outfilesize = parseInt(vals[1]);
                    outfile = fs.openSync(filename, "w");
                    line = "Reading \"" + filename + "\" (" + outfilesize + " bytes): ";
                } else if (line.startsWith("ERROR")) {
                    console.log(line);
                    command = null;
                    port.close();
                } else {
                    console.log(line);
                }
                line = "";
            } else {
                line += c;
            }
        }
    }
});
