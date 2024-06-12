#define BUTTON_DOWN       0
#define BUTTON_UP         1
#define BUTTON_NO_CHANGE -1

class Button{
  private:
    int pinNumber;
    long lastChange;
    int lastValue;
  public:
    static const int  DEBOUNCE_MS = 0;
    void start(const int pinNumber);
    int state();
};