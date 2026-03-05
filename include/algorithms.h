#ifndef ALGORITHMS_H
#define ALGORITHMS_H

#include "entities.h" 

// This header file contains the function prototypes for the algorithms used in the delivery system.

// Calculate Euclidean distance (Pythagorean theorem)
double calculateDistance(Position p1, Position p2);

// Calculate flight time according to the formula t = d / v
double calculateFlightTime(double distance, double velocity);

// Update the priorities of customers based on whether they have been served or not
void updateCustomerPriorities(Customer* customers, int customerCount);

// Calculates the priority-aware optimization score for a customer-drone assignment
double calculateCustomerScore(int priority, double estimatedTime);

#endif