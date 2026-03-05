#include "../include/entities.h"

#include <omp.h>

#include <stdlib.h>

void produceBread(Bakery* bakeries, int bakeryCount);

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

        // Now we know how much bread we need to produce
        const int breadProduction = bakeries[i].cumulativeProb[j].breadCount;

        const int newInventory = bakeries[i].inventory + breadProduction;
        bakeries[i].inventory = (newInventory < bakeries[i].capacity) ? newInventory : bakeries[i].capacity;
    }
}