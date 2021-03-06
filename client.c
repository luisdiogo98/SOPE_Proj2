#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>

#include "client.h"
#include "constants.h"

FILE* requests_fd, *answer_fd;
char ans_name[100];

int descrit_clog = -1;
int descrit_cbook = -1;

int main(int argc, char *argv[])
{
        signal(SIGALRM, timeout);
        int n_seats, open_time;
        char* seats_list = malloc(100* sizeof(char));

        client_argchk(argc, argv, &open_time, &n_seats, seats_list);

        alarm(open_time);

        char *client_logfile = "clog.txt";
        descrit_clog = open(client_logfile, O_WRONLY | O_APPEND | O_CREAT, 0644);

        char *client_booking = "cbook.txt";
        descrit_cbook = open(client_booking, O_WRONLY | O_APPEND | O_CREAT, 0644);

        int fd = open("requests", O_WRONLY | O_NONBLOCK);
        if(fd == -1)
        {
                exit(1);
        }
        requests_fd = fdopen(fd, "w");

        snprintf(ans_name, 100, "ans%lu", (unsigned long)getpid());
        mkfifo(ans_name, 0660);

        sendMessage(requests_fd, n_seats, seats_list);

        fd = open(ans_name, O_RDONLY);
        answer_fd = fdopen(fd, "r");

        char* reserve = malloc(100 * sizeof(char));

        fgets(reserve, 100, answer_fd);

        long return_status = strtol(reserve, NULL, 10);

        if(return_status == REQ_SUCCESSFUL)
        {
                fgets(reserve, 100, answer_fd);
                printReserve(reserve);
                remove(ans_name);
                return 0;
        }
        else
        {
                if(descrit_clog != -1)
                {
                        char idclient[6];
                        snprintf(idclient, 6, "%05lu", (unsigned long)getpid());
                        write(descrit_clog, idclient,strlen(idclient));
                }
                printError(return_status);
                remove(ans_name);
                return return_status;
        }

}

void client_argchk(int argc, char* argv[], int* open_time, int* n_seats, char* seats_list)
{
        if(argc == 1)
        {
                printf("Usage: %s  <time_out> <num_wanted_seats> <pref_seat_list>\n", argv[0]);
                exit(0);
        }
        if(argc != 4)
        {
                printf("Invalid number of arguments: %s\n", argv[0]);
                exit(1);
        }
        else
        {
                *open_time = strtol(argv[1], NULL, 10);

                if(*open_time <= 0)
                {
                        printf("Invalid argument: %s\n", argv[1]);
                        exit(1);
                }

                *n_seats = strtol(argv[2], NULL, 10);

                if(*n_seats <= 0)
                {
                        printf("Invalid argument: %s\n", argv[2]);
                        exit(1);
                }

                strcpy(seats_list, argv[3]);
        }
}

void timeout(int signo)
{
        //fclose(requests_fd);
        //fclose(answer_fd);
        remove(ans_name);
        printf("Timed out - no response from server.\n");
        if(descrit_clog!=-1)
        {
                char idclient[6];
                snprintf(idclient, 6, "%05lu", (unsigned long)getpid());
                write(descrit_clog, idclient,strlen(idclient));
                write(descrit_clog, " OUT\n",5);
                close(descrit_clog);
        }
        if(descrit_cbook != -1)
                close(descrit_cbook);

        exit(2);
}

void sendMessage(FILE* fd, int n_seats, char* seats_list)
{
        char message[200];
        snprintf(message, 200, "%d %d %s\n", getpid(), n_seats, seats_list);
        setbuf(fd, NULL);
        fprintf(fd, "%s", message);
}

void printReserve(char* reserve)
{
        int n_seats = strtol(reserve, NULL, 10);

        char* seats = reserve;

        while(*seats!= ' ')
        {
                seats++;
        }
        seats++;

        printf("Number of seats reserved: %d\n", n_seats);
        printf("Seats reserved: %s", seats);

        char * reserveAux = reserve;
        char * delim_1 = strtok(reserveAux, " ");

        int s[n_seats+1];
        int index = 0;
        while(delim_1 != NULL)
        {
                int aux = atoi(delim_1);
                s[index] = aux;
                index++;
                delim_1 = strtok(NULL, " ");
        }

        if(descrit_clog != -1)
        {

                char idclient[6];
                snprintf(idclient, 6, "%05lu", (unsigned long)getpid());

                char number_seats[3];
                snprintf(number_seats, 3, "%02d", n_seats);

                int i;
                for(i = 1; i <= n_seats; i++)
                {
                        char* result = malloc(18);
                        strcpy(result,idclient);
                        strcat(result," ");

                        char current_seat[3];
                        snprintf(current_seat, 3, "%02d", i);
                        strcat(result,current_seat);

                        strcat(result,".");
                        strcat(result,number_seats);

                        strcat(result," ");

                        char number_Seat[4 + 1];

                        snprintf(number_Seat, 4 + 1, "%04d", s[i]);
                        strcat(result, number_Seat);

                        strcat(result, "\n");

                        write(descrit_clog, result, strlen(result));

                        strcpy(result, number_Seat);
                        strcat(result,"\n");

                        if(descrit_cbook != -1)
                                write(descrit_cbook, result, strlen(result));
                }
        }
}

void printError(int return_status)
{
        printf("Failed to reserve seats: ");

        char* result = malloc(6);
        strcpy(result, " ");

        switch(return_status)
        {
        case REQ_ERR_OVER_MAX_SEATS:
        {
                printf("Exceeded maximum number of seats per request.\n");
                strcat(result,"MAX\n");
                break;
        }

        case REQ_ERR_UNDER_SEAT_ID:
        {
                printf("Invalid number of seat identifiers.\n");
                strcat(result,"NST\n");
                break;
        }

        case REQ_ERR_SEAT_ID_INV:
        {
                printf("Invalid seat identifiers.\n");
                strcat(result,"IID\n");
                break;
        }

        case REQ_ERR_PARAM_OTHER:
        {
                printf("Input error.\n");
                strcat(result,"ERR\n");
                break;
        }

        case REQ_ERR_UNNAV_SEAT:
        {
                printf("Desired seats unnavailable.\n");
                strcat(result,"NAV\n");
                break;
        }

        case REQ_ERR_ROOM_FULL:
        {
                printf("Room is Full.\n");
                strcat(result,"FUL\n");
                break;
        }

        default:
        {
                printf("Unknown error.\n");
                break;
        }
        }

        if(descrit_clog != -1)
        {
                write(descrit_clog,result,strlen(result));
        }
}
