package net.fijma.serial;

public class MockSerial extends AbstractSerial {

    public void write(String s) {
        System.out.println("mock serial port writing: " + s);
    }
}
