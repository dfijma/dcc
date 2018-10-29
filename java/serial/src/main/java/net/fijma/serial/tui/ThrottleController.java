package net.fijma.serial.tui;

import net.fijma.serial.Model.Model;


public class ThrottleController {

    private final Model model;
    private final View view;

    public ThrottleController(Model model, View view) {
        this.model = model;
        this.view = view;
    }

    public void onDown(Model.Throttle throttle) {
        throttle.setSpeed(throttle.getSpeed() - 1);
    }

    public void onUp(Model.Throttle throttle) {
        throttle.setSpeed(throttle.getSpeed() + 1);
    }

    public  void onSwitch(Model.Throttle throttle) {
        throttle.switchDirection();
    }

    public void OnFn(ThrottleView.FnEvent e) {
        e.throttle.toggleFunction(e.fn);
    }

    public void run() {
        view.run();
    }

    public static ThrottleController setup(Model model) {

        // create views
        ThrottleView leftView = new ThrottleView(model, 0);
        ThrottleView rightView = new ThrottleView(model, 1);
        View view = new View(leftView, rightView);

        // wire model -> views
        model.changed.attach(leftView::onUpdate);
        model.changed.attach(rightView::onUpdate);

        // create controller
        ThrottleController controller = new ThrottleController(model, view);

        // wire views -> controller
        leftView.up.attach(controller::onUp);
        leftView.down.attach(controller::onDown);
        leftView.sw.attach(controller::onSwitch);
        rightView.up.attach(controller::onUp);
        rightView.down.attach(controller::onDown);
        rightView.sw.attach(controller::onSwitch);

        return controller;
    }

}
