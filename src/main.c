#include "../include/entities.h"

#include <omp.h>

#include <stdlib.h>

void produceBread(Bakery* bakeries, int bakeryCount);
void updateDrones(Drone* drones, int droneCount, int currentRound);
void initSystemMock(Bakery** bakeries, int* bCount, Drone** drones, int* dCount, Customer** customers, int* cCount);

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

    // Made-up bakery stats
    (*bakeries)[0].id = 1;
    (*bakeries)[0].inventory = 0;
    (*bakeries)[0].capacity = 100;
    (*bakeries)[0].seed = 42;
    (*bakeries)[0].ruleCount = 1;
    (*bakeries)[0].cumulativeProb = (ProductionRule*)malloc(sizeof(ProductionRule));
    (*bakeries)[0].cumulativeProb[0].probability = 1.0;
    (*bakeries)[0].cumulativeProb[0].breadCount = 10;

    // Made-up drones stats
    for(int i = 0; i < *dCount; i++) {
        (*drones)[i].id = i + 1;
        (*drones)[i].availableAtRound = 0;
        (*drones)[i].currentCustomer = NULL;
    }

    // Made-up customer-stats
    for(int i = 0; i < *cCount; i++) {
        (*customers)[i].id = i + 1;
        (*customers)[i].priority = 1;
        (*customers)[i].status = CUSTOMER_ACTIVE;
        (*customers)[i].demand = 5;
    }
}


int main() {

}