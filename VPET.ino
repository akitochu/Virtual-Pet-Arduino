#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_RGBLCDShield.h>
#include <utility/Adafruit_MCP23017.h>

Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();


///////////////////////////////////////////////////////////////////////////
// Hardware constants

#define RED 0x1
#define YELLOW 0x3
#define GREEN 0x2
#define TEAL 0x6
#define BLUE 0x4
#define VIOLET 0x5
#define WHITE 0x7

#define LCD_WIDTH  16
#define LCD_HEIGHT  2




///////////////////////////////////////////////////////////////////////////
// Applicatoin constants

#define VIEW_START 0   //! showing start option when there is saved data
#define VIEW_PET   1   //! VirtualPet view
#define VIEW_MENU  2   //! menu view 

#define START_SAVED 0  //! Start with saved virtual pet
#define START_NEW   1  //! Start with new virtual pet

#define MENU_SAVE   0  //! Save virtual pet
#define MENU_DELETE 1  //! Delete virtual pet
#define MENU_NEW    2  //! Start new virtual pet
#define MENU_CANCEL 3  //! Back to Pet view

#define ACTION_FEED 0  //! Feed the virtual pet
#define ACTION_PLAY 1  //! Play with the virtual pet
#define ACTION_GROW 2  //! Grow the virtual pet

#define FRAME_MS  200  //! Handle a single frame in 200 ms 
//! IMPORTANT: FRAME_MS must be a factor of 1000, so that we can judge
//!            when to fire the virtual pet event by (age_msec % INTERVAL == 0)


///////////////////////////////////////////////////////////////////////////
// Structures

struct VPet {
  byte dev_stage;
  byte happiness;
  byte fullness;
  unsigned long age_msec; //! age in second
};

struct App {
  byte view;
  byte selected_option;
  unsigned long last_action_at;
  VPet pet;
};


///////////////////////////////////////////////////////////////////////////
// global variables


App g_app;


///////////////////////////////////////////////////////////////////////////
// Custom Bitmap

byte BMP_HAPPY[8] = {
  0b00000,
  0b00000,
  0b01010,
  0b00000,
  0b00000,
  0b10001,
  0b01110,
  0b00000
};

byte BMP_CONTENT[8] = {
  0b00000,
  0b00000,
  0b01010,
  0b00000,
  0b00000,
  0b11111,
  0b00000,
  0b00000
};

byte BMP_UNHAPPY[8] = {
  0b00000,
  0b00000,
  0b01010,
  0b00000,
  0b01110,
  0b10001,
  0b00000,
  0b00000
};

byte BMP_EGG[8] = {
  0b00000,
  0b00000,
  0b01110,
  0b11101,
  0b11111,
  0b01110,
  0b00000,
  0b00000
};

byte BMP_CHILD_LEFT_OPEN[8] = {
  0b00000,
  0b00000,
  0b01000,
  0b11000,
  0b01110,
  0b01110,
  0b10001,
  0b00000
};

byte BMP_CHILD_LEFT_CLOSED[8] = {
  0b00000,
  0b00000,
  0b01000,
  0b11000,
  0b01110,
  0b01110,
  0b01010,
  0b00000
};

byte BMP_CHILD_RIGHT_OPEN[8] = {
  0b00000,
  0b00000,
  0b00010,
  0b00011,
  0b01110,
  0b01110,
  0b10001,
  0b00000
};

byte BMP_CHILD_RIGHT_CLOSED[8] = {
  0b00000,
  0b00000,
  0b00010,
  0b00011,
  0b01110,
  0b01110,
  0b01010,
  0b00000
};

byte BMP_ADULT_LEFT_OPEN_0[8] = {
  0b00010,
  0b00110,
  0b11110,
  0b11111,
  0b00111,
  0b01111,
  0b10000,
  0b10000
};

byte BMP_ADULT_LEFT_OPEN_1[8] = {
  0b00000,
  0b00000,
  0b11101,
  0b11110,
  0b11110,
  0b01110,
  0b00001,
  0b00001
};

byte BMP_ADULT_LEFT_CLOSED_0[8] = {
  0b00010,
  0b00110,
  0b11110,
  0b11111,
  0b00111,
  0b00111,
  0b00100,
  0b00100
};

byte BMP_ADULT_LEFT_CLOSED_1[8] = {
  0b00000,
  0b00000,
  0b11101,
  0b11110,
  0b11110,
  0b01110,
  0b00010,
  0b00010
};

byte BMP_ADULT_RIGHT_OPEN_0[8] = {
  0b00000,
  0b00000,
  0b10111,
  0b01111,
  0b01111,
  0b01110,
  0b10000,
  0b10000
};

byte BMP_ADULT_RIGHT_OPEN_1[8] = {
  0b01000,
  0b01100,
  0b01111,
  0b11111,
  0b11100,
  0b11110,
  0b00001,
  0b00001
};

byte BMP_ADULT_RIGHT_CLOSED_0[8] = {
  0b00000,
  0b00000,
  0b10111,
  0b01111,
  0b01111,
  0b01110,
  0b01000,
  0b01000
};

byte BMP_ADULT_RIGHT_CLOSED_1[8] = {
  0b01000,
  0b01100,
  0b01111,
  0b11111,
  0b11100,
  0b11100,
  0b00100,
  0b00100
};

const byte CHAR_UNHAPPY = 0;
const byte CHAR_CONTENT = 1;
const byte CHAR_HAPPY = 2;
const byte CHAR_EGG = 3;
const byte CHAR_CHILD_OPEN = 4;
const byte CHAR_CHILD_CLOSED = 5;
const byte CHAR_ADULT_0 = 6;
const byte CHAR_ADULT_1 = 7;


///////////////////////////////////////////////////////////////////////////
//  Detect the pushed button from the analogue input
byte read_LCD_buttons()
{
  byte buttons = lcd.readButtons();
  if (buttons & BUTTON_UP) {
    return BUTTON_UP;
  }
  if (buttons & BUTTON_DOWN) {
    return BUTTON_DOWN;
  }
  if (buttons & BUTTON_LEFT) {
    return BUTTON_LEFT;
  }
  if (buttons & BUTTON_RIGHT) {
    return BUTTON_RIGHT;
  }
  if (buttons & BUTTON_SELECT) {
    return BUTTON_SELECT;
  }
  return 0;
}

///////////////////////////////////////////////////////////////////////////
// pet functions

void initPet(VPet* pet)
{
  if (pet) {
    pet->dev_stage = 0;
    pet->happiness = 1;
    pet->fullness = 3;
    pet->age_msec = 0;
  }
}

//! EEPROM save format
//! 0..1 'V', 'P'     'VP' header to check it is virtual pet data
//! 2..8 struct VPet

bool loadPet(VPet* pet)
{
  bool ok = false;
  if (pet) {
    //! Check header
    if (EEPROM.read(0) == 'V' && EEPROM.read(1) == 'P') {

      //! Read data to tempPet
      VPet tempPet;
      byte* p = (byte*) &tempPet;
      for (int i = 0; i < sizeof(VPet); ++i) {
        *(p + i) = EEPROM.read(i + 2);
      }

      //! Check data range and copy tempPet to pet, if all OK
      if (tempPet.dev_stage <= 2 && tempPet.happiness <= 2 && tempPet.fullness <= 4) {
        *pet = tempPet;
        ok = true;
      }
    }
  }
  return ok;
}

void savePet(VPet* pet)
{
  if (pet) {
    //! Write header
    EEPROM.write(0, 'V');
    EEPROM.write(1, 'P');

    //! Write data
    byte* p = (byte*) pet;
    for (int i = 0; i < sizeof(VPet); ++i) {
      EEPROM.write(i + 2, *(p + i));
    }
  }
}

void deleteSavePet()
{
  for (int i = 0; i < sizeof(VPet) + 2; ++i) {
    EEPROM.write(i, 0);
  }
}


///////////////////////////////////////////////////////////////////////////
// Start view handlers

void showStartView(App* app)
{
  //             "0123456789ABCDEF"
  char line0[] = " Load           ";
  char line1[] = " New            ";
  if (app->selected_option == START_SAVED) {
    line0[0] = '*';
  } else if (app->selected_option == START_NEW) {
    line1[0] = '*';
  }
  lcd.setCursor(0, 0);
  lcd.print(line0);
  lcd.setCursor(0, 1);
  lcd.print(line1);
}

void handleStartView(App* app, byte button)
{
  if (button == BUTTON_UP) {
    app->selected_option = START_SAVED;
  } else if (button == BUTTON_DOWN) {
    app->selected_option = START_NEW;
  } else if (button == BUTTON_SELECT) {
    if (app->selected_option == START_SAVED) {
      app->view = VIEW_PET;
    } else if (app->selected_option == START_NEW) {
      initPet(&app->pet);
      app->view = VIEW_PET;
    }
  }
}

///////////////////////////////////////////////////////////////////////////
// Pet view handlers

void showPetView(VPet* pet)
{
  const char* FULLNESS[] = {
    "    ",
    "o   ",
    "oo  ",
    "ooo ",
    "XXXX",
  };

  //             "0123456789ABCDEF"
  char line0[] = "                ";
  char line1[] = "                ";

  snprintf(line0, 16, "D:    F:%s H: ", FULLNESS[pet->fullness]);

  unsigned long age_sec = pet->age_msec / 1000;
  unsigned long age_min = age_sec / 60;
  age_sec -= age_min * 60;
  bool legsClosed = (age_sec % 2 == 0);

  snprintf(line1, 16, "Age:%3lu:%02lu     ", age_min, age_sec);
  lcd.setCursor(0, 0);
  lcd.print(line0);
  if (pet->dev_stage < 2) {
    lcd.setCursor(3, 0);
    if (pet->dev_stage == 0) {
      lcd.write(CHAR_EGG);
    } else {
      lcd.write(legsClosed ? CHAR_CHILD_CLOSED : CHAR_CHILD_OPEN);
    }
  } else {
    lcd.setCursor(3, 0);
    lcd.write(CHAR_ADULT_0);
    lcd.write(CHAR_ADULT_1);
  }

  lcd.setCursor(15, 0);
  lcd.write(pet->happiness);
  lcd.setCursor(0, 1);
  lcd.print(line1);
}

void handlePetView(App* app, byte button)
{
  unsigned long now = millis();
  unsigned long timeSinceLatAction = now - g_app.last_action_at;

  if (button == BUTTON_UP) {
    //! Grow
    if (app->pet.dev_stage == 1 && 35000 <= app->pet.age_msec && 1 <= app->pet.happiness && 3 <= app->pet.fullness) {
      //! grow its dev_stage to 2 when the age is at least 35sec, happiness is at least 1, and fullness is at least 3
      app->pet.dev_stage = 2;
    }
  } else if (button == BUTTON_LEFT && 1000 <= timeSinceLatAction) {
    //! Feed
    if (0 < app->pet.dev_stage && app->pet.fullness < 4) {
      //! increase the fullness by 1
      ++app->pet.fullness;
      app->last_action_at = now;
    }
  } else if (button == BUTTON_DOWN && 1000 <= timeSinceLatAction) {
    //! Play
    if (0 < app->pet.dev_stage && app->pet.happiness < 2 && 2 <= app->pet.fullness) {
      //! increase the happiness by 1, when its fullness is at least 2
      ++app->pet.happiness;
      app->last_action_at = now;
    }
  } else if (button == BUTTON_SELECT) {
    app->view = VIEW_MENU;
    app->selected_option = MENU_SAVE;
  }

  //! when age reaches 5 seconds, the development stage is set to 1
  if (app->pet.dev_stage == 0 && 5000 <= app->pet.age_msec) {
    app->pet.dev_stage = 1;
  }

  if (0 < app->pet.dev_stage) {
    unsigned long ageSinceBorn = app->pet.age_msec - 5000;
    //! every 7 seconds, happiness is reduced by 1
    if (0 < ageSinceBorn && ageSinceBorn % 7000 == 0 && 0 < app->pet.happiness) {
      --app->pet.happiness;
    }

    //! every 11 seconds, fullness is reduced by 1
    if (0 < ageSinceBorn && ageSinceBorn % 11000 == 0 && 0 < app->pet.fullness) {
      --app->pet.fullness;
    }

    //! set happiness to 0 when overfull
    if (3 < app->pet.fullness) {
      app->pet.happiness = 0;
    }

    //! if fullness is 0, happiness is set to 0
    if (app->pet.fullness == 0) {
      app->pet.happiness = 0;
    }
  }

  app->pet.age_msec += FRAME_MS;
}

///////////////////////////////////////////////////////////////////////////
// Menu view handlers

void showMenuView(App* app)
{
  //             "0123456789ABCDEF"
  char line0[] = " Save    New    ";
  char line1[] = " Cancel  Delete ";
  if (app->selected_option == MENU_SAVE) {
    line0[0] = '*';
  } else if (app->selected_option == MENU_NEW) {
    line0[8] = '*';
  } else if (app->selected_option == MENU_CANCEL) {
    line1[0] = '*';
  } else if (app->selected_option == MENU_DELETE) {
    line1[8] = '*';
  }
  lcd.setCursor(0, 0);
  lcd.print(line0);
  lcd.setCursor(0, 1);
  lcd.print(line1);
}

void handleMenuView(App* app, byte button)
{
  if (button == BUTTON_UP) {
    if (app->selected_option == MENU_CANCEL) {
      app->selected_option = MENU_SAVE;
    } else if (app->selected_option == MENU_DELETE) {
      app->selected_option = MENU_NEW;
    }
  } else if (button == BUTTON_DOWN) {
    if (app->selected_option == MENU_SAVE) {
      app->selected_option = MENU_CANCEL;
    } else if (app->selected_option == MENU_NEW) {
      app->selected_option = MENU_DELETE;
    }
  } else if (button == BUTTON_LEFT) {
    if (app->selected_option == MENU_NEW) {
      app->selected_option = MENU_SAVE;
    } else if (app->selected_option == MENU_DELETE) {
      app->selected_option = MENU_CANCEL;
    }
  } else if (button == BUTTON_RIGHT) {
    if (app->selected_option == MENU_SAVE) {
      app->selected_option = MENU_NEW;
    } else if (app->selected_option == MENU_CANCEL) {
      app->selected_option = MENU_DELETE;
    }
  } else if (button == BUTTON_SELECT) {
    if (app->selected_option == MENU_CANCEL) {
      app->view = VIEW_PET;
    } else if (app->selected_option == MENU_SAVE) {
      savePet(&app->pet);
      app->view = VIEW_PET;
    } else if (app->selected_option == MENU_NEW) {
      initPet(&app->pet);
      app->view = VIEW_PET;
    } else if (app->selected_option == MENU_DELETE) {
      deleteSavePet();
      initPet(&app->pet);
      app->view = VIEW_PET;
    }
  }
}


///////////////////////////////////////////////////////////////////////////
// Top level application functions

void updateLCD(App* app)
{
  switch (app->view) {
    case VIEW_START:
      showStartView(app);
      break;
    case VIEW_PET:
      showPetView(&app->pet);
      break;
    case VIEW_MENU:
      showMenuView(app);
      break;
    default:
      break;
  }
}

void updateAppState(App* app, byte button)
{
  switch (app->view) {
    case VIEW_START:
      handleStartView(app, button);
      break;
    case VIEW_PET:
      handlePetView(app, button);
      break;
    case VIEW_MENU:
      handleMenuView(app, button);
      break;
    default:
      break;
  }
}


///////////////////////////////////////////////////////////////////////////
// Functions called by Arduino framework

void setup()
{
  Serial.begin(9600); //! Use Serial monitor for debugging purpose

  //! Setup LCD
  lcd.begin(LCD_WIDTH, LCD_HEIGHT);
  lcd.createChar(CHAR_UNHAPPY, BMP_UNHAPPY);
  lcd.createChar(CHAR_CONTENT, BMP_CONTENT);
  lcd.createChar(CHAR_HAPPY, BMP_HAPPY);
  lcd.createChar(CHAR_EGG, BMP_EGG);
  lcd.createChar(CHAR_CHILD_OPEN, BMP_CHILD_LEFT_OPEN);
  lcd.createChar(CHAR_CHILD_CLOSED, BMP_CHILD_LEFT_CLOSED);
  lcd.createChar(CHAR_ADULT_0, BMP_ADULT_LEFT_CLOSED_0);
  lcd.createChar(CHAR_ADULT_1, BMP_ADULT_LEFT_CLOSED_1);

  //! Init application state
  if (loadPet(&g_app.pet)) {
    Serial.println("Loaded saved pet data. Show startup menu.");
    g_app.view = VIEW_START;
    g_app.selected_option = START_SAVED;
  } else {
    Serial.println("There is no saved data. Start a new pet.");
    initPet(&g_app.pet);
    g_app.view = VIEW_PET;
  }
  g_app.last_action_at = 0;
}


void loop()
{
  unsigned long startTime = millis();

  updateLCD(&g_app);
  byte button = read_LCD_buttons();
  updateAppState(&g_app, button);

  unsigned long elapsed = millis() - startTime;
  if (elapsed < FRAME_MS) {
    delay(FRAME_MS - elapsed); //! adjust the time we spend in a single loop
  }
}
