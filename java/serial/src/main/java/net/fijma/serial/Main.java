package net.fijma.serial;

import net.fijma.mvc.Application;
import net.fijma.mvc.serial.Serial;
import net.fijma.serial.model.Model;
import net.fijma.serial.tui.MainView;
import net.fijma.serial.tui.SerialController;
import net.fijma.serial.tui.ThrottleView;
import org.apache.commons.cli.*;

public class Main extends Application {

    private static void usage(Options options) {
        HelpFormatter formatter = new HelpFormatter();
        formatter.printHelp("serial", options);
        System.exit(1);
    }

    public static void main(String[] args) throws Exception {
        new Main().init(args);
    }

    private void init(String[] args) throws Exception {

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

        System.out.println("start using: " + device);
        exec(device);
    }

    private void exec(String device) throws Exception {
        // register optional module(s)
        registerModule(new net.fijma.mvc.serial.Serial(this,device));

        // Setup model
        Model model = new Model(getModule(Serial.class));

        // create views
        ThrottleView leftView = new ThrottleView(model, 0); // TODO mainView can set these up
        ThrottleView rightView = new ThrottleView(model, 1);
        MainView view = new MainView(model, leftView, rightView);

        // create controller
        SerialController controller = new SerialController(this, model, view, leftView, rightView);

        run(controller);
    }


}
