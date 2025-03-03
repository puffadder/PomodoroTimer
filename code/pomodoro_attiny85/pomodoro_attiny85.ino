#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

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
uint32_t pomodoroCount = 0U;
uint32_t pomodoroTotalCount = 0U;

const uint32_t clock_div = 1U; //16U;

const uint32_t numShortBreaks = 3U;

const uint32_t pomodoroOnMs = 1500000U; // 25 minutes
const uint32_t pomodoroShortBreakMs = 300000U; // 5 minutes
const uint32_t pomodoroLongBreakMs = 900000U; // 15 minutes

#ifdef RESET_INTERRUPT
const uint32_t deBounceTimeMs = 200U;

uint32_t numSleeps = 0U;
const uint32_t wakeupTime = 64U;
const uint32_t wdtError = 24U;
extern volatile unsigned long millis_timer_millis;
#endif

volatile uint32_t isReset = 0U;

void setup() {
  //clock_prescale_set(clock_div_16); /* Make it run at 16.5 / 16 MHz - roughly 1 MHz */

  //Serial.begin(9600);
  pinMode(buzzerPin, OUTPUT);
  pinMode(redLEDPin, OUTPUT);
  pinMode(greenLEDPin, OUTPUT);

#ifdef RESET_INTERRUPT
  cli(); /* Disable interrupts during setup */
  PCMSK |= (1 << INTERRUPT_PIN);    /* Enable ISR for chosen interrupt pin (PCINT1/PB1/pin 6) */
  GIMSK |= (1 << PCIE);             /* Enable PCINT interrupt in the general interrupt mask */
  ADCSRA &= ~ADEN; /* Disable ADC */
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  pinMode(resetPin, INPUT_PULLUP);
  sleep_enable();
  sei(); /* Enable interrupts after setup */
#else
  pinMode(A0, INPUT); /* Set PB5 to input */
#endif
}

void loop() { 
  switch (state)
  {
    case INIT:
      state = POMODORO;
      break;
    case POMODORO:
      SignalPomodoroOn();
      SleepMs(pomodoroOnMs);
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
      SignalPomodoroOff();
      SleepMs(pomodoroShortBreakMs);
      if (1U == isReset)
      {
        isReset = 0U;
      }
      state = POMODORO;
      break;
    case POMODORO_LONG_BREAK:
      //Serial.println("In POMODORO_LONG_BREAK state");
      SignalPomodoroOff();
      SleepMs(pomodoroLongBreakMs);
      if (1U == isReset)
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

  uint32_t ledBlinkCount = 0U;
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

  uint32_t ledBlinkCount = 0U;
  for (ledBlinkCount = 0; ledBlinkCount < 10; ledBlinkCount++)
  {
    DelayMs(100U, 0U);
    digitalWrite(greenLEDPin, LOW);
    DelayMs(100U, 0U);
    digitalWrite(greenLEDPin, HIGH);
  }
  
  SoundBuzzer(1U);
}

void SoundBuzzer(uint32_t breakIfReset)
{
  uint32_t buzzCount = 0;
  for (buzzCount = 0U; (buzzCount < 3U) && (((1U == breakIfReset) && (0U == isReset)) || (0U == breakIfReset)); buzzCount++)
  {
    digitalWrite(buzzerPin, HIGH);
    DelayMs(1000U, breakIfReset);
    digitalWrite(buzzerPin, LOW);
    DelayMs(1000U, breakIfReset);
  }
}

/*
 * @param aWatchdogPrescaler (see wdt.h) can be one of WDTO_15MS, 30, 60, 120, 250, WDTO_500MS, WDTO_1S to WDTO_8S
 *                                                    0 (15 ms) to 3(120 ms), 4 (250 ms) up to 9 (8000 ms)
 */
uint16_t computeSleepMillis(uint8_t aWatchdogPrescaler) {
    uint16_t tResultMillis = 8000;
    for (uint8_t i = 0; i < (9 - aWatchdogPrescaler); ++i) {
        tResultMillis = (tResultMillis / 2);
    }
    return tResultMillis + wakeupTime; // + 64 for the startup time
}

/*
 * @param aWatchdogPrescaler (see wdt.h) can be one of WDTO_15MS, 30, 60, 120, 250, WDTO_500MS, WDTO_1S to WDTO_8S
 * @param aAdjustMillis if true, adjust the Arduino internal millis counter the get quite correct millis() 
 * results even after sleep, since the periodic 1 ms timer interrupt is disabled while sleeping.
 */
void sleepWithWatchdog(uint8_t aWatchdogPrescaler, bool aAdjustMillis) {
    MCUSR = 0; // Clear MCUSR to enable a correct interpretation of MCUSR after reset

    // use wdt_enable() since it handles that the WDP3 bit is in bit 5 of the WDTCR register
    wdt_enable(aWatchdogPrescaler);
    WDTCR |= _BV(WDIE) | _BV(WDIF); // Watchdog interrupt enable + reset interrupt flag -> needs ISR(WDT_vect)
    sei();         // Enable interrupts
    sleep_cpu();   // The watchdog interrupt will wake us up from sleep

    // We wake up here :-)
    wdt_disable(); // Because next interrupt will otherwise lead to a reset, since wdt_enable() sets WDE / Watchdog System Reset Enable
    /*
     * Since timer clock may be disabled adjust millis only if not slept in IDLE mode (SM2...0 bits are 000)
     */
    if (aAdjustMillis && (MCUCR & ((_BV(SM1) | _BV(SM0)))) != 0) {
        millis_timer_millis += computeSleepMillis(aWatchdogPrescaler);
    }
}

void SleepMs(uint32_t sleepTimeMs)
{
  uint32_t totalTimeMs = 0U;

  while (0U == isReset)
  {
    sleepWithWatchdog(WDTO_1S, false);
    totalTimeMs += (1000U + wdtError + wakeupTime);

    if (totalTimeMs >= sleepTimeMs)
    {
      break;
    }
  }
}

void DelayMs(uint32_t delayMs, uint32_t breakIfReset)
{
  uint32_t prevTime = millis();
  delayMs = delayMs / clock_div;

  while (1)
  {
    uint32_t currTime = millis();
    if (((1U == breakIfReset) && (1U == isReset)) || ((currTime - prevTime) >= delayMs))
    {
      break;
    }
  }
}

#ifdef RESET_INTERRUPT
ISR(PCINT0_vect)
{
  static uint32_t lastInterruptTime = 0U;
  uint32_t currInterruptTime = millis();
  /* If interrupts come faster than 200ms, assume it's a bounce and ignore */
  if ((currInterruptTime - lastInterruptTime) > deBounceTimeMs)
  {
    isReset = 1U;
  }
  lastInterruptTime = currInterruptTime;
}

ISR(WDT_vect) {
    numSleeps++;
}
#endif
