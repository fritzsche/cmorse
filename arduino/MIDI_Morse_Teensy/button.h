#define BUTTON_DOWN 0
#define BUTTON_UP 1
#define BUTTON_NO_CHANGE -1

#define DEBOUNCE 0
class Button {
private:
  int pinNumber;
  unsigned long lastStateChangeTime;
  int lastState;
public:
  void start(const int pinNumber);
  int state();
  int currentState();
};