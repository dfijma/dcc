package net.fijma.serial.tui;

import net.fijma.serial.Event;
import net.fijma.serial.Loconet;
import net.fijma.serial.model.Model;
import net.fijma.serial.Serial;

import java.util.Queue;
import java.util.concurrent.ConcurrentLinkedQueue;

public class Controller {

    private final Model model;
    private final MainView view;
    private Loconet ln = new Loconet();

    private Controller(Model model, MainView view, Event<Integer> key, Serial serial) {
        this.model = model;
        this.view = view;
        key.attach(this::onKey);
        // say that we like to know complete lines received on the serial line
        serial.lineAvailable.attach(this::onSerialLine);
        // when full loconet message is available, ...
        // TODO: do something more than msg'ing the loconet msg
        ln.decoded.attach(model::msg);
    }

    private void onDown(Model.Throttle throttle) {
        throttle.setSpeed(throttle.getSpeed() - 1);
    }

    private void onUp(Model.Throttle throttle) {
        throttle.setSpeed(throttle.getSpeed() + 1);
    }

    private void onSwitch(Model.Throttle throttle) {
        throttle.switchDirection();
    }

    private void onFn(ThrottleView.FnEvent e) {
        e.throttle.toggleFunction(e.fn);
    }

    public void run() {
        view.clear();
        view.draw();
        boolean r = true;
        while (r) {
            try {
                Msg msg = msgQueue.poll();
                if (msg == null) {
                    Thread.sleep(10);
                    continue;
                }
                if (msg instanceof KeyMsg) {
                    r = view.key(((KeyMsg)msg).key);
                } else if (msg instanceof SerialMsg) {
                    String line = ((SerialMsg)msg).line;
                    if (line.startsWith("LN")) {
                        // Loconet packet received, split and parse bytes
                        for (String s: line.split(" ")) {
                            try {
                                ln.pushByte(Integer.parseInt(s, 16));
                            } catch (NumberFormatException ignored) {}
                        }
                    } else if (line.startsWith("POFF")) {
                            model.msg("power overload, switched off");
                            model.setPower(false);
                    } else if (line.contains("HELO")) {
                            // contains, not "startsWith", as initial message sometimes start with one or two chars of garbage
                            model.msg("controller initialized");
                    } else {
                        model.msg(line);
                    }
                }
            } catch (InterruptedException ignored) {  }
        }
    }

    public static Controller setup(Model model, Event<Integer> key, Serial serial) {

        // create views
        ThrottleView leftView = new ThrottleView(model, 0);
        ThrottleView rightView = new ThrottleView(model, 1);
        MainView view = new MainView(model, leftView, rightView);

        // wire model -> views
        model.throttleChanged.attach(leftView::onUpdate);
        model.throttleChanged.attach(rightView::onUpdate);
        model.msg.attach(view::onMsg);
        model.powerChanged.attach(view::onPowerChanged);

        // create controller
        Controller controller = new Controller(model, view, key, serial);

        // wire views -> controller
        leftView.up.attach(controller::onUp);
        leftView.down.attach(controller::onDown);
        leftView.sw.attach(controller::onSwitch);
        leftView.fn.attach(controller::onFn);
        rightView.up.attach(controller::onUp);
        rightView.down.attach(controller::onDown);
        rightView.sw.attach(controller::onSwitch);
        rightView.fn.attach(controller::onFn);

        return controller;
    }

    static class Msg {

    }

    static class KeyMsg extends Msg {
        final Integer key;
        KeyMsg(Integer key) { this.key = key; }
    }

    static class SerialMsg extends Msg {
        final String line;
        SerialMsg(String line) { this.line = line; }
    }

    private Queue<Msg> msgQueue = new ConcurrentLinkedQueue<>();

    private void onKey(Integer key) {
        // post keypress to msg queue
        msgQueue.offer(new KeyMsg(key));
    }

    private void onSerialLine(String line) {
        // post available serial line to msg queue
        msgQueue.offer(new SerialMsg(line));
    }

}
