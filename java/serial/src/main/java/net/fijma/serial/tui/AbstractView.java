package net.fijma.serial.tui;

import net.fijma.serial.model.Model;

public abstract class AbstractView  {

    final Model model;

    public abstract void draw();
    public abstract boolean key(int k);

    AbstractView(Model model) {
        this.model = model;
    }

    // http://www.lihaoyi.com/post/BuildyourownCommandLinewithANSIescapecodes.html

    protected void clear() {
        System.out.print("\u001b[2J");
        setRC(0,0);
    }

    protected String green() {
        return "\u001b[32m";
    }

    protected String red() {
        return "\u001b[31m";
    }

    protected String reset() {
        return "\u001b[0m";
    }

    protected void setRC(int row, int col) {
        System.out.print("\u001b[" + row + ";" + col + "H");
    }



}
