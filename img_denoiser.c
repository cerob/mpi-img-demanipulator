#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <math.h>

/* Predefined values */
#define IMG_HEIGTH 200      // Image height
#define IMG_WIDTH 200       // Image width
#define ITERATIONS 500000   // Total number of iterations

/* Macro for minimum operations */
#define MIN(a,b) (((a)<(b))?(a):(b))

/* C does not have bool type, so defined here. */
typedef enum { false, true } bool;


/* Get random number between minimum and maximum values
    given as arguments. */
int getRandom(int min, int max) {
    return rand() % (max - min + 1) + min;
}

/* Get a random number between 0 and 1
    and decide to flip. */
bool flipRandomly(double score) {
    double r = (double)rand() / (double)RAND_MAX;
    return r < score ? true : false;
}


int main(int argc, char** argv)
{
    // Check for argument count
    if (argc < 4) {
        printf("Error: Too few arguments.\n");
        exit(1);
    }
    
    // Define constant values, get parameters
    const char* IMAGE_FILE = argv[1];
    const char* OUTPUT_FILE = argv[2];
    const double BETA = strtod(argv[3], NULL);
    const double PI = strtod(argv[4], NULL);
    const double GAMMA = (1.0/2.0) * log((1.0-PI) / PI) / log(2.0);

    // MPI initial routine for all processes
    MPI_Init(NULL, NULL);
    
    int world_size; // Number of processes
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    int world_rank; // Rank of the working process
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);


    int N = world_size-1;   // Number of slaves
    int S = IMG_HEIGTH / N; // Lines for every process
    int i, j;               // Used for for loops

    // Check if image is evenly distributable to the slave processes.
    //  It not, give exception and exit with code 1.
    if (IMG_HEIGTH % N != 0) {
        printf("Invalid number of processes for height %d.\n", IMG_HEIGTH);
        exit(1);
    }

    if (world_rank == 0) {
        /* -------------------------- MASTER PROCESSES -------------------------- */
        printf("\n---------------------------------------------------------------\n");
        printf("|  Input file: %s, output file: %s\n", IMAGE_FILE, OUTPUT_FILE);
        printf("|  Parameters: Pi:%f, Beta:%f, Gamma:%f\n", PI, BETA, GAMMA);
        printf("|  Total iterations: %d\n", ITERATIONS);
        printf("|  Number of processes: %d + 1 master process\n", N);
        printf("|  Iterations per process: %d\n", ITERATIONS/N);
        printf("---------------------------------------------------------------\n\n");

        // Main image array or arrays
        int img[IMG_HEIGTH][IMG_WIDTH];

        // Read data
        FILE *imgf;
        imgf = fopen(IMAGE_FILE, "r");

        if (imgf) {
            for (i=0; i<IMG_WIDTH; i++) {
                for (j=0; j<IMG_HEIGTH; j++) {
                    fscanf(imgf, "%d", &img[i][j]);
                }
            }
        }

        // Send data to child processes
        for (i=0; i<N; i++) {
            for (j=i*S; j<(i+1)*S; j++) {
                MPI_Send(img[j], IMG_WIDTH, MPI_INT, i+1, 0, MPI_COMM_WORLD);
            }
        }
        

        /* Here, the master process waits for childs to complete their inputs to be complete. */

        // Collect data
        for (i=0; i<N; i++) {
            for (j=0; j<S; j++) {
                MPI_Recv(img[i*S + j], IMG_WIDTH, MPI_INT, i+1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
            printf("Process %d data completely received back.\n", i+1);
        }
        
        // Print data to file
        printf("Writing into %s\n", OUTPUT_FILE);
        FILE *of = fopen(OUTPUT_FILE, "w");
        if (of == NULL) {
            printf("Error opening output file.\n");
            exit(1);
        }

        for (i=0; i<IMG_HEIGTH; i++) {
            for (j=0; j<IMG_WIDTH; j++) {
                fprintf(of, "%d ", img[i][j]);
            }
            fprintf(of, "\n");
        }
        printf("Written into %s\n\n", OUTPUT_FILE);

        /* -------------------------- END MASTER PROCESSES -------------------------- */
    }
    else {
        /* -------------------------- CHILD PROCESSES -------------------------- */


        int slice[S][IMG_WIDTH];        // Partitioned data, will be used for flips
        int slice_orig[S][IMG_WIDTH];   // Original copy of partitioned data
        
        // Receive data
        for (i=0; i<S; i++) {
            MPI_Recv(slice[i], IMG_WIDTH, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            for (j=0; j<IMG_WIDTH; j++) {
                slice_orig[i][j] = slice[i][j];
            }
        }

        // Determine if the process it at the top or at the bottom
        int highest = world_rank == 1 ? true : false;
        int lowest = world_rank == N ? true : false;

        printf("Process %d (isHighest:%d, isLowest:%d) is starting...\n", world_rank, highest, lowest);

        
        int top[IMG_WIDTH], bottom[IMG_WIDTH];  // Used for received data
        int flipCount = 0;  // Used for statictical console output.

        // Seed random method with a relatively higher number
        srand(pow(world_rank, 4) + world_rank);


        // Main iteration cycle for child processes
        for (int k=ITERATIONS/N; k>0; k--) {
            
            if (!highest) {
                // Send to above process
                MPI_Send(slice[0], IMG_WIDTH, MPI_INT, world_rank-1, 1, MPI_COMM_WORLD); 
            }

            if (!lowest) {
                // Receive from below process
                MPI_Recv(bottom, IMG_WIDTH, MPI_INT, world_rank+1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }

            if (!lowest) {
                // Send to below process
                MPI_Send(slice[S-1], IMG_WIDTH, MPI_INT, world_rank+1, 2, MPI_COMM_WORLD);
            }

            if (!highest) {
                // Receive from above process
                MPI_Recv(top, IMG_WIDTH, MPI_INT, world_rank-1, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }

            // Pick a random cell
            int y = getRandom(0, S-1);
            int x = getRandom(0, IMG_WIDTH-1);

            // Logical values if randomly selected cell has a critical point
            bool atTop = y == 0;
            bool atBottom = y == S-1;
            bool atLeft = x == 0;
            bool atRight = x == IMG_WIDTH-1;
            
            /* Cell map:
            [0] [1] [2]
            [7] [C] [3]
            [6] [5] [4] */
            int neighbours[8];
            int neighbourSum = 0;

            // Fill top values
            if (highest && atTop) {
                // The process is at the highest line, so no line above.
                neighbours[0] = 0;
                neighbours[1] = 0;
                neighbours[2] = 0;
            }
            else if (atTop) {
                // Use received data
                neighbours[0] = atLeft ? 0 : top[x-1];
                neighbours[1] = top[x];
                neighbours[2] = atRight ? 0 : top[x+1];
            }
            else {
                // Else, use self data
                neighbours[0] = atLeft ? 0 : slice[y-1][x-1];
                neighbours[1] = slice[y-1][x];
                neighbours[2] = atRight ? 0 : slice[y-1][x+1];
            }

            // Fill bottom values
            if (lowest && atBottom) {
                // The process is at the lowest line, so no line below.
                neighbours[6] = 0;
                neighbours[5] = 0;
                neighbours[4] = 0;
            }
            else if (atBottom) {
                // Use received data
                neighbours[6] = atLeft ? 0 : bottom[x-1];
                neighbours[5] = bottom[x];
                neighbours[4] = atRight ? 0 : bottom[x+1];
            }
            else {
                // Else, use self data
                neighbours[6] = atLeft ? 0 : slice[y+1][x-1];
                neighbours[5] = slice[y+1][x];
                neighbours[4] = atRight ? 0 : slice[y+1][x+1];
            }

            // Fill right value
            neighbours[3] = atRight ? 0 : slice[y][x+1];

            // Fill left value
            neighbours[7] = atLeft ? 0 : slice[y][x-1];

            // Sum neighbours
            for (i=0; i<8; i++) {
                neighbourSum += neighbours[i];
            }

            // Calculate probability
            //  Note that here, double values are utilized.
            double score = exp(-2.0 * GAMMA * slice[y][x] * slice_orig[y][x] - 2.0 * BETA * slice[y][x] * neighbourSum);

            // Choose for a flip
            bool flip = flipRandomly(MIN(1, score));

            if (flip) {
                // Flip the value
                slice[y][x] *= -1;

                // Add to flip count
                flipCount++;
            }
        }

        // Send result data to the master process
        for (i=0; i<S; i++) {
            MPI_Send(slice[i], IMG_WIDTH, MPI_INT, 0, 0, MPI_COMM_WORLD);
        }

        printf("%d iterations finished. %d (%f%%) were flipped.\n", world_rank, flipCount, (double)flipCount / ITERATIONS * 100);

        /* -------------------------- END CHILD PROCESSES -------------------------- */
    }

    MPI_Finalize();
    return 0;
}
