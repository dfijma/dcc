package net.fijma.serial;

import net.fijma.serial.model.Model;
import net.fijma.serial.tui.Controller;
import org.apache.commons.cli.*;

import java.io.Console;
import java.io.IOException;

public class Main  {
    private volatile boolean run = true;
    private Event<Integer> keyAvailable = new Event<>();
    private Serial serial = new Serial();

    private static void usage(Options options) {
        HelpFormatter formatter = new HelpFormatter();
        formatter.printHelp("serial", options);
        System.exit(1);
    }

    public static void main(String[] args) {
        Options options = new Options();
        options.addOption("p", false, "probe serial ports");
        options.addOption(Option.builder("d").optionalArg(false).hasArg().argName("device").desc("serial port device").build());

        CommandLineParser parser = new DefaultParser();
        String device = null;

        try {
            CommandLine cmd = parser.parse(options, args);
            if (cmd.hasOption("p")) {
                Serial.probe();
                System.exit(0);
            }
            if (cmd.hasOption("d")) {
                device = cmd.getOptionValue("d");
            } else {
                usage(options);
            }

        } catch (ParseException e) {
            System.err.println(e.getMessage());
            usage(options);
        }

        try {
            System.out.println("start using: " + device);
            new Main().run(device);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void run(String device)  {
        // Setup model
        Model model = new Model(serial);

        // Setup controller and view
        Controller controller = Controller.setup(model, keyAvailable, serial);

        // start serial communication
        serial.start(device);

        // start reading the keyboard
        Thread kb = new Thread(this::keyboardLoop);
        kb.start();

        // run controller on main thread until it terminates
        controller.run();

        // wait for keyboard loop to stop
        run = false;
        while (true) {
            try {
                kb.join();
                break;
            } catch (InterruptedException ignored) { }
        }

        serial.stop();
    }

    // http://develorium.com/2016/03/unbuffered-standard-input-in-java-console-applications/
    private void keyboardLoop() {
        Console console = System.console();
        while (run) {
            try {
                boolean a = console.reader().ready();
                if (!a) {
                    Thread.sleep(10);
                    continue;
                }
                keyAvailable.trigger(console.reader().read());
            } catch (IOException | InterruptedException ignored) {  }
        }
    }

}
