#include <mpi.h>
#include <iostream>
#include <string>
#include <fstream>
#include <stdio.h>
#include <cstring>
#include <cstdlib>
#include "bmp.h"

using namespace std;
#define For(i,n) for(int i = 0; i < n; i++)
//定義平滑運算的次數
#define NSmooth 10000

/*********************************************************/
/*變數宣告：                                             */
/*  bmpHeader    ： BMP檔的標頭                          */
/*  bmpInfo      ： BMP檔的資訊                          */
/*  **BMPSaveData： 儲存要被寫入的像素資料               */
/*  **BMPData    ： 暫時儲存要被寫入的像素資料           */
/*********************************************************/
BMPHEADER bmpHeader;
BMPINFO bmpInfo;
RGBTRIPLE **BMPSaveData = NULL;
RGBTRIPLE **BMPData = NULL;

/*********************************************************/
/*函數宣告：                                             */
/*  readBMP    ： 讀取圖檔，並把像素資料儲存在BMPSaveData*/
/*  saveBMP    ： 寫入圖檔，並把像素資料BMPSaveData寫入  */
/*  swap       ： 交換二個指標                           */
/*  **alloc_memory： 動態分配一個Y * X矩陣               */
/*********************************************************/
int readBMP(char *fileName); // read file
int saveBMP(char *fileName); // save file
void swap(RGBTRIPLE *a, RGBTRIPLE *b);
RGBTRIPLE **alloc_memory(int Y, int X); // allocate memory

int main(int argc, char *argv[])
{
    /*********************************************************/
    /*變數宣告：                                             */
    /*  *infileName  ： 讀取檔名                             */
    /*  *outfileName ： 寫入檔名                             */
    /*********************************************************/
    char *infileName = "input.bmp";
    char *outfileName = "output.bmp";

    //讀取檔案
    if (readBMP(infileName))
        cout << "Read file successfully!!" << '\n';
    else
        cout << "Read file fails!!" << '\n';

    //動態分配記憶體給暫存空間
    BMPData = alloc_memory(bmpInfo.biHeight, bmpInfo.biWidth);
    int BMP_size = bmpInfo.biHeight * bmpInfo.biWidth;
    // time
    double start_wall_time = 0.0, total_wall_time;
    /******************************************************************************************/
    /*變數宣告：                                                                               */
    /*  // MPI_Scatterv會用到的參數                                                            */
    /*  // Input                                                                              */
    /*  *MPI_send_buffer    :  傳送buffer的指標，陣列包含root要傳送的資料                        */ 
    /*  *MPI_send_counts    :  要傳送到每個process的資料數目，包含root自己，非root的會忽略掉此變數 */
    /*  *MPI_displacement   :  要傳送到每個process的資料的起始位置                               */
    /*  MPI_send_type       :  傳送buffer內的資料類型                                           */
    /*  // Output                                                                              */
    /*  *MPI_receive_buffer :  接收buffer的指標，陣列包含每個process收到的資料                    */   
    /*  *MPI_receive_counts :  接收buffer的資料數目                                             */ 
    /*  MPI_receive_type    :  接收buffer內的資料類型                                           */ 
    /*  root                :  指定root                                                        */                                
    /******************************************************************************************/
    // MPI_send_buffer = BMPData
    int *MPI_send_counts;
    int *MPI_displacement;
    // MPI_send_type = MPI_RGB
    // MPI_receiver_buffer = BMPData
    int *MPI_receive_counts;
    // MPI_receive_type = MPI_RGB
    int root = 0;


    // Initialize MPI
    int MPI_err;
    MPI_err = MPI_Init(&argc, &argv);

    // Determine the number of processors
    int number_of_processes;
    MPI_err = MPI_Comm_size(MPI_COMM_WORLD, &number_of_processes);

    // Determine the rank of this processor
    int MPI_rank; /* process id */
    MPI_err = MPI_Comm_rank(MPI_COMM_WORLD, &MPI_rank);

    // 創建新的Data type
    MPI_Datatype MPI_RGB;

    // 定義新的Data type，是多個現有的Data type所組成
    MPI_Type_contiguous(sizeof(RGBTRIPLE) / sizeof(BYTE), MPI_UNSIGNED_CHAR, &MPI_RGB);

    // Commit出去讓MPI知道有這個Data type
    MPI_Type_commit(&MPI_RGB);

    // 等待所有process
    MPI_Barrier(MPI_COMM_WORLD);
    
    // start timer
    start_wall_time = MPI_Wtime();
    // scatterv的參數memory allocate
    MPI_send_counts = (int*)malloc(sizeof(int) * number_of_processes);
    MPI_displacement = (int*)malloc(sizeof(int) * number_of_processes);
    // 計算displacement
    int displacement_sum = 0;
    int send_section = (bmpInfo.biHeight/number_of_processes)*bmpInfo.biWidth;
    int remainder = BMP_size % send_section;
    
    For(i, number_of_processes) {
        MPI_send_counts[i] = send_section;
        if (remainder != 0 && i == number_of_processes-1)
            MPI_send_counts[i] += remainder;
        MPI_displacement[i] = displacement_sum;
        displacement_sum += MPI_send_counts[i];
    }
    // 每個區塊的邊界暫存空間，分上下兩個1 * 寬的長方形
    RGBTRIPLE **upper_buffer = alloc_memory(1, bmpInfo.biWidth);
    RGBTRIPLE **lower_buffer = alloc_memory(1, bmpInfo.biWidth);
    
    RGBTRIPLE **BMP_temp = alloc_memory(bmpInfo.biHeight, bmpInfo.biWidth);

    // Scatterv
    MPI_Scatterv(BMPSaveData[0], MPI_send_counts, MPI_displacement, MPI_RGB, BMPData[0], MPI_send_counts[MPI_rank], MPI_RGB, 0, MPI_COMM_WORLD);
    int len = (MPI_send_counts[MPI_rank]/bmpInfo.biWidth);
    // 有平行運算的話
    if (number_of_processes != 1) {
        int lower, upper;
        // 決定下一層是誰
        if (MPI_rank == number_of_processes-1)
            lower = 0;
        else 
            lower = MPI_rank + 1;
        // 決定上一層是誰
        if (MPI_rank == 0)
            upper = number_of_processes-1;
        else 
            upper = MPI_rank - 1;
        // 將資料送到lower，接收upper的資料
        MPI_Sendrecv(BMPData[len-1], bmpInfo.biWidth, MPI_RGB, lower, 0, upper_buffer[0], bmpInfo.biWidth, MPI_RGB, upper, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        // 將資料送到upper，接收lower的資料
        MPI_Sendrecv(BMPData[0], bmpInfo.biWidth, MPI_RGB, upper, 0, lower_buffer[0], bmpInfo.biWidth, MPI_RGB, lower, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    //進行多次的平滑運算
    for (int count = 0; count < NSmooth; count++)
    {
        //進行平滑運算
        for (int i = 0; i < len; i++)
            for (int j = 0; j < bmpInfo.biWidth; j++)
            {
                /*********************************************************/
                /*設定上下左右像素的位置                                 */
                /*********************************************************/
                int Top = i > 0 ? i - 1 : bmpInfo.biHeight - 1;
                int Down = i < bmpInfo.biHeight - 1 ? i + 1 : 0;
                int Left = j > 0 ? j - 1 : bmpInfo.biWidth - 1;
                int Right = j < bmpInfo.biWidth - 1 ? j + 1 : 0;
                /*********************************************************/
                /*與上下左右像素做平均，並四捨五入                       */
                /*********************************************************/
                if (number_of_processes != 1) {
                    // upper buffer
                    if (i == 0) {
                        BMPSaveData[i][j].rgbBlue = (double)(BMPData[i][j].rgbBlue + BMPData[0][j].rgbBlue + BMPData[0][Left].rgbBlue + BMPData[0][Right].rgbBlue + BMPData[Down][j].rgbBlue + BMPData[Down][Left].rgbBlue + BMPData[Down][Right].rgbBlue + BMPData[i][Left].rgbBlue + BMPData[i][Right].rgbBlue) / 9 + 0.5;
                        BMPSaveData[i][j].rgbGreen = (double)(BMPData[i][j].rgbGreen + BMPData[0][j].rgbGreen + BMPData[0][Left].rgbGreen + BMPData[0][Right].rgbGreen + BMPData[Down][j].rgbGreen + BMPData[Down][Left].rgbGreen + BMPData[Down][Right].rgbGreen + BMPData[i][Left].rgbGreen + BMPData[i][Right].rgbGreen) / 9 + 0.5;
                        BMPSaveData[i][j].rgbRed = (double)(BMPData[i][j].rgbRed + BMPData[0][j].rgbRed + BMPData[0][Left].rgbRed + BMPData[0][Right].rgbRed + BMPData[Down][j].rgbRed + BMPData[Down][Left].rgbRed + BMPData[Down][Right].rgbRed + BMPData[i][Left].rgbRed + BMPData[i][Right].rgbRed) / 9 + 0.5;
                    }
                    // lower buffer
                    else if (i == len-1) {
                        BMPSaveData[i][j].rgbBlue =  (double) (BMPData[i][j].rgbBlue+BMPData[Top][j].rgbBlue+BMPData[Top][Left].rgbBlue+BMPData[Top][Right].rgbBlue+lower_buffer[0][j].rgbBlue+lower_buffer[0][Left].rgbBlue+lower_buffer[0][Right].rgbBlue+BMPData[i][Left].rgbBlue+BMPData[i][Right].rgbBlue)/9+0.5;
				        BMPSaveData[i][j].rgbGreen =  (double) (BMPData[i][j].rgbGreen+BMPData[Top][j].rgbGreen+BMPData[Top][Left].rgbGreen+BMPData[Top][Right].rgbGreen+lower_buffer[0][j].rgbGreen+lower_buffer[0][Left].rgbGreen+lower_buffer[0][Right].rgbGreen+BMPData[i][Left].rgbGreen+BMPData[i][Right].rgbGreen)/9+0.5;
				        BMPSaveData[i][j].rgbRed =  (double) (BMPData[i][j].rgbRed+BMPData[Top][j].rgbRed+BMPData[Top][Left].rgbRed+BMPData[Top][Right].rgbRed+lower_buffer[0][j].rgbRed+lower_buffer[0][Left].rgbRed+lower_buffer[0][Right].rgbRed+BMPData[i][Left].rgbRed+BMPData[i][Right].rgbRed)/9+0.5;
                    }
                    else {
                        BMPSaveData[i][j].rgbBlue =  (double) (BMPData[i][j].rgbBlue+BMPData[Top][j].rgbBlue+BMPData[Top][Left].rgbBlue+BMPData[Top][Right].rgbBlue+BMPData[Down][j].rgbBlue+BMPData[Down][Left].rgbBlue+BMPData[Down][Right].rgbBlue+BMPData[i][Left].rgbBlue+BMPData[i][Right].rgbBlue)/9+0.5;
				        BMPSaveData[i][j].rgbGreen =  (double) (BMPData[i][j].rgbGreen+BMPData[Top][j].rgbGreen+BMPData[Top][Left].rgbGreen+BMPData[Top][Right].rgbGreen+BMPData[Down][j].rgbGreen+BMPData[Down][Left].rgbGreen+BMPData[Down][Right].rgbGreen+BMPData[i][Left].rgbGreen+BMPData[i][Right].rgbGreen)/9+0.5;
				        BMPSaveData[i][j].rgbRed =  (double) (BMPData[i][j].rgbRed+BMPData[Top][j].rgbRed+BMPData[Top][Left].rgbRed+BMPData[Top][Right].rgbRed+BMPData[Down][j].rgbRed+BMPData[Down][Left].rgbRed+BMPData[Down][Right].rgbRed+BMPData[i][Left].rgbRed+BMPData[i][Right].rgbRed)/9+0.5;
                    }
                }
                else {
                    BMPSaveData[i][j].rgbBlue = (double)(BMPData[i][j].rgbBlue + BMPData[Top][j].rgbBlue + BMPData[Top][Left].rgbBlue + BMPData[Top][Right].rgbBlue + BMPData[Down][j].rgbBlue + BMPData[Down][Left].rgbBlue + BMPData[Down][Right].rgbBlue + BMPData[i][Left].rgbBlue + BMPData[i][Right].rgbBlue) / 9 + 0.5;
                    BMPSaveData[i][j].rgbGreen = (double)(BMPData[i][j].rgbGreen + BMPData[Top][j].rgbGreen + BMPData[Top][Left].rgbGreen + BMPData[Top][Right].rgbGreen + BMPData[Down][j].rgbGreen + BMPData[Down][Left].rgbGreen + BMPData[Down][Right].rgbGreen + BMPData[i][Left].rgbGreen + BMPData[i][Right].rgbGreen) / 9 + 0.5;
                    BMPSaveData[i][j].rgbRed = (double)(BMPData[i][j].rgbRed + BMPData[Top][j].rgbRed + BMPData[Top][Left].rgbRed + BMPData[Top][Right].rgbRed + BMPData[Down][j].rgbRed + BMPData[Down][Left].rgbRed + BMPData[Down][Right].rgbRed + BMPData[i][Left].rgbRed + BMPData[i][Right].rgbRed) / 9 + 0.5;
                }
                BMP_temp[i][j] = BMPSaveData[i][j];
            }
        
        if (number_of_processes != 1) {
            int lower, upper;
            // 決定下一層是誰
            if (MPI_rank == number_of_processes-1)
                lower = 0;
            else 
                lower = MPI_rank + 1;
            // 決定上一層是誰
            if (MPI_rank == 0)
                upper = number_of_processes-1;
            else 
                upper = MPI_rank - 1;
            // 將資料送到lower，接收upper的資料
            MPI_Sendrecv(BMPData[len-1], bmpInfo.biWidth, MPI_RGB, lower, 0, upper_buffer[0], bmpInfo.biWidth, MPI_RGB, upper, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            // 將資料送到upper，接收lower的資料
            MPI_Sendrecv(BMPData[0], bmpInfo.biWidth, MPI_RGB, upper, 0, lower_buffer[0], bmpInfo.biWidth, MPI_RGB, lower, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        //把像素資料與暫存指標做交換
        swap(BMPSaveData, BMPData);
    }
    MPI_Gatherv(BMP_temp[0], MPI_send_counts[MPI_rank], MPI_RGB, BMPSaveData[0], MPI_send_counts, MPI_displacement, MPI_RGB, 0, MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD);

    // complete
    if (MPI_rank == 0) {
        total_wall_time = MPI_Wtime();
    }
    //寫入檔案
    if (MPI_rank == 0) {
        if (saveBMP(outfileName))
            cout << "Save file successfully!!" << '\n';
        else
            cout << "Save file fails!!" << '\n';

        cout << "The execution time is: " << total_wall_time - start_wall_time << '\n';
    }
    // 釋放記憶體
    free(upper_buffer[0]);
    free(lower_buffer[0]);
    free(upper_buffer);
    free(lower_buffer);
    free(MPI_send_counts);
    free(MPI_displacement);
    free(BMPData[0]);
    free(BMPData);
    free(BMP_temp[0]);
    free(BMP_temp);

    // finish the job
    MPI_Finalize();

    return 0;
}

/*********************************************************/
/* 讀取圖檔                                              */
/*********************************************************/
int readBMP(char *fileName)
{
    //建立輸入檔案物件
    ifstream bmpFile(fileName, ios::in | ios::binary);

    //檔案無法開啟
    if (!bmpFile)
    {
        cout << "It can't open file!!" << '\n';
        return 0;
    }

    //讀取BMP圖檔的標頭資料
    bmpFile.read((char *)&bmpHeader, sizeof(BMPHEADER));

    //判決是否為BMP圖檔
    if (bmpHeader.bfType != 0x4d42)
    {
        cout << "This file is not .BMP!!" << '\n';
        return 0;
    }

    //讀取BMP的資訊
    bmpFile.read((char *)&bmpInfo, sizeof(BMPINFO));

    //判斷位元深度是否為24 bits
    if (bmpInfo.biBitCount != 24)
    {
        cout << "The file is not 24 bits!!" << '\n';
        return 0;
    }

    //修正圖片的寬度為4的倍數
    while (bmpInfo.biWidth % 4 != 0)
        bmpInfo.biWidth++;

    //動態分配記憶體
    BMPSaveData = alloc_memory(bmpInfo.biHeight, bmpInfo.biWidth);

    //讀取像素資料
    // for(int i = 0; i < bmpInfo.biHeight; i++)
    //  bmpFile.read( (char* )BMPSaveData[i], bmpInfo.biWidth*sizeof(RGBTRIPLE));
    bmpFile.read((char *)BMPSaveData[0], bmpInfo.biWidth * sizeof(RGBTRIPLE) * bmpInfo.biHeight);

    //關閉檔案
    bmpFile.close();

    return 1;
}
/*********************************************************/
/* 儲存圖檔                                              */
/*********************************************************/
int saveBMP(char *fileName)
{
    //判決是否為BMP圖檔
    if (bmpHeader.bfType != 0x4d42)
    {
        cout << "This file is not .BMP!!" << '\n';
        return 0;
    }

    //建立輸出檔案物件
    ofstream newFile(fileName, ios::out | ios::binary);

    //檔案無法建立
    if (!newFile)
    {
        cout << "The File can't create!!" << '\n';
        return 0;
    }

    //寫入BMP圖檔的標頭資料
    newFile.write((char *)&bmpHeader, sizeof(BMPHEADER));

    //寫入BMP的資訊
    newFile.write((char *)&bmpInfo, sizeof(BMPINFO));

    //寫入像素資料
    // for( int i = 0; i < bmpInfo.biHeight; i++ )
    //        newFile.write( ( char* )BMPSaveData[i], bmpInfo.biWidth*sizeof(RGBTRIPLE) );
    newFile.write((char *)BMPSaveData[0], bmpInfo.biWidth * sizeof(RGBTRIPLE) * bmpInfo.biHeight);

    //寫入檔案
    newFile.close();

    return 1;
}

/*********************************************************/
/* 分配記憶體：回傳為Y*X的矩陣                           */
/*********************************************************/
RGBTRIPLE **alloc_memory(int Y, int X)
{
    //建立長度為Y的指標陣列
    RGBTRIPLE **temp = new RGBTRIPLE *[Y];
    RGBTRIPLE *temp2 = new RGBTRIPLE[Y * X];
    memset(temp, 0, sizeof(RGBTRIPLE) * Y);
    memset(temp2, 0, sizeof(RGBTRIPLE) * Y * X);

    //對每個指標陣列裡的指標宣告一個長度為X的陣列
    for (int i = 0; i < Y; i++)
    {
        temp[i] = &temp2[i * X];
    }

    return temp;
}
/*********************************************************/
/* 交換二個指標                                          */
/*********************************************************/
void swap(RGBTRIPLE *a, RGBTRIPLE *b)
{
    RGBTRIPLE *temp;
    temp = a;
    a = b;
    b = temp;
}
