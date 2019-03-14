package net.fijma.serial.tui;

import net.fijma.mvc.Application;
import net.fijma.mvc.Controller;

import net.fijma.mvc.Msg;
import net.fijma.mvc.serial.Serial;
import net.fijma.serial.Loconet;
import net.fijma.serial.Main;
import net.fijma.serial.model.Model;

public class SerialController extends Controller<Main, Model, MainView> {

    private Loconet ln = new Loconet(); // TODO: loconet onMsg's should run through app queue, too

    public SerialController(Main app, Model model, MainView view, ThrottleView leftView, ThrottleView rightView) {
        super(app, model, view);

        ln.decoded.attach(model::onMsg);
        // wire model -> views
        model.throttleChanged.attach(leftView::onUpdate);
        model.throttleChanged.attach(rightView::onUpdate);
        model.msg.attach(view::onMsg);
        model.powerChanged.attach(view::onPowerChanged);

        // wire views -> controller
        leftView.up.attach(this::onUp);
        leftView.down.attach(this::onDown);
        leftView.sw.attach(this::onSwitch);
        leftView.fn.attach(this::onFn);
        rightView.up.attach(this::onUp);
        rightView.down.attach(this::onDown);
        rightView.sw.attach(this::onSwitch);
        rightView.fn.attach(this::onFn);
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

    @Override
    protected boolean onEvent(Msg msg) {
        if (msg instanceof Application.KeyMsg) {
            return mainView.key(((Application.KeyMsg) msg).key);
        } else if (msg instanceof Serial.SerialMsg) {
            String line = ((Serial.SerialMsg) msg).line;
            if (line.startsWith("LN")) {
                // Loconet packet received, split and parse bytes
                for (String s : line.split(" ")) {
                    try {
                        ln.pushByte(Integer.parseInt(s, 16));
                    } catch (NumberFormatException ignored) { }
                }
            } else if (line.startsWith("POFF")) {
                model.onMsg("power overload, switched off");
                model.setPower(false);
            } else if (line.contains("HELO")) {
                // contains, not "startsWith", as initial message sometimes start with one or two chars of garbage
                model.onMsg("controller initialized");
            } else {
                model.onMsg(line);
            }
        }
        return true;
    }
}
