package net.fijma.serial.Model;

import net.fijma.serial.Event;
import net.fijma.serial.Serial;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class Model {

    public static final int SLOTS = 2;
    public static final int FUNCTIONS = 13; // F0-F12

    public final Event<Throttle> throttleChanged = new Event<>();
    public final Event<List<String>> msg = new Event<>();
    public final Event<Integer> loconetByte = new Event<>();

    // Very nice discussion of a basic implementation of MVC in JavaAhumScript:
    // https://medium.com/@ToddZebert/a-walk-through-of-a-simple-javascript-mvc-implementation-c188a69138dc

    // basically, model is number of throttles and a msg ring
    private final Throttle[] slots = new Throttle[SLOTS];
    private final ArrayList<String> msgs = new ArrayList<>();
    private final Serial serial;

    public Model(Serial serial) {
        // attach to receive byts from serial port
        this.serial = serial;
        serial.byteAvailable.attach(this::onSerialByte);
    }

    public Throttle getThrottleFor(int address) {
        // get or create throttle for address, while SLOTS last
        var i=0;
        while (i<SLOTS && slots[i] != null && slots[i].address!= address) ++i;
        if (i>=SLOTS) {
            // no more free slots
            msg("NO FREE SLOTS");
            return errorThrottle;
        };
        if (slots[i] == null) {
            slots[i] = new Throttle(i, address);
            // initial update of refresh buffer
            throttelChanged(slots[i]);
        }
        assert(slots[i].address == address);
        return slots[i];
    }

    // keep incoming serial data and decode to string, display as msg on complete
    private StringBuilder serialString = new StringBuilder();

    private void onSerialByte(int b) {
        // decode incoming serial bytes as ASCII (yes, no fancy UTF-8 stuff expected)
        if (b == 10) return;
        if (b == 13) {
            String m = serialString.toString();
            if (m.startsWith("LN")) {
                // Loconet packet received, split and parse bytes
                for (String s: m.split(" ")) {
                    try {
                        loconetByte.trigger(Integer.parseInt(s, 16));
                    } catch (NumberFormatException ignored) {}
                }
            } else if (m.startsWith("POFF")) {
                // overload detected
            } else if (m.startsWith("HELO")) {
                // initialized
            } else {
                // everything else, at least for now, it threated as msg that is printed
                msg(m);
            }
            serialString = new StringBuilder();
            return;
        }
        serialString.append((char)b);
    }

    // add msg
    public void msg(String s) {
        msgs.add(s);
        msg.trigger(msgs);
    }

    // Poor little controller expects order FL-F4-F3-F2-F1-F8-F7-F6-F5-F12-F11-F10-F9
    // We have them in order FL-F1-F2-F3-F4-F5-F6-F7-F8-F9-F10-F11-F12
    // Let us do the heavy lifting
    static private final int functionBitReorder[] = {
            0,
            4,
            3,
            2,
            1,
            8,
            7,
            6,
            5,
            12,
            11,
            10,
            9
    };

    private void throttelChanged(Throttle t) {
        throttleChanged.trigger(t);
        try {
            // send S ("SLOT") cmd
            StringBuilder sb = new StringBuilder();
            sb.append("S").append(t.slot).append(" ").append(t.address).append(" ").append(t.speed)
                    .append(" ").append(t.direction ? "1" : "0").append(" ");
            for (int i=0; i< FUNCTIONS; ++i) sb.append(t.f[functionBitReorder[i]] ? "1" : "0");
            msg(sb.toString());
            sb.append("\n");
            serial.write(sb.toString());
        } catch (IOException e) {
            msg("IO ERROR");
        }
    }

    public class Throttle {

        // a single trottle
        public final int slot; // slot
        public final int address; // loco

        private int speed; // 0..126
        private boolean emergency;
        private boolean direction;
        private boolean[] f = new boolean[FUNCTIONS];

        Throttle(int slot, int address) {
            this.slot = slot;
            this.address = address;
            speed = 0;
            direction = true;
            for (var i=0; i<FUNCTIONS; ++i) {
                f[i] = false;
            }
        }

        public boolean hasError() { return address <= 0; }

        public int getSpeed() {
            return speed;
        }

        public void setSpeed(int x) {
            speed = x;
            if (speed < 0) speed = 0;
            if (speed > 126) speed = 126;
            Model.this.throttelChanged(this);
        }

        public boolean getDirection() {
            return direction;
        }

        public void switchDirection() {
            direction = !direction;
            Model.this.throttelChanged(this);
        }

        public void toggleFunction(int i) {
            f[i] = !f[i];
            Model.this.throttelChanged(this);
        }

        public boolean getFunction(int i) {
            return f[i];
        }

        @Override
        public String toString() {
            StringBuilder sb = new StringBuilder();
            sb.append("loco:").append(address);
            sb.append(",speed:").append(Math.abs(speed));
            sb.append(",direction:");
            if (direction) {
                sb.append("forward");
            } else {
                sb.append("backward");
            }
            if (emergency) {
                sb.append(",emergency");
            }
            for (int i=0; i<FUNCTIONS; ++i) {
                sb.append(",F").append(i);
                if (f[i]) {
                    sb.append("-1");
                } else {
                    sb.append("-0");
                }
            }
            return sb.toString();
        }
    }

    public final Throttle errorThrottle = new Throttle(0, 0);

}
