// Final Project: Deadlock Avoidance vs. Detection for the Dining Philosophers Problem (adapted from lab 6)
// Name: Jordyn Reichert, Alex Leonida
// Date: 5/10/25

#include <pthread.h>  // For pthread_t data type for thread identifiers and associated functions
#include <stdio.h>    // For input/output functions (printf(), scanf(), and file output data types and functions)
#include <unistd.h>   // For usleep() function (more precise sleep times over sleep() (under a second))
#include <sys/time.h> // For high resolution clock for time calculations

// Deadlock Solutions:
// Avoidance Methods:
// 1: Release left chopstick if unable to pick up right
// 2: Even-numbered philosophers pick up left chopstick first, and odd-numbered philosophers pick up right chopstick first
// Detection Methods:
// 3: If deadlock detected, force all philosophers to release both chopsticks
// 4: If deadlock detected, force one philosopher to release both chopsticks (iterate through philosophers)

#define NUM_PHIL 5 // Define the number of philosophers

// Enum and global variables for deadlock solutions 3 and 4
typedef enum
{
    THINKING,
    HUNGRY,
    EATING
} PhilosopherState;
PhilosopherState state[NUM_PHIL]; // Current state of philosopher (THINKING, HUNGRY, or EATING) (for solutions 3 and 4)
int lastSelectedPhilosopher = -1; // Initialize last selected philosopher to -1 (no philosopher selected yet) (for solution 4)

// Semaphore struct (binary semaphore)
typedef struct
{
    int value; // Can only be 0 or 1 (0 = locked, 1 = available)
} semaphore;

// Function declarations
void *philosopher(void *id);                              // Function called by the philosopher threads
void *monitorDeadlock(void *arg);                         // Function called by monitor thread (used to detect deadlock for solutions 3 and 4)
void think(int id);                                       // Make philosopher think for a static amount of time and print that time to the console
void eat(int id);                                         // Make philosopher eat for a static amount of time and print that time to the console
void semInit(semaphore *s, int value);                    // Initialize semaphore (value represents whether it is available or not)
void semWait(semaphore *s);                               // Semaphore wait (lock the semaphore)
void semSignal(semaphore *s);                             // Semaphore signal (unlock the semaphore)
int compare_and_swap(int *word, int testval, int newval); // Compare and swap function (implementation is from class example)
void outputStatsToFile();                                 // Outputs simulation stats to specific files determined by the chosen deadlock solution

/// Global variables
// Simulation
int deadlockSolution;         // User selected deadlock solution (1, 2, 3, or 4)
int simCompleted = 0;         // Flag to tell whether the simulation has finished (used by monitor thread to know when to exit)
unsigned int simDuration = 5; // Duration the simulation will execute (in seconds)
struct timeval startTime;     // Simulation start time
// Statistics
unsigned int numOfDeadlocks = 0;      // Number of deadlocks detected
unsigned int totalForcedReleases = 0; // Total forced releases
unsigned int maxWaitingTime;          // Max waiting time over all philosophers (in milliseconds)
unsigned int minWaitingTime;          // Min waiting time over all philosophers (in milliseconds)
unsigned int avgEatingTime = 0;       // Average eating time (in milliseconds)
unsigned int avgThinkingTime = 0;     // Average thinking time (in milliseconds)
unsigned int avgWaitingTime = 0;      // Average waiting time (in milliseconds)
// Individual philosopher stats
unsigned int forcedReleases[NUM_PHIL] = {0}; // Number of times each philosopher was forced to release chopsticks
unsigned int timeEating[NUM_PHIL] = {0};     // Time each philosopher spends eating (in milliseconds)
unsigned int timeThinking[NUM_PHIL] = {0};   // Time each philosopher spends thinking (in milliseconds)
unsigned int timeWaiting[NUM_PHIL] = {0};    // Time each philosopher spends waiting to eat (in milliseconds)
// Threads
pthread_t philosophers[NUM_PHIL]; // Philosopher threads
pthread_t monitor;                // Thread to monitor deadlock (only used for solutions 3 and 4)
pthread_attr_t attr;              // Set of thread attributes
// Semaphores
semaphore chopstick[NUM_PHIL]; // Array of chopstick semaphores (each element represents a chopstick)

// Main function
int main(int argc, char *argv[])
{
    // Prompt the user to select a deadlock solution algorithm
    printf("\nChoose desired deadlock solution algorithm [1, 2, 3, or 4]: ");
    scanf("%d", &deadlockSolution);

    // Initialize chopsticks
    for (int i = 0; i < NUM_PHIL; i++)
    {
        semInit(&chopstick[i], 1); // Each chopstick is available initially
    }

    // Get the default attributes before creating threads
    pthread_attr_init(&attr);

    // Create monitor thread (for deadlock solutions 3 and 4)
    if (deadlockSolution == 3 || deadlockSolution == 4)
    {
        pthread_create(&monitor, &attr, monitorDeadlock, NULL);
    }

    // Set simulation start time
    gettimeofday(&startTime, NULL);

    // Create philosopher threads
    int philosopherID[NUM_PHIL];
    for (int i = 0; i < NUM_PHIL; i++) // Set available ID values for philosophers
    {
        philosopherID[i] = i;
    }
    for (int i = 0; i < NUM_PHIL; i++) // Pass attributes to the threads
    {
        pthread_create(&philosophers[i], &attr, philosopher, &philosopherID[i]); // Create the thread (and pass ID as an arg to identify philosopher)
    }

    // Wait for philosopher threads to exit
    for (int i = 0; i < NUM_PHIL; i++)
    {
        pthread_join(philosophers[i], NULL);
    }
    simCompleted = 1; // Set simCompleted flag to tell monitor thread to stop looping

    // Wait for monitor thread to exit (for deadlock solutions 3 and 4)
    if (deadlockSolution == 3 || deadlockSolution == 4)
    {
        pthread_join(monitor, NULL);
    }

    /// Find min and max waiting times
    // Initialize min and max waiting times to the first philosopher's waiting time
    maxWaitingTime = timeWaiting[0];
    minWaitingTime = timeWaiting[0];
    // Max waiting time
    for (int i = 0; i < NUM_PHIL; i++)
    {
        if (timeWaiting[i] > maxWaitingTime)
        {
            maxWaitingTime = timeWaiting[i];
        }
    }
    // Min waiting time
    for (int i = 0; i < NUM_PHIL; i++)
    {
        if (timeWaiting[i] < minWaitingTime)
        {
            minWaitingTime = timeWaiting[i];
        }
    }
    // Find the average eating/thinking/waiting times and total forced releases over all philosophers
    for (int i = 0; i < NUM_PHIL; i++)
    {
        avgEatingTime += timeEating[i];
        avgThinkingTime += timeThinking[i];
        avgWaitingTime += timeWaiting[i];
        totalForcedReleases += forcedReleases[i];
    }
    avgEatingTime /= NUM_PHIL;
    avgThinkingTime /= NUM_PHIL;
    avgWaitingTime /= NUM_PHIL;

    // Print the length of time the simulation ran
    printf("\nSimulation ran for %u seconds.\n", simDuration);

    // Print simulation statistics to compare the performance of the deadlock solutions
    printf("\nDeadlock Solution Performance Stats:\n");
    printf("\nNumber of Deadlocks: %u\n", numOfDeadlocks);
    printf("Total Forced Releases: %u\n", totalForcedReleases);
    printf("Max Waiting Time: %u\n", maxWaitingTime);
    printf("Min Waiting Time: %u\n", minWaitingTime);
    printf("Avg Eating Time: %u\n", avgEatingTime);
    printf("Avg Thinking Time: %u\n", avgThinkingTime);
    printf("Avg Waiting Time: %u\n\n", avgWaitingTime);
    for (int i = 0; i < NUM_PHIL; i++)
    {
        printf("Philosopher %d:\n", i);
        printf("    Ate for %u milliseconds\n", timeEating[i]);
        printf("    Thought for %u milliseconds\n", timeThinking[i]);
        printf("    Waited for %u milliseconds\n", timeWaiting[i]);
        printf("    Was forced to release chopsticks %u times\n", forcedReleases[i]);
    }
    printf("\n");

    outputStatsToFile();
    return 0;
}

// Philosopher function
void *philosopher(void *id) // Function called by the philosopher threads
{
    int philosopherID = *(int *)id; // Philosopher id
    struct timeval currentTime;     // Keeps track of the current time (used in the calculation to stop the simulation after the set duration)
    while (1)
    {
        think(philosopherID); // Think for a static amount of time

        state[philosopherID] = HUNGRY; // Hungry - waiting for chopsticks

        // Wait start time for this cycle
        struct timeval waitStartTime;
        gettimeofday(&waitStartTime, NULL);

        //-------------------------------------------------------------------------------------------------------------------------------------------
        //// Deadlock Solution 1: Release left chopstick if unable to pick up right
        if (deadlockSolution == 1)
        {
            // Pick up left chopstick
            semWait(&chopstick[philosopherID]);
            printf("Philosopher %d picked up his left-hand chopstick.\n", philosopherID);

            usleep(5000); // Delay before trying to pick up second chopstick (to increase chance of deadlock)

            // Pick up right chopstick
            if (compare_and_swap(&(chopstick[(philosopherID + 1) % NUM_PHIL].value), 1, 0) != 1) // Attempt to pick up right chopstick (without waiting)
            {
                // If right chopstick was unavailable
                semSignal(&chopstick[philosopherID]); // Release the left chopstick
                printf("Philosopher %d put down his left-hand chopstick (could not pick up right).\n", philosopherID);
                usleep(1000); // Small delay to avoid excessive retries (which could slow down the system)
                continue;     // Restart the loop
            }
            // If right chopstick was available
            printf("Philosopher %d picked up his right-hand chopstick.\n", philosopherID);
        }
        //-------------------------------------------------------------------------------------------------------------------------------------------

        //-------------------------------------------------------------------------------------------------------------------------------------------
        //// Deadlock Solution 2: Even-numbered philosophers pick up left chopstick first, and odd-numbered philosophers pick up right chopstick first
        if (deadlockSolution == 2)
        {
            if (philosopherID % 2 == 0) // If philosopher has even-numbered ID
            {
                // Pick up left chopstick
                semWait(&chopstick[philosopherID]);
                printf("Philosopher %d picked up left-hand chopstick.\n", philosopherID);

                usleep(5000); // Delay before trying to pick up second chopstick (to increase chance of deadlock)

                // Pick up right chopstick
                semWait(&chopstick[(philosopherID + 1) % NUM_PHIL]);
                printf("Philosopher %d picked up right-hand chopstick.\n", philosopherID);
            }
            else // If philosopher has odd-numbered ID
            {
                // Pick up right chopstick
                semWait(&chopstick[(philosopherID + 1) % NUM_PHIL]);
                printf("Philosopher %d picked up right-hand chopstick.\n", philosopherID);

                usleep(5000); // Delay before trying to pick up second chopstick (to increase chance of deadlock)

                // Pick up left chopstick
                semWait(&chopstick[philosopherID]);
                printf("Philosopher %d picked up left-hand chopstick.\n", philosopherID);
            }
        }
        //-------------------------------------------------------------------------------------------------------------------------------------------

        //-------------------------------------------------------------------------------------------------------------------------------------------
        //// Deadlock Solutions 3 and 4: No prevention method - only deadlock detection (attempt to pick up chopsticks normally)
        if (deadlockSolution == 3 || deadlockSolution == 4)
        {
            // Pick up left chopstick
            semWait(&chopstick[philosopherID]);
            printf("Philosopher %d picked up left-hand chopstick.\n", philosopherID);

            usleep(5000); // Delay before trying to pick up second chopstick (to increase chance of deadlock)

            // Pick up right chopstick
            semWait(&chopstick[(philosopherID + 1) % NUM_PHIL]);
            printf("Philosopher %d picked up right-hand chopstick.\n", philosopherID);
        }
        //-------------------------------------------------------------------------------------------------------------------------------------------

        // Wait end time for this cycle
        struct timeval waitEndTime;
        gettimeofday(&waitEndTime, NULL);

        state[philosopherID] = EATING; // Eating - in possession of two chopsticks

        // Calculate the wait time for this cycle and add to the total waiting time for this philosopher (in milliseconds)
        long waitTimeForThisCycle = (waitEndTime.tv_sec - waitStartTime.tv_sec) * 1000L + (waitEndTime.tv_usec - waitStartTime.tv_usec) / 1000L;
        timeWaiting[philosopherID] += waitTimeForThisCycle;

        eat(philosopherID); // Eat for a static amount of time (if in possession of two chopsticks)

        // Put down right chopstick
        semSignal(&chopstick[(philosopherID + 1) % NUM_PHIL]);
        printf("Philosopher %d put down his right-hand chopstick.\n", philosopherID);

        // Put down left chopstick
        semSignal(&chopstick[philosopherID]);
        printf("Philosopher %d put down his left-hand chopstick.\n", philosopherID);

        state[philosopherID] = THINKING; // Thinking - not hungry yet, so not waiting for chopsticks

        // Calculate the elapsed time (in milliseconds)
        gettimeofday(&currentTime, NULL); // Update current time
        long elapsedTime = (currentTime.tv_sec - startTime.tv_sec) * 1000L + (currentTime.tv_usec - startTime.tv_usec) / 1000L;

        if (elapsedTime >= (simDuration * 1000))
            break;
    }
    pthread_exit(0);
}
void think(int id) // Philosopher think
{
    unsigned int time = 10000; // Always 10 milliseconds
    usleep(time);              // Think for the amount of time specified above

    timeThinking[id] += time / 1000; // Update the total time the philosopher has thought (in milliseconds)

    printf("Philosopher %d thought for %u milliseconds.\n", id, (time / 1000));
}
void eat(int id) // Philosopher eat
{
    unsigned int time = 10000; // Always 10 milliseconds
    usleep(time);              // Eat for the amount of time specified above

    timeEating[id] += time / 1000; // Update the total time the philosopher has eaten (in milliseconds)

    printf("Philosopher %d ate for %u milliseconds.\n", id, (time / 1000));
}

// Function called by the monitor thread - checks for deadlock every second
void *monitorDeadlock(void *arg)
{
    while (!simCompleted)
    {
        usleep(100000); // Check every 100 milliseconds

        // Count how many philosophers are waiting
        int numOfPhilWaiting = 0;
        for (int i = 0; i < NUM_PHIL; i++)
        {
            if (state[i] == HUNGRY)
                numOfPhilWaiting++;
        }
        // If all philsophers are waiting
        if (numOfPhilWaiting == NUM_PHIL)
        {
            printf("\n----DEADLOCK DETECTED----\n");

            numOfDeadlocks++; // Update stats

            // Deadlock solution 3: Force all philosophers to release their chopsticks
            if (deadlockSolution == 3)
            {
                printf("Solution 3: Forcing all philosophers to release chopsticks.\n");

                // Force all philosophers to release each of their chopsticks
                for (int i = 0; i < NUM_PHIL; i++)
                {
                    forcedReleases[i]++;                       // Update stats
                    semSignal(&chopstick[(i + 1) % NUM_PHIL]); // Put down right chopstick
                    semSignal(&chopstick[i]);                  // Put down left chopstick
                    state[i] = THINKING;                       // Reset philosopher state
                }
            }
            // Deadlock solution 4: Force only one philosopher to release his chopsticks
            if (deadlockSolution == 4)
            {
                printf("Solution 4: Selecting one philosopher to release chopsticks.\n");

                // Check if this is the first philosopher selection
                if (lastSelectedPhilosopher == -1) // No philosophers have been selected yet
                {
                    lastSelectedPhilosopher = 0; // Select the first philosopher
                }
                else // A philosopher has been selected previously
                {
                    if (lastSelectedPhilosopher < NUM_PHIL) // If not the last philosopher yet
                    {
                        lastSelectedPhilosopher++; // Increment to select the next philosopher
                    }
                    else // If already at the last philosopher
                    {
                        lastSelectedPhilosopher = 0; // Reset to first
                    }
                }

                forcedReleases[lastSelectedPhilosopher]++;                       // Update stats
                semSignal(&chopstick[(lastSelectedPhilosopher + 1) % NUM_PHIL]); // Put down right chopstick
                semSignal(&chopstick[lastSelectedPhilosopher]);                  // Put down left chopstick
                state[lastSelectedPhilosopher] = THINKING;                       // Reset philosopher state
            }
            printf("-------------------------\n\n");
        }
    }
    pthread_exit(0);
}

// Semaphore functions
void semInit(semaphore *s, int value) // Initialize semaphore
{
    s->value = value;
}
void semWait(semaphore *s) // Semaphore wait
{
    while (compare_and_swap(&(s->value), 1, 0) != 1)
    {
        // Wait until the semaphore is available (unlocked)
    }
}
void semSignal(semaphore *s) // Semaphore signal
{
    s->value = 1; // Release the semaphore (unlock)
}
// Compare and swap function (from class example)
int compare_and_swap(int *word, int testval, int newval)
{
    int oldval;
    oldval = *word;
    if (oldval == testval)
        *word = newval;
    return oldval;
}

// File output function
void outputStatsToFile()
{
    FILE *filePtr;     // File pointer
    char fileName[30]; // File name

    // Write main simulation stats to a file
    // Open a different file based on the selected deadlock solution
    for (int i = 1; i < 5; i++) // Iterate 4 times because there's 4 deadlock solutions
    {
        if (deadlockSolution == i)
        {
            snprintf(fileName, sizeof(fileName), "sol_%d_stats.csv", i);
        }
    }
    filePtr = fopen(fileName, "a"); // Open and append data to file
    // Write header and data
    fprintf(filePtr, "Deadlocks,Total Forced Releases,Max Waiting Time,Min Waiting Time,Avg Eating Time,Avg Thinking Time,Avg Waiting Time\n");
    fprintf(filePtr, "%u,%u,%u,%u,%u,%u,%u\n", numOfDeadlocks, totalForcedReleases, maxWaitingTime, minWaitingTime, avgEatingTime, avgThinkingTime, avgWaitingTime);
    // Close the file
    fclose(filePtr);

    // Write individual philosopher stats to a different file (for calculating fairness)
    // Open a different file based on the selected deadlock solution
    for (int i = 1; i < 5; i++) // Iterate 4 times because there's 4 deadlock solutions
    {
        if (deadlockSolution == i)
        {
            snprintf(fileName, sizeof(fileName), "sol_%d_indiv_phil_stats.csv", i);
        }
    }
    filePtr = fopen(fileName, "a"); // Open and append data to file
    /// Write header and data
    // Header
    fprintf(filePtr, "Eating Times,                              Thinking Times,                            Waiting Times,\n");
    for (int i = 0; i < 3; i++) // Each set of stats
    {
        for (int i = 0; i < NUM_PHIL; i++) // Each philosopher
        {
            fprintf(filePtr, "Phil %d,", i);
        }
        fprintf(filePtr, "        ");
    }
    fprintf(filePtr, "\n");
    // Data
    for (int i = 0; i < NUM_PHIL; i++) // Philosophers' Eating Times
    {
        fprintf(filePtr, "%u,", timeEating[i]);
    }
    fprintf(filePtr, "                       ");
    for (int i = 0; i < NUM_PHIL; i++) // Philosophers' Thinking Times
    {
        fprintf(filePtr, "%u,", timeThinking[i]);
    }
    fprintf(filePtr, "                       ");
    for (int i = 0; i < NUM_PHIL; i++) // Philosophers' Waiting Times
    {
        fprintf(filePtr, "%u,", timeWaiting[i]);
    }
    fprintf(filePtr, "\n");
    // Close the file
    fclose(filePtr);
}