package net.fijma.serial;

import net.fijma.mvc.serial.Serial;
import net.fijma.serial.model.Model;
import org.junit.Test;
import static org.mockito.Mockito.*;

import java.io.IOException;
import java.util.concurrent.atomic.AtomicInteger;

import static org.junit.Assert.*;

public class MainTest {

    @Test public void testMain() throws IOException {

        Serial s = mock(Serial.class);
        doNothing().when(s).write(anyString());

        Model m = new Model(s);
        Model.Throttle t = m.getThrottleFor(23);
        assertEquals("throttle address",  23, t.address);

        AtomicInteger changedSpeed = new AtomicInteger();
        m.throttleChanged.attach((throttle) -> {
            changedSpeed.set(throttle.getSpeed());
        });

        t.setSpeed(10);
        assertEquals("speed change", changedSpeed.get(), 10);

        t.toggleFunction(0);
        t.toggleFunction(3);

        assertEquals("throttle status", "loco:23,speed:10,direction:forward,F0-1,F1-0,F2-0,F3-1,F4-0,F5-0,F6-0,F7-0,F8-0,F9-0,F10-0,F11-0,F12-0", t.toString());
    }
}
