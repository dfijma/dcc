
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

    def event(self):
        if (self.state != self.START):
            print("shortcut in state: "+str(self.state))
        self.callback(self.state)
    
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
                    self.event()
                    self.state = self.START # skip THREE
                elif symbol == "10":
                    self.event()
                    self.state = self.ONE # skip THREE and START
                # else: ignore repeated 11    
            elif self.state == self.THREE:
                if symbol == "00":
                    # normal, event and back to start
                    self.state = self.START
                    self.event()
                elif symbol == "10":
                    self.event()
                    self.state = self.ONE # skip START
                elif symbol == "11":
                    self.event()
                    self.state = self.TWO # skip ONE and STARt
                # else: ignore repeated "01"
        
