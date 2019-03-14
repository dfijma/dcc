package net.fijma.serial.model;

import net.fijma.mvc.Event;
import net.fijma.mvc.serial.Serial;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class Model {

    private static final int NSLOTS = 2;
    private static final int FUNCTIONS = 13; // F0-F12

    private static final String NO_SLOTS = "No free slots";
    private static final String IO_ERROR = "IO error";

    public final Serial serial;
    public final Event<Throttle> throttleChanged = new Event<>();
    public final Event<List<String>> msg = new Event<>();
    public final Event<Boolean> powerChanged = new Event<>();

    // Very nice discussion of a basic implementation of MVC in JavaAhumScript:
    // https://medium.com/@ToddZebert/a-walk-through-of-a-simple-javascript-mvc-implementation-c188a69138dc

    // basically, model is number of throttles and a onMsg ring
    private final Throttle[] slots = new Throttle[NSLOTS];
    private final ArrayList<String> msgs = new ArrayList<>();
    private boolean power = false;

    public Model(Serial serial) {
        this.serial = serial;
    }

    public Throttle getThrottleFor(int address) {
        // get or create throttle for address, while SLOTS last
        int i=0;
        while (i<NSLOTS && slots[i] != null && slots[i].address!= address) ++i;
        if (i>=NSLOTS) {
            // no more free slots
            onMsg(NO_SLOTS);
            return errorThrottle;
        }
        if (slots[i] == null) {
            slots[i] = new Throttle(i, address);
            // initial update of refresh buffer
            throttleChanged(slots[i]);
        }
        assert(slots[i].address == address);
        return slots[i];
    }

    // add msg
    public void onMsg(String s) {
        if (msgs.size() > 100) msgs.remove(msgs.size()-1);
        msgs.add(0, s);
        msg.trigger(msgs);
    }

    public boolean power() { return power; }

    public void setPower(boolean power) {
        if (this.power == power) return;
        this.power = power;

        try {
            // send S ("SLOT") cmd
            StringBuilder sb = new StringBuilder();
            if (this.power) {
                sb.append("P"); // power on command
            } else {
                sb.append("O"); // power off command
            }
            onMsg(sb.toString());
            sb.append("\n");
            serial.write(sb.toString());
        } catch (IOException e) {
            onMsg(IO_ERROR);
        }

        powerChanged.trigger(this.power);
    }

    public void set_OPC_SW_REQ(int adr, boolean dir, boolean on) {
        try {
            StringBuilder sb = new StringBuilder("L B0 ");
            int sw1 = adr / 16; // high bits of address
            int sw2 = adr % 16; // low bits of address
            if (dir) {
                sw2 = sw2 | 0b0010_0000;
            }
            if (on) {
                sw2 = sw2 | 0b0001_0000;
            }
            sb.append(String.format("%02X", sw1)).append(" ").append(String.format("%02X", sw2));
            onMsg(sb.toString());
            sb.append("\n");
            serial.write(sb.toString());
        } catch (IOException e) {
            onMsg(IO_ERROR);
        }
    }

    // Poor little controller expects order FL-F4-F3-F2-F1-F8-F7-F6-F5-F12-F11-F10-F9
    // We have them in order FL-F1-F2-F3-F4-F5-F6-F7-F8-F9-F10-F11-F12
    // Let us do the heavy lifting
    private static final int[] functionBitReorder = {
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

    private void throttleChanged(Throttle t) {
        throttleChanged.trigger(t);
        try {
            // send S ("SLOT") cmd
            StringBuilder sb = new StringBuilder();
            sb.append("S").append(t.slot).append(" ").append(t.address).append(" ").append(t.speed)
                    .append(" ").append(t.direction ? "1" : "0").append(" ");
            for (int i=0; i< FUNCTIONS; ++i) sb.append(t.f[functionBitReorder[i]] ? "1" : "0");
            onMsg(sb.toString());
            sb.append("\n");
            serial.write(sb.toString());
        } catch (IOException e) {
            onMsg(IO_ERROR);
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
            for (int i=0; i<FUNCTIONS; ++i) {
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
            Model.this.throttleChanged(this);
        }

        public boolean getDirection() {
            return direction;
        }

        public void switchDirection() {
            direction = !direction;
            Model.this.throttleChanged(this);
        }

        public void toggleFunction(int i) {
            f[i] = !f[i];
            Model.this.throttleChanged(this);
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

    private final Throttle errorThrottle = new Throttle(0, 0);

}
