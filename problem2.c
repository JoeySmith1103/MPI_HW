#include <stdio.h>	// printf()
#include <time.h>   // time()
#include <stdlib.h> // rand()
#include <mpi.h>	// MPI
#define POS(x, y) (double)(x*x+y*y) /* 計算在正方形的位置 */
#define ll long long int
void MPI_calculate(int, int, long long int *);
void MPI_total_tosses_comm(int, int, int, long long int *);
int main(int argc, char *argv[]) {
	ll i;				 /* loop variable (32 bits) */
	ll number_of_tosses;
	ll number_in_circle = 0;	
	double MPI_result;	  /* result */
	double start_wall_time = 0.0, total_wall_time; /* time */
	double x, y, distance_squared;
	double min = -1.0, max = 1.0;
	srand(time(NULL));

	// Initialize MPI
	int MPI_err;
	MPI_err = MPI_Init(&argc, &argv);
	// Determine the number of processors
	int number_of_processes;
	MPI_err = MPI_Comm_size(MPI_COMM_WORLD, &number_of_processes);
	// Determine the rank of this processor
	int MPI_rank; /* process id */
	MPI_err = MPI_Comm_rank(MPI_COMM_WORLD, &MPI_rank);
	// job 開始，process 0 接收number of tosses
	if (MPI_rank == 0) {
		printf("Please enter the number of tosses: ");
		scanf("%lld", &number_of_tosses);
	}
	// broadcast it to the other processes by using point-to-point communications
	MPI_total_tosses_comm(MPI_rank, 1, number_of_processes, &number_of_tosses);
	// start timer
	start_wall_time = MPI_Wtime();

	// different rank do different different job, every rank will plus the number of processes, so the job will be distributed averagely.
	for (i = MPI_rank; i <= number_of_tosses; i += number_of_processes) {
		x = (double)rand() / RAND_MAX * 2 - 1;
		y = (double)rand() / RAND_MAX * 2 - 1;
		distance_squared = POS(x, y);
		if (distance_squared <= 1) {
			number_in_circle += 1;
		}
	}

	// tree structured
	MPI_calculate(MPI_rank, number_of_processes, &number_in_circle);

	printf("Process %d finished.\n", MPI_rank);
	fflush(stdout);
	// job done
	if (MPI_rank == 0) {
		MPI_result = 4 * number_in_circle/((double)number_of_tosses);
		total_wall_time = MPI_Wtime() - start_wall_time;
		printf("Process %d finished in time %lf secs.\n", MPI_rank, total_wall_time);
		fflush(stdout);
		printf("pi is approximately %.16f\n", MPI_result);
	}
	// end the MPI
	MPI_Finalize();
	return 0;
}

// 以樹狀的方式將number of tosses 傳出去
void MPI_total_tosses_comm(int MPI_rank, int partner, int number_of_processes, ll *number_of_tosses) {
    while (partner < number_of_processes) {
        if (MPI_rank < partner*2) {
            if (MPI_rank < partner) {
                if (MPI_rank + partner < number_of_processes) {
                    MPI_Send(number_of_tosses, 1, MPI_LONG_LONG_INT, MPI_rank+partner,  0, MPI_COMM_WORLD);
                }
            }
            else 
                MPI_Recv(number_of_tosses, 1, MPI_LONG_LONG_INT, MPI_rank-partner, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        partner *= 2;
    }
}
void MPI_calculate(int MPI_rank, int number_of_processes, ll *number_in_circle) {
	//當數量等於1時就停止
    while(number_of_processes != 1) {
		ll MPI_local_count;
		// 用mid和remainder來避免奇數process會有錯誤的問題
    	int mid = number_of_processes/2;
        int remainder = number_of_processes%2;
        if(MPI_rank < mid) { // rank小於mid就負責接收
            MPI_Recv(&MPI_local_count, 1, MPI_LONG_LONG_INT, MPI_rank+mid+remainder, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			// 將local count加入答案
        	*number_in_circle += MPI_local_count;
		}
        else if(MPI_rank >= mid+remainder && MPI_rank < number_of_processes) { // 否則負責接收
            MPI_Send(number_in_circle, 1, MPI_LONG_LONG_INT, MPI_rank-mid-remainder, 0, MPI_COMM_WORLD);
            return;
        }
        number_of_processes = mid+remainder;
    }
}
