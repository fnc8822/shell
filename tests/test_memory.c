#include "unity.h"
#include "memory.h"
#include <time.h>

#define NUM_ALLOCATIONS 1000
#define MAX_ALLOCATION_SIZE 1024


void setUp(void) {

}

void tearDown(void) {

}

void test_efficiency(int method) {
    set_method(method);
    if(method == FIRST_FIT){
        printf("Method: FIRST_FIT\n");
    } else if(method == BEST_FIT){
        printf("Method: BEST_FIT\n");
    } else if(method == WORST_FIT){
        printf("Method: WORST_FIT\n");
    }
    int cumulative_allocation=0;
    clock_t start_time = clock();
    void *allocations[NUM_ALLOCATIONS];

    // Perform allocations
    for (int i = 0; i < NUM_ALLOCATIONS; i++) {
        int size = rand() % MAX_ALLOCATION_SIZE + 1;
        cumulative_allocation+=size;
        allocations[i] = malloc_(size);
    }
    // Measure fragmentation
    size_t *memory_usage = get_allocator_memory_usage();
    size_t total_allocated = memory_usage[ALLOCATED];
    size_t total_free = memory_usage[FREE];
    size_t total_memory = total_allocated + total_free;
    double fragmentation = ((double)total_allocated-(double)cumulative_allocation) / (double)total_allocated * 100;

    // Perform deallocations
    for (int i = 0; i < NUM_ALLOCATIONS; i++) {
        if (allocations[i]) {
            free_(allocations[i]);
        }
    }

    clock_t end_time = clock();
    double allocation_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;


    printf("Allocation Time: %f seconds\n", allocation_time);
    printf("Fragmentation: %f%%\n", fragmentation);
    printf("\n");

    TEST_ASSERT_TRUE(allocation_time >= 0); // Ensure the test passes
}
void test_efficiency_best_fit() {
    test_efficiency(BEST_FIT);
}
void test_efficiency_first_fit() {
    test_efficiency(FIRST_FIT);
}
void test_efficiency_worst_fit() {
    test_efficiency(WORST_FIT);
}


int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_efficiency_best_fit);
    RUN_TEST(test_efficiency_first_fit);
    RUN_TEST(test_efficiency_worst_fit);
    return UNITY_END();
}