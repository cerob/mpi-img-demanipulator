#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <math.h>
//#include <time.h>


//#define IMAGE_FILE "yinyang_noisy.txt"
//#define IMAGE_FILE "gk_sn.txt"
//#define OUTPUT_FILE "output.txt"
#define IMG_HEIGTH 200
#define IMG_WIDTH 200
#define ITERATIONS 500000
//#define BETA 0.6
//#define PI 0.2

#define MIN(a,b) (((a)<(b))?(a):(b))

typedef enum { false, true } bool;


int getRandom(int min, int max, int p, int k) {
    //srand(time(NULL) - min + max + k);
    //srand(p - min + max + k);
    return rand() % (max - min + 1) + min;
}

bool flipRandomly(double score, int k) {
    //srand(time(NULL) + score + k);
    double r = (double)rand() / (double)RAND_MAX;
    return r < score ? true : false;
}

int main(int argc, char** argv)
{
    // check for argument count
    if (argc < 4) {
        printf("Error: Too few arguments.\n");
        exit(1);
    }
    
    // define constants
    const char* IMAGE_FILE = argv[1];
    const char* OUTPUT_FILE = argv[2];
    const double BETA = strtod(argv[3], NULL);
    const double PI = strtod(argv[4], NULL);
    const double GAMMA = (1.0/2.0) * log((1.0-PI) / PI) / log(2.0);

    // MPI initial routine for all processes
    MPI_Init(NULL, NULL);
    
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);


    int N = world_size-1;   // number of slaves
    int S = IMG_HEIGTH / N; // 
    int i, j;               // used for for loops

    // check if image is evenly distributable to the slave processes.
    if (IMG_HEIGTH % N != 0) {
        printf("Invalid number of processes for height %d.\n", IMG_HEIGTH);
        exit(1);
    }

    if (world_rank == 0) {
        printf("\n---------------------------------------------------------------\n");
        printf("|  Input file: %s, output file: %s\n", IMAGE_FILE, OUTPUT_FILE);
        printf("|  Parameters: Pi:%f, Beta:%f, Gamma:%f\n", PI, BETA, GAMMA);
        printf("|  Total iterations: %d\n", ITERATIONS);
        printf("|  Number of processes: %d + 1 master process\n", N);
        printf("|  Iterations per process: %d\n", ITERATIONS/N);
        printf("---------------------------------------------------------------\n\n");

        int img[IMG_HEIGTH][IMG_WIDTH];

        // read data
        FILE *imgf;
        imgf = fopen(IMAGE_FILE, "r");

        if (imgf) {
            for (i=0; i<IMG_WIDTH; i++) {
                for (j=0; j<IMG_HEIGTH; j++) {
                    fscanf(imgf, "%d", &img[i][j]);
                }
            }
        }

        // send data
        for (i=0; i<N; i++) {
            //printf("Sending to process %d[%d:%d]:", i+1, i*S, (i+1)*S);
            for (j=i*S; j<(i+1)*S; j++) {
                //printf("%d, ", j);
                MPI_Send(img[j], IMG_WIDTH, MPI_INT, i+1, 0, MPI_COMM_WORLD);
            }
            //printf("\n");
        }

        // collect data
        for (i=0; i<N; i++) {
            for (j=0; j<S; j++) {
                MPI_Recv(img[i*S + j], IMG_WIDTH, MPI_INT, i+1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
            printf("Process %d data completely received back.\n", i+1);
        }
        
        // print data to file
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

    }
    else {
        /* ------------------------------------- CHILD PROCESSES ------------------------------------- */

        int slice[S][IMG_WIDTH];
        int slice_orig[S][IMG_WIDTH];
        for (i=0; i<S; i++) {
            MPI_Recv(slice[i], IMG_WIDTH, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            for (j=0; j<IMG_WIDTH; j++) {
                slice_orig[i][j] = slice[i][j];
            }
        }
        //printf("Process %d received initial data.\n", world_rank);
        /*for (i=0; i<S; i++) {
            //printf("(%d) ", world_rank);
            for (j=0; j<IMG_WIDTH; j++) {
                printf("%d ", slice[i][j]);
            }
            //printf("\n");
        }*/

        int highest = world_rank == 1 ? true : false;
        int lowest = world_rank == N ? true : false;

        printf("Process %d (isHighest:%d, isLowest:%d) is starting...\n", world_rank, highest, lowest);

        int top[IMG_WIDTH], bottom[IMG_WIDTH];
        int flipCount = 0;


        srand(pow(world_rank, 4) + world_rank);

        for (int k=ITERATIONS/N; k>0; k--) {
            
            if (!highest) {
                MPI_Send(slice[0], IMG_WIDTH, MPI_INT, world_rank-1, 1, MPI_COMM_WORLD); // send up
                
                /*int highFlag;
                MPI_Iprobe(world_rank-1, 1, MPI_COMM_WORLD, &highFlag, MPI_STATUS_IGNORE);
                if (highFlag) {
                    MPI_Recv(top, IMG_WIDTH, MPI_INT, world_rank-1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                }*/
            }

            if (!lowest) {
                MPI_Recv(bottom, IMG_WIDTH, MPI_INT, world_rank+1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE); // recv from bottom
            }

            if (!lowest) {
                MPI_Send(slice[S-1], IMG_WIDTH, MPI_INT, world_rank+1, 2, MPI_COMM_WORLD); // send down

                /*int lowFlag;
                MPI_Iprobe(world_rank+1, 2, MPI_COMM_WORLD, &lowFlag, MPI_STATUS_IGNORE);
                if (lowFlag) {
                    MPI_Recv(bottom, IMG_WIDTH, MPI_INT, world_rank+1, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                }*/
            }

            if (!highest) {
                MPI_Recv(top, IMG_WIDTH, MPI_INT, world_rank-1, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE); // recv from top
            }

            /* Cell map:
            [0] [1] [2]
            [7] [C] [3]
            [6] [5] [4] */

            // pick cell
            int y = getRandom(0, S-1, world_rank, k);
            int x = getRandom(0, IMG_WIDTH-1, world_rank, k);

            bool atTop = y == 0;
            bool atBottom = y == S-1;
            bool atLeft = x == 0;
            bool atRight = x == IMG_WIDTH-1;
            
            int neighbours[8];
            int neighbourSum = 0;

            // fill top values
            if (highest && atTop) {
                neighbours[0] = 0;
                neighbours[1] = 0;
                neighbours[2] = 0;
            }
            else if (atTop) {
                /*int highFlag;
                MPI_Iprobe(world_rank-1, 1, MPI_COMM_WORLD, &highFlag, MPI_STATUS_IGNORE);
                if (highFlag) {
                    MPI_Recv(top, IMG_WIDTH, MPI_INT, world_rank-1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                }*/

                neighbours[0] = atLeft ? 0 : top[x-1];
                neighbours[1] = top[x];
                neighbours[2] = atRight ? 0 : top[x+1];
                /*neighbours[0] = atLeft ? 0 : 0;
                neighbours[1] = 0;
                neighbours[2] = atRight ? 0 : 0;*/
            }
            else {
                neighbours[0] = atLeft ? 0 : slice[y-1][x-1];
                neighbours[1] = slice[y-1][x];
                neighbours[2] = atRight ? 0 : slice[y-1][x+1];
            }

            // fill bottom values
            if (lowest && atBottom) {
                neighbours[6] = 0;
                neighbours[5] = 0;
                neighbours[4] = 0;
            }
            else if (atBottom) {
                /*int lowFlag;
                MPI_Iprobe(world_rank+1, 2, MPI_COMM_WORLD, &lowFlag, MPI_STATUS_IGNORE);
                if (lowFlag) {
                    MPI_Recv(bottom, IMG_WIDTH, MPI_INT, world_rank+1, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                }*/

                neighbours[6] = atLeft ? 0 : bottom[x-1];
                neighbours[5] = bottom[x];
                neighbours[4] = atRight ? 0 : bottom[x+1];
                /*neighbours[6] = atLeft ? 0 : 0;
                neighbours[5] = 0;
                neighbours[4] = atRight ? 0 : 0;*/
            }
            else {
                neighbours[6] = atLeft ? 0 : slice[y+1][x-1];
                neighbours[5] = slice[y+1][x];
                neighbours[4] = atRight ? 0 : slice[y+1][x+1];
            }

            // fill right singleton value
            neighbours[3] = atRight ? 0 : slice[y][x+1];

            // fill left singleton value
            neighbours[7] = atLeft ? 0 : slice[y][x-1];

            // sum neighbours
            for (i=0; i<8; i++) {
                neighbourSum += neighbours[i];
            }

            // calculate probability
            double score = exp(-2.0 * GAMMA * slice[y][x] * slice_orig[y][x] - 2.0 * BETA * slice[y][x] * neighbourSum);

            bool flip = flipRandomly(MIN(1, score), k);

            if (flip) {
                //slice[y][x] = -slice[y][x];
                slice[y][x] *= -1;

                flipCount++;
                if (atTop && !highest) {
                    //MPI_Send(slice[y], IMG_WIDTH, MPI_INT, world_size-1, 2, MPI_COMM_WORLD);
                }
                if (atBottom && !lowest) {
                    //MPI_Send(slice[y], IMG_WIDTH, MPI_INT, world_size+1, 1, MPI_COMM_WORLD);
                }
            }
        }

        // send result data
        for (i=0; i<S; i++) {
            MPI_Send(slice[i], IMG_WIDTH, MPI_INT, 0, 0, MPI_COMM_WORLD);
        }

        printf("%d iterations finished. %d (%f%%) were flipped.\n", world_rank, flipCount, (double)flipCount / ITERATIONS * 100);
    }

    MPI_Finalize();
    return 0;
}
