#include "../include/entities.h"
#include "../include/algorithms.h"

#include <omp.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <float.h>

void produceBread(Bakery* bakeries, int bakeryCount);
void updateDrones(Drone* drones, int droneCount, int currentRound);
void initSystemMock(Bakery** bakeries, int* bCount, Drone** drones, int* dCount, Customer** customers, int* cCount);
void findClosestBakery(Bakery* bakeries, int bCount, Customer* customers, int cCount, double** distanceMatrix);

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
void updateDrones(Drone* drones, const int droneCount, const int currentRound) {

    // Iterating over the drones array and finding newly available drones
    #pragma omp parallel for default(none) shared(drones, droneCount, currentRound)
    for (int i = 0; i < droneCount; i++) {
        Drone* drone = &drones[i]; // Current drone

        // If the current drone is now available, we set the current customer he serves to NULL
        if (drone->availableAtRound <= currentRound) {
            drone->availableAtRound = currentRound;
            drone->currentCustomer = NULL;
        }
    }
}

// This function is a helper function that for checks, used to mock a city (bakeries, drones, customers, etc.)
void initSystemMock(Bakery** bakeries, int* bCount, Drone** drones, int* dCount, Customer** customers, int* cCount) {

    // 1 bakery, 2 drones, 2 customers
    *bCount = 1;
    *dCount = 2;
    *cCount = 2;

    // Allocate the memory
    *bakeries = (Bakery*) malloc(sizeof(Bakery) * (*bCount));
    *drones = (Drone*) malloc(sizeof(Drone) * (*dCount));
    *customers = (Customer*) malloc(sizeof(Customer) * (*cCount));

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
        (*customers)[i].id = i + 1;
        (*customers)[i].pos.x = (double)(i + 1) * 10.0;
        (*customers)[i].pos.y = 0.0;
        (*customers)[i].priority = 1;
        (*customers)[i].status = CUSTOMER_ACTIVE;
        (*customers)[i].demand = 5;
        (*customers)[i].closestBakeryDistance = DBL_MAX;
        (*customers)[i].tempScore = -1.0;
        (*customers)[i].distanceMatrixRow = -1; // Will be set by calculateDistanceMatrix()
    }
}

/*
 * Stress-test scenario: simulates a city grid with:
 *   - 5 bakeries (different capacities & production distributions)
 *   - 10 drones (varying speeds & capacities, deployed from 2 bases)
 *   - 20 customers (scattered positions, varying demands)
 *
 * Positions are spread across a 200x200 coordinate space.
 */
void initSystemStress(Bakery** bakeries, int* bCount, Drone** drones, int* dCount, Customer** customers, int* cCount) {

    *bCount = 5;
    *dCount = 10;
    *cCount = 20;

    *bakeries = (Bakery*) malloc(sizeof(Bakery) * (*bCount));
    *drones   = (Drone*)  malloc(sizeof(Drone)  * (*dCount));
    *customers = (Customer*) malloc(sizeof(Customer) * (*cCount));
    if (*bakeries == NULL || *drones == NULL || *customers == NULL) exit(1);

    // ── Bakeries ──
    // Each bakery has 3 production rules to create a non-trivial probability distribution
    // Rule format: {breadCount, cumulativeProbability}
    //   e.g. {5, 0.3}, {10, 0.8}, {20, 1.0} means:
    //     30% chance of 5, 50% chance of 10, 20% chance of 20

    typedef struct { double x, y; int capacity; int rules[3][2]; } BakeryDef;
    BakeryDef bDefs[] = {
        {  20.0,  30.0, 150, { {5, 30}, {12, 80}, {25, 100} } },   // B1: downtown, high cap
        { 180.0,  20.0,  80, { {3, 40}, { 8, 85}, {15, 100} } },   // B2: east side, medium
        { 100.0, 100.0, 200, { {8, 25}, {15, 70}, {30, 100} } },   // B3: central, highest cap
        {  40.0, 180.0,  60, { {2, 50}, { 6, 90}, {10, 100} } },   // B4: south-west, small
        { 160.0, 170.0, 100, { {4, 35}, {10, 75}, {18, 100} } },   // B5: south-east, medium
    };

    for (int i = 0; i < *bCount; i++) {
        (*bakeries)[i].id = i + 1;
        (*bakeries)[i].pos.x = bDefs[i].x;
        (*bakeries)[i].pos.y = bDefs[i].y;
        (*bakeries)[i].inventory = 0;
        (*bakeries)[i].capacity = bDefs[i].capacity;
        (*bakeries)[i].seed = (unsigned int)time(NULL) ^ (i * 31 + 7);
        (*bakeries)[i].ruleCount = 3;

        (*bakeries)[i].cumulativeProb = (ProductionRule*)malloc(3 * sizeof(ProductionRule));
        if ((*bakeries)[i].cumulativeProb == NULL) exit(1);

        for (int r = 0; r < 3; r++) {
            (*bakeries)[i].cumulativeProb[r].breadCount = bDefs[i].rules[r][0];
            (*bakeries)[i].cumulativeProb[r].probability = bDefs[i].rules[r][1] / 100.0;
        }
    }

    // ── Drones ──
    // Two drone bases: Base A at (20, 50) and Base B at (160, 160)
    // Drones have varying speed/capacity tradeoffs (fast+small vs slow+large)

    typedef struct { double x, y, velocity; int capacity; } DroneDef;
    DroneDef dDefs[] = {
        // Base A drones (north-west)
        {  20.0,  50.0, 3.0, 4 },   // D1:  fast, small
        {  20.0,  50.0, 8.0, 8 },   // D2:  slow, large
        {  25.0,  55.0, 2.5, 5 },   // D3:  balanced
        {  15.0,  45.0, 5.0, 3 },   // D4:  very fast, tiny
        {  20.0,  50.0, 11.0, 6 },   // D5:  medium
        // Base B drones (south-east)
        { 160.0, 160.0, 3.5, 3 },   // D6:  fast, small
        { 160.0, 160.0, 1.8, 7 },   // D7:  slow, large
        { 155.0, 165.0, 2.2, 6 },   // D8:  balanced
        { 165.0, 155.0, 7.8, 4 },   // D9:  medium-fast
        { 160.0, 160.0, 5.2, 10 },  // D10: slowest, biggest
    };

    for (int i = 0; i < *dCount; i++) {
        (*drones)[i].id = i + 1;
        (*drones)[i].pos.x = dDefs[i].x;
        (*drones)[i].pos.y = dDefs[i].y;
        (*drones)[i].velocity = dDefs[i].velocity;
        (*drones)[i].capacity = dDefs[i].capacity;
        (*drones)[i].availableAtRound = 0;
        (*drones)[i].currentCustomer = NULL;
    }

    // ── Customers ──
    // 20 customers scattered across the city with varying demands (1–10 loaves)
    // Some are clustered near bakeries, others are far away to stress the routing

    typedef struct { double x, y; int demand; } CustomerDef;
    CustomerDef cDefs[] = {
        // Near bakery B1 (20,30) — easy to serve
        {  10.0,  15.0,  3 },   // C1
        {  35.0,  25.0,  5 },   // C2
        // Near bakery B3 (100,100) — central cluster
        {  90.0,  85.0,  4 },   // C3
        { 110.0, 115.0,  7 },   // C4
        { 105.0,  90.0,  2 },   // C5
        // Far north-east corner — long delivery distances
        { 190.0,   5.0,  6 },   // C6
        { 195.0,  15.0,  8 },   // C7
        // South-west corner — only B4 is close
        {  15.0, 190.0,  4 },   // C8
        {  30.0, 195.0, 10 },   // C9: high demand, remote
        {  50.0, 175.0,  3 },   // C10
        // Scattered middle
        {  70.0,  50.0,  5 },   // C11
        { 130.0,  60.0,  6 },   // C12
        {  60.0, 130.0,  4 },   // C13
        { 140.0, 130.0,  3 },   // C14
        // Far edges — worst case for scheduling
        {   5.0,  100.0, 9 },   // C15: high demand, edge
        { 195.0, 100.0,  7 },   // C16: high demand, edge
        { 100.0,   5.0,  2 },   // C17: far north
        { 100.0, 195.0,  5 },   // C18: far south
        // Dense demand cluster south-east
        { 170.0, 185.0,  8 },   // C19: high demand near B5
        { 150.0, 190.0,  6 },   // C20
    };

    for (int i = 0; i < *cCount; i++) {
        (*customers)[i].id = i + 1;
        (*customers)[i].pos.x = cDefs[i].x;
        (*customers)[i].pos.y = cDefs[i].y;
        (*customers)[i].priority = 1;
        (*customers)[i].status = CUSTOMER_ACTIVE;
        (*customers)[i].demand = cDefs[i].demand;
        (*customers)[i].closestBakeryDistance = DBL_MAX;
        (*customers)[i].tempScore = -1.0;
        (*customers)[i].distanceMatrixRow = -1;
    }
}

// This function computes the closest bakery to every customer.
// Uses each customer's distanceMatrixRow field for correct matrix lookup after sorting.
void findClosestBakery(Bakery* bakeries, const int bCount, Customer* customers, const int cCount,
    double** distanceMatrix) {

    /*
     * We already have the distance matrix so we need to find the minimum distance for each customer.
     * We use distanceMatrixRow for the correct row lookup, so this works even after qsort.
     */
    #pragma omp parallel for default(none) shared(cCount, bCount, distanceMatrix, bakeries, customers)
    for (int i = 0; i < cCount; i++) {

        // Double-checking in case the field is not initialized
        customers[i].closestBakeryDistance = DBL_MAX;

        // Use stable row index instead of current array position
        int matrixRow = customers[i].distanceMatrixRow;

        for (int j = 0; j < bCount; j++) {
            if (distanceMatrix[matrixRow][j] < customers[i].closestBakeryDistance) {
                customers[i].closestBakeryDistance = distanceMatrix[matrixRow][j];
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

void printCustomerSummary(Customer* customers, int cCount) {
    int active = 0, served = 0, departed = 0;
    int totalDemand = 0, totalPriority = 0;
    for (int i = 0; i < cCount; i++) {
        if (customers[i].status == CUSTOMER_ACTIVE) {
            active++;
            totalDemand += customers[i].demand;
            totalPriority += customers[i].priority;
        }
        else if (customers[i].status == CUSTOMER_SERVED) served++;
        else departed++;
    }
    printf("  Customers: %d active (demand=%d, avg_priority=%.1f), %d served, %d departed\n",
           active, totalDemand,
           active > 0 ? (double)totalPriority / active : 0.0,
           served, departed);
}

int main() {

    Bakery* bakeries;
    Drone* drones;
    Customer* customers;
    int bCount, dCount, cCount;

    initSystemStress(&bakeries, &bCount, &drones, &dCount, &customers, &cCount);

    printf("=== Drone Bakery Delivery — Stress Test ===\n");
    printf("Config: %d bakeries, %d drones, %d customers, 50 rounds\n\n", bCount, dCount, cCount);

    // Build distance matrix (also sets distanceMatrixRow for each customer)
    double** distanceMatrix = calculateDistanceMatrix(bakeries, bCount, customers, cCount);
    if (distanceMatrix == NULL) {
        printf("Failed to allocate distance matrix!\n");
        exit(1);
    }

    findClosestBakery(bakeries, bCount, customers, cCount, distanceMatrix);

    // Track stats
    int totalBreadDelivered = 0;
    int totalAssignments = 0;
    double startTime = omp_get_wtime();

    int t = 1;
    int maxRounds = 50;

    while (t <= maxRounds) {

        printf("── Round %d ──\n", t);

        // Stage 1: Production & drone updates
        produceBread(bakeries, bCount);
        updateDrones(drones, dCount, t);
        printBakeryStatus(bakeries, bCount);
        printDroneStatus(drones, dCount, t);

        // Stage 2: Scoring & sorting
        double avgVelocity = 0.0, avgCapacity = 0.0;
        calculateDroneAverages(drones, dCount, &avgVelocity, &avgCapacity);
        calculateCustomerScoresStage2(customers, cCount, avgVelocity, avgCapacity);
        qsort(customers, cCount, sizeof(Customer), compareCustomersDesc);

        // Print top 3 customers by score
        printf("  Top scores: ");
        int shown = 0;
        for (int i = 0; i < cCount && shown < 3; i++) {
            if (customers[i].tempScore > 0) {
                printf("C%d=%.2f  ", customers[i].id, customers[i].tempScore);
                shown++;
            }
        }
        printf("\n");

        // Stage 3: Drone assignment
        // Count bread before and after to track deliveries
        int breadBefore = 0;
        for (int i = 0; i < bCount; i++) breadBefore += bakeries[i].inventory;

        assignDronesStage3(customers, cCount, bakeries, bCount, drones, dCount, distanceMatrix, t);

        int breadAfter = 0;
        for (int i = 0; i < bCount; i++) breadAfter += bakeries[i].inventory;
        int breadDispatched = breadBefore - breadAfter;
        totalBreadDelivered += breadDispatched;

        // Count assignments this round
        int assignsThisRound = 0;
        for (int i = 0; i < dCount; i++) {
            if (drones[i].availableAtRound > t) assignsThisRound++;
        }
        totalAssignments += assignsThisRound;

        // Stage 4: Transitions
        updateCustomerPriorities(customers, cCount);
        processCustomerTransitions(customers, cCount, t);
        printCustomerSummary(customers, cCount);

        // Check if all customers have departed
        int allDeparted = 1;
        for (int i = 0; i < cCount; i++) {
            if (customers[i].status != CUSTOMER_DEPARTED) {
                allDeparted = 0;
                break;
            }
        }
        if (allDeparted) {
            printf("\n*** All customers have departed at round %d ***\n", t);
            break;
        }

        printf("\n");
        t++;
    }

    double elapsed = omp_get_wtime() - startTime;

    // ── Final Summary ──
    printf("\n========================================\n");
    printf("         SIMULATION SUMMARY\n");
    printf("========================================\n");
    printf("  Rounds completed:    %d\n", t > maxRounds ? maxRounds : t);
    printf("  Total bread sent:    %d loaves\n", totalBreadDelivered);
    printf("  Total assignments:   %d\n", totalAssignments);
    printf("  Wall-clock time:     %.4f seconds\n", elapsed);
    printf("  Threads used:        %d\n", omp_get_max_threads());
    printf("  Final customer status:\n");

    int finalActive = 0, finalDeparted = 0;
    for (int i = 0; i < cCount; i++) {
        if (customers[i].status == CUSTOMER_ACTIVE) finalActive++;
        else if (customers[i].status == CUSTOMER_DEPARTED) finalDeparted++;
    }
    printf("    Active:   %d\n", finalActive);
    printf("    Departed: %d\n", finalDeparted);

    printf("  Final bakery inventory:\n");
    for (int i = 0; i < bCount; i++) {
        printf("    B%d: %d/%d\n", bakeries[i].id, bakeries[i].inventory, bakeries[i].capacity);
    }
    printf("========================================\n");

    // Cleanup
    freeDistanceMatrix(distanceMatrix, cCount);
    for (int i = 0; i < bCount; i++) free(bakeries[i].cumulativeProb);
    free(bakeries);
    free(drones);
    free(customers);
}
