// <FilterData.c>

#include "filterData.h"
#include "Logcat.h"

void printLogcat(const char *buff, int count,char* dev_name)
{
    int i;
    char temp[1024];
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "  ▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼");
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "  ▼ <%s> %d", dev_name, count);

    for(i = 0; i < count * 3; i+=3) {
        sprintf(&temp[i], "%02X", buff[i/3]);
        sprintf(&temp[i+2], " ");
    }

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "  ▲ %s", temp);
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "  ▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲");
}

void printHex2(const char *buff, int count,char* dev_name)
{
    int i;

    printf("-------%s PrintHex--------\n",dev_name);
    for(i = 0; i < count; i++) {
        printf("%02X ", buff[i]);
    }

    printf("\n");
}

int F_checkStartBit(listNode* p, unsigned char* readBuff)
{
    int readSize = 0;
    if(0 == strcmp(p->dev_maker,"HANBACK"))
    {
        if(readBuff[0] == HANBACK_START_BIT_FRONT &&
                readBuff[1] == HANBACK_START_BIT_REAR)
        {
            return 1;
        }
    }
    ///add code
    return 0;
}

void F_readData(listNode* p)
{
    int readSize = 0;
    int readTotalSize = 0;
    unsigned char readBuff[MAX_BUFF_SIZE] = { 0 };
    unsigned char readTemp[MAX_BUFF_SIZE] = { 0 };

    //printf("---before acces F_readData %s\n",p->dev_path);
    if (access(p->dev_path, R_OK & W_OK) == 0)
    {
        //printf("Access OK!\n");
        p->fd = user_uart_open(p->dev_path);
        user_uart_config(p->fd, 57600, 8, 0, 1);

        //start bit가 일치 할 때 까지 계속 data를 받음
        for(readTotalSize = 0; readTotalSize < p->dev_pacLen;) {
            if((readSize = user_uart_read(p->fd, readTemp, p->dev_pacLen)) == -1) {
                readSize = 0;
                continue;
            }

            strncat_s(readBuff, readTemp, readTotalSize, readSize);

            if(F_checkStartBit(p, readBuff)) {
                readTotalSize += readSize;
            }
            else {
                readSize = 0;
                readTotalSize = 0;
            }
        }
        //        printLogcat(readBuff, readTotalSize, p->dev_name);

        //////save CURRDATA,PREVDATA///////////////////////
        strncat_s(p->p_buf, p->c_buf, 0, p->dev_pacLen);
        strncat_s(p->c_buf, readBuff, 0, p->dev_pacLen);
        ///////////////////////////////////////////////////

        ////data/////
        F_filterData(p, p->c_buf, p->dev_pacLen);
        memset(readBuff, 0, sizeof(readBuff));

        user_uart_close(p->fd);
    }
    //    else //node delete
    //    {
    //        user_uart_close(p->fd);
    //        deleteNode(p, p->dev_path);
    //    }
}

void F_filterData(listNode *p, unsigned char *readBuff, int count)
{
    int i;
    int index;
    int value[2] = {0};     // measure data of readBuff
    int data[2] = {0};    //

    int front = p->a_front;//현재 Queue의 front가 가리키는 위치는 빈 공간이기 때문에
    int rear = p->a_rear;

    if( strcmp(p->dev_maker,"HANBACK") == 0 )
    {
        // data start location
        index = 4;

        //        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "HANBACK Filtering");
        //// 1.make Analyzed Data ////
//        for(i = 0; i < p->dev_datalen / 2; i++)
        for(i = 0; i < 1; i++)
        {
            value[0] = (int)readBuff[index+(i*2)];
            value[1] = (int)readBuff[index+(i*2)+1];

            //상위비트 하위비트 계산해서 하나의 데이터로 analyzed data 생성
            data[i] = (int)( ((0xFF & value[0])<<8) + (0xFF & value[1]) );

            if( p->dev_id[0] == HANBACK_THRMMTR_FRONT &&
            	p->dev_id[1] == HANBACK_THRMMTR_REAR &&
				data[i] < 61)
                return ;

            __android_log_print(ANDROID_LOG_INFO, "GraphData", "\t%d\t%d\t%d", p->D_data.a_Data[0], p->D_data.f_Data[0], p->D_data.s_Data[0]);
        }

        if( p->dev_id[0] == HANBACK_THRMMTR_FRONT &&
                p->dev_id[1] == HANBACK_THRMMTR_REAR )
        {

            ////  2-1. Data store
            //            for(i = 0; i < p->dev_datalen / 2; i++) // Body Temp, Interior Temp store
            for(i = 0; i < p->dev_datalen / 4; i++) // Body Temp store
            {
                p->analyzedData[i] = data[i];
            	p->D_data.a_Data[i] = data[i];

                ////  2-2. Remove Outlier
                // p->analyzedData[front] == Body Temperature
                // Fixed value: because body temperature is never high 40...
                if( p->analyzedData[i] < 4000 &&
                        p->analyzedData[i] > 3000)
                {
                    setFilterQdata(p, p->analyzedData[i], CURRDATA);
                    p->D_data.f_Data[i] = p->analyzedData[i];
//                    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%d", p->analyzedData[front]);
                }
                else if(p->analyzedData[i] != 0)
                    setFilterQdata(p, p->D_data.f_Data[0], CURRDATA);
            }
        }//end else if
        else
        {
            for(i = 0; i < p->dev_datalen / 2; i++)
            {
                if( front < MAX_BUFF_SIZE )
                {
                    p->analyzedData[front] = data[i];
                	p->D_data.a_Data[i] = data[i];

                    front++;
                }
                else
                {
                    front = 0;
                    rear++;
                }
            }

            if( p->a_flag == 1 ) {
                int tmp_data = 0;
                int displacement[2] = {0};

                int tmp_front = 0;
                int c_front = 0;
                int p_front = 0;

                if(front > 1)
                	tmp_front = front-2; // front - 1 == recent previous analyzedData
                else if(front == 1)
                	tmp_front = MAX_BUFF_SIZE-1;
                else
                	tmp_front = MAX_BUFF_SIZE-2;

                // previous front
                c_front = front-1;
                p_front = tmp_front;

                for(i = 0; i < 10; i++)
                {
                    if(tmp_front > 0 && tmp_front < MAX_BUFF_SIZE)
                        tmp_data += abs(p->analyzedData[tmp_front] - p->analyzedData[tmp_front-1]);
                    else {
                        tmp_data += abs(p->analyzedData[tmp_front] - p->analyzedData[MAX_BUFF_SIZE-1]);
                        tmp_front = MAX_BUFF_SIZE-1;
                    }
                    tmp_front--;
                }

                // "i" is window size : n-1 == 9
                tmp_data /= i-1;

                // displacement calculation
                displacement[0] = p->analyzedData[p_front] - tmp_data;
                displacement[1] = p->analyzedData[p_front] + tmp_data;

                // Filter Data
                if( p->analyzedData[c_front] >= displacement[0] &&
                    p->analyzedData[c_front] <= displacement[1] &&
					p->analyzedData[c_front] > 60)
                {
                    setFilterQdata(p, p->analyzedData[c_front], CURRDATA);
                    p->D_data.f_Data[0] = p->analyzedData[c_front];
//                    __android_log_print(ANDROID_LOG_INFO, "GraphData", "Not Filtered // %d <= %d <= %d", displacement[0], p->analyzedData[c_front], displacement[1]);
                }
                else if(p->analyzedData[i] != 0)
                {
                    setFilterQdata(p, p->D_data.f_Data[0], CURRDATA);
//                    __android_log_print(ANDROID_LOG_INFO, "GraphData", "Filtered // %d <= %d <= %d", displacement[0], p->analyzedData[c_front], displacement[1]);
                }
            }
        }
    }

    p->a_front = front;
    p->a_rear += rear;

    if(p->a_cnt < 10)
        p->a_cnt += 1;
    else if( rear != 0 || front > 9 )
        p->a_flag = 1;

}
