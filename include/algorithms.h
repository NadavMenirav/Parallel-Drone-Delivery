#ifndef ALGORITHMS_H
#define ALGORITHMS_H

#include "entities.h" 

// This header file contains the function prototypes for the algorithms used in the delivery system.

// Calculate Euclidean distance (Pythagorean theorem)
double calculateDistance(Position p1, Position p2);

// Calculate flight time according to the formula t = d / v
double calculateFlightTime(double distance, double velocity);

// Update the priorities of customers based on whether they have been served or not
void updateCustomerPriorities(Customer** customers, int customerCount);

// Calculates the priority-aware optimization score for a customer-drone assignment
double calculateCustomerScore(int priority, double estimatedTime);

// Creates and computes a 2D distance matrix [Customer][Bakery] in parallel
double** calculateDistanceMatrix(Bakery* bakeries, int bakeryCount, Customer** customers, int customerCount);

// Builds a mapping from customer ID to their original row index in the distance matrix
int* buildIdToRowMap(Customer* customers, int customerCount, int maxId);

// Frees the memory allocated for the distance matrix
void freeDistanceMatrix(double** matrix, int customerCount);

// Processes customers who have been served, deciding if they leave or order again
void processCustomerTransitions(Customer** customers, int customerCount,int currentRound);

// Comparison function for qsort to sort customers in descending order based on their tempScore
int compareCustomersDesc(const void* a, const void* b);

// Calculates the average velocity and capacity of the fleet of drones
void calculateDroneAverages(Drone* drones, int droneCount, double* avgVelocity, double* avgCapacity);

// Calculates the heuristic score for each active customer and stores it in their tempScore field.
void calculateCustomerScoresStage2(Customer** customers, int cCount, double avgVelocity, double avgCapacity);

// Assigns drones to customers, planning the route and updating inventory
void assignDronesStage3(Customer** customers, int cCount, Bakery* bakeries, int bCount, Drone* drones, int dCount, double** distanceMatrix, int currentRound);

//sorts customers in parallel based on their tempScore field, in descending order
void sortCustomersParallel(Customer** customers, int count);

#endif