#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "options.h"

#define MAX_AMOUNT 20

struct bank {
    int num_accounts;        // number of accounts
    int *accounts;           // balance array
    pthread_mutex_t *mutex_array;
};

struct args {
    int          thread_num;  // application defined thread #
    int          delay;       // delay between operations
    int	         iterations;  // number of operations
    int          net_total;   // total amount deposited by this thread
    struct bank *bank;        // pointer to the bank (shared with other threads)
};

struct thread_info {
    pthread_t    id;    // id returned by pthread_create()
    struct args *args;  // pointer to the arguments
};

struct args1 {
    int		     delay;
    int	             *signal;
    struct bank       *bank;
    pthread_mutex_t  *mutex1;
};

struct thread_info1 {
    pthread_t     id;
    struct args1  *args;
};

// Threads run on this function
void *deposit(void *ptr)
{
    struct args *args =  ptr;
    int amount, account, balance;

    while(args->iterations--) {
        amount  = rand() % MAX_AMOUNT;
        account = rand() % args->bank->num_accounts;

        printf("Thread %d depositing %d on account %d\n",
            args->thread_num, amount, account);

        pthread_mutex_lock(&args->bank->mutex_array[account]);

        balance = args->bank->accounts[account];
        if(args->delay) usleep(args->delay); // Force a context switch

        balance += amount;
        if(args->delay) usleep(args->delay);

        args->bank->accounts[account] = balance;
        if(args->delay) usleep(args->delay);

        args->net_total += amount;
        pthread_mutex_unlock(&args->bank->mutex_array[account]);
    }
    return NULL;
}

void *transfer(void *ptr)
{
    struct args *args =  ptr;
    int amount, cuentaOrigen, cuentaDestino;

    while(args->iterations--) {
    	cuentaOrigen = rand() % args->bank->num_accounts;
    	while(1) {
    		cuentaDestino = rand() % args->bank->num_accounts;
    		if (cuentaDestino != cuentaOrigen){
    			break;
    		}
    	}
    	
    	if (&args->bank->mutex_array[cuentaOrigen] < &args->bank->mutex_array[cuentaDestino]){
    		pthread_mutex_lock(&args->bank->mutex_array[cuentaOrigen]);
    		pthread_mutex_lock(&args->bank->mutex_array[cuentaDestino]);
    	} else {
    		pthread_mutex_lock(&args->bank->mutex_array[cuentaDestino]);
    		pthread_mutex_lock(&args->bank->mutex_array[cuentaOrigen]);
    	}
    	
   
    	amount  = args->bank->accounts[cuentaOrigen];
        //printf("La cuenta %d tiene %d para transferir a %d\n",cuentaOrigen, amount, cuentaDestino);
        if (amount != 0){
        	amount  = rand() % amount;
        	if(args->delay) usleep(args->delay);
        	
        	printf("Thread %d transferring %d from account %d to account %d\n", args->thread_num, amount, cuentaOrigen, cuentaDestino);
        	    
        	args->bank->accounts[cuentaDestino] += amount;
        	if(args->delay) usleep(args->delay);
        	
        	args->bank->accounts[cuentaOrigen] -= amount;
        	if(args->delay) usleep(args->delay);
        }
        	
        pthread_mutex_unlock(&args->bank->mutex_array[cuentaDestino]);
        pthread_mutex_unlock(&args->bank->mutex_array[cuentaOrigen]);		
    }
    return NULL;
}

void *total_salary(void *ptr){

    struct args1 *args =  ptr;
    int salario = 0;

    pthread_mutex_lock(args->mutex1);
    while(*args->signal) {
        pthread_mutex_unlock(args->mutex1);
        salario = 0;
        for(int i=0; i < args->bank->num_accounts; i++) {
            pthread_mutex_lock(&args->bank->mutex_array[i]),
                    salario += args->bank->accounts[i];
        }
        //printf("El salario total es %d\n", salario);
        if(args->delay) usleep(args->delay);
        for(int i=0; i < args->bank->num_accounts; i++){
            pthread_mutex_unlock(&args->bank->mutex_array[i]);
        }
        printf("El salario total es %d\n", salario);
        pthread_mutex_lock(args->mutex1);
    }
    pthread_mutex_unlock(args->mutex1);
    return NULL;

}


struct thread_info *start_threads(struct options opt, struct bank *bank, void *func)
{
    int i;
    struct thread_info *threads;

    printf("creating %d threads\n", opt.num_threads);
    threads = malloc(sizeof(struct thread_info) * opt.num_threads);

    if (threads == NULL) {
        printf("Not enough memory\n");
        exit(1);
    }

    // Create num_thread threads running swap()
    for (i = 0; i < opt.num_threads; i++) {
        threads[i].args = malloc(sizeof(struct args));

        threads[i].args -> thread_num = i;
        threads[i].args -> net_total  = 0;
        threads[i].args -> bank       = bank;
        threads[i].args -> delay      = opt.delay;
        threads[i].args -> iterations = opt.iterations;

        if (0 != pthread_create(&threads[i].id, NULL, func, threads[i].args)) {
            printf("Could not create thread #%d", i);
            exit(1);
        }        
    }

    return threads;
}

struct thread_info1 *start_threadIt(struct options opt, struct bank *bank, int *signal){

    struct thread_info1 *thread;

    thread = malloc(sizeof(struct thread_info1));

    if (thread == NULL) {
        printf("Not enough memory\n");
        exit(1);
    }

    thread[0].args = malloc(sizeof(struct args1));
    thread[0].args -> delay = opt.delay;
    thread[0].args -> bank  = bank;
    thread[0].args -> signal = signal;
    thread[0].args -> mutex1 = malloc(sizeof(pthread_mutex_t));

    pthread_mutex_init(thread[0].args->mutex1,NULL);

    if (0 != pthread_create(&thread[0].id, NULL, total_salary, thread[0].args)) {
        printf("Could not create thread #%d", 0);
        exit(1);
    }

    return thread;
}

// Print the final balances of accounts and threads
void print_balances(struct bank *bank, struct thread_info *thrs, int num_threads) {
    int total_deposits=0, bank_total=0;
    printf("\nNet deposits by thread\n");

    for(int i=0; i < num_threads; i++) {
        printf("%d: %d\n", i, thrs[i].args->net_total);
        total_deposits += thrs[i].args->net_total;
    }
    printf("Total: %d\n", total_deposits);

    printf("\nAccount balance\n");
    for(int i=0; i < bank->num_accounts; i++) {
        printf("%d: %d\n", i, bank->accounts[i]);
        bank_total += bank->accounts[i];
    }
    printf("Total: %d\n", bank_total);
}

// wait for all threads to finish, print totals, and free memory
void wait(struct options opt, struct bank *bank, struct thread_info *threads) {
    // Wait for the threads to finish
    for (int i = 0; i < opt.num_threads; i++)
        pthread_join(threads[i].id, NULL);

    print_balances(bank, threads, opt.num_threads);

    for (int i = 0; i < opt.num_threads; i++)
        free(threads[i].args);
    free(threads);

    int *signal = malloc(sizeof(int));
    *signal = 1;
    struct thread_info1 *threadIt;
    threadIt = start_threadIt(opt, bank, signal);

    threads = start_threads(opt, bank, transfer);

    for (int i = 0; i < opt.num_threads; i++){
        pthread_join(threads[i].id, NULL);
    }
    print_balances(bank, threads, opt.num_threads);

    pthread_mutex_lock(threadIt[0].args->mutex1);
    *signal = 0;
    pthread_mutex_unlock(threadIt[0].args->mutex1);

    pthread_join(threadIt[0].id,NULL);

    pthread_mutex_destroy(threadIt[0].args->mutex1);
    free(threadIt[0].args->mutex1);
    free(threadIt[0].args);
    free(signal);
    free(threadIt);

    for (int i = 0; i < opt.num_threads; i++)
        free(threads[i].args);

    for(int i=0; i < bank->num_accounts; i++)
        pthread_mutex_destroy(&bank->mutex_array[i]);


    free(threads);
    free(bank->accounts);
    free(bank->mutex_array);
}

// allocate memory, and set all accounts to 0
void init_accounts(struct bank *bank, int num_accounts) {
    bank->num_accounts = num_accounts;
    bank->accounts     = malloc(bank->num_accounts * sizeof(int));
    bank->mutex_array  = malloc(bank->num_accounts * sizeof(pthread_mutex_t));

    for(int i=0; i < bank->num_accounts; i++) {
        bank->accounts[i] = 0;
        pthread_mutex_init(&bank->mutex_array[i], NULL);
    }
}


int main (int argc, char **argv)
{
    struct options      opt;
    struct bank         bank;
    struct thread_info *thrs;

    srand(time(NULL));

    // Default values for the options
    opt.num_threads  = 5;
    opt.num_accounts = 10;
    opt.iterations   = 100;
    opt.delay        = 10;

    read_options(argc, argv, &opt);

    init_accounts(&bank, opt.num_accounts);

    thrs = start_threads(opt, &bank, deposit);
    wait(opt, &bank, thrs);

    return 0;
}
