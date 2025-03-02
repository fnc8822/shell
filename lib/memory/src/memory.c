#include "memory.h"
#include <sys/mman.h>

typedef struct s_block *t_block;
void *base = NULL;
int method = FIRST_FIT;
int first_fit_counter = 0, best_fit_counter = 0, worst_fit_counter = 0;
size_t memory_metrics[] = {0, 0};
const char *log_file_name = "memlog.txt";

t_block find_block_(t_block *last, size_t size){
    t_block b = base;

    if (method == FIRST_FIT){
        while (b && !(b->free && b->size >= size)){
            *last = b;
            b = b->next;
        }
        return (b);
    } else if (method == BEST_FIT){
        size_t dif = PAGESIZE;
        t_block best = NULL;
        
        while (b){
            if (b->free){
                if (b->size == size){
                    return b;
                }
                if (b->size > size && (b->size - size) < dif){
                    dif = b->size - size;
                    best = b;
                }
            }
            *last = b;
            b = b->next;
        }
        return best;
    } else if (method == WORST_FIT) {
        size_t max_size = 0;
        t_block worst = NULL;
        while(b){
            if(b->free && b->size >= size && b->size > max_size){
                max_size = b->size;
                worst = b;
            }
            *last = b;
            b = b->next;
        }
        return worst;
    }
}

void split_block(t_block b, size_t s){
    if (b->size <= s + BLOCK_SIZE){
        return;
    }
    
    t_block new;
    new = (t_block)(b->data + s);
    new->size = b->size - s - BLOCK_SIZE;
    new->next = b->next;
    new->free = 1;
    b->size = s;
    b->next = new;
    
    if(new->next)
        new->next->prev = new;
    
}

void copy_block(t_block src, t_block dst){
    int *sdata, *ddata;
    size_t i;
    sdata = src->ptr;
    ddata = dst->ptr;
    
    for (i = 0; (i * 4) < src->size && (i * 4) < dst->size; i++)
        ddata[i] = sdata[i];
}

t_block get_block_(void *p){
    char *tmp;
    tmp = p;
    
    if (tmp >= (char *)base + BLOCK_SIZE){
        tmp -= BLOCK_SIZE;
    }
    return (t_block)(tmp);
    }

int valid_addr_(void *p){
    if (base){
        if(p > base && p < sbrk(0)){
            t_block b = get_block_(p);
            return b && (p == b->ptr);
        }
    }
    return (0);
}

t_block fusion(t_block b){
    while(b->next && b->next->free){
        b->size += BLOCK_SIZE + b->next->size;
        b->next = b->next->next;
        if (b->next)
            b->next->prev = b;
    }
    return b;
}

t_block extend_heap_(t_block last, size_t s){
    t_block b;
    b = mmap(0, s, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    
    if (b == MAP_FAILED){
        return NULL;
    }
    b->size = s;
    b->next = NULL;
    b->ptr = b->data;
    
    if (last){
        last->next = b;
        b->prev = last;
    }
    
    if(sbrk(b->size+BLOCK_SIZE) == (void *)-1){
        return NULL;
    }
    b->free = 0;

    return b;
}

int get_method(){
    return method;
}

void set_method(int m){
    switch (m){
        case FIRST_FIT:
            method = FIRST_FIT;
            first_fit_counter++;
            break;
        case BEST_FIT:
            method = BEST_FIT;
            best_fit_counter++;
            break;
        case WORST_FIT:
            method = WORST_FIT;
            worst_fit_counter++;
            break;
        default:
            printf("Error: invalid method\n");
            break;
    }
}

void malloc_control(int m){
    if (m == FIRST_FIT) {
        set_method(FIRST_FIT);
    } else if (m == BEST_FIT) {
        set_method(BEST_FIT);
    } else if (m == WORST_FIT) {
        set_method(WORST_FIT);
    } else {
        printf("Error: invalid method\n");
    }
}


void *malloc_(size_t size){
    t_block b, last;
    size_t s;
    s = align(size);
    log_event("malloc", size);
    if (base){
        last = base;
        b = find_block_(&last, s);
        if (b) {
            if ((b->size - s) >= (BLOCK_SIZE + 4))
                split_block(b, s);
            b->free = 0;
        } else {
            b = extend_heap_(last, s);
            if (!b)
                return (NULL);
        }
    } else {
        b = extend_heap_(NULL, s);
        if (!b)
            return (NULL);
        base = b;
    }
    return (b->data);
}

void free_(void *ptr){
    if (ptr == NULL){
        log_event("failed free_ with null pointer", ZERO_SIZE_EVENT);
        return;
    }
    t_block b;
    log_event("free", ZERO_SIZE_EVENT);
    if (valid_addr_(ptr)){
        b = get_block_(ptr);
        b->free = 1;

        if (b->next && b->next->free)
            fusion(b);
        if (b->prev && b->prev->free)
            fusion(b->prev);
        else{
            if (b->next)
                b->next->prev = b;
            if (b->prev)
                b->prev->next = b;
            else
                base = b;
            b->free = 1;
            b->prev = NULL;
        }
    }
    else{
        log_event("failed free_ with invalid pointer", ZERO_SIZE_EVENT);
    }
}


void *calloc(size_t number, size_t size){
    size_t *new;
    size_t s4, i;
    log_event("calloc", number * size);
    if (!number || !size){
        return (NULL);
    }
    new = malloc_(number * size);
    if (new){
        s4 = align(number * size) << 2;
        for (i = 0; i < s4; i++)
            new[i] = 0;
    }
    return new;
}

void *realloc_(void *ptr, size_t size){
    size_t s;
    t_block b, new;
    void *newp;
    log_event("realloc", size);
    if (!ptr){
        return (malloc_(size));
    }

    if (valid_addr_(ptr)){
        s = align(size);
        b = get_block_(ptr);

        if (b->size >= s){
            if (b->size - s >= (BLOCK_SIZE + 4))
                split_block(b, s);
        } else {
            if (b->next && b->next->free && (b->size + BLOCK_SIZE + b->next->size) >= s){
                fusion(b);
                if (b->size - s >= (BLOCK_SIZE + 4))
                    split_block(b, s);
            } else {
                newp = malloc_(s);
                if (!newp)
                    return (NULL);
                new = get_block_(newp);
                copy_block(b, new);
                free_(ptr);
                return (newp);
            }
        }
        return (ptr);
    }
    return (NULL);
}

void check_heap(void *data){
    if (data == NULL){
        printf("Data is NULL\n");
        return;
    }

    t_block block = get_block_(data);

    if (block == NULL){
        printf("Block is NULL\n");
        return;
    }

    printf("\033[1;33mHeap check\033[0m\n");
    printf("Size: %zu\n", block->size);
    t_block current = base;
    while(current){
        if(current->size <= 0){
            printf("Inconsistency: invalid block size in %p\n", current);
        }
        if(current->free && current->next && current->next->free){
            printf("Inconsistency: two adjacent free blocks in %p and %p\n", current, current->next);
        }
        if(current->free && current->prev && current->prev->free){
            printf("Inconsistency: two adjacent free blocks in %p and %p\n", current, current->prev);
        }
        if(current->next && current->next->size <= 0){
            printf("Inconsistency: invalid block size in %p\n", current->next);
        }
        if(current->next && current->next->prev <= 0){
            printf("Inconsistency: invalid block size in %p\n", current->next);
        }
        current = current->next;
    }
    if (block->next != NULL) {
        printf("Next block: %p\n", (void *)(block->next));
    } else {
        printf("Next block: NULL\n");
    }

    if (block->prev != NULL){
        printf("Prev block: %p\n", (void *)(block->prev));
    } else {
        printf("Prev block: NULL\n");
    }

    printf("Free: %d\n", block->free);

    if (block->ptr != NULL){
        printf("Beginning data address: %p\n", block->ptr);
        printf("Last data address: %p\n", (void *)((char *)(block->ptr) + block->size));
    } else {
        printf("Data address: NULL\n");
    }

    printf("Heap address: %p\n", sbrk(0));
}

size_t* get_allocator_memory_usage() {
    if(base == NULL){
        return NULL;
    }
    t_block current = base;
    size_t total_allocated = 0;
    size_t total_free = 0;
    while(current){
        if(current->free){
            total_free += current->size;
        } else {
            total_allocated += current->size;
        }
        current = current->next;
    }
    memory_metrics[ALLOCATED] = total_allocated;
    memory_metrics[FREE] = total_free;
    return memory_metrics;
}

int log_event(const char *event, size_t size){
    if (log_file_name == NULL){
        return -1;
    }
    FILE *file = fopen(log_file_name, "a");
    if(file == NULL){
        perror("Error opening log file");
        return -1;
    }
    fprintf(file, "Event: %s, size: %zu\n", event, size);
    fclose(file);
    return 0;
}

