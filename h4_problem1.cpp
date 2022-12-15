// #include <mpi.h>
#include <chrono>
#include <pthread.h> // thread

#include "bmp.h"
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <string>
#define For(i, n) for (int i = 0; i < n; i++)
using namespace std;

// 定義平滑運算的次數
#define NSmooth 800

#define THREAD_NUMBER 16

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
/*變數宣告：                                             */
/*  all_thread    ： thread create output               */
/*  mutex         : 用來lock的                           */
/*  thread_count  ： thread數量                          */
/*  counter       : 用兩個counter避免錯誤                 */
/*********************************************************/
pthread_t *all_thread = NULL;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int thread_count = 1;
int counter[2] = {0};

/*********************************************************/
/*函數宣告：                                             */
/*  readBMP    ： 讀取圖檔，並把像素資料儲存在BMPSaveData*/
/*  saveBMP    ： 寫入圖檔，並把像素資料BMPSaveData寫入  */
/*  swap       ： 交換二個指標                           */
/*  **alloc_memory： 動態分配一個Y * X矩陣               */
/*  *thread_function: 執行平滑的函式                       */
/*********************************************************/
int readBMP(char *fileName); // read file
int saveBMP(char *fileName); // save file
void swap(RGBTRIPLE *a, RGBTRIPLE *b);
RGBTRIPLE **alloc_memory(int Y, int X); // allocate memory

void *thread_function(void *arg_p);

int main(int argc, char *argv[]) {
    /*********************************************************/
    /*變數宣告：                                             */
    /*  *infileName  ： 讀取檔名                             */
    /*  *outfileName ： 寫入檔名                             */
    /*  startwtime   ： 記錄開始時間                         */
    /*  endwtime     ： 記錄結束時間                         */
    /*********************************************************/
    char *infileName = "input.bmp";
    char *outfileName = "output1.bmp";
    //     double startwtime = 0.0, endwtime = 0;

    chrono::system_clock::time_point startwtime, endwtime;
    long thread_rank; // using long in case of 64-bit system

    thread_count = THREAD_NUMBER;
    // allocate the thread
    all_thread = (pthread_t *)malloc(thread_count * sizeof(pthread_t));

    // 讀取檔案
    if (readBMP(infileName))
        cout << "Read file successfully!!" << endl;
    else
        cout << "Read file fails!!" << endl;

    // 動態分配記憶體給暫存空間
    BMPData = alloc_memory(bmpInfo.biHeight, bmpInfo.biWidth);

    // start timer
    startwtime = chrono::system_clock::now();

    // create threads
    For(thread_rank, thread_count) {
        pthread_create(&all_thread[thread_rank], NULL, thread_function,
                       (void *)thread_rank);
    }

    // 做完運算，join回來
    For(thread_rank, thread_count) {
        pthread_join(all_thread[thread_rank], NULL);
    }

    // 寫入檔案
    if (saveBMP(outfileName))
        cout << "Save file successfully!!" << endl;
    else
        cout << "Save file fails!!" << endl;

    // end timer
    endwtime = chrono::system_clock::now();
    cout << "Total execution time: "
         << chrono::duration_cast<chrono::milliseconds>(endwtime - startwtime)
                .count()
         << " (ms)" << '\n';

    // free the memory
    free(BMPData[0]);
    free(BMPSaveData[0]);
    free(BMPData);
    free(BMPSaveData);

    return 0;
    // �o�쵲���ɶ��A�æL�X����ɶ�
    // endwtime = MPI_Wtime();
    // cout << "The execution time = "<< endwtime-startwtime <<endl ;
}

/*********************************************************/
/* 讀取圖檔                                              */
/*********************************************************/
int readBMP(char *fileName) {
    // 建立輸入檔案物件
    ifstream bmpFile(fileName, ios::in | ios::binary);

    // 檔案無法開啟
    if (!bmpFile) {
        cout << "It can't open file!!" << endl;
        return 0;
    }

    // 讀取BMP圖檔的標頭資料
    bmpFile.read((char *)&bmpHeader, sizeof(BMPHEADER));

    // 判決是否為BMP圖檔
    if (bmpHeader.bfType != 0x4d42) {
        cout << "This file is not .BMP!!" << endl;
        return 0;
    }

    // 讀取BMP的資訊
    bmpFile.read((char *)&bmpInfo, sizeof(BMPINFO));

    // 判斷位元深度是否為24 bits
    if (bmpInfo.biBitCount != 24) {
        cout << "The file is not 24 bits!!" << endl;
        return 0;
    }

    // 修正圖片的寬度為4的倍數
    while (bmpInfo.biWidth % 4 != 0)
        bmpInfo.biWidth++;

    // 動態分配記憶體
    BMPSaveData = alloc_memory(bmpInfo.biHeight, bmpInfo.biWidth);

    // 讀取像素資料
    //  for(int i = 0; i < bmpInfo.biHeight; i++)
    //	bmpFile.read( (char* )BMPSaveData[i],
    //  bmpInfo.biWidth*sizeof(RGBTRIPLE));
    bmpFile.read((char *)BMPSaveData[0],
                 bmpInfo.biWidth * sizeof(RGBTRIPLE) * bmpInfo.biHeight);

    // 關閉檔案
    bmpFile.close();

    return 1;
}
/*********************************************************/
/* 儲存圖檔                                              */
/*********************************************************/
int saveBMP(char *fileName) {
    // 判決是否為BMP圖檔
    if (bmpHeader.bfType != 0x4d42) {
        cout << "This file is not .BMP!!" << endl;
        return 0;
    }

    // 建立輸出檔案物件
    ofstream newFile(fileName, ios::out | ios::binary);

    // 檔案無法建立
    if (!newFile) {
        cout << "The File can't create!!" << endl;
        return 0;
    }

    // 寫入BMP圖檔的標頭資料
    newFile.write((char *)&bmpHeader, sizeof(BMPHEADER));

    // 寫入BMP的資訊
    newFile.write((char *)&bmpInfo, sizeof(BMPINFO));

    // 寫入像素資料
    //  for( int i = 0; i < bmpInfo.biHeight; i++ )
    //         newFile.write( ( char* )BMPSaveData[i],
    //         bmpInfo.biWidth*sizeof(RGBTRIPLE) );
    newFile.write((char *)BMPSaveData[0],
                  bmpInfo.biWidth * sizeof(RGBTRIPLE) * bmpInfo.biHeight);

    // 關閉檔案
    newFile.close();

    return 1;
}

/*********************************************************/
/* 分配記憶體：回傳為Y*X的矩陣                           */
/*********************************************************/
RGBTRIPLE **alloc_memory(int Y, int X) {
    // 建立長度為Y的指標陣列
    RGBTRIPLE **temp = new RGBTRIPLE *[Y];
    RGBTRIPLE *temp2 = new RGBTRIPLE[Y * X];
    memset(temp, 0, sizeof(RGBTRIPLE) * Y);
    memset(temp2, 0, sizeof(RGBTRIPLE) * Y * X);

    // 對每個指標陣列裡的指標宣告一個長度為X的陣列
    for (int i = 0; i < Y; i++) {
        temp[i] = &temp2[i * X];
    }

    return temp;
}
/*********************************************************/
/* 交換二個指標                                          */
/*********************************************************/
void swap(RGBTRIPLE *a, RGBTRIPLE *b) {
    RGBTRIPLE *temp;
    temp = a;
    a = b;
    b = temp;
}

void *thread_function(void *arg_p) {
    long thread_rank = (long)arg_p;
    long height = bmpInfo.biHeight / thread_count;

    if (bmpInfo.biHeight % thread_count) {
        height += 1;
    }

    // 進行多次的平滑運算
    for (int count = 0; count < NSmooth; count++) {
        // critical section, 只允許一個thread進來, 避免counter出錯
        pthread_mutex_lock(&mutex);
        // 每個thread都通過lock了, 就將counter歸零進行下一次的運算
        if (counter[count % 2] == thread_count - 1) {
            counter[(count + 1) % 2] = 0;
            // 把像素資料與暫存指標做交換
            swap(BMPSaveData, BMPData);
        }
        counter[count % 2] += 1;
        pthread_mutex_unlock(&mutex);
        while (counter[count % 2] < thread_count)
            ;

        // 進行平滑運算
        int lower = thread_rank * height;
        int upper = (thread_rank + 1) * height;
        for (int i = lower; (i < upper) && (i < bmpInfo.biHeight); i++)
            for (int j = 0; j < bmpInfo.biWidth; j++) {
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
                BMPSaveData[i][j].rgbBlue =
                    (double)(BMPData[i][j].rgbBlue + BMPData[Top][j].rgbBlue +
                             BMPData[Top][Left].rgbBlue +
                             BMPData[Top][Right].rgbBlue +
                             BMPData[Down][j].rgbBlue +
                             BMPData[Down][Left].rgbBlue +
                             BMPData[Down][Right].rgbBlue +
                             BMPData[i][Left].rgbBlue +
                             BMPData[i][Right].rgbBlue) /
                        9 +
                    0.5;
                BMPSaveData[i][j].rgbGreen =
                    (double)(BMPData[i][j].rgbGreen + BMPData[Top][j].rgbGreen +
                             BMPData[Top][Left].rgbGreen +
                             BMPData[Top][Right].rgbGreen +
                             BMPData[Down][j].rgbGreen +
                             BMPData[Down][Left].rgbGreen +
                             BMPData[Down][Right].rgbGreen +
                             BMPData[i][Left].rgbGreen +
                             BMPData[i][Right].rgbGreen) /
                        9 +
                    0.5;
                BMPSaveData[i][j].rgbRed =
                    (double)(BMPData[i][j].rgbRed + BMPData[Top][j].rgbRed +
                             BMPData[Top][Left].rgbRed +
                             BMPData[Top][Right].rgbRed +
                             BMPData[Down][j].rgbRed +
                             BMPData[Down][Left].rgbRed +
                             BMPData[Down][Right].rgbRed +
                             BMPData[i][Left].rgbRed +
                             BMPData[i][Right].rgbRed) /
                        9 +
                    0.5;
            }
    }

    return NULL;
}
