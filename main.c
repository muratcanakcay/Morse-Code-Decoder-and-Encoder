#define _GNU_SOURCE 

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

#define APP_TIMEOUT 10000               // depress duration [ms] for input end
#define SYMBOLS_TIMEOUT 1500            // depress duration [ms] for signalling end of pattern
#define SPACE_TIMEOUT 4500              // depress duration [ms] for space char
#define SHORT_PRESS_TIMEOUT 1500        // max. press duration [ms] for dot char
#define LONG_PRESS_TIMEOUT 4500         //  if pressed longer than this [ms] nothing will be added
#define BOUNCE_TIMEOUT 100              // max. bounce interval [ms]


#define BUTTON 0
#define LEDS !BUTTON

typedef unsigned int UINT;
typedef struct timespec timespec_t;

const char DOT[1] = ".";
const char DASH[1] = "-";
const char SPACE[1] = " ";
const int UNIT_MILI_SECONDS = 100;
const int GPIO_LOW = 0;
const int GPIO_HIGH = 1;

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

int outputMorseCharacter(int gpioPin, char *character);
int outputBuzzer(int gpioPin, char dotDash);
int setGPIOPin(int gpioPin, int miliSeconds);
bool isValidMorseCodeCharacter(char *character);
void msleep(UINT miliSecconds);



int main(int argc, char **argv)
{
    char *chipname = "gpiochip0";
    struct gpiod_line_event event;
    struct gpiod_chip *chip;
    struct gpiod_line *line;
    int ret;

    if ((chip = gpiod_chip_open_by_name(chipname)) == NULL)
    {
        perror("Open chip failed\n");
        ret = -1;
        goto end;
    }	
    
    if (BUTTON)
    {        
        UINT line_num = 13;	// GPIO Pin #13        

        timespec_t bounceTimeout = { 0, BOUNCE_TIMEOUT * 1000000 };
        timespec_t appTimeout = { APP_TIMEOUT/1000, 0 };
        timespec_t prevTime, lastTime;
        
        int newVal, oldVal;
        int morse_code_array_size = sizeof(MORSE_CODE) /  sizeof(char[6]);
        int i = -1; // symbols index
        int l = 0;  // letters index    
        int prevVal = 1; // starting value of input

        char symbols[500] = "\0"; // will be [6] at the end
        char letters[500] = "\0";        

        if ((line = gpiod_chip_get_line(chip, line_num)) == NULL)
        {
            perror("Get line failed\n");
            ret = -1;
            goto close_chip;
        }
        
        // main loop
        while (true)
        {
            // request events
            if ((ret = gpiod_line_request_both_edges_events(line, CONSUMER)) < 0) 
            {
                perror("Request event notification failed\n");
                ret = -1;
                goto release_line;
            }        

            if ((ret = gpiod_line_event_wait(line, &appTimeout)) < 0)        
            {
                perror("Wait event notification failed\n");
                ret = -1;
                goto release_line;
            }
            else if (ret == 0) // appTimeout
            {
                printf("Input ended\n");
                goto release_line;
            }

            if ((ret = gpiod_line_event_read(line, &event)) < 0)        
            {
                perror("Read last event notification failed\n");
                ret = -1;
                goto release_line;
            }

            lastTime = event.ts; // keep record of when initial event occurred to calculate duration

            /* informative code */
            // puts("");
            // printf("INITIAL event notification on line #%u %d times\n", line_num, i);
            // printf("Event type: %d\n", event.event_type);
            // printf("Event time: %lo sec %lo nsec\n", event.ts.tv_sec, event.ts.tv_nsec);
            // puts("");
            
            // debouncing loop - wait for bounce events to end
            while(true)
            {
                if ((ret = gpiod_line_event_wait(line, &bounceTimeout)) < 0) 
                {
                    perror("Wait event notification failed\n");
                    ret = -1;
                    goto release_line;
                }
                else if (ret == 0) // timeout, i.e. bouncing ended - exit loop
                {
                    break;
                }

                if ((ret = gpiod_line_event_read(line, &event)) < 0) 
                {
                    perror("Read last event notification failed\n");
                    ret = -1;
                    goto release_line;
                }

                
                /* informative code */            
                //puts("");
                //printf("BOUNCE event notification on line #%u %d times\n", line_num, i);
                //printf("Event type: %d\n", event.event_type);
                //printf("Event time: %lo sec %lo nsec\n", event.ts.tv_sec, event.ts.tv_nsec);
                //puts("");
            }

            // puts("");
            // printf("End of bounces!\n");
            // puts("");
            
            // release line from event request
            gpiod_line_release(line);

            // request input        
            if ((ret = gpiod_line_request_input(line, CONSUMER)) < 0) 
            {
                perror("Request line as input failed\n");
                goto release_line;
            }

            if ((newVal = gpiod_line_get_value(line)) < 0) 
            {
                perror("Read line input failed\n");
                goto release_line;
            }

            /* informative code */
            //printf("Input %d on line #%u\n", newVal, line_num);
            // printf("PREV Input %d on line #%u\n", prevVal, line_num);
            // printf("FINAL Input %d on line #%u\n", newVal, line_num);
            // printf("lastTime: %ld.%ld\n", lastTime.tv_sec, lastTime.tv_nsec);
            // printf("prevTime: %ld.%ld\n", prevTime.tv_sec, prevTime.tv_nsec);

            // calculate duration between previous input and current input
            int duration = (int)((lastTime.tv_sec - prevTime.tv_sec) * 1000 + (lastTime.tv_nsec - prevTime.tv_nsec) / 1000000); 
            printf("%d -> %dms -> %d\n", prevVal, duration, newVal);        

            if (newVal == 1 && prevVal == 0) // button released, duration is the duration the button kept pressed
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
                    symbols[i] = '_';
                    symbols[i+1] = '\0';
                }
                else                                                    // add '.'
                {
                    printf("[%d] Adding '.'\n", i);
                    symbols[i] = '.';
                    symbols[i+1] = '\0';                    
                }
                
                puts("");            
            }
            else if (newVal == 0 && prevVal == 1) // button pressed, duration is the duration the button kept depressed
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
                        letters[l] = ' ';
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
    
    if (LEDS)
    {
        char *input = NULL;
        size_t zero = 0;
        ssize_t read = 0;
        
        while (1) 
        {
            puts("Enter a your input:");
            if ((read = getline(&input, &zero, stdin)) == -1)
            {
                break; 
            }
            input[read-1] = '\0';
            printf("You entered = '%s'\n", input);
            printf("Input length = %zu\n", strlen(input));
            

            puts("");
        }
        
        free(input);
    }


    
    ret = 0;

release_line:
    gpiod_line_release(line);
close_chip:
    gpiod_chip_close(chip);
end:
    return ret;
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
 * Output a morse code sentence
 */
int outputMorseCodesentence(int gpioPin, char *sentence)
{
    int sentenceLength = strlen(sentence);
    for(int charIndex=0; charIndex<sentenceLength; charIndex++)
    {
        if (outputMorseCharacter(gpioPin, (char*)toupper(sentence[charIndex])) != EXIT_SUCCESS)
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
int outputMorseCharacter(int gpioPin, char *character)
{
    if (isValidMorseCodeCharacter(character) == false)
    {
        return EXIT_SUCCESS;
    }

    putchar((int)character);
    putchar(*SPACE);

    int morseIndex = (int)character - 32;
    int morseCodeLength = strlen(MORSE_CODE[morseIndex]);
    for(int charIndex=0; charIndex<morseCodeLength; charIndex++)
    {
        if (MORSE_CODE[morseIndex][charIndex] == *SPACE)
        {
            usleep(UNIT_MILI_SECONDS * 7); // Space between words = 7 units
            continue;
        }

        if (outputBuzzer(gpioPin, MORSE_CODE[morseIndex][charIndex]) != EXIT_SUCCESS)
        {
            return EXIT_FAILURE;
        }

        usleep(UNIT_MILI_SECONDS); // Space between parts of same letters = 1 unit
    }

    putchar(*SPACE);

    return EXIT_SUCCESS;
}

/*
 * Output the dot or dash from the buzzer
 */
int outputBuzzer(int gpioPin, char dotDash)
{
    if (dotDash == *DOT)
    {
        putchar(*DOT);
        return setGPIOPin(gpioPin, UNIT_MILI_SECONDS); // Dot = 1 unit
    }

    if (dotDash == *DASH)
    {
        putchar(*DASH);
        return setGPIOPin(gpioPin, UNIT_MILI_SECONDS * 3); // Dash = 3 units
    }

    printf("Error cannot determine if dot or dash.");

    return EXIT_FAILURE;
}

/*
 * Set GPIO pin to high then low for a length of micro seconds
 */
int setGPIOPin(int gpioPin, int miliSeconds)
{
    // if (gpioWrite(gpioPin, GPIO_HIGH) != EXIT_SUCCESS)
    // {
    //     return EXIT_FAILURE;
    // }

    // usleep(miliSeconds);

    // if (gpioWrite(gpioPin, GPIO_LOW) != EXIT_SUCCESS)
    // {
    //     return EXIT_FAILURE;
    // }

    return EXIT_SUCCESS;
}

/*
 * Validate if we support the entered character
 */
bool isValidMorseCodeCharacter(char* character)
{
    int charIndex = (int)character;

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