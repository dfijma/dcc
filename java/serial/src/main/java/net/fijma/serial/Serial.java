package net.fijma.serial;

import com.fazecast.jSerialComm.*;
import java.io.IOException;
import java.nio.charset.Charset;

public class Serial extends AbstractSerial implements SerialPortDataListener {

    public final Event<String> lineAvailable = new Event<>();

    private SerialPort serial = null;

    static void probe() {
        // list serial ports
        for (SerialPort s: SerialPort.getCommPorts()){
            System.out.println(s.getSystemPortName());
        }
    }

    void start(String device) throws Exception {

        // TODO: remove hardcoded stuff
        int baudRate = 57600; // 115200; //

        // open serial port and read data in asynchronous fashion
        serial = SerialPort.getCommPort(device);
        serial.setComPortParameters(baudRate, 8, 1, SerialPort.NO_PARITY);
        serial.addDataListener(this);
        if (!serial.openPort()) throw new Exception("cannot open serial port");

    }

    @Override
    public void write(String s) throws IOException {
        byte[] bs = s.getBytes(Charset.forName("ASCII"));  // This is the eighties, we don't do UTF-8
        serial.writeBytes(bs, bs.length);
    }

    void stop() {
        if (serial != null) serial.closePort();
    }

    @Override
    public void serialEvent(SerialPortEvent event) {
        if (event.getEventType() != SerialPort.LISTENING_EVENT_DATA_AVAILABLE) return;
        byte[] newData = new byte[serial.bytesAvailable()];
        int numRead = serial.readBytes(newData, newData.length);
        for (int i=0; i<numRead; ++i) {
            this.onSerialByte(newData[i]);
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

    public int getListeningEvents() {
        return SerialPort.LISTENING_EVENT_DATA_AVAILABLE;
    }
}
