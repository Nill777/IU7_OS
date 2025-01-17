#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include "calculator.h"

#define MAX_CLIENTS 1000
#define TIMEOUT_SEC 1

int client_num[MAX_CLIENTS] = {0};
bool choosing[MAX_CLIENTS] = {false};
int last_num = 0;

int *calc_get_index_1_svc(void *argp, struct svc_req *rqstp) {
    static int result;
    int i = 0;
    choosing[i] = true;

    // Генерация уникального клиента
    int max_num = 0;
    for (int j = 0; j < MAX_CLIENTS; j++) {
        if (client_num[j] > max_num) max_num = client_num[j];
    }
    client_num[i] = ++max_num;
    result = client_num[i];

    choosing[i] = false;
    return &result;
}

struct CALCULATOR *calc_serv_1_svc(struct CALCULATOR *argp, struct svc_req *rqstp) {
    static struct CALCULATOR result;
    int ret_num = argp->number;
    result.number = ret_num;
	int i = ret_num;
	time_t start, end;
	for (int j = 0; j < MAX_CLIENTS; j++)
	{
		if (last_num > client_num[i])
		{
			client_num[i] = 0;
			result.result = -1;
			return &result;
		}
	    start = clock();
		while ((client_num[j] > 0) && (client_num[j] < client_num[i] || client_num[j] == client_num[i] && j < i))
		{
			end = clock();
			if ((end - start) / CLOCKS_PER_SEC > TIMEOUT_SEC)
				break;
		}
	}

    switch (argp->op) {
        case ADD: result.result = argp->arg1 + argp->arg2; break;
        case SUB: result.result = argp->arg1 - argp->arg2; break;
        case MUL: result.result = argp->arg1 * argp->arg2; break;
        case DIV: 
            if (fabs(argp->arg2) == 0.0)
                result.result = -11111111111;
            else
                result.result = argp->arg1 / argp->arg2; break;
        default: break;
    }

    last_num = client_num[i];
    client_num[i] = 0;
    for (int j = 0; j < MAX_CLIENTS; j++)
	    if (client_num[j] > 0)
		    return &result;
    last_num = 0;
    return &result;
}

