// The goal of this file is to initialize the struct and allocate / free the required memory
#include "../include/entities.h"

#include <stdlib.h>
#include <time.h>
#include <float.h>

// This function initializes a bakery and handles the allocated memory
void initBakery(const int id, const Position pos, const ProductionRule* distribution, const int ruleCount,
    const int capacity, Bakery* bakery) {
    bakery->id = id;
    bakery->pos = pos;
    bakery->ruleCount = ruleCount;
    bakery->capacity = capacity;

    // The inventory is initialized to 0
    bakery->inventory = 0;

    // We receive the distribution array which was created in the stack and allocate the memory for it
    bakery->cumulativeProb = malloc(sizeof(ProductionRule) * ruleCount);
    if (bakery->cumulativeProb == NULL) exit(1);

    // Insert the data into the array
    bakery->cumulativeProb[0] = distribution[0];
    for (int i = 1; i < ruleCount; i++) {
        bakery->cumulativeProb[i].breadCount = distribution[i].breadCount;
        bakery->cumulativeProb[i].probability = bakery->cumulativeProb[i - 1].probability
        + distribution[i].probability;
    }

    // In order to ensure each bakery starts with a different seed, we use a unique formula to calculate the seed
    bakery->seed = (unsigned int)time(NULL) ^ id;
}

// This function initializes a customer and handles the allocated memory
void initCustomer(const int id, const Position pos, const int demand, Customer* customer) {
    customer->id = id;
    customer->pos = pos;
    customer->demand = demand;
    customer->priority = 1; // When a new order is placed, the priority of the customer is 1
    customer->status = CUSTOMER_ACTIVE; // When a new order is placed, the customer is currently active
    customer->closestBakeryDistance = DBL_MAX;
}

// This function initializes a drone and handles the allocated memory
void initDrone(const int id, const int capacity, const int currentRound, const Position pos, const double velocity, Drone* drone) {
    drone->id = id;
    drone->capacity = capacity;
    drone->load = 0; // Drones spawn without any bread loaves
    drone->availableAtRound = currentRound; // New drones are available right away
    drone->pos = pos; // In main, we will call this with the position of the drone base
    drone->velocity = velocity;
    drone->currentCustomer = NULL; // New drones are yet to be assigned a customer
}

// This function initializes a drone base and handles the allocated memory
void initDroneBase(const Position pos, DroneBase* droneBase) {
    droneBase->pos = pos;

    // When a new drone base is created it doesn't have any drones yet
    droneBase->drones = NULL;
    droneBase->droneCount = 0;
}

// This function frees the allocated memory from initBakery
void freeBakery(const Bakery* bakery) {
    free(bakery->cumulativeProb);
}

// This function frees the allocated memory of the drones in the DroneBase
void freeDroneBase(const DroneBase* droneBase) {
    free(droneBase->drones);
}