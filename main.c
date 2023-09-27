#include "mcc_generated_files/system.h"
#include "mcc_generated_files/pin_manager.h"
#include "mcc_generated_files/delay.h"
#include "string.h"
#include "mcc_generated_files/uart1.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#define STANDBY_TIME 5000
#define STANDBY_TIME_AFTER_RST 5000
#define BUFFER_SIZE 512
//#define INTERRUPT_DEBUG

int commandStep = 0;
bool searchFlag, searchRST_flag, send = false;
int wait, wait_, counter, led, wait1, wait3, wait4, wait2, wait_AfterRST, searchCount = 0;
volatile int CurrentTime, previousTime, CurrentTime_, previousTime_, CurrentTime1, previousTime1, CurrentTime2, previousTime2, CurrentTime3, previousTime3, CurrentTime4, previousTime4, previousTime_AfterRST, CurrentTime_AfterRST = 0;
bool resetPrinted, wifiConnected, sendON_OFF = false;

const char command_AT[] = "AT\r\n";
const char command_AT_GMR[] = "AT+GMR\r\n";
const char command_ATE1[] = "ATE1\r\n";
const char command_AT_CWAUTOCONN [] = "AT+CWAUTOCONN=0\r\n";
const char command_AT_CWJAP [] = "AT+CWJAP=\"Delsa_Ustkat\",\"Delsa2021*\"\r\n";
//const char command_AT_HTTPCLIENT[] = "AT+HTTPCLIENT=3,0,\"https://jsonplaceholder.typicode.com/users\",\"httpbin.org\",\"/post\",1,\"field1=value1&field2=value2\"\r\n";
const char command_AT_HTTPCLIENT[] = "AT+HTTPCLIENT=2,0,\"https://api.thingspeak.com/talkbacks/49760/commands/35748381.json?api_key=Z96ZUZ0ZIQSTP5ID\",\"httpbin.org\",\"/get\",1\r\n";


//// START BUFFER BAS?T ?SLEMLER START - DEL -PUSH -POP - SEARCH

typedef struct {
    char buffer[BUFFER_SIZE];
    int head;
    int tail;
    int count;

} RingBuffer;

RingBuffer rxBuffer;

void bufferStart(RingBuffer* buffer) {
    buffer->head = 0; //Baslang?c indeksi
    buffer->tail = 0; // Bitis indeksi
    buffer->count = 0; //eleman say?s?
}

void circ_buf_del(RingBuffer* buffer) {
    int i, count = 0;
    //    for (i = buffer->tail; i < BUFFER_SIZE; i++) {
    for (i = 0; i < BUFFER_SIZE; i++) {
        buffer->buffer[i] = 0;
    }
    buffer->head = 0; //Baslangic indeksi
    buffer->tail = 0; // Bitis indeksi
    buffer->count = 0; //eleman sayisi

}

char circ_buf_push(RingBuffer* buffer, char data) {

    if (buffer->count >= BUFFER_SIZE) { //DOLU eski elemanin kaybolmasini saglar
        //        printf("buffer is full");
        circ_buf_del(buffer);
    }

    buffer->buffer[buffer->head] = data; // yeni veri eklenir
    //    printf("Added Elem[%d] = %c\n", buffer->head, data);

    //    printf("%c", buffer->buffer[buffer->head]);

    buffer->head = (buffer->head + 1) % BUFFER_SIZE; //head konumu bir ileri kaydirilir
    buffer->count++; // eleman sayisi artar

    //    printf(" count = %d\n", buffer->count);
    return data;
}

char circ_buf_pop(RingBuffer* buffer) {
    char data = -1;

    if (buffer->count <= 0) { //tampon bos olup olmadigi
        return -1;
    }

    if (buffer ->count || (buffer->tail) % BUFFER_SIZE != buffer->head) {
        data = buffer->buffer[buffer->tail];
        // printf("Removed Elem[%d] = %c\n", buffer->tail, data);

        buffer->tail = (buffer->tail + 1) % BUFFER_SIZE; // bir ileri kaydirilir
        buffer->count--; //eleman sayisi azaltilir

        if (buffer->count == 0) {
            buffer->tail = 0; // Tüm veriler c?kar?ld?g?nda ta?lde basa donulur
            buffer->head = 0;
        }
    }
    return data;
}

bool search_fonk(RingBuffer* buffer, const char* searchString) {
    int i;
    int searchLen = strlen(searchString);
    bool searchFlag = false;
    if (searchLen == 0 || buffer->count == 0) {
        return false;
    }
    int searchIndex = 0;
    int matchCount = 0;
    for (i = buffer->tail; i != buffer->head; i = (i + 1) % BUFFER_SIZE) {
        if (buffer->buffer[i] == searchString[searchIndex]) {
            searchIndex++;
            matchCount++;
            searchCount++;
            if (matchCount == searchLen) {
                searchFlag = true;
                for (int j = buffer->tail; j < buffer->head; j++) {
                    buffer->buffer[j] = 0;
                }
                buffer->count = 0; // her seyi sifirliyoruz
                buffer->tail = 0;
                buffer->head = 0;
                break;
            }
        } else {
            searchIndex = 0;
            matchCount = 0;
        }
    }
    if (rxBuffer.count >= BUFFER_SIZE / 2 && searchFlag) {
        circ_buf_del(&rxBuffer);
    }
    return searchFlag;
}
/////////////////// END BAS?T ISLEMLER RING BUF //////////////

/////////// START INTERRUPT UART VE TIMER

static void uart1_rx_interrupt(void) {

    while (U1STAbits.URXDA) {

        uint8_t receivedData = U1RXREG;

#ifdef INTERRUPT_DEBUG
        //   printf("%c", receivedData);
#endif

        circ_buf_push(&rxBuffer, receivedData);
    }

    IFS0bits.U1RXIF = 0;
}

static void tim1_interrupt(void) {

    CurrentTime++;
    counter++;
    if (CurrentTime - previousTime > STANDBY_TIME) {
        previousTime = CurrentTime;
        wait = 1;
    }
    CurrentTime1++;
    if (CurrentTime1 - previousTime1 > 3000) {
        previousTime1 = CurrentTime1;
        wait1 = 1;
    }
    CurrentTime2++;
    if (CurrentTime2 - previousTime2 > 6000) {
        previousTime2 = CurrentTime2;
        wait2 = 1;
    }
    CurrentTime3++;
    if (CurrentTime3 - previousTime3 > 9000) {
        previousTime3 = CurrentTime3;
        wait3 = 1;
    }
    CurrentTime4++;
    if (CurrentTime4 - previousTime4 > 12000) {
        previousTime4 = CurrentTime4;
        wait4 = 1;
    }




    IFS0bits.T1IF = false;
}

static void tim2_interrupt(void) {

    CurrentTime_AfterRST++;
    if (CurrentTime_AfterRST - previousTime_AfterRST > STANDBY_TIME_AFTER_RST) {
        previousTime_AfterRST = CurrentTime_AfterRST;
        wait_AfterRST = 1;
    }

    CurrentTime_++;
    if (CurrentTime_ - previousTime_ > 6000 && CurrentTime_ - previousTime_ < 9000) {
        previousTime_ = CurrentTime_;
        wait_ = 1;
    }
    IFS0bits.T2IF = false;
}

void uart1_set_rx_interrupt_handler(void (* interruptHandler)(void)) {
    UART1_SetRxInterruptHandler(interruptHandler);
}

void tim1_set_interrupt_handler(void (* interruptHandler)(void)) {
    TMR1_SetInterruptHandler();
}

void tim2_set_interrupt_handler(void (* interruptHandler)(void)) {
    TMR1_SetInterruptHandler();
}

///////////////////////// END UART TIMER ///////////////////////

////////////START AT COMMANDS ISLEMLERI //////////////////////

void send_fonk(const char* command) {
    uint8_t i = 0;
    uint8_t commandSize = strlen(command);

    while (i < commandSize) {
        UART1_Write(command[i]);
        i++;
    }
}

void send_RST() {
    if (commandStep > 0 && wait == 1 && !searchRST_flag) { // data varsa ve bekleme süresini gecmisse
        send_fonk("AT+RST\r\n");
        printf("!! RESET !!\n ");
        wait = 0;
        wait1 = 0;
        wait2 = 0;
        wait3 = 0;
        wait4 = 0;

        searchRST_flag = true;
    }
}

void send_CMD_afterReset() { // reset attiktan sonra tekrar AT gonder?r
    if (wait_AfterRST == 1 && !send) {
        commandStep = 0;
        wait_AfterRST = 0;
        wait = 0;
        send_fonk(command_AT);
    }
}

void timeIsUp() {
    if (searchCount == 0 && wait == 1 && !resetPrinted) { //hic data gonder?lmem?sse ve bekleme suresini gecmisse
        send_fonk("AT+RST\r\n");
        printf("!! SÜRE DOLDU RESET !!");
        wait = 0;
        resetPrinted = true;
    }
}
//////////// AT COMMANDS END ///////////////////////////

int main(void) {
    // initialize the device
    SYSTEM_Initialize();

    circ_buf_del(&rxBuffer);
    // interrupt settings
    IFS0bits.U1RXIF = 1;
    IFS0bits.T1IF = true;
    IFS0bits.T2IF = true;
    uart1_set_rx_interrupt_handler(uart1_rx_interrupt);
    TMR1_SetInterruptHandler(tim1_interrupt);
    TMR2_SetInterruptHandler(tim2_interrupt);

    // send AT command
    send_fonk(command_AT);

    while (1) {
        if (search_fonk(&rxBuffer, "OK") && wait == 0 && commandStep == 0 && wait1 == 1) {
            send_fonk(command_AT_GMR);
            commandStep++; // bir sonraki komuta gecmek icin
            searchFlag = false;
        } else if (search_fonk(&rxBuffer, "version") && wait == 0 && commandStep == 1 && wait2 == 1) {
            send_fonk(command_AT_CWAUTOCONN);
            commandStep++; // bir sonraki komuta gecmek icin
            searchFlag = false;
        } else if (search_fonk(&rxBuffer, "OK") && wait == 0 && commandStep == 2 && wait3 == 1) {
            send_fonk(command_AT_CWJAP);
            commandStep++;
            searchFlag = false;
        } else if (search_fonk(&rxBuffer, "WIFI GOT IP") && wait == 0 && commandStep == 3 && wait4 == 1) {
            wifiConnected = true;
            send_fonk(command_AT_HTTPCLIENT);
            commandStep++;
            searchFlag = false;
        } else if (!wifiConnected) {
            timeIsUp();
            send_RST(); // reset at
            send_CMD_afterReset(); // at gonder
            wait_AfterRST = 0;
            wait = 0;
        }
        while (wifiConnected) {
            if (wait_ == 1 && !sendON_OFF) {
                wait = 0;
                sendON_OFF = true;
                send_fonk(command_AT_HTTPCLIENT);

                printf("count = %d\n", rxBuffer.count);
            }
            if (search_fonk(&rxBuffer, "OFF")) {
                IO_RB4_SetHigh();
                sendON_OFF = false;
            } else if (search_fonk(&rxBuffer, "ON")) {
                IO_RB4_SetLow();
                sendON_OFF = false;
            }
        }
    }
    return 1;
}