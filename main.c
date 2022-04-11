#define _GNU_SOURCE 

#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#ifndef	CONSUMER
#define	CONSUMER	"Consumer"
#endif

#define APP_TIMEOUT 10000               // depress duration [ms] for input end
#define SYMBOLS_TIMEOUT 1500            // depress duration [ms] for signalling end of pattern
#define SPACE_TIMEOUT 4500              // depress duration [ms] for space char
#define SHORT_PRESS_TIMEOUT 1500        // max. press duration [ms] for dot char
#define LONG_PRESS_TIMEOUT 4500        //  if pressed longer than this [ms] nothing will be added
#define BOUNCE_TIMEOUT 100              // max. bounce interval [ms]

#define BUTTON 1
#define LEDS !BUTTON

typedef unsigned int UINT;
typedef struct timespec timespec_t;

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

void msleep(UINT milisec) 
{
    time_t sec= (int)(milisec/1000);
    milisec = milisec - (sec*1000);
    timespec_t req= {0};
    req.tv_sec = sec;
    req.tv_nsec = milisec * 1000000L;
    if(nanosleep(&req,&req)) perror("nanosleep");
}

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

        line = gpiod_chip_get_line(chip, line_num);
        if (!line) {
            perror("Get line failed\n");
            ret = -1;
            goto close_chip;
        }
        
        // main loop
        while (true)
        {
            // request events
            ret = gpiod_line_request_both_edges_events(line, CONSUMER);
            if (ret < 0) 
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
            // printf("\n");
            // printf("INITIAL event notification on line #%u %d times\n", line_num, i);
            // printf("Event type: %d\n", event.event_type);
            // printf("Event time: %lo sec %lo nsec\n", event.ts.tv_sec, event.ts.tv_nsec);
            // printf("\n");
            
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
                //printf("\n");
                //printf("BOUNCE event notification on line #%u %d times\n", line_num, i);
                //printf("Event type: %d\n", event.event_type);
                //printf("Event time: %lo sec %lo nsec\n", event.ts.tv_sec, event.ts.tv_nsec);
                //printf("\n");
            }

            // printf("\n");
            // printf("End of bounces!\n");
            // printf("\n");
            
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
                else if (duration < SHORT_PRESS_TIMEOUT)                // add '.'
                {
                    printf("[%d] Adding '.'\n", i);
                    symbols[i] = '.';
                    symbols[i+1] = '\0';
                }
                else                                                    // add '_'
                {
                    printf("[%d] Adding '_'\n", i);
                    symbols[i] = '_';
                    symbols[i+1] = '\0';
                }
                
                printf("\n");            
            }
            else if (newVal == 0 && prevVal == 1) // button pressed, duration is the duration the button kept depressed
            {
                prevVal = 0;
                prevTime = lastTime;            
                printf("Button kept depressed for %dms\n", duration);
                
                if (duration > SYMBOLS_TIMEOUT && duration < APP_TIMEOUT)
                {
                    
                    i++;
                    printf("[%d] Calculating symbol\n", i);
                    // symbols[i] = ' ';
                    // symbols[i+1] = '\0';

                    // at this point a letter should be formed, check the MORSE_CODE array for it
                    int idx;
                    for(idx = 0; idx < sizeof(MORSE_CODE) /  sizeof(char[6]); idx++)
                    {
                        if (strcmp(symbols, MORSE_CODE[idx]) == 0) // idx found
                            break;
                    }

                    printf("The pattern '%s' is at index %d and matches to %c\n", symbols, idx, idx+32);
                    
                    // clear symbols[] and insert corresponding letter in letters[]
                    i = -1;
                    symbols[0] = '\0';
                    letters[l] = ' ' + idx;
                    
                    if (idx == morse_code_array_size)
                    {
                        letters[l] = '#';   // error in pattern
                    }

                    letters[l+1] = '\0';
                    l++;
                    
                    if (duration > SPACE_TIMEOUT) // insert a space in letters[]
                    {
                        printf("[%d] Adding space\n", i);
                        letters[l] = ' ';
                        letters[l+1] = '\0';
                        l++;
                    }
                }
                else
                {
                    printf("[%d] Not adding anything\n", i);
                }
                
                printf("\n");
            }
            else
            {
                printf("[%d] Not adding anything\n", i);
                printf("\n");
                prevTime = lastTime;
            }

            printf("Symbols: '%s' (%lu)\n", symbols, strlen(symbols));
            printf("Letters: '%s' (%lu)\n", letters, strlen(letters));
            
            if (strlen(symbols) > 5) // TODO: should reset symbols[]
            {
                printf("INPUT TOO LONG!\n\n");
            }

            gpiod_line_release(line);
        } // end main loop
    }
    
    if (LEDS)
    {
        printf("START HERE!\n");
        sleep(5);
    }


    
    ret = 0;

release_line:
    gpiod_line_release(line);
close_chip:
    gpiod_chip_close(chip);
end:
    return ret;
}
