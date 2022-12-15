/* circuitSatifiability.c solves the Circuit Satisfiability
 *  Problem using a brute-force sequential solution.
 *
 *   The particular circuit being tested is "wired" into the
 *   logic of function 'checkCircuit'. All combinations of
 *   inputs that satisfy the circuit are printed.
 *
 *   16-bit version by Michael J. Quinn, Sept 2002.
 *   Extended to 32 bits by Joel C. Adams, Sept 2013.
 */

#include <stdio.h>	// printf()
#include <limits.h> // UINT_MAX
#include <mpi.h>	// MPI
int checkCircuit(int, int);
void MPI_calculate(int, int, int *, int *);
int main(int argc, char *argv[]) {
	int i;				 /* loop variable (32 bits) */
	int MPI_local_count; /* local sum */
	int MPI_sum;	  /* total sum */

	// time 
	double start_wall_time = 0.0, total_wall_time;

	// Initialize MPI
	int MPI_err;
	MPI_err = MPI_Init(&argc, &argv);

	// Determine the number of processors
	int number_of_processes;
	MPI_err = MPI_Comm_size(MPI_COMM_WORLD, &number_of_processes);

	// Determine the rank of this processor
	int MPI_rank; /* process id */
	MPI_err = MPI_Comm_rank(MPI_COMM_WORLD, &MPI_rank);
	// start timer
	start_wall_time = MPI_Wtime();
	// different rank do different job, every rank plus the number of processes, so the job will be distributed averagely.
	for (i = MPI_rank; i <= USHRT_MAX; i += number_of_processes) {
		MPI_sum += checkCircuit(MPI_rank, i);
	}
	// tree structured
	MPI_calculate(MPI_rank, number_of_processes, &MPI_sum, &MPI_local_count);

	printf("Process %d finished.\n", MPI_rank);
	fflush(stdout);
	// job done
	if (MPI_rank == 0) {
		printf("\nA total of %d solutions were found.\n\n", MPI_sum);
		fflush(stdout);
		total_wall_time = MPI_Wtime() - start_wall_time;
		printf("Process %d finished in time %lf secs.\n", MPI_rank, total_wall_time);
	}
	// end the MPI
	MPI_Finalize();
	return 0;
}

void MPI_calculate(int MPI_rank, int number_of_processes, int *MPI_sum, int *MPI_local_count) {
	// 剩下1的時候就結束
    while(number_of_processes != 1) {
		// 用mid和remainder，避免掉奇數個process發生問題的風險
    	int mid = number_of_processes/2;
        int remainder = number_of_processes%2;
        if(MPI_rank < mid) { // 小於mid的負責接收
            MPI_Recv(MPI_local_count, 1, MPI_INT, MPI_rank+mid+remainder, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			// 將接收到的加入MPI_sum
        	*MPI_sum += *MPI_local_count;
		}
        else if(MPI_rank >= mid+remainder && MPI_rank < number_of_processes) { // 大於的負責傳遞，如果有奇數的則不會做事
            MPI_Send(MPI_sum, 1, MPI_INT, MPI_rank-mid-remainder, 0, MPI_COMM_WORLD);
            return;
        }
		// 將數量更新
        number_of_processes = mid+remainder;
    }
}

/* EXTRACT_BIT is a macro that extracts the ith bit of number n.
 *
 * parameters: n, a number;
 *             i, the position of the bit we want to know.
 *
 * return: 1 if 'i'th bit of 'n' is 1; 0 otherwise
 */

#define EXTRACT_BIT(n, i) ((n & (1 << i)) ? 1 : 0)

/* checkCircuit() checks the circuit for a given input.
 * parameters: id, the id of the process checking;
 *             bits, the (long) rep. of the input being checked.
 *
 * output: the binary rep. of bits if the circuit outputs 1
 * return: 1 if the circuit outputs 1; 0 otherwise.
 */

#define SIZE 16

int checkCircuit(int id, int bits)
{
	int v[SIZE]; /* Each element is a bit of bits */
	int i;

	for (i = 0; i < SIZE; i++)
		v[i] = EXTRACT_BIT(bits, i);

	if ((v[0] || v[1]) && (!v[1] || !v[3]) && (v[2] || v[3]) && (!v[3] || !v[4]) && (v[4] || !v[5]) && (v[5] || !v[6]) && (v[5] || v[6]) && (v[6] || !v[15]) && (v[7] || !v[8]) && (!v[7] || !v[13]) && (v[8] || v[9]) && (v[8] || !v[9]) && (!v[9] || !v[10]) && (v[9] || v[11]) && (v[10] || v[11]) && (v[12] || v[13]) && (v[13] || !v[14]) && (v[14] || v[15]))
	{
		printf("%d) %d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d \n", id,
			   v[15], v[14], v[13], v[12],
			   v[11], v[10], v[9], v[8], v[7], v[6], v[5], v[4], v[3], v[2], v[1], v[0]);
		fflush(stdout);
		return 1;
	}
	else
	{
		return 0;
	}
}

