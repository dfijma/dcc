package net.fijma.serial;

import org.junit.Test;
import static org.junit.Assert.*;

public class MainTest {
    @Test public void testMain() {
        //Main m = new Main();
        //assertTrue("Main::hello should return 'true'", m.hello());
        int x = 1;
        System.out.println(Integer.toBinaryString(x));
        x = 128;
        System.out.println(String.format("%05x", x));

    }
}
