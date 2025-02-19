#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/power.h>

#define RESET_INTERRUPT

#ifdef RESET_INTERRUPT
#define INTERRUPT_PIN PCINT0 /* Same as PB0 */
#endif

const int redLEDPin = PB1;
const int greenLEDPin = PB2;
const int buzzerPin = PB3;

#ifdef RESET_INTERRUPT
const int resetPin = PB0;
#else
const int resetPin = PB5;
#endif

typedef enum
{
  INIT = 0,
  POMODORO = 1,
  POMODORO_SHORT_BREAK = 2,
  POMODORO_LONG_BREAK = 3,
  INVALID = 4
}PomodoroState;

PomodoroState state = INIT;
unsigned long pomodoroCount = 0U;
unsigned long pomodoroTotalCount = 0U;

const unsigned int clock_div = 1U; //16U;

const unsigned int numShortBreaks = 3U;
const unsigned long pomodoroOnMs = 1500000UL; // 25 minutes
const unsigned long pomodoroShortBreakMs = 300000UL; // 5 minutes
const unsigned long pomodoroLongBreakMs = 900000UL; // 15 minutes

#ifdef RESET_INTERRUPT
const unsigned int deBounceTimeMs = 200U;
#endif

volatile unsigned int isReset = 0U;

void setup() {
  //clock_prescale_set(clock_div_16); /* Make it run at 16.5 / 16 MHz - roughly 1 MHz */

  //Serial.begin(9600);
  pinMode(buzzerPin, OUTPUT);
  pinMode(redLEDPin, OUTPUT);
  pinMode(greenLEDPin, OUTPUT);

  ADCSRA &= ~ADEN; /* Disable ADC */

#ifdef RESET_INTERRUPT
  cli(); /* Disable interrupts during setup */
  PCMSK |= (1 << INTERRUPT_PIN);    /* Enable ISR for chosen interrupt pin (PCINT1/PB1/pin 6) */
  GIMSK |= (1 << PCIE);             /* Enable PCINT interrupt in the general interrupt mask */
  pinMode(resetPin, INPUT_PULLUP);
  sei(); /* Enable interrupts after setup */
#else
  pinMode(A0, INPUT); /* Set PB5 to input */
#endif
}

void loop() { 
  switch (state)
  {
    case INIT:
      //Serial.println("In INIT state");
      state = POMODORO;
      break;
    case POMODORO:
      //Serial.println("In POMODORO state");
      SignalPomodoroOn();
      if (1U == isReset)
      {
        isReset = 0U;
      }
      DelayMs(pomodoroOnMs, 1U);
      if (1U == isReset)
      {
        isReset = 0U;
      }
      else
      {
        pomodoroCount++;
        pomodoroTotalCount++;
        if (pomodoroCount <= numShortBreaks)
        {
          state = POMODORO_SHORT_BREAK;
        }
        else
        {
          state = POMODORO_LONG_BREAK;
        }
      }
      break;
    case POMODORO_SHORT_BREAK:
      //Serial.println("In POMODORO_SHORT_BREAK state");
      SignalPomodoroOff();
      DelayMs(pomodoroShortBreakMs, 1U);
      if (1U == isReset)
      {
        isReset = 0U;
      }
      state = POMODORO;
      break;
    case POMODORO_LONG_BREAK:
      //Serial.println("In POMODORO_LONG_BREAK state");
      SignalPomodoroOff();
      DelayMs(pomodoroLongBreakMs, 1U);
      if (1 == isReset)
      {
        isReset = 0;
      }
      pomodoroCount = 0U;
      state = POMODORO;
      break;
    default:
      /* Should never come here */
      break;
  }
}

void SignalPomodoroOff()
{
  digitalWrite(greenLEDPin, LOW);

  unsigned int ledBlinkCount = 0U;
  for (ledBlinkCount = 0; ledBlinkCount < 10; ledBlinkCount++)
  {
    digitalWrite(redLEDPin, LOW);
    DelayMs(100U, 1U);
    digitalWrite(redLEDPin, HIGH);
    DelayMs(100U, 1U);
  }

  SoundBuzzer(1U);
}

void SignalPomodoroOn()
{
  digitalWrite(redLEDPin, LOW);

  unsigned int ledBlinkCount = 0U;
  for (ledBlinkCount = 0; ledBlinkCount < 10; ledBlinkCount++)
  {
    DelayMs(100U, 0U);
    digitalWrite(greenLEDPin, LOW);
    DelayMs(100U, 0U);
    digitalWrite(greenLEDPin, HIGH);
  }
  
  SoundBuzzer(0U);
}

void SoundBuzzer(unsigned int breakIfReset)
{
  unsigned int buzzCount = 0;
  for (buzzCount = 0U; (buzzCount < 3U) && (((1U == breakIfReset) && (0U == isReset)) || (0U == breakIfReset)); buzzCount++)
  {
    digitalWrite(buzzerPin, HIGH);
    DelayMs(1000U, breakIfReset);
    digitalWrite(buzzerPin, LOW);
    DelayMs(1000U, breakIfReset);
  }
}

void DelayMs(unsigned long delayMs, unsigned int breakIfReset)
{
  unsigned long prevTime = millis();
  delayMs = delayMs / clock_div;

  while (1)
  {
    unsigned long currTime = millis();
    if(analogRead(A0) < 930)
    {
      isReset = 1U;
    }
    if (((1U == breakIfReset) && (1U == isReset)) || ((currTime - prevTime) >= delayMs))
    {
      //Serial.println("delay loop over!");
      break;
    }
  }
}

#ifdef RESET_INTERRUPT
ISR(PCINT0_vect)
{
  static unsigned long lastInterruptTime = 0;
  unsigned long currInterruptTime = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if ((currInterruptTime - lastInterruptTime) > deBounceTimeMs)
  {
    //Serial.println("inside button ISR");
    isReset = 1U;
  }
  lastInterruptTime = currInterruptTime;
}
#endif
