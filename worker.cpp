#include <iostream>
#include <cstdlib>
#include <unistd.h>         // getpid(), getppid()
#include <sys/ipc.h>        // shmat()
#include <sys/shm.h>        // shmat(), shmdt()
#include <sys/msg.h>        // msgget(), msgsnd(), msgrcv()
#include <signal.h>
#include <ctime>            // srand, rand

// SimClock
struct SimClock {
    unsigned int seconds;
    unsigned int nanoseconds;
};

struct MsgBuf {
    long mtype;
    int status;
    int quantum;
    int result;
};

#define RESOURCE_CLASSES 10
#define INSTANCES_PER_RESOURCE 5

// Signal handler for SIGTERM
void handle_sigterm(int) {
    std::cout << "WORKER: Received SIGTERM, exiting immediately." << std::endl;
    exit(0);
}

// MAIN
    // Check for # of args: program name, seconds, nanoseconds, shm_id, msq_id
int main(int argc, char* argv[]) {
    signal(SIGTERM, handle_sigterm);

    // If not enough args, print error and example usage
    if (argc < 5) {
        std::cerr << "[ERROR] Not enough arguments provided.\n";
        std::cout << "[Example] ./worker <seconds> <nanoseconds> <shm_id> <msq_id>\n";
        return 1;
    }

    // Parse args
        // [std::atoi] = convert arg from string --> int and store in respective variable
    int intervalSec = std::atoi(argv[1]);
    int intervalNano = std::atoi(argv[2]);
    int shm_id = std::atoi(argv[3]);
    int msq_id = std::atoi(argv[4]);

    // SimClock* = pointer to SimClock struct to access shared memory as a clock
        // [shmat()]: attach shm segment to process's address space so it can be accessed
        // [nullptr, 0] = let system choose address to attach segment, default flags
    SimClock* clock = (SimClock*)shmat(shm_id, nullptr, 0);
    // If shmat() fails, print error and exit
    if (clock == (void*) -1) {
        std::cerr << "WORKER: shmat failed.\n";
        return 1;
    }

    // Retrieve and store PID + PPID
    pid_t pid = getpid();
    pid_t ppid = getppid();
    // Seed random number generated with current time + PID so each worker process gets different sequence
    srand(time(NULL) ^ pid);

    int totalCpuUsed = 0;
    int cpuBurstLimit = intervalSec * 1000000000 + intervalNano;
    int done = 0;

    // Store start time from shared clock
    int startSec = clock->seconds;
    int startNano = clock->nanoseconds;
    // Calculate termination time (start time + interval)
    int termSec = startSec + intervalSec;
    int termNano = startNano + intervalNano;
    // Handle nanoseconds overflow; If termNano >= 1 billion, convert excess to seconds ((termNano % 1e9) + termsec)
    if (termNano >= 1000000000) {
        termSec += termNano / 1000000000;
        termNano = termNano % 1000000000;
    }

    // Print startup info (PID, PPID, system clock, term time)
    std::cout << "WORKER PID: " << pid << ", PPID: " << ppid << std::endl;
    std::cout << "SysClockS: " << startSec << " SysClockNano: " << startNano << std::endl;
    std::cout << "TermTimeS: " << termSec << " TermTimeNano: " << termNano << std::endl;
    std::cout << "--Just Starting" << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    // // [messagesRecieved]: Track # of messages received from OSS
    int messagesReceived = 0;
    // MAIN LOOP
    while (!done) {
        MsgBuf msg;
        // [msgrcv]: receive message from OSS with mtype == this worker's PID (so worker only receives messages intended for it)
        msgrcv(msq_id, &msg, sizeof(MsgBuf) - sizeof(long), pid, 0);
        // Increment messages received count
        messagesReceived++;

        // Read current time from shared clock
        int currentSec = clock->seconds;
        int currentNano = clock->nanoseconds;

        // Print current info (PID, PPID, system clock, term time, # of messages received)
        std::cout << "WORKER PID: " << pid << " PPID: " << ppid << std::endl;
        std::cout << "SysClockS: " << currentSec << " SysClockN: " << currentNano << std::endl;
        std::cout << "TermTimeS: " << termSec << " TermTimeNano: " << termNano << std::endl;
        std::cout << "--" << messagesReceived << " messages received from OSS." << std::endl;

        int done = 0;
        // Check if current time has reached or exceeded scheduled termination time
            // if true, set done = 1 to indicate worker should terminate and print termination message
        if (currentSec > termSec || (currentSec == termSec && currentNano >= termNano)) {
            std::cout << "--Terminating after sending message back to OSS after " << messagesReceived << " received messages." << std::endl;
            done = 1;
        }

        int quantum = msg.quantum;
        int action = rand() % 100;

        // Reply message to OSS
        MsgBuf reply;
        // set mtype = this worker's PID + 1000 (so OSS can differentiate from messages sent to worker)
        // status = done (0 if terminating, 1 if continuing)
        // [msgsnd]: send message back to OSS
        reply.mtype = pid + 1000;
        
        if (totalCpuUsed + quantum >= cpuBurstLimit) {
            int use = cpuBurstLimit - totalCpuUsed;
            reply.result = -use;
            reply.status = 0;
            totalCpuUsed += use;
            done = 1;
        } else if (action < 10) {
            int use = rand() % quantum;
            reply.result = use;
            reply.status = 2;       // blocked
            totalCpuUsed += use;
        } else if (action < 80) {
            reply.result = quantum;
            reply.status = 1;       // used full quantum
            totalCpuUsed += quantum;
        } else { 
            int use = rand() % quantum;
            reply.result = -use;
            reply.status = 0;       // terminating
            totalCpuUsed += use;
            done = 1;
        }
        msgsnd(msq_id, &reply, sizeof(MsgBuf) - sizeof(long), 0);

        if (done) {
            // DEBUG PRINT: Check worker process actually exits after sending final message
            std::cout << "WORKER PID: " << pid << " is exiting now." << std::endl;
            break;
        }

        // [myResources]: Track instances of each resource class currently held by worker process, initialized to 0
        int myResources[RESOURCE_CLASSES] = {0};

        // Generate rand int (0-99) to determine action
        int action = rand() % 100;
        // [resourceRequest]: array to specify how many instances of each resource class the worker wants to request (+N) or release (-N), initialized to 0
        int resourceRequest[RESOURCE_CLASSES] = {0};

        // 60% chance: worker requests resources
        if (action < 60) {
            int r = rand() % RESOURCE_CLASSES;                          // [r]: randomly select resource class (0-9)
            int maxRequest = INSTANCES_PER_RESOURCE - myResources[r];   // Calculate how many more instances of resource r can be requested
            if (maxRequest > 0) {                                       // If at least 1 instance can be requested
                int req = 1 + rand() % maxRequest;                      // Randomly decide to request between 1 and maxRequest instances
                bool canRequest = true;                                 // Deadlock Prevention:
                for (int i = 0; i < r; ++i) {                           // worker can only request resource [r] IF... 
                    if (myResources[i] == 0) canRequest = false;        // it already holds at least 1 instance of every lower-numbered resource class [0 to r-1]
                }
                if (canRequest) {                                       // If deadlock prevention condition met
                    resourceRequest[r] = req;                           // Record request in resourceRequest array
                }
            }
        } else if (action < 100) {                                      // 40% chance: worker releases resources
            int r = rand() % RESOURCE_CLASSES;                          // [r]: randomly select resource class (0-9)
            if (myResources[r] > 0) {                                   // If worker holds at least 1 instance of resource r
                int rel = 1 + rand() % myResources[r];                  // Randomly decide to release between 1 and myResources[r] instances
                resourceRequest[r] = -rel;                              // Record release as negative value in resourceRequest array
            }
        }

        // Send request to oss
        MsgBuf msg;
        msg.mtype = pid;
        msg.status = 1; // 1=request/release
        for (int i = 0; i < RESOURCE_CLASSES; ++i)
            msg.resourceRequest[i] = resourceRequest[i];
        msgsnd(msq_id, &msg, sizeof(MsgBuf) - sizeof(long), 0);

        // Wait for oss to reply (granted or blocked)
        msgrcv(msq_id, &msg, sizeof(MsgBuf) - sizeof(long), pid + 1000, 0);

        // If granted, update myResources
        if (msg.result == 1) {
            for (int i = 0; i < RESOURCE_CLASSES; ++i) {
                myResources[i] += resourceRequest[i];
                if (myResources[i] < 0) myResources[i] = 0;
            }
        }
    }
   
    // NO SLEEP

    // Detach from shared memory
    shmdt(clock);
    return 0;
}
