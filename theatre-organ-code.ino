// MCP23017 - Version: Latest 
#include <MCP23017.h>
#include <Wire.h>
#include "MIDIUSB.h"

#define DEFAULT_VELOCITY 64

#define TCAADDR 0x70

// IMPORTANT: change to 10 if on arduino uno
#define READ_RESOLUTION 12

#define EXPRESSION_THRESHOLD 63

#define SOSTENUTO_PIN 2

struct KeyboardExpander {
  MCP23017 ios[4];
  int iosLength;
  int channel;
  int offset;
  int multiplexerNum;
  uint8_t last[8];
  uint8_t current[8];
  uint8_t sostenuto[8];
  bool sostenutoEnabled;
};


// keyboards
KeyboardExpander solo {
  {MCP23017(0x20), MCP23017(0x21), MCP23017(0x22), MCP23017(0x23)},
  4, 0, 36, 0,
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  false
};

KeyboardExpander great{
  {MCP23017(0x24), MCP23017(0x25), MCP23017(0x26), MCP23017(0x27)},
  4, 1, 36, 0,
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  true
};

KeyboardExpander accomp{
  {MCP23017(0x20), MCP23017(0x21), MCP23017(0x22), MCP23017(0x23)},
  4, 2, 36, 1,
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  false
};

KeyboardExpander pedal{
  {MCP23017(0x24), MCP23017(0x25), NULL, NULL},
  2, 3, 36, 1,
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
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

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);

  analogReadResolution(12); // on

  for(int i = 0; i < KEYBOARD_EXPANDER_LENGTH; i++) {
    tcaselect(keyboard_expanders[i].multiplexerNum);
    
    for(int j = 0; j < keyboard_expanders[i].iosLength; j++) {
      keyboard_expanders[i].ios[j].init();
      keyboard_expanders[i].ios[j].portMode(MCP23017Port::A, 0b11111111);
      keyboard_expanders[i].ios[j].portMode(MCP23017Port::B, 0b11111111);
    }
  }

  for(int i = 0; i < BUTTON_EXPANDERS_LENGTH; i++) {
    tcaselect(button_expanders[i].multiplexerNum);
    
    for(int j = 0; j < button_expanders[i].iosLength; j++) {
      button_expanders[i].ios[j].init();
      button_expanders[i].ios[j].portMode(MCP23017Port::A, 0b11111111);
      button_expanders[i].ios[j].portMode(MCP23017Port::B, 0b11111111);
    }
  }

  pinMode(SOSTENUTO_PIN, INPUT_PULLUP);
}

void loop() {
  update_sostenuto();
  scan_notes();
  update_expression_pedals();
  scan_buttons();
  
  lastSostenuto = sostenutoEngaged;
  
  delay(5);
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

void scan_notes() {
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

// midi implementation //
void controlChange(byte channel, byte control, byte value) {
  midiEventPacket_t event = {0x0B, 0xB0 | channel, control, value};
  MidiUSB.sendMIDI(event);

  MidiUSB.flush();
  delayMicroseconds(100);
}

void programChange(byte channel, byte value) {
  midiEventPacket_t event = {0x0C, 0xC0 | channel, value & 127};
  MidiUSB.sendMIDI(event);

  MidiUSB.flush();
  delayMicroseconds(100);
}

void noteOn(byte channel, byte pitch, byte velocity) {  
  midiEventPacket_t noteOn = {0x09, 0x90 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOn);

  MidiUSB.flush();
  delayMicroseconds(100);
}

void noteOff(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOff = {0x08, 0x80 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOff);

  MidiUSB.flush();
  delayMicroseconds(100);
}

// multiplexer //
void tcaselect(uint8_t i) {
  if (i > 7) return;
 
  Wire.beginTransmission(TCAADDR);
  Wire.write(1 << i);
  Wire.endTransmission();  
}

// debugging //
void printByteReverse(uint8_t b) {
  for(byte i = 0; i < 8; i++){
    Serial.write(bitRead(b, i) ? '1' : '0');
  }
}
