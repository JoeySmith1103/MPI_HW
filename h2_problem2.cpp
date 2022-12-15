#include<bits/stdc++.h>
#include<mpi.h>

using namespace std;
#define COM MPI_COMM_WORLD
#define For(i,n) for(int i = 0; i < n; i++)
#define ROOT 0
int main(int argc, char *argv[]) {
	
	// sort的size以及每個process要處理的size
	int N, localN;
	// sort的array以及每個process負責的array
	int *Arr, *localArr;
	// time
	double start_wall_time, total_wall_time; 
    // Initialize MPI
    int MPI_err;
    MPI_err = MPI_Init(&argc, &argv);

    // Determine the number of processors
    int number_of_processes;
    MPI_err = MPI_Comm_size(MPI_COMM_WORLD, &number_of_processes);

    // Determine the rank of this processor
    int MPI_rank; /* process id */
    MPI_err = MPI_Comm_rank(MPI_COMM_WORLD, &MPI_rank);

	// read the number of sort array
	if (MPI_rank == ROOT) {
		cout << "Please enter the number of size to sort: ";
		fflush(stdout);
		cin >> N;
	}

	// process 0 收到N, 傳出去給其他process
	MPI_Bcast(&N, 1, MPI_INTEGER, 0, COM);
	// 等待全部的process
	MPI_Barrier(COM);

	// distribute the number
	localN = N / number_of_processes;
	// 剩下的數就平均分給前面的process
	if (MPI_rank < (N % number_of_processes))
		localN += 1;
	
	// allocate the array
	Arr = (int*)malloc(sizeof(int) * N);
	localArr = (int*)malloc(sizeof(int)*localN);

	// parameter of Gatherv
	int *MPI_send_counts = (int*)malloc(sizeof(int) * number_of_processes);
	int *MPI_displacement = (int*)malloc(sizeof(int) * number_of_processes);
	// 用於計算displacement從哪開始
	int MPI_prefix_sum = 0; 
	// 計算Gatherv參數
	For (i, number_of_processes) {
		// 有多分配到一個數字
		if (i < (N%number_of_processes))
			MPI_send_counts[i] = N / number_of_processes + 1;
		else 
			MPI_send_counts[i] = N / number_of_processes;

		MPI_displacement[i] = MPI_prefix_sum;
		MPI_prefix_sum += MPI_send_counts[i];
	}

	// start the time
	start_wall_time = MPI_Wtime();

	// 亂數產生
	srand(time(NULL)+MPI_rank);	
	
	// 隨機給數字
	For (i, localN) {
		localArr[i] = rand();
	}

	// 對每個process的array進行sort
	sort(localArr, localArr+localN);
	// 等每個process都sort完
	MPI_Barrier(COM);

	// 將結果收回去
	MPI_Gatherv(localArr, MPI_send_counts[MPI_rank], MPI_INTEGER, Arr, MPI_send_counts, MPI_displacement, MPI_INTEGER, 0, COM);

	// check point
	if (MPI_rank == ROOT) {
		For (i, number_of_processes) {
			cout << "Process " << i << "local array" << '\n';
			For(j, MPI_send_counts[i]) {
				cout << Arr[MPI_displacement[i]+j] << ' ';
			}
			cout << '\n';
		}
	}

	// 等gather完
	MPI_Barrier(COM);
	
	// begin the odd-even sort
	int *tempArr = (int*)malloc(sizeof(int)*(MPI_send_counts[0]*2));
	int *partnerArr = (int*)malloc(sizeof(int)*(MPI_send_counts[0]));
	For (phase, number_of_processes) {
		int partner = -1;
		if (phase % 2 == 1) { // odd phase 
			if (MPI_rank % 2 == 1) {
				partner = MPI_rank + 1;
			}
			else {
				partner = MPI_rank - 1;
			}	
		}
		else { // even phase
			if (MPI_rank % 2 == 1) {
				partner = MPI_rank - 1;
			}
			else {
				partner = MPI_rank + 1;
			}
		}
		// 如果rank超過大小就不處理
		if (partner == -1 || partner == number_of_processes)
			continue;
		if (MPI_rank < partner) { 
			//	MPI_Sendrecv(localArr, localN, MPI_INTEGER, partner, 0, partnerArr, MPI_send_counts[partner], MPI_INTEGER, partner, 0, COM, MPI_STATUS_IGNORE); 
			MPI_Send(localArr, localN, MPI_INTEGER, partner, 0, COM);
			MPI_Recv(partnerArr, MPI_send_counts[partner], MPI_INTEGER, partner, 0, COM, MPI_STATUS_IGNORE);
		}
		else { 
		//		MPI_Sendrecv(localArr, localN, MPI_INTEGER, partner, 0, partnerArr, MPI_send_counts[partner], MPI_INTEGER, partner,  0, COM, MPI_STATUS_IGNORE);
			MPI_Recv(partnerArr, MPI_send_counts[partner], MPI_INTEGER, partner, 0, COM, MPI_STATUS_IGNORE);
			MPI_Send(localArr, localN, MPI_INTEGER, partner, 0, COM);
		}

		For (i, localN) {
			tempArr[i] = localArr[i];
		}
		For (i, MPI_send_counts[partner]) {
			tempArr[localN+i] = partnerArr[i];
		}
		sort(tempArr, tempArr+localN+MPI_send_counts[partner]);
		if (MPI_rank < partner) {
			For (i, localN) {
				localArr[i] = tempArr[i];
			}
		}
		else {
			For (i, localN) {
				localArr[i] = tempArr[MPI_send_counts[partner]+i];
			}
		}
	}

	MPI_Barrier(COM);
	MPI_Gatherv(localArr, MPI_send_counts[MPI_rank], MPI_INTEGER, Arr, MPI_send_counts, MPI_displacement, MPI_INTEGER, 0, COM);

	// job finish
	total_wall_time = MPI_Wtime() - start_wall_time;

	if (MPI_rank == 0) {
		cout << "Total execution time is: " << total_wall_time << '\n';
		cout << "The sorted array: ";
		For (i, N) {
			cout << Arr[i] << " ";
		}
		cout << '\n';
	}

	// job terminate
	MPI_Finalize();
	return 0;
}
