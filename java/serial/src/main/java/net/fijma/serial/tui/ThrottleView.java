package net.fijma.serial.tui;

import net.fijma.serial.Event;
import net.fijma.serial.Model.Model;

public class ThrottleView extends AbstractView {

    final Model model;
    public final Event<Model.Throttle> up = new Event<>();
    public final Event<Model.Throttle> down = new Event<>();
    public final Event<Model.Throttle> sw = new Event<>();
    public final Event<FnEvent> fn = new Event<>();


    static class FnEvent{
        public final Model.Throttle throttle;
        public final int fn;
        public FnEvent (Model.Throttle throttle, int fn) {
            this.throttle = throttle;
            this.fn = fn;
        }
    }
    final int pos;

    private Model.Throttle throttle = null;
    private int address = 0;
    private int nextFunctionShift = 0;

    public ThrottleView(Model model, int pos) {
        this.model = model;
        this.pos = pos;
    }

    public void draw() {
        green();
        /*
        01234567890
        +===+=+
        |999|^|
        +===+=+
         */
        int base = 10+pos*20;

        setRC(1,base+1);  System.out.print("┏━━━━┳━━┓");
        setRC(2,base+1); System.out.print("┃0000┃--┃");
        setRC(3,base+1);  System.out.print("┣━━━┳┻┳━┫");
        setRC(4,base+1);  System.out.print("┃---┃-┃-┃");
        setRC(5,base+1);  System.out.print("┣━┳━╋━╋━┫");
        for (int i=0; i<3; i++) {
            setRC(6+(i*2),base+1); System.out.print("┃-┃-┃-┃-┃");
            if (i < 2) {
                setRC(7 + (i * 2), base+1); System.out.print("┣━╋━╋━╋━┫");
            } else {
                setRC(7+(i*2),base+1); System.out.print("┗━┻━┻━┻━┛");
            }
        }

        if (throttle == null || throttle.hasError()) {
            // no throttle, let user enter one
            setRC(2,base+2);
            if (address == 0) {
                System.out.println("   ?");
            } else {
                System.out.print(String.format("%4d", address));
            }
            if (throttle != null) {
                // throttle has error (probably no slots available)
                setRC(2, base+7);
                System.out.print("Err");
            }
        } else {
            setRC(2, base+2);
            System.out.print(String.format("%4d", throttle.address));
            setRC(2, base+7);
            System.out.print(String.format("%2d", throttle.slot));

            setRC(4, base+2);
            System.out.print(String.format("%3d", throttle.getSpeed()));
            setRC(4, base+6);
            if (throttle.getDirection()) {
                System.out.print("\u2b06"); // upwards black arrow
            } else {
                System.out.print("\u2b07"); // downwards black arrow
            }
            setRC(4, base+8);
            System.out.print(throttle.getFunction(0) ? "✓" : "𐄂");

            for (int i = 0; i < 3; i++) {
                for (int j = 1; j <= 4; j++) {
                    setRC(6 + (i * 2), base+2 * j);
                    System.out.print(throttle.getFunction(j + i * 4) ? "✓" : "𐄂");
                }
            }
        }
    }

    public void onUpdate(Model.Throttle throttle) {
        // model notifies update
        if (throttle == this.throttle) {
            draw();
        }
    }

    public boolean key(int k) {

        if (throttle == null || throttle.hasError()) {
            setRC(20, 2);
            if (k>='0' && k<='9') {
                int newAddress = 10 *address + k - '0';
                if (newAddress >=0 && newAddress <= 9999) {
                    address = newAddress;
                }
            } else if (k==127) {
                address = address / 10;
            } else if (k==10) {
                throttle = model.getThrottleFor(address);
            }
            draw();
        } else {
            int functionShift = 0;
            switch (k) {
                case 48: // 0
                    fn.trigger(new FnEvent(throttle, 0));
                    break;
                case 49:
                    fn.trigger(new FnEvent(throttle, 1+ nextFunctionShift * 4));
                    break;
                case 50:
                    fn.trigger(new FnEvent(throttle, 2+ nextFunctionShift * 4));
                    break;
                case 51:
                    fn.trigger(new FnEvent(throttle, 3+ nextFunctionShift * 4));
                    break;
                case 52:
                    fn.trigger(new FnEvent(throttle, 4+ nextFunctionShift * 4));
                    break;
                case 74:
                case 106: // H
                    down.trigger(throttle);
                    break;
                case 75:
                case 107: // J
                    up.trigger(throttle);
                    break;
                case 76: // L
                case 108:
                    throttle = null;
                    address = 0;
                    draw();
                    break;
                case 81: //Q
                case 113:
                    functionShift = 0;
                    break;
                case 87: // W
                case 119:
                    functionShift = 1;
                    break;
                case 69: // E
                case 101:
                    functionShift = 2;
                case 32: // Space, toggle direction
                    sw.trigger(throttle);
                    break;
            }
            nextFunctionShift = functionShift;
        }
        return true;
    }
}
