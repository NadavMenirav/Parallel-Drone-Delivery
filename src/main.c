#include "../include/entities.h"
#include "../include/algorithms.h"

#include <omp.h>

#include <stdlib.h>
#include <stdio.h>
#include <float.h>

void produceBread(Bakery* bakeries, int bakeryCount);
void updateDrones(Drone* drones, int droneCount, int currentRound, Bakery* bakeries, int bakeryCount);
void initSystemMock(Bakery** bakeries, int* bCount, Drone** drones, int* dCount, Customer** customers, int* cCount);
void findClosestBakery(Bakery* bakeries, int bCount, Customer* customers, int cCount, double** distanceMatrix);
Bakery* closestBakeryToDrone(Bakery* bakeries, int bakeryCount, Drone* drone);
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
Bakery* closestBakeryToDrone(Bakery* bakeries, const int bakeryCount, Drone* drone) {
    double closestDistance = DBL_MAX, currentDistance = DBL_MAX;
    Bakery* closestBakery = NULL, *currentBakery = NULL;

    // This loop will be performed sequentially because we already paralleled the outer loop in update drones
    for (int i = 0; i < bakeryCount; i++) {
        currentBakery = &bakeries[i];

        currentDistance = calculateDistance(currentBakery->pos, drone->pos);
        if (currentDistance < closestDistance) {
            closestDistance = currentDistance;
            closestBakery = currentBakery;
        }
    }

    return closestBakery;
}

// This function receives an idle drone and shall get this drone one step closer to its closest bakery
void idleDroneRepositioning(Bakery* bakeries, const int bakeryCount, Drone* drone) {

    // First we find the closest bakery and the distance to it
    const Bakery* closestBakery = closestBakeryToDrone(bakeries, bakeryCount, drone);
    if (closestBakery == NULL) return;

    const double closestDistance = calculateDistance(closestBakery->pos, drone->pos);

    if (closestDistance <= 0) return;

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
    }
}

// This function computes the closest bakery to every customer
void findClosestBakery(Bakery* bakeries, const int bCount, Customer* customers, const int cCount,
    double** distanceMatrix) {

    /*
     * We already have the distance matrix so we need to find the minimum distance for each customer.
     * In order to do that, we will iterate over the customers in parallel, each thread gets a chunk of customers.
     * For each customer, the thread will iterate over the corresponding row in the distancesMatrix and will find the
     * minimal distance
     */
    #pragma omp parallel for default(none) shared(cCount, bCount, distanceMatrix, bakeries, customers)
    for (int i = 0; i < cCount; i++) {

        // Double-checking in case the field is not initialized
        customers[i].closestBakeryDistance = DBL_MAX;

        for (int j = 0; j < bCount; j++) {
            if (distanceMatrix[i][j] < customers[i].closestBakeryDistance) {
                customers[i].closestBakeryDistance = distanceMatrix[i][j];
            }
        }
    }
}

int main() {

    // Creating the mocked system
    Bakery* bakeries;
    Drone* drones;
    Customer* customers;

    int bCount;
    int dCount;
    int cCount;

    initSystemMock(&bakeries, &bCount, &drones, &dCount, &customers, &cCount);

    // Calculating the distances matrix before the simulation loop
    double** distanceMatrix = calculateDistanceMatrix(bakeries, bCount, customers, cCount);
    if (distanceMatrix == NULL) {
        printf("Failed to allocate distance matrix!\n");
        exit(1);
    }

    // Initializing the closestBakeryDistance field for every customer
    findClosestBakery(bakeries, bCount, customers, cCount, distanceMatrix);

    int t = 1; // the current round

    // This while function simulates the entire delivery system. Each iteration is a different round.
    while (t < 10) {

        // Prints to decorate the round
        printf("\n=== Round %d ===\n", t);

        /*
         * Stage 1 - State update
         * The bakeries increase their stock by their production rate
         */
        produceBread(bakeries, bCount);
        printf("Bakery %d inventory after production: %d\n", bakeries[0].id, bakeries[0].inventory);

        // Now, drones update their position and availability after previous deliveries
        updateDrones(drones, dCount, t, bakeries, bCount);
        printf("Drone %d available at round: %d\n", drones[0].id, drones[0].availableAtRound);
        printf("Drone %d available at round: %d\n", drones[1].id, drones[1].availableAtRound);

        // ToDo: stages 2, 3

        /*
         * Stage 4 - State transition
         * Customers update their status.
         * Customers which just got served can either leave the system or place another order
         * Unserved customers increase their priority
         */

        // Increase priority for unserved customers
        updateCustomerPriorities(customers, cCount);

        // Newly served customers will either leave or order more
        processCustomerTransitions(customers, cCount);

        printf("Customer %d Priority after Stage 4: %d\n", customers[0].id, customers[0].priority);
        printf("Customer %d Priority after Stage 4: %d\n", customers[1].id, customers[1].priority);

        t++;
    }

    // Freeing the allocated memory
    freeDistanceMatrix(distanceMatrix, cCount);

    free(bakeries[0].cumulativeProb);
    free(bakeries);
    free(drones);
    free(customers);
}