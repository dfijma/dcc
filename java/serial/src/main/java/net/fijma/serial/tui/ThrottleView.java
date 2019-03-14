package net.fijma.serial.tui;

import net.fijma.mvc.Event;
import net.fijma.mvc.View;
import net.fijma.serial.model.Model;

public class ThrottleView extends View<Model> {

    final Event<Model.Throttle> up = new Event<>();
    final Event<Model.Throttle> down = new Event<>();
    final Event<Model.Throttle> sw = new Event<>();
    final Event<FnEvent> fn = new Event<>();


    static class FnEvent{
        final Model.Throttle throttle;
        final int fn;
        FnEvent (Model.Throttle throttle, int fn) {
            this.throttle = throttle;
            this.fn = fn;
        }
    }
    private final int pos;

    private Model.Throttle throttle = null;
    private int address = 0;
    private int nextFunctionShift = 0;

    public ThrottleView(Model model, int pos) {
        super(model);
        this.pos = pos;
    }

    private String colorCheck(boolean b) {
        if (b) {
            return green() +"✓" + reset();
        } else {
            return red() + "𐄂" + reset();
        }
    }
    public void draw() {
        int baseCol = 1+pos*59;

        final String[] pict = new String[] {
"+---------+---------+",
"| adr ----|  sl --  |",
"+---------+----+----+",
"| spd --- | -  |FL- |",
"+----+----+----+----+",
"|F01-|F02-|F03-|F04-|",
"+----+----+----+----+",
"|F05-|F06-|F07-|F08-|",
"+----+----+----+----+",
"|F09-|F10-|F11-|F12-|",
"+----+----+----+----+"          };

        int addressColOffset = 6;
        int slotColOffset = 16;
        int speedColOffset = 6;
        int directionColOffset = 12;
        int flColOffset = 18;
        int[] fColOffset = new int[] { 4, 9, 14, 19};

        for (int i=1; i<= pict.length; ++i) {
            setRC(i, baseCol);
            System.out.print(pict[i-1]);
        }

        if (throttle == null || throttle.hasError()) {
            // no throttle, let user enter one
            setRC(2,baseCol+addressColOffset);
            if (address == 0) {
                System.out.println("   ?");
            } else {
                System.out.print(String.format("%4d", address));
            }
            if (throttle != null) {
                // throttle has error (probably no slots available)
                setRC(2, baseCol+slotColOffset);
                System.out.print("EE");
            }
        } else {
            setRC(2, baseCol+addressColOffset);
            System.out.print(String.format("%4d", throttle.address));
            setRC(2, baseCol+slotColOffset);
            System.out.print(String.format("%2d", throttle.slot));

            setRC(4, baseCol+speedColOffset);
            System.out.print(String.format("%3d", throttle.getSpeed()));
            setRC(4, baseCol+directionColOffset);
            if (throttle.getDirection()) {
                System.out.print("\u2b06"); // upwards black arrow
            } else {
                System.out.print("\u2b07"); // downwards black arrow
            }
            setRC(4, baseCol+flColOffset);
            System.out.print(colorCheck(throttle.getFunction(0)));

            for (int i = 0; i < 3; i++) {
                for (int j = 1; j <= 4; j++) {
                    setRC(6 + (i * 2), baseCol + fColOffset[j-1]);
                    System.out.print(colorCheck(throttle.getFunction(j + i * 4)));
                }
            }
        }
    }

    void onUpdate(Model.Throttle throttle) {
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
                    break;
                case 32: // Space, toggle direction
                    sw.trigger(throttle);
                    break;
                default:
                    // intentionally left empty
                    break;
            }
            nextFunctionShift = functionShift;
        }
        return true;
    }
}
