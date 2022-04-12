//#include </malina/akcaym/buildroot-2022.02/output/build/libgpiod-1.6.3/include/gpiod.h>
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

const char DOT[1] = ".";
const char DASH[1] = "_";
const char SPACE[1] = " ";
const int UNIT_MILI_SECONDS = 250;
const int GPIO_LOW = 0;
const int GPIO_HIGH = 1;
const UINT BUTTON_GPIO = 13;	// GPIO Pin #13
const UINT LED_GPIO = 24;	// GPIO Pin #24

const char MORSE_CODE[][6] =
{
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
    "..",       // I
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
int wait_for_input(struct gpiod_line* line, timespec_t* eventTime);
int debounce_input(struct gpiod_line* line);
int read_stable_input(struct gpiod_line* line, int* value);
int outputMorseCodesentence(struct gpiod_line* line, char *sentence);
int outputMorseCharacter(struct gpiod_line* line, int character);
int blinkLed(struct gpiod_line* line, char dotDash);
int setGPIOPin(struct gpiod_line* line, int miliSeconds);
bool isValidMorseCodeCharacter(int character);
void msleep(UINT miliSeconds);
void close_chip_and_exit(struct gpiod_chip * chip, int status);
void release_line_and_exit(struct gpiod_chip * chip, struct gpiod_line* line, int status);

int main(int argc, char **argv) 
{
    char *chipname = "gpiochip0";
    struct gpiod_line_event event;
    struct gpiod_chip *chip;
    struct gpiod_line *line;
    int ret;
    bool MORSE_DECODE = true;    

    if ((chip = gpiod_chip_open_by_name(chipname)) == NULL)
    {
        perror("Open chip failed\n");        
        exit(EXIT_FAILURE);
    }	
    
    while(true)
    {
        if ((ret = display_menu(&MORSE_DECODE)) != EXIT_SUCCESS )
        {
            if (ret == EXIT_FAILURE) close_chip_and_exit(chip, EXIT_FAILURE);
            else close_chip_and_exit(chip, EXIT_SUCCESS);
        }

        if (MORSE_DECODE)
        {        
            UINT line_num = BUTTON_GPIO;

            if ((line = gpiod_chip_get_line(chip, line_num)) == NULL)
            {
                perror("Get line failed\n");
                close_chip_and_exit(chip, EXIT_FAILURE);
            }            
            
            timespec_t prevTime, lastTime;
            int newVal, oldVal, prevVal = GPIO_HIGH; // starting value of GPIO input
            int morse_code_array_size = sizeof(MORSE_CODE) /  sizeof(char[6]);
            int i = -1; // symbols index
            int l = 0;  // letters index    

            char symbols[500] = "\0"; // will be [6] at the end
            char letters[500] = "\0";    
            
            // main loop
            while (true)
            {
                if ((ret = wait_for_input(line, &lastTime)) != EXIT_SUCCESS)
                {
                    if (ret == EXIT_FAILURE)
                    {
                        release_line_and_exit(chip, line, EXIT_FAILURE);
                    }                
                    else if (ret == 2)
                    {
                        gpiod_line_release(line);
                        break; // return to menu
                    }
                }
                
                if (debounce_input(line) != EXIT_SUCCESS)
                { 
                    release_line_and_exit(chip, line, EXIT_FAILURE);
                }

                if (read_stable_input(line, &newVal) != EXIT_SUCCESS)
                { 
                    release_line_and_exit(chip, line, EXIT_FAILURE);
                }
                
                // calculate duration between previous input and current input
                int duration = (int)((lastTime.tv_sec - prevTime.tv_sec) * 1000 + (lastTime.tv_nsec - prevTime.tv_nsec) / 1000000); 
                printf("%d -> %dms -> %d\n", prevVal, duration, newVal);        

                if (newVal == GPIO_HIGH && prevVal == GPIO_LOW) // button released, duration is the duration the button kept pressed
                {
                    i++;
                    prevVal = 1;
                    prevTime = lastTime;            
                    printf("Button kept pressed for %dms\n", duration);

                    if (duration > LONG_PRESS_TIMEOUT)                      // do nothing
                    {
                        i--;
                        printf("[%d] Not adding anything\n", i);
                    }
                    else if (duration > SHORT_PRESS_TIMEOUT)                // add '_'
                    {
                        printf("[%d] Adding '_'\n", i);
                        symbols[i] = *DASH;
                        symbols[i+1] = '\0';
                    }
                    else                                                    // add '.'
                    {
                        printf("[%d] Adding '.'\n", i);
                        symbols[i] = *DOT;
                        symbols[i+1] = '\0';                    
                    }
                    
                    puts("");            
                }
                else if (newVal == GPIO_LOW && prevVal == GPIO_HIGH) // button pressed, duration is the duration the button kept depressed
                {
                    prevVal = 0;
                    prevTime = lastTime;            
                    printf("Button kept depressed for %dms\n", duration);
                    
                    if (duration > SYMBOLS_TIMEOUT && duration < APP_TIMEOUT) // long silence -> translate symbols into letter
                    {
                        
                        i++;
                        printf("[%d] Calculating symbol\n", i);
                        
                        // at this point a letter should be formed, check the MORSE_CODE array for it
                        int idx;
                        for(idx = 0; idx < sizeof(MORSE_CODE) /  sizeof(char[6]); idx++)
                        {
                            if (strcmp(symbols, MORSE_CODE[idx]) == 0) // idx found
                                break;
                        }

                        printf("The pattern '%s' is at index %d and matches to %c\n", symbols, idx, idx+32);
                        
                        // insert corresponding letter in letters[]                    
                        letters[l] = ' ' + idx;
                        
                        if (idx == morse_code_array_size)
                        {
                            letters[l] = '#';   // error in pattern
                        }

                        letters[l+1] = '\0';
                        l++;
                        
                        if (duration > SPACE_TIMEOUT) // also add a space after the letter in letters[]
                        {
                            printf("[%d] Adding space\n", i);
                            letters[l] = *SPACE;
                            letters[l+1] = '\0';
                            l++;
                        }

                        // clear symbols[]
                        i = -1;
                        symbols[0] = '\0';
                    }
                    else // short silence -> continue with reading the symbol pattern
                    {
                        printf("[%d] Not adding anything\n", i);
                    }
                    
                    puts("");
                }
                else // input did not change its value so do nothing
                {
                    printf("[%d] Not adding anything\n", i);
                    puts("");
                    prevTime = lastTime;
                }

                printf("Symbols: '%s' (%lu)\n", symbols, strlen(symbols));
                printf("Letters: '%s' (%lu)\n", letters, strlen(letters));
                
                if (strlen(symbols) > 5) // TODO: here I should reset symbols[]
                {
                    printf("INPUT TOO LONG!\n\n");
                }

                gpiod_line_release(line);
            } // end main loop
        }        
        else
        {
            UINT line_num = LED_GPIO;
            
            char *input = NULL;
            size_t zero = 0;
            ssize_t read = 0;

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
            
            while (1) 
            {
                puts("Enter a your input (q to finish):");
                if ((read = getline(&input, &zero, stdin)) == -1)
                {
                    break; 
                }
                if (strlen(input) == 2 && input[0] == 'q')
                {
                    break;
                }

                input[read-1] = '\0';
                printf("You entered = '%s'\n", input);
                printf("Input length = %zu\n", strlen(input));

                outputMorseCodesentence(line, input);            

                puts("");
            }
            
            free(input);
        }
    }
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
        Anything else to QUIT.");

        if ((read = getline(&input, &zero, stdin)) < 0) {            
            perror("getline");
            return EXIT_FAILURE;
        }

        if(input[0] == '1') {
            *MORSE_DECODE = true;
            return EXIT_SUCCESS;
        }
        else if (input[0] == '2') {
            *MORSE_DECODE = false;
            return EXIT_SUCCESS;
        }
        else {
            return 2;
        }
}

/*
 * Wait for an event to happen on the line
 */
int wait_for_input(struct gpiod_line* line, timespec_t* eventTime)
{
    int ret;
    struct gpiod_line_event event;
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
    // printf("INITIAL event notification on line #%u %d times\n", line_num, i);
    // printf("Event type: %d\n", event.event_type);
    // printf("Event time: %lo sec %lo nsec\n", event.ts.tv_sec, event.ts.tv_nsec);
    // puts("");

    *eventTime = event.ts; // keep record of when initial event occurred to calculate duration
    return EXIT_SUCCESS;

}

/*
 * Debounce the events on the line
 */
int debounce_input(struct gpiod_line* line)
{
    int ret;
    struct gpiod_line_event event;
    timespec_t bounceTimeout = { 0, BOUNCE_TIMEOUT * 1000000 };

    // debouncing loop - wait for bounce events to end
    while(true)
    {
        if ((ret = gpiod_line_event_wait(line, &bounceTimeout)) < 0) 
        {
            perror("Wait event notification failed\n");            
            return EXIT_FAILURE;
        }
        else if (ret == 0) // timeout, i.e. bouncing ended - exit loop
        {
            gpiod_line_release(line);
            return EXIT_SUCCESS;
        }

        if ((ret = gpiod_line_event_read(line, &event)) < 0) 
        {
            perror("Read last event notification failed\n");            
            return EXIT_FAILURE;
        }
        
        /* informative code */            
        //puts("");
        //printf("BOUNCE event notification on line #%u %d times\n", line_num, i);
        //printf("Event type: %d\n", event.event_type);
        //printf("Event time: %lo sec %lo nsec\n", event.ts.tv_sec, event.ts.tv_nsec);
        //puts("");
    }
}

/*
 * Read stable input from line
 */
int read_stable_input(struct gpiod_line* line, int* value)
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

    return(EXIT_SUCCESS);
}

/*
 * Output a morse code sentence
 */
int outputMorseCodesentence(struct gpiod_line* line, char *sentence)
{
    int sentenceLength = strlen(sentence);
    for(int charIndex=0; charIndex < sentenceLength; charIndex++)
    {
        if (outputMorseCharacter(line, toupper(sentence[charIndex])) != EXIT_SUCCESS)
        {
            return EXIT_FAILURE;
        }
        msleep(UNIT_MILI_SECONDS * 3); // Space between letters = 3 units
    }

    return EXIT_SUCCESS;
}

/*
 * Output a morse code character
 */
int outputMorseCharacter(struct gpiod_line* line, int character)
{
    if (isValidMorseCodeCharacter(character) == false)
    {
        return EXIT_SUCCESS;
    }

    putchar(character);
    putchar(*SPACE);

    int morseIndex = character - 32;
    int morseCodeLength = strlen(MORSE_CODE[morseIndex]);
    for(int charIndex=0; charIndex<morseCodeLength; charIndex++)
    {
        if (MORSE_CODE[morseIndex][charIndex] == *SPACE)
        {
            msleep(UNIT_MILI_SECONDS * 7); // Space between words = 7 units
            continue;
        }

        if (blinkLed(line, MORSE_CODE[morseIndex][charIndex]) != EXIT_SUCCESS)
        {
            return EXIT_FAILURE;
        }

        msleep(UNIT_MILI_SECONDS); // Space between parts of same letters = 1 unit
    }

    putchar(*SPACE);
    fflush(stdout);

    return EXIT_SUCCESS;
}

/*
 * Output the dot or dash from the led
 */
int blinkLed(struct gpiod_line* line, char dotDash)
{
    if (dotDash == *DOT)
    {
        putchar(*DOT);        
        fflush(stdout);
        return setGPIOPin(line, UNIT_MILI_SECONDS); // Dot = 1 unit
    }

    if (dotDash == *DASH)
    {
        putchar(*DASH);        
        fflush(stdout);
        return setGPIOPin(line, UNIT_MILI_SECONDS * 3); // Dash = 3 units
    }

    printf("Error cannot determine if dot or dash.");

    return EXIT_FAILURE;
}

/*
 * Set GPIO pin to high then low for a length of micro seconds
 */
int setGPIOPin(struct gpiod_line* line, int miliSeconds)
{
    int ret;

    if ((ret = gpiod_line_set_value(line, GPIO_HIGH))  < 0) 
    {
        perror("Set line output high failed\n");
        return EXIT_FAILURE;
    }

    msleep(miliSeconds);

    if ((ret = gpiod_line_set_value(line, GPIO_LOW))  < 0) 
    {
        perror("Set line output low failed\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/*
 * Validate if we support the entered character
 */
bool isValidMorseCodeCharacter(int charIndex)
{
    if (charIndex >= 65 && charIndex <= 90)
    {
        return true;    // Alphabet character
    }

    if (charIndex >= 48 && charIndex <= 57)
    {
        return true;    // Numeric character
    }

    if (charIndex == 32)
    {
        return true;    // Spaces
    }

    return false;
}

/*
 * Release the line, close the chip and exit with status code STATUS
 */
void release_line_and_exit(struct gpiod_chip * chip, struct gpiod_line* line, int status)
{
    gpiod_line_release(line);
    close_chip_and_exit(chip, status);
}

/*
 * Close the chip and exit with status code STATUS
 */
void close_chip_and_exit(struct gpiod_chip * chip, int status)
{
    gpiod_chip_close(chip);
    exit(status);
}
