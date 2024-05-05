#ifndef DISPLAY_H_
#define DISPLAY_H_
#include <functional>

void setup_display();
void display_toggle(bool toggle);
void cleanTFT();

void show_display(
    std::initializer_list<String> lines_of_text,
    int wait=0);

#endif