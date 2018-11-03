package net.fijma.serial;

import gnu.io.NRSerialPort;
import gnu.io.SerialPortEvent;
import gnu.io.SerialPortEventListener;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.nio.charset.Charset;
import java.util.TooManyListenersException;

public class Serial implements SerialPortEventListener {

    public final Event<String> lineAvailable = new Event<>();

    private DataInputStream ins;
    private DataOutputStream outs;
    private NRSerialPort serial = null;

    static void probe() {
        // list serial ports
        for (String s: NRSerialPort.getAvailableSerialPorts()){
            System.out.println(s);
        }
    }

    void start(String device)  {

        // TODO: remove hardcoded stuff
        int baudRate = 57600; // 115200; //

        // open serial port
        serial = new NRSerialPort(device, baudRate);
        serial.connect();

        // and read data in asynchronous fashion
        ins = new DataInputStream(serial.getInputStream());
        outs = new DataOutputStream(serial.getOutputStream());
        try {
            serial.addEventListener(this);
        } catch (TooManyListenersException e) {
            throw new RuntimeException(e);
        }
    }

    public void write(String s) throws IOException {
        outs.write(s.getBytes(Charset.forName("ASCII")));
        // outs.writeUTF(s);
    }

    void stop() {
        if (serial != null) serial.disconnect();
    }

    @Override
    public void serialEvent(SerialPortEvent ev) {
        try {
            for (;;) {
                int b = ins.read();
                if (b < 0) break; // no more date available (yet)
                onSerialByte(b);
            }
        } catch (IOException e) {
            // brr
        }
    }

    // keep incoming serial data and decode to string, display as msg on complete
    private StringBuilder serialString = new StringBuilder();

    private void onSerialByte(int b) {
        // decode incoming serial bytes as ASCII (yes, no fancy UTF-8 stuff expected)
        if (b == 10) return;
        if (b == 13) {
            lineAvailable.trigger(serialString.toString());
            serialString = new StringBuilder();
            return;
        }
        serialString.append((char)b);
    }
}
