#pragma once
struct DisplayPreferences {bool inverted=false;bool roll_rear=false;};
void display_preferences_load();
DisplayPreferences display_preferences_get();
bool display_preferences_set_inverted(bool inverted);
bool display_preferences_set_roll_rear(bool rear);
