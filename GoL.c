/* File:     p5.c
 * Author:   Ray Wang, 20228436
 * Purpose:  Implement John Conway's Game of Life.  The game is ``played''
 *           on a board or *world* consisting of a rectangular grid with
 *           m rows and n columns.  Each cell in the grid is ``alive''
 *           or ``dead.'' An initial generation (generation 0) is either
 *           entered by the user or generated using a random number
 *           generator.
 *
 *           Subsequent generations are computed according to the
 *           following rules:
 *
 *              - Any live cell with fewer than two live neighbors
 *              dies, as if caused by under-population.
 *              - Any live cell with two or three live neighbors
 *              lives on to the next generation.
 *              - Any live cell with more than three live neighbors
 *              dies, as if by over-population.
 *              - Any dead cell with exactly three live neighbors
 *              becomes a live cell, as if by reproduction.
 *
 *           Updates take place all at once.
 *
 * Compile:  gcc -g -Wall -o life life.c
 * Run:      ./life <r> <s> <rows> <cols> <max gens> <'i'|'g'>
 *              r = number of rows of threads
 *              s = number of columns of threads
 *              rows = number of rows in the world
 *              cols = number of cols in the world
 *              max gens = max number of generations
 *              'i' = user will enter generation 0
 *              'g' = program should generate generation 0
 *
 * Input:    If command line has the "input" char ('i'), the first
 *              generation.  Each row should be entered on a separate
 *              line of input.  Live cells should be indicated with
 *              a capital 'X', and dead cells with a blank, ' '.
 *           If command line had the "generate" char ('g'), the program will
 *              ask for the probability that a cell will be alive.
 *
 * Output:   The initial world (generation 0) and the world after
 *           each subsequent generation up to and including
 *           generation = max_gen.  If all of the cells die,
 *           the program will terminate.
 *
 * Notes:
 * 1.  This implementation uses a "toroidal world" in which the
 *     the last row of cells is adjacent to the first row, and
 *     the last column of cells is adjacent to the first.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

//#define DEBUG

#define LIVE 1
#define DEAD 0
#define LIVE_IO 'X'
#define DEAD_IO ' '
#define MAX_TITLE 1000

/* Global variables */
int thread_count;
int r, s, m, n;
int *wp, *twp;
int max_gens;
int barrier_count = 0;
int curr_gen = 0;
int live_count;
int break_flag = 0;
pthread_mutex_t barrier_mutex;
pthread_cond_t ok_to_proceed;

/* Functions */
void Usage(char prog_name[]);
void Read_world(char prompt[], int wp[], int m, int n);
void Gen_world(char prompt[], int wp[], int m, int n);
void Print_world(char title[], int wp[], int m, int n);
void *Play_life(void* rank);
int  Count_nbhrs(int *wp, int m, int n, int i, int j);
void Barrier(void);
void Pointer_swap(void);

/*----------------------------------------------------------------------------*/
int main(int argc, char* argv[]) {
    int *w1, *w2;
    char ig;
    pthread_t* thread_handles;
    long thread;
    
    if (argc != 7) Usage(argv[0]);
    
    r = strtol(argv[1], NULL, 10);
    s = strtol(argv[2], NULL, 10);
    m = strtol(argv[3], NULL, 10);
    n = strtol(argv[4], NULL, 10);
    max_gens = strtol(argv[5], NULL, 10);
    ig = argv[6][0];
    thread_count = r*s;
    
#  ifdef DEBUG
    printf("r = %d, s = %d, m = %d, n = %d, max_gens = %d, ig = %c\n",
           r, s, m, n, max_gens, ig);
#  endif
    
    pthread_mutex_init(&barrier_mutex, NULL);
    pthread_cond_init(&ok_to_proceed, NULL);
    thread_handles = malloc(thread_count*sizeof(pthread_t));
    w1 = malloc(m*n*sizeof(int));
    w2 = malloc(m*n*sizeof(int));
    wp = w1;
    twp = w2;
    
    if (ig == 'i')
        Read_world("Enter generation 0", wp, m, n);
    else
        Gen_world("What's the prob that a cell is alive?", wp, m, n);
    
    printf("\n");
    Print_world("Generation 0", wp, m, n);
    
    for (thread = 0; thread < thread_count; thread++) {
        pthread_create(&thread_handles[thread], NULL,
                       Play_life, (void*) thread);
    }
    
    for (thread = 0; thread < thread_count; thread++) {
        pthread_join(thread_handles[thread], NULL);
    }
    
    free(w1);
    free(w2);
    free(thread_handles);
    pthread_mutex_destroy(&barrier_mutex);
    pthread_cond_destroy(&ok_to_proceed);
    
    return 0;
}  /* main */


/*---------------------------------------------------------------------
 * Function:   Usage
 * Purpose:    Show user how to start the program and quit
 * In arg:     prog_name
 */
void Usage(char prog_name[]) {
    fprintf(stderr, "usage: %s <r> <s> <rows> <cols> <max> <i|g>\n", prog_name);
    fprintf(stderr, "       r = number of rows of threads\n");
    fprintf(stderr, "       s = number of columns of threads\n");
    fprintf(stderr, "    rows = number of rows in the world\n");
    fprintf(stderr, "    cols = number of cols in the world\n");
    fprintf(stderr, "     max = max number of generations\n");
    fprintf(stderr, "       i = user will enter generation 0\n");
    fprintf(stderr, "       g = program should generate generation 0\n");
    exit(0);
}  /* Usage */

/*---------------------------------------------------------------------
 * Function:   Read_world
 * Purpose:    Get generation 0 from the user
 * In args:    prompt
 *             m:  number of rows in visible world
 *             n:  number of cols in visible world
 * Out arg:    wp:  stores generation 0
 *
 */
void Read_world(char prompt[], int wp[], int m, int n) {
    int i, j;
    char c;
    
    printf("%s\n", prompt);
    for (i = 0; i < m; i++) {
        for (j = 0; j < n; j++) {
            scanf("%c", &c);
            if (c == LIVE_IO)
                wp[i*n + j] = LIVE;
            else
                wp[i*n + j] = DEAD;
        }
        /* Read end of line character */
        scanf("%c", &c);
    }
}  /* Read_world */


/*---------------------------------------------------------------------
 * Function:   Gen_world
 * Purpose:    Use a random number generator to create generation 0
 * In args:    prompt
 *             m:  number of rows in visible world
 *             n:  number of cols in visible world
 * Out arg:    wp:  stores generation 0
 *
 */
void Gen_world(char prompt[], int wp[], int m, int n) {
    int i, j;
    double prob;
#  ifdef DEBUG
    int live_count = 0;
#  endif
    
    printf("%s\n", prompt);
    scanf("%lf", &prob);
    
    srandom(1);
    for (i = 0; i < m; i++)
        for (j = 0; j < n; j++)
            if (random()/((double) RAND_MAX) <= prob) {
                wp[i*n + j] = LIVE;
#           ifdef DEBUG
                live_count++;
#           endif
            } else {
                wp[i*n + j] = DEAD;
            }
    
#  ifdef DEBUG
    printf("Live count = %d, request prob = %f, actual prob = %f\n",
           live_count, prob, ((double) live_count)/(m*n));
#  endif
}  /* Gen_world */


/*---------------------------------------------------------------------
 * Function:   Print_world
 * Purpose:    Print the current world
 * In args:    title
 *             m:  number of rows in visible world
 *             n:  number of cols in visible world
 *             wp:  current gen
 *
 */
void Print_world(char title[], int wp[], int m, int n) {
    int i, j;
    
    for (i = 0; i < m; i++) {
        for (j = 0; j < n; j++)
            if (wp[i*n + j] == LIVE)
                printf("%c", LIVE_IO);
            else
                printf("%c", DEAD_IO);
        printf("\n");
    }
    printf("%s\n\n", title);
}  /* Print_world */


/*---------------------------------------------------------------------
 * Function:     Play_life
 * Purpose:      Play Conway's game of life.  (See header doc)
 * In args:      rank = rank of threads
 * In globals:   max_gens, curr_gen, m, n, r, s, *wp, *twp, break_flag,
 *               live_count
 * Out globals:  *wp
 * Return val:   NULL
 */
void *Play_life(void* rank) {
    long myrank = (long) rank;
    int i, j, count;
    int my_first_row, my_last_row;
    int my_first_col, my_last_col;
    
    my_first_row = (myrank/s) * (m/r);
    my_last_row = my_first_row + m/r;
    my_first_col = (myrank%s) * (n/s);
    my_last_col = my_first_col + n/s;
    
    while (curr_gen < max_gens) {
        for (i = my_first_row; i < my_last_row; i++) {
            for (j = my_first_col; j < my_last_col; j++) {
                count = Count_nbhrs(wp, m, n, i, j);
                
#               ifdef DEBUG
                printf("curr_gen = %d, i = %d, j = %d, count = %d\n",
                       curr_gen, i, j, count);
#               endif
                if (count < 2 || count > 3) {
                    twp[i*n + j] = DEAD;
                } else if (count == 2) {
                    twp[i*n + j] = wp[i*n + j];
                } else {
                    twp[i*n + j] = LIVE;
                }
                if (twp[i*n + j] == LIVE) {
                    live_count++;
                }
            }
        }
        
        Barrier();
        if (break_flag == 1) {
            break;
        }
    }
    
    return NULL;
}  /* Play_life */

/*---------------------------------------------------------------------
 * Function:   Count_nbhrs
 * Purpose:    Count the number of living nbhrs of the cell (i,j)
 * In args:    wp:  current world
 *             m:   number of rows in world
 *             n:   number of cols in world
 *             i:   row number of current cell
 *             j:   col number of current cell
 * Ret val:    The number of neighboring cells with living neighbors
 *
 * Note:       Since the top row of cells is adjacent to the bottom
 *             row, and since the left col of cells is adjacent to the
 *             right col, in a very small world, it's possible to
 *             count a cell as a neighbor twice.  So we assume that
 *             m and n are at least 3.
 */
int Count_nbhrs(int* wp, int m, int n, int i, int j) {
    int i1, j1, i2, j2;
    int count = 0;
    
    for (i1 = i-1; i1 <= i+1; i1++)
        for (j1 = j-1; j1 <= j+1; j1++) {
            i2 = (i1 + m) % m;
            j2 = (j1 + n) % n;
            count += wp[i2*n + j2];
        }
    count -= wp[i*n + j];
    
    return count;
}  /* Count_nbhrs */

/*---------------------------------------------------------------------
 * Function:   Barrier
 * Purpose:    Block until all threads have called the barrier
 * In globals: barrier_count, break_flag, live_count
 */
void Barrier(void) {
    pthread_mutex_lock(&barrier_mutex);
    barrier_count++;
    if (barrier_count == thread_count) {
        if (live_count == 0) {
            break_flag = 1;
        } else {
            Pointer_swap();
        }
        barrier_count = 0;
        live_count = 0;
        pthread_cond_broadcast(&ok_to_proceed);
    } else {
        while (pthread_cond_wait(&ok_to_proceed, &barrier_mutex) != 0);
    }
    pthread_mutex_unlock(&barrier_mutex);
}

/*---------------------------------------------------------------------
 * Function:   Pointer_swap
 * Purpose:    Swaps pointers for generations
 * In globals: curr_gen, m, n
 * In/out:     *wp, *twp
 *
 */
void Pointer_swap(void) {
    int *tmp;
    char title[MAX_TITLE];
    
    tmp = wp;
    wp = twp;
    twp = tmp;
    curr_gen++;
    sprintf(title, "Generation %d", curr_gen);
    Print_world(title, wp, m, n);
}
