#include "../include/entities.h"
#include "../include/algorithms.h"

#include <omp.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <float.h>

void produceBread(Bakery* bakeries, int bakeryCount);
void updateDrones(Drone* drones, int droneCount, int currentRound, Bakery* bakeries, int bakeryCount);
void initSystemMock(Bakery** bakeries, int* bCount, Drone** drones, int* dCount, Customer*** customers, int* cCount);
void findClosestBakery(Bakery* bakeries, int bCount, Customer** customers, int cCount, double** distanceMatrix);
Bakery* closestBakeryToDrone(Bakery* bakeries, int bakeryCount, const Drone* drone, double* closestDistance);
void idleDroneRepositioning(Bakery* bakeries, int bakeryCount, Drone* drone);

// This function works in parallel, and is called at the start of every round. It produces the bread for the bakeries
void produceBread(Bakery* bakeries, const int bakeryCount) {

    // This function is embarrassingly parallelable. every bakery only works on its own
    #pragma omp parallel for default(none) shared(bakeries, bakeryCount)
    for (int i = 0; i < bakeryCount; i++) {

        /*
         * We will produce the bread like this:
         * Because each bakery has its own distribution of bread production per day, and we have the cumulative
         * probability for each different number of bread loaves, we will generate a random number between 0 and 1 and
         * the number of bread loaves the bakery will produce will be the first element in the cumulativeProb array
         * with probability at least the number we generated
         */

        const int randomInteger = rand_r(&bakeries[i].seed);

        // Dividing the integer we got by rand_max to get a number between 0 and 1
        const double random = (double) randomInteger / RAND_MAX;

        // Now we iterate over the cumulativeProb array of the bakery and find the desired ProductionRule
        int j;
        for (j = 0; j < bakeries[i].ruleCount; j++) {
            if (bakeries[i].cumulativeProb[j].probability >= random) {
                break;
            }
        }

        // If the condition is never met, we set j to be the last rule
        if (j == bakeries[i].ruleCount) j = bakeries[i].ruleCount - 1;

        // Now we know how much bread we need to produce
        const int breadProduction = bakeries[i].cumulativeProb[j].breadCount;

        const int newInventory = bakeries[i].inventory + breadProduction;

        // We cannot exceed the capacity
        bakeries[i].inventory = (newInventory < bakeries[i].capacity) ? newInventory : bakeries[i].capacity;
    }
}

// This function is called at the end of stage 1, and it updates the position and availability of the drones
void updateDrones(Drone* drones, const int droneCount, const int currentRound, Bakery* bakeries,
    const int bakeryCount) {

    // Iterating over the drones array and finding newly available drones
    #pragma omp parallel for default(none) shared(drones, droneCount, currentRound, bakeries, bakeryCount)
    for (int i = 0; i < droneCount; i++) {
        Drone* drone = &drones[i]; // Current drone

        /*
         * If the current drone is now available, we set the current customer he serves to NULL and we bring the drone
         * closer to a bakery!
         */
        if (drone->availableAtRound <= currentRound) {
            drone->availableAtRound = currentRound;
            drone->currentCustomer = NULL;
            drone->load = 0;
            idleDroneRepositioning(bakeries, bakeryCount, drone);
        }
    }
}

/*
 * This function receives a drone pointer and sets returns a pointer to the closest bakery to it, in order to get this
 * drone closer to it while having no customer to serve
 */
Bakery* closestBakeryToDrone(Bakery* bakeries, const int bakeryCount, const Drone* drone, double* closestDistance) {
    double closest = DBL_MAX, currentDistance = DBL_MAX;
    Bakery* closestBakery = NULL, *currentBakery = NULL;

    // This loop will be performed sequentially because we already paralleled the outer loop in update drones
    for (int i = 0; i < bakeryCount; i++) {
        currentBakery = &bakeries[i];

        currentDistance = calculateDistance(currentBakery->pos, drone->pos);
        if (currentDistance < closest) {
            closest = currentDistance;
            closestBakery = currentBakery;
        }
    }

    if (closestDistance) *closestDistance = closest;

    return closestBakery;
}

// This function receives an idle drone and shall get this drone one step closer to its closest bakery
void idleDroneRepositioning(Bakery* bakeries, const int bakeryCount, Drone* drone) {

    // First we find the closest bakery and the distance to it
    double closestDistance = 0.0;

    const Bakery* closestBakery = closestBakeryToDrone(bakeries, bakeryCount, drone, &closestDistance);
    if (closestBakery == NULL || closestDistance < 0) return;

    // How much should the drone move (percentage wise) in the direction of the bakery
    double ratio = drone->velocity / closestDistance;

    // We don't want the drone to pass the bakery
    ratio = ratio <=1 ? ratio : 1;

    // The difference in the x and y components
    const double dx = closestBakery->pos.x - drone->pos.x;
    const double dy = closestBakery->pos.y - drone->pos.y;

    // Updating the drone position
    drone->pos.x += dx * ratio;
    drone->pos.y += dy * ratio;
}

// This function is a helper function that for checks, used to mock a city (bakeries, drones, customers, etc.)
void initSystemMock(Bakery** bakeries, int* bCount, Drone** drones, int* dCount, Customer*** customers, int* cCount) {

    // 1 bakery, 2 drones, 2 customers
    *bCount = 1;
    *dCount = 2;
    *cCount = 2;

    // Allocate the memory
    *bakeries = (Bakery*) malloc(sizeof(Bakery) * (*bCount));
    *drones = (Drone*) malloc(sizeof(Drone) * (*dCount));

    // Allocate pointer array for customers
    *customers = (Customer**) malloc(sizeof(Customer*) * (*cCount));

    if (*bakeries == NULL || *drones == NULL || *customers == NULL) exit(1);

    // Made-up bakery stats
    (*bakeries)[0].id = 1;
    (*bakeries)[0].pos.x = 0.0;
    (*bakeries)[0].pos.y = 0.0;
    (*bakeries)[0].inventory = 0;
    (*bakeries)[0].capacity = 100;
    (*bakeries)[0].seed = 42;
    (*bakeries)[0].ruleCount = 1;

    (*bakeries)[0].cumulativeProb = (ProductionRule*)malloc(sizeof(ProductionRule));
    if ((*bakeries)[0].cumulativeProb == NULL) exit(1);

    (*bakeries)[0].cumulativeProb[0].probability = 1.0;
    (*bakeries)[0].cumulativeProb[0].breadCount = 10;

    // Made-up drones stats
    for(int i = 0; i < *dCount; i++) {
        (*drones)[i].id = i + 1;
        (*drones)[i].pos.x = 0.0;
        (*drones)[i].pos.y = 0.0;
        (*drones)[i].velocity = 2.0;
        (*drones)[i].capacity = 5;
        (*drones)[i].availableAtRound = 0;
        (*drones)[i].currentCustomer = NULL;
    }

    // Made-up customer-stats
    for(int i = 0; i < *cCount; i++) {
        (*customers)[i] = (Customer*) malloc(sizeof(Customer));
        if ((*customers)[i] == NULL) exit(1);

        (*customers)[i]->id = i + 1;
        (*customers)[i]->pos.x = (double)(i + 1) * 10.0;
        (*customers)[i]->pos.y = 0.0;
        (*customers)[i]->priority = 1;
        (*customers)[i]->status = CUSTOMER_ACTIVE;
        (*customers)[i]->demand = 5;
        (*customers)[i]->closestBakeryDistance = DBL_MAX;
        (*customers)[i]->tempScore = -1.0;
        (*customers)[i]->distanceMatrixRow = -1; // Will be set by calculateDistanceMatrix()
    }
}

/*
 * Stress-test scenario: simulates a city grid with:
 *   - 500 bakeries (different capacities & production distributions)
 *   - 1000 drones (varying speeds & capacities, deployed from 2 bases)
 *   - 2000 customers (scattered positions, varying demands)
 *
 * Positions are spread across a 200x200 coordinate space.
 */
void initSystemStress(Bakery** bakeries, int* bCount, Drone** drones, int* dCount, Customer*** customers, int* cCount) {
    *bCount = 500;
    *dCount = 50;
    *cCount = 10000;

    *bakeries = (Bakery*) malloc(sizeof(Bakery) * (*bCount));
    *drones   = (Drone*)  malloc(sizeof(Drone)  * (*dCount));
    *customers = (Customer**) malloc(sizeof(Customer*) * (*cCount));
    if (*bakeries == NULL || *drones == NULL || *customers == NULL) exit(1);

    typedef struct { double x, y; int capacity; int rules[3][2]; } BakeryDef;
    BakeryDef bDefs[] = {
        {  20.0,  30.0, 150, { {5, 30}, {12, 80}, {25, 100} } },
        { 180.0,  20.0,  80, { {3, 40}, { 8, 85}, {15, 100} } },
        { 100.0, 100.0, 200, { {8, 25}, {15, 70}, {30, 100} } },
        {  40.0, 180.0,  60, { {2, 50}, { 6, 90}, {10, 100} } },
        { 160.0, 170.0, 100, { {4, 35}, {10, 75}, {18, 100} } },
    };

    for (int i = 0; i < *bCount; i++) {
        int bIdx = i % 5;
        (*bakeries)[i].id = i + 1;
        (*bakeries)[i].pos.x = bDefs[bIdx].x;
        (*bakeries)[i].pos.y = bDefs[bIdx].y;
        (*bakeries)[i].inventory = 0;
        (*bakeries)[i].capacity = bDefs[bIdx].capacity;
        (*bakeries)[i].seed = 12345 + i; 
        (*bakeries)[i].ruleCount = 3;

        (*bakeries)[i].cumulativeProb = (ProductionRule*)malloc(3 * sizeof(ProductionRule));
        if ((*bakeries)[i].cumulativeProb == NULL) exit(1);

        for (int r = 0; r < 3; r++) {
            (*bakeries)[i].cumulativeProb[r].breadCount = bDefs[bIdx].rules[r][0];
            (*bakeries)[i].cumulativeProb[r].probability = bDefs[bIdx].rules[r][1] / 100.0;
        }
    }

    typedef struct { double x, y, velocity; int capacity; } DroneDef;
    DroneDef dDefs[] = {
        {  20.0,  50.0, 3.0, 4 }, {  20.0,  50.0, 8.0, 8 }, {  25.0,  55.0, 2.5, 5 },
        {  15.0,  45.0, 5.0, 3 }, {  20.0,  50.0, 11.0, 6 }, { 160.0, 160.0, 3.5, 3 },
        { 160.0, 160.0, 1.8, 7 }, { 155.0, 165.0, 2.2, 6 }, { 165.0, 155.0, 7.8, 4 },
        { 160.0, 160.0, 5.2, 10 }
    };

    for (int i = 0; i < *dCount; i++) {
        int dIdx = i % 10;
        (*drones)[i].id = i + 1;
        (*drones)[i].pos.x = dDefs[dIdx].x;
        (*drones)[i].pos.y = dDefs[dIdx].y;
        (*drones)[i].velocity = dDefs[dIdx].velocity;
        (*drones)[i].capacity = dDefs[dIdx].capacity;
        (*drones)[i].availableAtRound = 0;
        (*drones)[i].currentCustomer = NULL;
    }

    typedef struct { double x, y; int demand; } CustomerDef;
    CustomerDef cDefs[] = {
        {  10.0,  15.0,  3 }, {  35.0,  25.0,  5 }, {  90.0,  85.0,  4 }, { 110.0, 115.0,  7 },
        { 105.0,  90.0,  2 }, { 190.0,   5.0,  6 }, { 195.0,  15.0,  8 }, {  15.0, 190.0,  4 },
        {  30.0, 195.0, 10 }, {  50.0, 175.0,  3 }, {  70.0,  50.0,  5 }, { 130.0,  60.0,  6 },
        {  60.0, 130.0,  4 }, { 140.0, 130.0,  3 }, {   5.0, 100.0,  9 }, { 195.0, 100.0,  7 },
        { 100.0,   5.0,  2 }, { 100.0, 195.0,  5 }, { 170.0, 185.0,  8 }, { 150.0, 190.0,  6 }
    };

    // Use a fixed seed for reproducible randomness
    srand(9999);
    for (int i = 0; i < *cCount; i++) {
        int cIdx = i % 20;
        (*customers)[i] = (Customer*) malloc(sizeof(Customer));
        if ((*customers)[i] == NULL) exit(1);

        (*customers)[i]->id = i + 1;
        // Adding random noise so customers don't stack on the exact same coordinate
        (*customers)[i]->pos.x = cDefs[cIdx].x + ((rand() % 10) - 5);
        (*customers)[i]->pos.y = cDefs[cIdx].y + ((rand() % 10) - 5);
        (*customers)[i]->priority = 1;
        (*customers)[i]->status = CUSTOMER_ACTIVE;
        (*customers)[i]->demand = cDefs[cIdx].demand;
        (*customers)[i]->closestBakeryDistance = DBL_MAX;
        (*customers)[i]->tempScore = -1.0;
        (*customers)[i]->distanceMatrixRow = -1;
    }
}

// This function computes the closest bakery to every customer.
// Uses each customer's distanceMatrixRow field for correct matrix lookup after sorting.
void findClosestBakery(Bakery* bakeries, const int bCount, Customer** customers, const int cCount,
    double** distanceMatrix) {

    /*
     * We already have the distance matrix so we need to find the minimum distance for each customer.
     * We use distanceMatrixRow for the correct row lookup, so this works even after qsort.
     */
    #pragma omp parallel for default(none) shared(cCount, bCount, distanceMatrix, bakeries, customers)
    for (int i = 0; i < cCount; i++) {

        // Double-checking in case the field is not initialized
        customers[i]->closestBakeryDistance = DBL_MAX;

        // Use stable row index instead of current array position
        int matrixRow = customers[i]->distanceMatrixRow;

        for (int j = 0; j < bCount; j++) {
            if (distanceMatrix[matrixRow][j] < customers[i]->closestBakeryDistance) {
                customers[i]->closestBakeryDistance = distanceMatrix[matrixRow][j];
            }
        }
    }
}

// ── Print helpers ──

void printBakeryStatus(Bakery* bakeries, int bCount) {
    printf("  Bakeries: ");
    for (int i = 0; i < bCount; i++) {
        printf("B%d=%d/%d  ", bakeries[i].id, bakeries[i].inventory, bakeries[i].capacity);
    }
    printf("\n");
}

void printDroneStatus(Drone* drones, int dCount, int currentRound) {
    int idle = 0, busy = 0;
    for (int i = 0; i < dCount; i++) {
        if (drones[i].currentCustomer == NULL) idle++;
        else busy++;
    }
    printf("  Drones: %d idle, %d delivering\n", idle, busy);
}

void printCustomerSummary(Customer** customers, int cCount) {
    int active = 0, served = 0, departed = 0;
    int totalDemand = 0, totalPriority = 0;
    for (int i = 0; i < cCount; i++) {
        if (customers[i]->status == CUSTOMER_ACTIVE) {
            active++;
            totalDemand += customers[i]->demand;
            totalPriority += customers[i]->priority;
        }
        else if (customers[i]->status == CUSTOMER_SERVED) served++;
        else departed++;
    }
    printf("  Customers: %d active (demand=%d, avg_priority=%.1f), %d served, %d departed\n",
           active, totalDemand,
           active > 0 ? (double)totalPriority / active : 0.0,
           served, departed);
}

int main() {
    printf("==================================================\n");
    printf("   DRONE DELIVERY SIMULATION - STRESS BENCHMARK   \n");
    printf("==================================================\n");
    
    //threadCounts is the array of different thread counts we want to test, and numTests is the number of different tests we will run
    int threadCounts[] = {1, 2, 4, 6, 8, 16, 32, 64};
    int numTests = sizeof(threadCounts) / sizeof(threadCounts[0]);
    int maxRounds = 10; // Maximum number of rounds to simulate in each test, can be adjusted based on desired test duration and complexity
    printf("+---------+--------------+----------+\n");
    printf("| Threads | Time (sec)   | Speedup  |\n");
    printf("+---------+--------------+----------+\n");

    double baseTime = 0.0; // Will store the time of the sequential run (Thread 1)

    for (int i = 0; i < numTests; i++) {
        int threads = threadCounts[i];

        // Disable dynamic adjustment of threads
        omp_set_dynamic(0);

        // Force the specific number of threads
        omp_set_num_threads(threads);
    
        // ... the rest of your benchmark loop ...

        // reset the random seed for reproducibility in each test
        Bakery* bakeries;
        Drone* drones;
        Customer** customers;
        int bCount, dCount, cCount;

        initSystemStress(&bakeries, &bCount, &drones, &dCount, &customers, &cCount);

        double** distanceMatrix = calculateDistanceMatrix(bakeries, bCount, customers, cCount);
        if (distanceMatrix == NULL) {
            printf("Failed to allocate distance matrix!\n");
            exit(1);
        }

        findClosestBakery(bakeries, bCount, customers, cCount, distanceMatrix);

        // start the timer!
        double startTime = omp_get_wtime();

        int t = 1;
        while (t <= maxRounds) {
           // removed all the printfs from the main loop to avoid garbled output and focus on timing and speedup results 
            // printf("── Round %d ──\n", t);

            // Stage 1: Production & drone updates
            produceBread(bakeries, bCount);
            updateDrones(drones, dCount, t, bakeries, bCount);
            
            // printBakeryStatus(bakeries, bCount);
            // printDroneStatus(drones, dCount, t);

            // Stage 2: Scoring & sorting
            double avgVelocity = 0.0, avgCapacity = 0.0;
            calculateDroneAverages(drones, dCount, &avgVelocity, &avgCapacity);
            calculateCustomerScoresStage2(customers, cCount, avgVelocity, avgCapacity);

            //sorting the customers by their tempScore in descending order, using a parallel quicksort
            sortCustomersParallel(customers, cCount);

            // Stage 3: Drone assignment
            assignDronesStage3(customers, cCount, bakeries, bCount, drones, dCount, distanceMatrix, t);

            // Stage 4: Transitions
            updateCustomerPriorities(customers, cCount);
            processCustomerTransitions(customers, cCount, t);
            
            // printCustomerSummary(customers, cCount);

            // check if all customers are departed, if so, we can end the simulation early
            int allDeparted = 1;
            for (int j = 0; j < cCount; j++) {
                if (customers[j]->status != CUSTOMER_DEPARTED) {
                    allDeparted = 0;
                    break;
                }
            }
            if (allDeparted) {
                break;
            }

            t++;
        }

        // stop the clock!
        double elapsed = omp_get_wtime() - startTime;

        // calculate speedup compared to the single-threaded baseline
        if (i == 0) {
            baseTime = elapsed; // Set the baseline time for single-threaded execution
        }
        double speedup = baseTime / elapsed;

        // Print results for the current thread count
        printf("| %-7d | %-12.4f | %-8.2fx |\n", threads, elapsed, speedup);

        // Clean up memory for the current test before the next iteration
        freeDistanceMatrix(distanceMatrix, cCount);
        for (int b = 0; b < bCount; b++) free(bakeries[b].cumulativeProb);
        free(bakeries);
        free(drones);
        for (int c = 0; c < cCount; c++) free(customers[c]);
        free(customers);
    }

    printf("+---------+--------------+----------+\n");
    printf("Benchmark Completed Successfully.\n");

    return 0;
}