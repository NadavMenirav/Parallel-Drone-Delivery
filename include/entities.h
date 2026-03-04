// This header file contains the structs representing the different entities of the delivery system.

#ifndef FINAL_PROJECT_ENTITIES_H
#define FINAL_PROJECT_ENTITIES_H

// This enum represents the status of the customers
typedef enum {
    CUSTOMER_ACTIVE,
    CUSTOMER_SERVED,
    CUSTOMER_DEPARTED
} CustomerStatus;

// This struct represents a point in R^2
typedef struct {
    double x;
    double y;
} Position;

/*
 * Each bakery will have an array of Production Rules which will form the distribution it has to produce different
 * amount of bread each day
 */
typedef struct {
    int breadCount;
    double probability;
} ProductionRule;

/*
 * This struct represent the bakeries in the system. Each bakery has an id, coordinates, current inventory, the
 * distribution function of number of breads per day, and maximal number of bread loaves it can hold.
 */
typedef struct {
    int id;
    Position pos;
    int inventory; // Current bread count
    ProductionRule* distribution; // The different amount of bread the bakery can make each round and the probability
    int ruleCount; // Size of distribution array
    int capacity; // Maximum number of bread loaves.
} Bakery;

// This struct represents the customer
typedef struct {
    int id;
    Position pos;
    int demand; // remaining bread demand
    int priority; // Goes up by 1 in each round where the customer isn't served
    CustomerStatus status;
} Customer;

// This struct represents a single drone
typedef struct {
    int id;
    int capacity; // How many bread loaves the drone can hold
    int load; // Current bread load
    int availableAtRound; // The round in which the drone will finish its task and will be free
    Position pos;
    double velocity; // The speed in which the drone flies
    Customer* currentCustomer; // The current customer it serves
} Drone;

// This struct represents the DroneBase, where all the drones spawn
typedef struct {
    Position pos;
    Drone* drones; // Has an array of all the drones
    int droneCount;
} DroneBase;

void initBakery(int id, Position pos, const ProductionRule* distribution, int ruleCount, int capacity, Bakery* bakery);
void initCustomer(int id, Position pos, int demand, Customer* customer);
void initDrone(int id, int capacity, int currentRound, Position pos, double velocity, Drone* drone);
void initDroneBase(const Position pos, DroneBase* droneBase);

#endif //FINAL_PROJECT_ENTITIES_H