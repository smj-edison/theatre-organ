#include <Arduino.h>

// MCP23017 - Version: Latest 
#include <MCP23017.h>
#include <Wire.h>
#include "MIDIUSB.h"

#define DEFAULT_VELOCITY 64

#define I2C_CLOCK_SPEED 400000
#define TCAADDR 0x70

// IMPORTANT: change to 10 if on arduino uno
#define READ_RESOLUTION 12

#define EXPRESSION_THRESHOLD 63

#define SOSTENUTO_PIN 2

#define DEBOUNCE_TIME_MICROS 50000

struct KeyboardExpander {
    MCP23017 ios[4];
    int iosLength;
    int channel;
    int offset;
    int multiplexerNum;
    uint8_t last[8];
    uint8_t current[8];
    uint8_t sostenuto[8];
    uint64_t last_micros_toggled[64];
    bool sostenutoEnabled;
};


// keyboards
KeyboardExpander solo {
    {MCP23017(0x20), MCP23017(0x21), MCP23017(0x22), MCP23017(0x23)},
    4, 0, 36, 0,
    {0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0},
    {},
    false
};

KeyboardExpander great{
    {MCP23017(0x24), MCP23017(0x25), MCP23017(0x26), MCP23017(0x27)},
    4, 1, 36, 0,
    {0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0},
    {},
    true
};

KeyboardExpander accomp{
    {MCP23017(0x20), MCP23017(0x21), MCP23017(0x22), MCP23017(0x23)},
    4, 2, 36, 1,
    {0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0},
    {},
    false
};

KeyboardExpander pedal{
    {MCP23017(0x24), MCP23017(0x25), NULL, NULL},
    2, 3, 36, 1,
    {0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0},
    {},
    false
};

KeyboardExpander keyboard_expanders[] = { solo, great, accomp, pedal };

const int KEYBOARD_EXPANDER_LENGTH = sizeof(keyboard_expanders) / sizeof(KeyboardExpander);

const int MAX_ANALOG_READING = (1 << READ_RESOLUTION);

// expression pedals
struct ExpressionPedal {
    uint8_t channel;
    int last_value;
    int analog_pin;
    bool invert_value;
};

ExpressionPedal main_expression {
    0, -100, A0, true
};

ExpressionPedal solo_expression {
    1, -100, A1, true
};

ExpressionPedal expression_pedals[] = {
    main_expression, solo_expression
};

const int EXPRESSION_PEDALS_LENGTH = sizeof(expression_pedals) / sizeof(ExpressionPedal);

// buttons
struct ButtonExpander {
    MCP23017 ios[4];
    int iosLength;
    int channel;
    int multiplexerNum;
    uint8_t last[8];
    uint8_t current[8];
};

ButtonExpander pistons {
    {MCP23017(0x20), MCP23017(0x21), NULL, NULL},
    2, 1, 2,
    {0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0}
};

ButtonExpander button_expanders[] = {
    pistons
};

const int BUTTON_EXPANDERS_LENGTH = sizeof(button_expanders) / sizeof(ButtonExpander);

// sostenuto
bool lastSostenuto = false;
bool sostenutoEngaged = false;

// multiplexer //
void tcaselect(uint8_t i) {
    if (i > 7) return;
    
    Wire.beginTransmission(TCAADDR);
    Wire.write(1 << i);
    Wire.endTransmission();  
}

// midi implementation //
void controlChange(uint8_t channel, uint8_t control, uint8_t value) {
    midiEventPacket_t event = {0x0B, 0xB0 | channel, control, value};
    MidiUSB.sendMIDI(event);

    MidiUSB.flush();
    delayMicroseconds(100);
}

void programChange(uint8_t channel, uint8_t value) {
    midiEventPacket_t event = {0x0C, 0xC0 | channel, value & 127};
    MidiUSB.sendMIDI(event);

    MidiUSB.flush();
    delayMicroseconds(100);
}

void noteOn(uint8_t channel, uint8_t pitch, uint8_t velocity) {  
    midiEventPacket_t noteOn = {0x09, 0x90 | channel, pitch, velocity};
    MidiUSB.sendMIDI(noteOn);

    MidiUSB.flush();
    delayMicroseconds(100);
}

void noteOff(uint8_t channel, uint8_t pitch, uint8_t velocity) {
    midiEventPacket_t noteOff = {0x08, 0x80 | channel, pitch, velocity};
    MidiUSB.sendMIDI(noteOff);

    MidiUSB.flush();
    delayMicroseconds(100);
}

void init_chips() {
    Wire.begin();
    Wire.setClock(I2C_CLOCK_SPEED);

    for(int i = 0; i < KEYBOARD_EXPANDER_LENGTH; i++) {
        tcaselect(keyboard_expanders[i].multiplexerNum);
        
        for(int j = 0; j < keyboard_expanders[i].iosLength; j++) {
            keyboard_expanders[i].ios[j].init();
            keyboard_expanders[i].ios[j].portMode(MCP23017Port::A, 0b11111111);
            keyboard_expanders[i].ios[j].portMode(MCP23017Port::B, 0b11111111);
        }

        Serial.println(i);
    }

    for(int i = 0; i < BUTTON_EXPANDERS_LENGTH; i++) {
        tcaselect(button_expanders[i].multiplexerNum);
        
        for(int j = 0; j < button_expanders[i].iosLength; j++) {
            button_expanders[i].ios[j].init();
            button_expanders[i].ios[j].portMode(MCP23017Port::A, 0b11111111);
            button_expanders[i].ios[j].portMode(MCP23017Port::B, 0b11111111);
        }
    }
}

void update_sostenuto() {
    sostenutoEngaged = digitalRead(SOSTENUTO_PIN);

    if(!sostenutoEngaged && lastSostenuto) { // sostenuto turned off
        for(int i = 0; i < KEYBOARD_EXPANDER_LENGTH; i++) {
            for(int j = 0; j < keyboard_expanders[i].iosLength * 2; j++) { // reset sostenuto state
                keyboard_expanders[i].sostenuto[j] = 0;
            }
        }
    }
}

void update_buttons(uint8_t channel, uint8_t current, uint8_t last, uint8_t offset) {
    uint8_t changed = current ^ last;

    if(changed > 0) {
        for(int i = 0; i < 8; i++) {
            if((changed >> i) & 1) { // this changed from last time
                if((current >> i) & 1) { // and it was turned on
                    programChange(channel, i + offset);
                } else {
                    programChange(channel, i + offset + 64);
                }
            }
        }
    }
}


void scan_buttons() {
    for(int i = 0; i < BUTTON_EXPANDERS_LENGTH; i++) {
        ButtonExpander button_expander = button_expanders[i];

        tcaselect(button_expander.multiplexerNum);
        
        for(int j = 0; j < button_expander.iosLength; j++) {
            button_expander.current[j * 2]       = ~button_expander.ios[j].readPort(MCP23017Port::A);
            button_expander.current[(j * 2) + 1] = ~button_expander.ios[j].readPort(MCP23017Port::B);

            update_buttons(button_expander.channel, button_expander.current[j * 2], button_expander.last[j * 2], j * 16);
            update_buttons(button_expander.channel, button_expander.current[(j * 2) + 1], button_expander.last[(j * 2) + 1], j * 16 + 8);

            button_expanders[i].last[j * 2] = button_expander.current[j * 2];
            button_expanders[i].last[(j * 2) + 1] = button_expander.current[(j * 2) + 1];
        }
    }
}

void update_expression_pedals() {
    for(int i = 0; i < EXPRESSION_PEDALS_LENGTH; i++) {
        int reading_raw = analogRead(expression_pedals[i].analog_pin);

        if(abs(reading_raw - expression_pedals[i].last_value) > EXPRESSION_THRESHOLD) {
            int reading = (int) (((long) reading_raw) * 128 / MAX_ANALOG_READING); // between 0 - 127

            if(expression_pedals[i].invert_value) {
                reading = 127 - reading;
            }
            
            controlChange(expression_pedals[i].channel, 0x07, reading);
            
            expression_pedals[i].last_value = reading_raw;
        }
    }
}

void update_notes(uint8_t channel, uint8_t current, uint8_t last, uint8_t offset) {
    uint8_t changed = current ^ last;

    if(changed > 0) {
        for(int i = 0; i < 8; i++) {
            if((changed >> i) & 1) { // this changed from last time
                if((current >> i) & 1) { // and it was turned on
                    noteOn(channel, i + offset, DEFAULT_VELOCITY);
                } else {
                    noteOff(channel, i + offset, DEFAULT_VELOCITY);
                }
            }
        }
    }
}

uint8_t debounce(uint64_t now, uint64_t last_micros_toggled[64], int last_micros_offset, uint8_t last, uint8_t current) {
    uint8_t changed = current ^ last;
    uint8_t to_remove = 0; // a mask for which notes to remove

    if(changed > 0) {
        for(int i = 0; i < 8; i++) {
            if((changed >> i) & 1) { // if this changed from last time
                if(now - last_micros_toggled[i + last_micros_offset] < DEBOUNCE_TIME_MICROS) { // was it not long enough?
                    to_remove |= 0 << i;
                }
            }
        }
    }

    // revert any bits that changed too fast
    return (current & ~to_remove) | (last & to_remove);
}

void scan_notes() {
    uint64_t now = micros();

    for(int i = 0; i < KEYBOARD_EXPANDER_LENGTH; i++) {
        KeyboardExpander keyboard_expander = keyboard_expanders[i];

        tcaselect(keyboard_expander.multiplexerNum);
        
        for(int j = 0; j < keyboard_expander.iosLength; j++) {
            keyboard_expander.current[j * 2]       = ~keyboard_expander.ios[j].readPort(MCP23017Port::A);
            keyboard_expander.current[(j * 2) + 1] = ~keyboard_expander.ios[j].readPort(MCP23017Port::B);

            if(keyboard_expander.sostenutoEnabled) {
                keyboard_expander.current[j * 2] |= keyboard_expander.sostenuto[j * 2];
                keyboard_expander.current[(j * 2) + 1] |= keyboard_expander.sostenuto[(j * 2) + 1];
            }

            keyboard_expander.current[j * 2] = debounce(now, keyboard_expander.last_micros_toggled, j * 16, keyboard_expander.last[j * 2], keyboard_expander.current[j * 2]);
            keyboard_expander.current[j * 2 + 1] = debounce(now, keyboard_expander.last_micros_toggled, j * 16 + 8, keyboard_expander.last[(j * 2) + 1], keyboard_expander.current[(j * 2) + 1]);

            update_notes(keyboard_expander.channel, keyboard_expander.current[j * 2], keyboard_expander.last[j * 2], keyboard_expander.offset + j * 16);
            update_notes(keyboard_expander.channel, keyboard_expander.current[(j * 2) + 1], keyboard_expander.last[(j * 2) + 1], keyboard_expander.offset + j * 16 + 8);

            keyboard_expanders[i].last[j * 2] = keyboard_expander.current[j * 2];
            keyboard_expanders[i].last[(j * 2) + 1] = keyboard_expander.current[(j * 2) + 1];

            if(!lastSostenuto && sostenutoEngaged) {
                keyboard_expanders[i].sostenuto[j * 2] = keyboard_expander.current[j * 2];
                keyboard_expanders[i].sostenuto[(j * 2) + 1] = keyboard_expander.current[(j * 2) + 1];
            }
        }
    }
}

int I2C_clear_bus() {
#if defined(TWCR) && defined(TWEN)
    TWCR &= ~(_BV(TWEN)); //Disable the Atmel 2-Wire interface so we can control the SDA and SCL pins directly
#endif

    pinMode(SDA, INPUT_PULLUP); // Make SDA (data) and SCL (clock) pins Inputs with pullup.
    pinMode(SCL, INPUT_PULLUP);

    //delay(2500);  // Wait 2.5 secs. This is strictly only necessary on the first power
    // up of the DS3231 module to allow it to initialize properly,
    // but is also assists in reliable programming of FioV3 boards as it gives the
    // IDE a chance to start uploaded the program
    // before existing sketch confuses the IDE by sending Serial data.

    boolean SCL_LOW = (digitalRead(SCL) == LOW); // Check is SCL is Low.
    if (SCL_LOW) { //If it is held low Arduno cannot become the I2C master. 
        return 1; //I2C bus error. Could not clear SCL clock line held low
    }

    boolean SDA_LOW = (digitalRead(SDA) == LOW);  // vi. Check SDA input.
    int clockCount = 20; // > 2x9 clock

    while (SDA_LOW && (clockCount > 0)) { //  vii. If SDA is Low,
        clockCount--;
        // Note: I2C bus is open collector so do NOT drive SCL or SDA high.
        pinMode(SCL, INPUT); // release SCL pullup so that when made output it will be LOW
        pinMode(SCL, OUTPUT); // then clock SCL Low
        delayMicroseconds(10); //  for >5uS
        pinMode(SCL, INPUT); // release SCL LOW
        pinMode(SCL, INPUT_PULLUP); // turn on pullup resistors again
        // do not force high as slave may be holding it low for clock stretching.
        delayMicroseconds(10); //  for >5uS
        // The >5uS is so that even the slowest I2C devices are handled.
        SCL_LOW = (digitalRead(SCL) == LOW); // Check if SCL is Low.
        int counter = 20;
        while (SCL_LOW && (counter > 0)) {  //  loop waiting for SCL to become High only wait 2sec.
            counter--;
            delay(100);
            SCL_LOW = (digitalRead(SCL) == LOW);
        }

        if (SCL_LOW) { // still low after 2 sec error
            return 2; // I2C bus error. Could not clear. SCL clock line held low by slave clock stretch for >2sec
        }

        SDA_LOW = (digitalRead(SDA) == LOW); //   and check SDA input again and loop
    }
    if (SDA_LOW) { // still low
        return 3; // I2C bus error. Could not clear. SDA data line held low
    }

    // else pull SDA line low for Start or Repeated Start
    pinMode(SDA, INPUT); // remove pullup.
    pinMode(SDA, OUTPUT);  // and then make it LOW i.e. send an I2C Start or Repeated start control.
    // When there is only one I2C master a Start or Repeat Start has the same function as a Stop and clears the bus.
    /// A Repeat Start is a Start occurring after a Start with no intervening Stop.
    delayMicroseconds(10); // wait >5uS
    pinMode(SDA, INPUT); // remove output low
    pinMode(SDA, INPUT_PULLUP); // and make SDA high i.e. send I2C STOP control.
    delayMicroseconds(10); // x. wait >5uS
    pinMode(SDA, INPUT); // and reset pins as tri-state inputs which is the default state on reset
    pinMode(SCL, INPUT);
    return 0; // all ok
}

// debugging //
void printByteReverse(uint8_t b) {
    for(byte i = 0; i < 8; i++){
        Serial.write(bitRead(b, i) ? '1' : '0');
    }
}

void setup() {
    Serial.begin(115200);
    analogReadResolution(12); // on

    pinMode(SOSTENUTO_PIN, INPUT_PULLUP);
}

void loop() {
    init_chips();
    update_sostenuto();
    scan_notes();
    update_expression_pedals();
    scan_buttons();
    I2C_clear_bus(); 

    lastSostenuto = sostenutoEngaged;
}
