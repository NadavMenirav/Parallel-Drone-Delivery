// This header file contains the structs representing the different entities of the delivery system.

#ifndef FINAL_PROJECT_ENTITIES_H
#define FINAL_PROJECT_ENTITIES_H

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

#endif //FINAL_PROJECT_ENTITIES_H