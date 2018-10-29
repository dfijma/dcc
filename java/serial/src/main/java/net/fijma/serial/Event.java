package net.fijma.serial;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;

public class Event<T> {

    private List<Consumer<T>> listeners = new ArrayList<>();

    public void attach(Consumer<T> listener) {
        listeners.add(listener);
    }

    public void trigger(T arg) {
        for (Consumer<T> listener : listeners) {
            listener.accept(arg);
        }
    }
}
