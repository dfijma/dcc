import RPi.GPIO as GPIO

class RotaryEncoder:
    
    CLOCKWISE=1
    ANTICLOCKWISE=2
    BUTTONDOWN=3
    BUTTONUP=4

    START=0
    ONE=1
    TWO=2
    THREE=3

    def __init__(self,pinA,pinB,button,callback):
        self.pinA = pinA
        self.pinB = pinB
        self.button = button
        self.callback = callback
        self.state = self.START
        
        GPIO.setmode(GPIO.BCM)
        
        # The following lines enable the internal pull-up resistors
        # on version 2 (latest) boards
        GPIO.setwarnings(False)
        GPIO.setup(self.pinA, GPIO.IN)
        GPIO.setup(self.pinB, GPIO.IN)
        GPIO.setup(self.button, GPIO.IN, pull_up_down=GPIO.PUD_UP)

        # For version 1 (old) boards comment out the above four lines
        # and un-comment the following 3 lines
        #GPIO.setup(self.pinA, GPIO.IN)
        #GPIO.setup(self.pinB, GPIO.IN)
        #GPIO.setup(self.button, GPIO.IN)

        # Add event detection to the GPIO inputs
        GPIO.add_event_detect(self.pinA, GPIO.BOTH, callback=self.switch_event)
        GPIO.add_event_detect(self.pinB, GPIO.BOTH, callback=self.switch_event)
        GPIO.add_event_detect(self.button, GPIO.BOTH, callback=self.button_event, bouncetime=200)

    def emit_rotary_event(self):
        # if (self.state != self.START):
        #    print("shortcut in state: "+str(self.state))
        self.callback(self.direction)
    
    def switch_event(self, switch):
        # read both rotary switches on both edges and feed to state machine
        self.step(GPIO.input(self.pinA), GPIO.input(self.pinB))
        
    # Push button up event
    def button_event(self,button):
        if GPIO.input(button): 
            event = self.BUTTONUP 
        else:
            event = self.BUTTONDOWN 
        self.callback(event)

    def step(self, A, B): 
        # normal right click: 00 10 11 01
        # normal left click: 00 01 11 10
        if self.state == self.START:
            if A == 0 and B == 1:
                # normal: seen [00,01] of left click
                self.state = self.ONE
                self.direction = self.ANTICLOCKWISE
            elif A == 1 and B == 0:
                # normal: seen [00,10] of right click
                self.state = self.ONE
                self.direction = self.CLOCKWISE
            # else: ignore repeat 00 or unknown direction 11
        else:
            # normalize symbol based on direction:
            if self.direction == self.CLOCKWISE:
                symbol = str(A)+str(B)
            else:
                symbol = str(B)+str(A)
            if self.state == self.ONE:
                if symbol == "11":
                    # normal, seen 00 10 11
                    self.state = self.TWO
                elif symbol == "00":
                    self.state = self.START # skip TWO and THREE, no event
                elif symbol == "01": #  skip TWO
                    self.state = self.THREE
                # else: ignore repeated 10
            elif self.state == self.TWO:
                if symbol == "01":
                    # normal, seen 00 10 11 01
                    self.state = self.THREE
                elif symbol == "00":
                    self.emit_rotary_event()
                    self.state = self.START # skip THREE
                elif symbol == "10":
                    self.emit_rotary_event()
                    self.state = self.ONE # skip THREE and START
                # else: ignore repeated 11    
            elif self.state == self.THREE:
                if symbol == "00":
                    # normal, event and back to start
                    self.state = self.START
                    self.emit_rotary_event()
                elif symbol == "10":
                    self.emit_rotary_event()
                    self.state = self.ONE # skip START
                elif symbol == "11":
                    self.emit_rotary_event()
                    self.state = self.TWO # skip ONE and STARt
                # else: ignore repeated "01"
        
