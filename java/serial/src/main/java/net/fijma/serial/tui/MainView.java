package net.fijma.serial.tui;

import net.fijma.mvc.View;
import net.fijma.serial.model.Model;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class MainView extends View<Model> {

    // aggregation combination of views
    private final List<View> views;
    private int currentView;

    public MainView(Model model, View... views) {
        super(model);
        this.views = new ArrayList<>();
        Collections.addAll(this.views, views);
        if (this.views.isEmpty()) throw new IllegalArgumentException("at least on subview required");
        currentView = 0;
    }

    void onPowerChanged(Boolean power) {
        setRC(2, 42);
        if (power) { System.out.print(green() + " ON" + reset()); } else { System.out.print(red() + "OFF" +  reset()); }
    }

    void onMsg(List<String> msg) {
        // print at most 10 messages
        int c = Math.min(10, msg.size());
        for (int i=0; i<c; i++) {
            setRC(13+i, 2);
            String l = String.format("%-78s", msg.get(c-i-1));
            l = l.substring(0, Math.min(l.length(), 78));
            System.out.print(l);
        }
    }

    @Override
    public void draw() {
        // TODO: read this from file
        final String[] pict = new String[] {
"+---------------------------------+----------+---------------------------------+",
"|                                 | pwr: ONF |                                 |",
"|                                 +----------+                                 |",
"|                                                                              |",
"|                                                                              |",
"|                                                                              |",
"|                                                                              |",
"|                                                                              |",
"|                                                                              |",
"|                                                                              |",
"|                                                                              |",
"|                                                                              |",
"|                                                                              |",
"|                                                                              |",
"|                                                                              |",
"|                                                                              |",
"|                                                                              |",
"|                                                                              |",
"|                                                                              |",
"|                                                                              |",
"|                                                                              |",
"|                                                                              |",
"|                                                                              |",
"+------------------------------------------------------------------------------+" };
        for (int i=1; i<=24; ++i){
            setRC(i, 1);
            System.out.print(pict[i-1]);
        }

        onPowerChanged(model.power());

        for (View v: views) v.draw();
    }

    @Override
    public boolean key(int k) {
        switch (k) {
        case 44:  // ","
            currentView = Math.max(currentView - 1, 0);
            break;
        case 46: // "."
            currentView = Math.min(currentView + 1, views.size()-1);
            break;
        case 67: // C, power on
        case 99:
            model.setPower(true);
            break;
        case 86: // V, power ff
        case 118:
            model.setPower(false);
            break;
        case 88: // X, quit
        case 120:
            return false;
        case 77: // M
        case 109:
            model.set_OPC_SW_REQ(0, false, true);
            break;
        case 78: // N
        case 110:
            model.set_OPC_SW_REQ(0, true, true);
            break;
        default:
            views.get(currentView).key(k);
        }
        return true;
    }

}
