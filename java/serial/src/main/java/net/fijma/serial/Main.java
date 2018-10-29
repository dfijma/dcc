package net.fijma.serial;

import net.fijma.serial.Model.Model;
import net.fijma.serial.tui.ThrottleController;
import org.apache.commons.cli.*;

public class Main  {

    // http://develorium.com/2016/03/unbuffered-standard-input-in-java-console-applications/

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

    private Loconet ln = new Loconet();
    private Serial serial = new Serial();

    private void run(String device) throws Exception {

        ln.decoded.attach(System.out::println);
        serial.byteAvailable.attach(ln::pushByte);
        serial.start(device);

        // Setup and run TUI
        Model model = new Model();
        ThrottleController controller = ThrottleController.setup(model);
        controller.run();

        serial.stop();
    }

}
