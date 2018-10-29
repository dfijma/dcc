package net.fijma.serial.tui;

public abstract class AbstractView  {

    public abstract void draw();
    public abstract boolean key(int k);

    //http://www.lihaoyi.com/post/BuildyourownCommandLinewithANSIescapecodes.html

    protected void clear() {
        System.out.print("\u001b[2J");
        setRC(0,0);
    }

    protected void green() {
        System.out.print("\u001b[32m");
    }

    protected  void reset() {
        System.out.print("\u001b[0m");
    }

    protected void setRC(int row, int col) {
        System.out.print("\u001b[" + row + ";" + col + "H");
    }



}
