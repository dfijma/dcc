package net.fijma.serial;

import gnu.io.NRSerialPort;
import gnu.io.SerialPortEvent;
import gnu.io.SerialPortEventListener;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;

public class Serial implements SerialPortEventListener {

    public final Event<Integer> byteAvailable = new Event<>(); // TODO: that is a bit expensive

    private DataInputStream ins;
    private DataOutputStream outs;
    private NRSerialPort serial = null;

    public static void probe() {
        // list serial ports
        for (String s: NRSerialPort.getAvailableSerialPorts()){
            System.out.println(s);
        }
    }

    public void start(String device) throws Exception {

        // TODO: remove hardcoded stuff
        int baudRate = 57600; // 115200; //

        // open serial port
        serial = new NRSerialPort(device, baudRate);
        serial.connect();

        // and read data in asynchronous fashion
        ins = new DataInputStream(serial.getInputStream());
        outs = new DataOutputStream(serial.getOutputStream());
        serial.addEventListener(this);
    }

    public void write(String s) throws IOException {
        outs.writeUTF(s);
    }

    public void stop() {
        if (serial != null) serial.disconnect();
    }

    @Override
    public void serialEvent(SerialPortEvent ev) {
        try {
            for (;;) {
                int b = ins.read();
                if (b < 0) break; // no more date available (yet)
                byteAvailable.trigger(b);
            }
        } catch (IOException e) {
            // brr
        }
    }
}
