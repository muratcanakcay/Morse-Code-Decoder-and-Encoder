//#include </malina/akcaym/buildroot-2022.02/output/build/libgpiod-1.6.3/include/gpiod.h>      // for packaging
#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>

#ifndef	CONSUMER
#define	CONSUMER	"Consumer"
#endif

#define APP_TIMEOUT 10000               // depress duration [ms] for input end                              10000
#define SYMBOLS_TIMEOUT 1500            // depress duration [ms] for signalling end of pattern              1500
#define SPACE_TIMEOUT 4500              // depress duration [ms] for space char                             4500
#define SHORT_PRESS_TIMEOUT 1000        // max. press duration [ms] for dot char                             500 (RPi) / 1000 (QEMU)
#define LONG_PRESS_TIMEOUT 3000         //  if pressed longer than this [ms] nothing will be added          2000 (RPi) / 3000 (QEMU)
#define BOUNCE_TIMEOUT 100              // max. bounce interval [ms]                                         50  (RPi) / 100 (QEMU)

typedef unsigned int UINT;
typedef struct timespec timespec_t;
typedef struct gpiod_line gpiod_line_t;
typedef struct gpiod_chip gpiod_chip_t;
typedef struct gpiod_line_event gpiod_line_event_t;

const char DOT[1] = ".";
const char DASH[1] = "_";
const char SPACE[1] = " ";
const int UNIT_MILI_SECONDS = 250;
const int GPIO_LOW = 0;
const int GPIO_HIGH = 1;
const UINT BUTTON_GPIO = 13;	// GPIO Pin #13
const UINT LED_GPIO = 24;	// GPIO Pin #24

const char MORSE_CODE[][6] = {
    " ",        // Space = 32
    "", "", "", "", "", "", "", // Not used
    "", "", "", "", "", "", "", // Not used
    "",                         // Not used
    "_____",    // 0 = 48
    ".____",    // 1
    "..___",    // 2
    "...__",    // 3
    "...._",    // 4
    ".....",    // 5
    "_....",    // 6
    "__...",    // 7
    "___..",    // 8
    "____.",    // 9 = 57
    "", "", "", "", "", "", "", // Not used
    "._",       // A = 65
    "_...",     // B
    "_._.",     // C
    "_..",      // D
    ".",        // E
    ".._.",     // F
    "__.",      // G
    "....",     // H
    "..",       // L
    ".___",     // J
    "_._",      // K
    "._..",     // L
    "__",       // M
    "_.",       // N
    "___",      // O
    ".__.",     // P
    "__._",     // Q
    "._.",      // R
    "...",      // S
    "_",        // T
    ".._",      // U
    "..._",     // V
    ".__",      // W
    "_.._",     // X
    "_.__",     // Y
    "__..",     // Z = 90
    "X"         // error
};

int display_menu(bool* MORSE_DECODE);
int decode_input(gpiod_line_t* line);
int wait_for_input(gpiod_line_t* line, timespec_t* eventTime);
int debounce_input_and_release_line(gpiod_line_t* line);
int read_stable_input_and_release_line(gpiod_line_t* line, int* value);
void process_button_event(int* prevValPtr, int newVal, timespec_t* prevTimePtr, timespec_t lastTime, char* symbols, int* symbolIndexPtr, char* letters, int* letterIndexPtr);
void process_button_release(int pressDuration, char* symbols, int* symbolIndexPtr);
void process_button_press(int releaseDuration, char* symbols, int* symbolIndexPtr, char* letters, int* letterIndexPtr);
int get_user_input(char** input);
int encode_input(gpiod_line_t* line, char *sentence);
int encode_letter(gpiod_line_t* line, int character);
int blinkLed(gpiod_line_t* line, char dotDash);
int set_gpio_pin(gpiod_line_t* line, int miliSeconds);
bool is_valid_morse_code(int character);
void msleep(UINT miliSeconds);
void close_chip_and_exit(gpiod_chip_t* chip, int status);
void release_line_and_exit(gpiod_chip_t* chip, gpiod_line_t* line, int status);

int main(int argc, char **argv) 
{
    gpiod_chip_t *chip;
    char* chipname = "gpiochip0";
    if ((chip = gpiod_chip_open_by_name(chipname)) == NULL)
    {
        perror("Open chip failed\n");        
        exit(EXIT_FAILURE);
    }	
    
    // main loop
    while(true)
    {
        int ret;
        bool MORSE_DECODE = true;
        if ((ret = display_menu(&MORSE_DECODE)) != EXIT_SUCCESS )
        {
            if (ret == EXIT_FAILURE) close_chip_and_exit(chip, EXIT_FAILURE);
            else close_chip_and_exit(chip, EXIT_SUCCESS);
        }
        
        gpiod_line_event_t event;
        gpiod_line_t *line;
        if (MORSE_DECODE)
        {        
            UINT line_num = BUTTON_GPIO;
            if ((line = gpiod_chip_get_line(chip, line_num)) == NULL)
            {
                perror("Get line failed\n");
                close_chip_and_exit(chip, EXIT_FAILURE);
            }

            puts("You can start entering your morse code using the button.");

            if (decode_input(line) != EXIT_SUCCESS)
                release_line_and_exit(chip, line, EXIT_FAILURE);
        }        
        else // MORSE_ENCODE
        {
            UINT line_num = LED_GPIO;            
            if ((line = gpiod_chip_get_line(chip, line_num)) == NULL) 
            {
                perror("Get line failed\n");
                close_chip_and_exit(chip, EXIT_FAILURE);
            }

            if ((ret = gpiod_line_request_output(line, CONSUMER, 0)) < 0) 
            {
                perror("Request line as output failed\n");
                release_line_and_exit(chip, line, EXIT_FAILURE);
            }
            
            char* input = NULL;
            if ((ret = get_user_input(&input)) != EXIT_SUCCESS)
                release_line_and_exit(chip, line, EXIT_FAILURE);

            if (encode_input(line, input) != EXIT_SUCCESS)
                release_line_and_exit(chip, line, EXIT_FAILURE);
                     
            free(input);
            gpiod_line_release(line);
        }
    } // end of main loop
}


/*
 * Sleep for given miliseconds
 */
void msleep(UINT miliSeconds) 
{
    time_t sec= (int)(miliSeconds/1000);
    miliSeconds = miliSeconds - (sec*1000);
    timespec_t req= {0};
    req.tv_sec = sec;
    req.tv_nsec = miliSeconds * 1000000L;
    if(nanosleep(&req,&req)) perror("nanosleep");
}

/*
 * Get user choice for decoding or encoding
 */
int display_menu(bool* MORSE_DECODE) 
{
    char *input = NULL;
    size_t zero = 0;
    ssize_t read = 0;

    puts("Enter:\n\
    1 for DECODING a message entered by buttons to letters\n\
    2 for ENCODING a message entered by keyboard using LEDs\n\
    Anything else to QUIT.\n");
    putchar('>');

    if ((read = getline(&input, &zero, stdin)) < 0) {
        perror("getline");
        return EXIT_FAILURE;
    }

    if(input[0] == '1')
        *MORSE_DECODE = true;
    else if (input[0] == '2')
        *MORSE_DECODE = false;
    else {
        free(input);
        return 2;
    }

    free(input);
    return EXIT_SUCCESS;
}

int decode_input(gpiod_line_t* line)
{
    timespec_t prevTime, lastTime;
    int newVal, oldVal, prevVal = GPIO_HIGH; // starting value of GPIO input
    int symbolIndex = -1; // symbols index
    int letterIndex = 0;  // letters index
    int ret;    

    char symbols[500] = "\0"; // will be [6] at the end
    char letters[500] = "\0";    
    
    // DECODE loop
    while (true)
    {
        if ((ret = wait_for_input(line, &lastTime)) != EXIT_SUCCESS)
        {
            if (ret == EXIT_FAILURE) 
                return EXIT_FAILURE;
            else
            {
                gpiod_line_release(line);
                return EXIT_SUCCESS; // return to menu
            }
        }
        
        if (debounce_input_and_release_line(line) != EXIT_SUCCESS) 
            return EXIT_FAILURE;
        if (read_stable_input_and_release_line(line, &newVal) != EXIT_SUCCESS) 
            return EXIT_FAILURE;
        
        process_button_event(&prevVal, newVal, &prevTime, lastTime, symbols, \
                            &symbolIndex, letters, &letterIndex);
        
        // print the new state of the symbols and the letters
        printf("Symbols: '%s' (%lu)\n", symbols, strlen(symbols));
        printf("Letters: '%s' (%lu)\n", letters, strlen(letters));        
        
        if (strlen(symbols) > 5) // TODO: here symbolIndex should reset symbols[]
        {
            printf("INPUT TOO LONG!\n\n");
        }
    } // end DECODE loop
}

/*
 * Wait for an event to occur on the given line and store the time of the event in EVENTTIME
 */
int wait_for_input(gpiod_line_t* line, timespec_t* eventTime)
{
    int ret;
    gpiod_line_event_t event;
    timespec_t appTimeout = { APP_TIMEOUT/1000, 0 };
    
    // request events
    if ((ret = gpiod_line_request_both_edges_events(line, CONSUMER)) < 0) 
    {
        perror("Request event notification failed\n");
        return EXIT_FAILURE;
    }

    if ((ret = gpiod_line_event_wait(line, &appTimeout)) < 0)
    {
        perror("Wait event notification failed\n");
        return EXIT_FAILURE;
    }
    else if (ret == 0) // appTimeout
    {
        printf("Input ended\n");
        return 2;
    }

    if ((ret = gpiod_line_event_read(line, &event)) < 0)        
    {
        perror("Read last event notification failed\n");
        return EXIT_FAILURE;
    }

    /* informative code */
    // puts("");
    // printf("INITIAL event notification on line #%u %d times\n", line_num, symbolIndex);
    // printf("Event type: %d\n", event.event_type);
    // printf("Event time: %lo sec %lo nsec\n", event.ts.tv_sec, event.ts.tv_nsec);
    // puts("");

    *eventTime = event.ts; // keep record of when initial event occurred to calculate duration
    return EXIT_SUCCESS;
}

/*
 * Debounce the bouncing events on the given line
 */
int debounce_input_and_release_line(gpiod_line_t* line)
{
    int ret;
    gpiod_line_event_t event;
    timespec_t bounceTimeout = { 0, BOUNCE_TIMEOUT * 1000000 };

    // debouncing loop - wait for bounce events to end
    while(true)
    {
        if ((ret = gpiod_line_event_wait(line, &bounceTimeout)) < 0) 
        {
            perror("Wait event notification failed\n");            
            gpiod_line_release(line);
            return EXIT_FAILURE;
        }
        else if (ret == 0) // timeout, symbolIndex.e. bouncing ended - exit loop
        {
            gpiod_line_release(line);
            return EXIT_SUCCESS;
        }

        if ((ret = gpiod_line_event_read(line, &event)) < 0) 
        {
            perror("Read last event notification failed\n");
            gpiod_line_release(line);
            return EXIT_FAILURE;
        }
        
        /* informative code */            
        //puts("");
        //printf("BOUNCE event notification on line #%u %d times\n", line_num, symbolIndex);
        //printf("Event type: %d\n", event.event_type);
        //printf("Event time: %lo sec %lo nsec\n", event.ts.tv_sec, event.ts.tv_nsec);
        //puts("");
    }
}

/*
 * Read stable input value from the given line and store it in VALUE.
 * Releases the line on EXIT_SUCCESS, does not release on EXIT_FAILURE
 */
int read_stable_input_and_release_line(gpiod_line_t* line, int* value)
{
    int ret;

    if ((ret = gpiod_line_request_input(line, CONSUMER)) < 0) 
    {
        perror("Request line as input failed\n");
        return(EXIT_FAILURE);
    }

    if ((*value = gpiod_line_get_value(line)) < 0) 
    {
        perror("Read line input failed\n");
        return(EXIT_FAILURE);
    }

    /* informative code */
    //printf("Input %d on line #%u\n", newVal, line_num);
    // printf("PREV Input %d on line #%u\n", prevVal, line_num);
    // printf("FINAL Input %d on line #%u\n", newVal, line_num);
    // printf("lastTime: %ld.%ld\n", lastTime.tv_sec, lastTime.tv_nsec);
    // printf("prevTime: %ld.%ld\n", prevTime.tv_sec, prevTime.tv_nsec);

    gpiod_line_release(line);
    return(EXIT_SUCCESS);
}

/*
 * Process the button press or release event 
 */
void process_button_event(int *prevValPtr, int newVal, timespec_t* prevTimePtr, timespec_t lastTime, char* symbols, int* symbolIndexPtr, char* letters, int* letterIndexPtr)
{
    // calculate duration between previous input and current input
    int duration = (int)((lastTime.tv_sec - prevTimePtr->tv_sec) * 1000 + (lastTime.tv_nsec - prevTimePtr->tv_nsec) / 1000000); 
    //printf("%d [%dms] -> %d\n", prevVal, duration, newVal);        
    
    if (newVal == GPIO_HIGH && *prevValPtr == GPIO_LOW) // button released, duration is the duration the button kept pressed
    {
        *prevValPtr = 1;
        *prevTimePtr = lastTime;
        process_button_release(duration, symbols, symbolIndexPtr);
    }
    else if (newVal == GPIO_LOW && *prevValPtr == GPIO_HIGH) // button pressed, duration is the duration the button kept released
    {
        *prevValPtr = 0;
        *prevTimePtr = lastTime;
        process_button_press(duration, symbols, symbolIndexPtr, letters, letterIndexPtr);
    }
    else // input did not change its value so do nothing
    {
        *prevTimePtr = lastTime;
        printf("[%d] Not adding anything\n\n", *symbolIndexPtr);
    }
}

/*
 * Process what happens when the button is released based on the press duration
 */
void process_button_release(int pressDuration, char* symbols, int* symbolIndexPtr)
{
    (*symbolIndexPtr)++;
    
    printf("Button kept pressed for %dms\n", pressDuration);

    if (pressDuration > LONG_PRESS_TIMEOUT)                      // do nothing
    {
        (*symbolIndexPtr)--;
        printf("[%d] Not adding anything\n", *symbolIndexPtr);
    }
    else if (pressDuration > SHORT_PRESS_TIMEOUT)                // add '_'
    {
        printf("[%d] Adding '_'\n", *symbolIndexPtr);
        symbols[*symbolIndexPtr] = *DASH;
        symbols[*symbolIndexPtr+1] = '\0';
    }
    else                                                        // add '.'
    {
        printf("[%d] Adding '.'\n", *symbolIndexPtr);
        symbols[*symbolIndexPtr] = *DOT;
        symbols[*symbolIndexPtr+1] = '\0';
    }
    
    puts("");
}

/*
 * Process what happens when the button is pressed based on the release duration and current pattern in symbols[]
 */
void process_button_press(int releaseDuration, char* symbols, int* symbolIndexPtr, char* letters, int* letterIndexPtr)
{
    printf("Button kept released for %dms\n", releaseDuration);
                    
    if (releaseDuration < SYMBOLS_TIMEOUT || releaseDuration > APP_TIMEOUT) // short release -> continue reading the symbol pattern
    {
        // printf("[%d] Not adding anything\n", *symbolIndex);
    }
    else // long release -> try to translate symbols into letter
    {
        (*symbolIndexPtr)++;
        printf("[%d] Calculating symbol\n", *symbolIndexPtr);
        
        // check if symbols[] forms a morse pattern
        int morseIndex;
        for(morseIndex = 0; morseIndex < sizeof(MORSE_CODE) /  sizeof(char[6]); morseIndex++)
        {
            if (strcmp(symbols, MORSE_CODE[morseIndex]) == 0) // morseIndex found
                break;
        }
        
        if (morseIndex == sizeof(MORSE_CODE) /  sizeof(char[6])) // if last element of the array
        {
            printf("The pattern '%s' is not a valid morse code!\n", symbols);
            letters[*letterIndexPtr] = '#';   // error in pattern
        }
        else
        {
            printf("The pattern '%s' is at index %d and matches to %c\n", symbols, morseIndex, morseIndex+32);
            letters[*letterIndexPtr] = ' ' + morseIndex; // insert corresponding letter in letters[]
        }

        letters[++(*letterIndexPtr)] = '\0';
        
        if (releaseDuration > SPACE_TIMEOUT) // also add a space after the letter in letters[]
        {
            printf("[%d] Adding space\n", *symbolIndexPtr);
            letters[*letterIndexPtr] = *SPACE;
            letters[++(*letterIndexPtr)] = '\0';
        }

        // clear symbols[]
        *symbolIndexPtr = -1;
        symbols[0] = '\0';
    }
    
    puts("");
}

/*
 * Get text user input from terminal for encoding
 */
int get_user_input(char** input)
{
    size_t zero = 0;
    ssize_t read = 0;

    puts("Enter a text to be encoded into morse code:");
    putchar('>');

    if ((read = getline(input, &zero, stdin)) == -1)
    {
        perror("getline() failed in get_user_input()");
        return EXIT_FAILURE;
    }

    (*input)[read-1] = '\0';
    printf("You entered = '%s'\n", *input);
    printf("Input length = %zu\n", strlen(*input));

    return EXIT_SUCCESS;
}

/*
 * Output a morse code sentence
 */
int encode_input(gpiod_line_t* line, char *input)
{
    int sentenceLength = strlen(input);
    for(int letterIndex=0; letterIndex < sentenceLength; letterIndex++)
    {
        if (encode_letter(line, toupper(input[letterIndex])) != EXIT_SUCCESS)
            return EXIT_FAILURE;
        
        msleep(UNIT_MILI_SECONDS * 3); // Space between letters = 3 units
    }
    
    puts("");
    return EXIT_SUCCESS;
}

/*
 * Output a morse code character
 */
int encode_letter(gpiod_line_t* line, int letter)
{
    if (is_valid_morse_code(letter) == false)
        return EXIT_SUCCESS;

    putchar(letter);
    putchar(*SPACE);

    int morseIndex = letter - 32;
    int morseCodeLength = strlen(MORSE_CODE[morseIndex]);
    for(int letterIndex=0; letterIndex<morseCodeLength; letterIndex++)
    {
        if (MORSE_CODE[morseIndex][letterIndex] == *SPACE)
        {
            msleep(UNIT_MILI_SECONDS * 7); // Space between words = 7 units
            continue;
        }

        if (blinkLed(line, MORSE_CODE[morseIndex][letterIndex]) != EXIT_SUCCESS)
            return EXIT_FAILURE;
        
        msleep(UNIT_MILI_SECONDS); // Space between parts of same letters = 1 unit
    }

    putchar(*SPACE);
    fflush(stdout);
    return EXIT_SUCCESS;
}

/*
 * Output the dot or dash from the led
 */
int blinkLed(gpiod_line_t* line, char dotDash)
{
    if (dotDash == *DOT)
    {
        putchar(*DOT);        
        fflush(stdout);
        return set_gpio_pin(line, UNIT_MILI_SECONDS); // Dot = 1 unit
    }

    if (dotDash == *DASH)
    {
        putchar(*DASH);        
        fflush(stdout);
        return set_gpio_pin(line, UNIT_MILI_SECONDS * 3); // Dash = 3 units
    }

    printf("Error cannot determine if dot or dash.");
    return EXIT_SUCCESS;
}

/*
 * Set GPIO pin to high then low for a length of micro seconds
 */
int set_gpio_pin(gpiod_line_t* line, int miliSeconds)
{
    if (gpiod_line_set_value(line, GPIO_HIGH)  < 0) 
    {
        perror("Set line output high failed\n");
        return EXIT_FAILURE;
    }

    msleep(miliSeconds);

    if (gpiod_line_set_value(line, GPIO_LOW)  < 0) 
    {
        perror("Set line output low failed\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/*
 * Check if the letter is in morse code array
 */
bool is_valid_morse_code(int letterIndex)
{
    if ((letterIndex >= 65 && letterIndex <= 90) || // Alphabet character
        (letterIndex >= 48 && letterIndex <= 57) || // Numeric character
        (letterIndex == 32))                        // Space
        return true;
    
    return false;
}

/*
 * Release the line, close the chip and exit with status code STATUS
 */
void release_line_and_exit(gpiod_chip_t * chip, gpiod_line_t* line, int status)
{
    gpiod_line_release(line);
    close_chip_and_exit(chip, status);
}

/*
 * Close the chip and exit with status code STATUS
 */
void close_chip_and_exit(gpiod_chip_t * chip, int status)
{
    gpiod_chip_close(chip);
    exit(status);
}