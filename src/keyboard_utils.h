#ifndef KEYBOARD_UTILS_H
#define KEYBOARD_UTILS_H

#include <Arduino.h>

namespace KEYBOARD_Utils {

    void upArrow();
    void downArrow();
    void leftArrow();
    void rightArrow();
    void processPressedKey(char key);
    void mouseRead();
    void read();
    void setup();
  
}
namespace Object {
    void place();
    void remove_last();
    void remove_all();
    void retransmit_all();
}

#endif
