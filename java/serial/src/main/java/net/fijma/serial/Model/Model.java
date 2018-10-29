package net.fijma.serial.Model;

import net.fijma.serial.Event;

public class Model {

    public static final int SLOTS = 10;
    public static final int FUNCTIONS = 13; // F0-F12
    public final Event<Throttle> changed = new Event<>();

    // https://medium.com/@ToddZebert/a-walk-through-of-a-simple-javascript-mvc-implementation-c188a69138dc

    private Throttle[] slots = new Throttle[SLOTS];

    public Model() { }

    public Throttle getThrottleFor(int address) {
        var i=0;
        while (i<SLOTS && slots[i] != null && slots[i].address!= address) ++i;
        if (i>=SLOTS) return errorThrottle; // no more free slots
        if (slots[i] == null) slots[i] = new Throttle(this, i, address);
        assert(slots[i].address == address);
        return slots[i];
    }

    public static class Throttle {

        // a single trottle
        public final Model model;
        public final int slot; // slot
        public final int address; // loco

        private int speed; // -126..126
        private boolean emergency;
        private boolean direction;
        private boolean[] f = new boolean[FUNCTIONS];

        Throttle(Model model, int slot, int address) {
            this.model = model;
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
            model.changed.trigger(this);
        }

        public boolean getDirection() {
            return direction;
        }

        public void switchDirection() {
            direction = !direction;
            model.changed.trigger(this);
        }

        public void toggleFunction(int i) {
            f[i] = !f[i];
            model.changed.trigger(this);
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

    public final Throttle errorThrottle = new Throttle(this, 0, 0);

}
