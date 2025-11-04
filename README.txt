Name: Megan Garrett
Date: 11/04/2025
Environment: VS Code, Linux
Version Control: GitHub https://github.com/kor1andr/Project-5
- - - - - - - - - -
[AI] GPT 4.1
    [Prompts]
    - 
[Summary]
    Found to be very useful, unfortunately... I'm not a fan of the increasing over-reliance on generative AI and
    do not feel it should be a used as a replacement, as it so often is pushed to be in professional settings--but
    rather an aid. Still, it's been some years since I have worked with C/C++, so it has been especially useful in
    helping me refresh quickly and reducing testing/debug time.

    My output is stalling because OSS is trying to send a message to a worker that has already exited (i.e. a race condition).
    I spent significant time troubleshooting this--verifying the PCB entry is marked as free *immediately* after a worker exits,
    double-checking before sending a message that the process is still occupied and PID is valid,
    marking PCB unoccupied in the waitpid cleanup NOT the scheduler...
    I have checks before and after msgrcv, but there is a small window where the process can exit between checks. I have a timeout
    and cleanup after msgrcv to try and recover from this, but am not sure how else to prevent this race condition.
    Any advice you can offer on this is appreciated!
- - - - - - - - - -
[How to Compile]
    - In the terminal, type "make"
[How to Run]
After compiling:
    1) For detailed instructions and help:
        ./oss -h
    2) Run the main program 'oss' with default parameters: ./oss
        - This will launch 5 workers, up to 2 at a time, each for 3 econds, with 0.1 seconds between launches.
    [OR]
    3) Input your own command-line arguments to run:
        -n <number>     :   Total number of workers to launch
        -s <number>     :   Max number of workers to run simultaneously
        -t <float>      :   Max simulated time for each worker (seconds, can be fractional)
        -i <float>      :   Min time interval between worker launches (seconds, can be fractional)
        -f <filename>   :   Log file for oss output
            Example     :   ./oss  -n 3    -s 1    -t 2.5  -i 0.2   -f log.txt
        - This will launch 3 workers, up to 1 at a time, each for up to 2.5 seconds, with 0.2 seconds between launches.
    4)  OSS will alternate sending messages to each worker and coordinate their actions.
    5)  Worker output will appear on the screen; oss output will appear both on the screen and in the log file specified.
    6)  Program will automatically clean up resources and terminate after 60 real seconds or if interrupted with CTRL+C.
