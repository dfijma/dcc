package net.fijma.serial.tui;

import java.io.Console;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class View extends AbstractView {

    // aggregation combination of views
    private final List<AbstractView> views;
    private int currentView = 0;

    public View(AbstractView... views) {
        this.views = new ArrayList<>();
        Collections.addAll(this.views, views);
        if (this.views.size() < 1) throw new IllegalArgumentException("at least on subview required");
        currentView = 0;
    }

    public void onMsg(List<String> msg) {
        String one = "";
        String two = "";
        if (msg.size() >= 2) {
            one = msg.get(msg.size()-2);
            two = msg.get(msg.size()-1);
        } else if (msg.size() == 1) {
            one = msg.get(msg.size()-1);
        }
        setRC(13, 2);
        System.out.println(String.format("%78s", one));
        setRC(14, 2);
        System.out.println(String.format("%78s", two));
    }

    @Override
    public void draw() {
        for (var v: views) v.draw();
        setRC(12, 1);
        System.out.print("┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓");
        setRC(13, 1);
        System.out.print("┃                                                                              ┃");
        setRC(14, 1);
        System.out.print("┃                                                                              ┃");
        setRC(15, 1);
        System.out.print("┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛");
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
        case 88: // X, quit
        case 120:
            return false;
        default:
            views.get(currentView).key(k);
        }
        return true;
    }

    void run() {
        Console console = System.console();
        clear();
        draw();
        boolean r = true;
        while (r) {
            try {
                boolean a = console.reader().ready();
                if (!a) {
                    Thread.sleep(10);
                    continue;
                }
                r = key(console.reader().read());
            } catch (IOException | InterruptedException ignored) {  }
        }

    }


}
