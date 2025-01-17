#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include "calculator.h"

#define eps 10e-5

void calc_bakery_prog_1(char *host, int op, double arg1, double arg2) {
    CLIENT *clnt;
    struct CALCULATOR calc_request;
    struct CALCULATOR *calc_response;

#ifndef DEBUG
    clnt = clnt_create(host, CALCBAKERY_PROG, CALCULATOR_VER, "udp");
    if (clnt == NULL) {
        clnt_pcreateerror(host);
        exit(1);
    }
#endif /* DEBUG */

    while (true){
    op = rand() % 4;
    arg1 = rand() % 100;
    arg2 = rand() % 100;
    calc_request.op = op;
    calc_request.arg1 = arg1;
    calc_request.arg2 = arg2;
    calc_request.number = 0; // Номер клиента будет назначен сервером

    // Получение ID клиента
    int *client_id = calc_get_index_1(NULL, clnt);
    if (client_id == NULL) {
        clnt_perror(clnt, "RPC call failed");
        exit(1);
    }
    calc_request.number = *client_id;
    printf("Assigned client NUM: %d\n", calc_request.number);

    // Задержка перед отправкой запроса
    srand(time(NULL));
    sleep(rand() % 5);

    // Выполнение RPC-вызова
    calc_request.number = *client_id;
    calc_response = calc_serv_1(&calc_request, clnt);
    if (calc_response == NULL) {
        clnt_perror(clnt, "RPC call failed");
        exit(1);
    }
    if ((abs(calc_response->result) + 1) < eps)
		printf("client arrived too late\n");
    else
	{
	    if (op == 0)
		    printf("client %d num %d %lf + %lf = %lf\n",getpid(), calc_response->number, arg1, arg2, calc_response->result);
	    else if (op == 1)
	        printf("client %d num %d %lf - %lf = %lf\n",getpid(), calc_response->number, arg1, arg2, calc_response->result);
	    else if (op == 2)
	        printf("client %d num %d %lf * %lf = %lf\n",getpid(), calc_response->number, arg1, arg2, calc_response->result);
	    else
	        printf("client %d num %d %lf / %lf = %lf\n",getpid(), calc_response->number, arg1, arg2, calc_response->result);
	}
    }

#ifndef DEBUG
    clnt_destroy(clnt);
#endif /* DEBUG */
}

int main(int argc, char *argv[]) {
    char *host = argv[1];
    int op;
    double arg1;
    double arg2;

    calc_bakery_prog_1(host, op, arg1, arg2);
    return 0;
}

