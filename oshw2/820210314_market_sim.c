//820210314 ada berfu kaynak

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>

#include <pthread.h>

#define MAX_PRODUCTS 50

#define MAX_REQUESTS 2000

#define MAX_CUSTOMERS 50

typedef struct 
{
    int customer_id;
    int product_id;
    int quantity;
}ProductOrder;


pthread_mutex_t stk_mutex[MAX_PRODUCTS];
pthread_mutex_t paying_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t payment_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
ProductOrder* requests[MAX_REQUESTS];

int max_concurrent_payments;
int current_payments = 0;
int num_customers;
int num_products;
int reservation_timeout_ms;
FILE *log_file;
int request_count = 0;
int retry_given[MAX_REQUESTS] = {0};
int product_stock[MAX_PRODUCTS];

long time_ms() 
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000L) + (tv.tv_usec / 1000);
}

int load_input_config(const char *filename) 
{
    FILE *file = fopen(filename, "r");
    if(!file) 
    {
        perror("failed to open input.txt");
        return -1;
    }

    char line[256];
    int line_num = 0;

    while(fgets(line, sizeof(line), file)) 
    {
        if(line[0] == '\n' || line[0] == '\r' || strlen(line) <= 1) 
        {
            requests[request_count++] = NULL;
            continue;
        }

        if(line_num == 0) sscanf(line, "num_customers=%d", &num_customers);
        else if(line_num == 1) sscanf(line, "num_products=%d", &num_products);
        else if(line_num == 2) sscanf(line, "reservation_timeout_ms=%d", &reservation_timeout_ms);
        else if(line_num == 3) sscanf(line, "max_concurrent_payments=%d", &max_concurrent_payments);
        else if(line_num == 4) {
            char *tkn = strtok(line + strlen("initial_stock="), ",");
            int i = 0;

            while(tkn != NULL && i < num_products) 
            {
                product_stock[i] = atoi(tkn);
                pthread_mutex_init(&stk_mutex[i], NULL);
                tkn = strtok(NULL, ",");
                i++;
            }
        } 
        else 
        {
            ProductOrder *req = malloc(sizeof(ProductOrder));
            if(!req) 
            {
                perror("malloc failed");
                fclose(file);
                return -1;
            }
            sscanf(line, "%d,%d,%d", &req->customer_id, &req->product_id, &req->quantity);
            requests[request_count++] = req;
        }
        line_num++;
    }

    fclose(file);
    return 0;
}

void print_stock() 
{
    for(int i = 0; i < num_products; i++) 
    {
        fprintf(log_file, "product %d: %d", i, product_stock[i]);
        if(i<num_products - 1) fprintf(log_file, ", ");
    }
}

void* reg_handle(void *arg) 
{
    ProductOrder *req = (ProductOrder *)arg;

    long now;
    int reserved = 0;


    int id = req->customer_id;
    int pid = req->product_id;
    int qty = req->quantity;
    

    //long start_time = current_time_ms();

    pthread_mutex_lock(&stk_mutex[pid]);
    if(product_stock[pid] >= qty) 
    {
        product_stock[pid] -= qty;
        reserved = 1;
    }
    pthread_mutex_unlock(&stk_mutex[pid]);

    pthread_mutex_lock(&log_mutex);
    now = time_ms();
    if(reserved)
    {
        fprintf(log_file, "[%ld] Customer %d tried to add product %d ( qty : %d) to cart | Stock : [ ",now, id, pid, qty);
    }else 
    {
        long remaining = reservation_timeout_ms; 
        fprintf(log_file, "[%ld] Customer %d tried to add product %d ( qty : %d) but only %d units were available | Stock : [ ",now, id, pid, qty, product_stock[pid]);
        print_stock();
        fprintf(log_file, "]\nProduct %d is currently reserved. Try again in %ld milliseconds.\n", pid, remaining);
    }
    print_stock();
    fprintf(log_file, "] %s\n", reserved ? "// succeed" : "// failed");
    fflush(log_file);
    pthread_mutex_unlock(&log_mutex);

    if(!reserved) return NULL;

    int will_purchase = rand() % 2;

    if(!will_purchase) 
    {
        usleep(reservation_timeout_ms * 1000);

        pthread_mutex_lock(&stk_mutex[pid]);
        product_stock[pid] += qty;
        pthread_mutex_unlock(&stk_mutex[pid]);

        pthread_mutex_lock(&log_mutex);
        now = time_ms();
        fprintf(log_file, "[%ld] Customer %d could not purchase product %d ( qty : %d) in time . Timeout is expired !!! Product %d ( qty : %d) returned to the stock ! | Stock : [ ",now, id, pid, qty, pid, qty);
        print_stock();
        fprintf(log_file, "]\n");
        fflush(log_file);
        pthread_mutex_unlock(&log_mutex);
        return NULL;
    }

    int purchased = 0;
    int waited_ms = 0;
    int check_interval = 100;

    pthread_mutex_lock(&log_mutex);
    now = time_ms();
    fprintf(log_file, "[%ld] Customer %d attempted to purchase product %d ( qty : %d ) | Stock : [ ",now, id, pid, qty);
    print_stock();
    fprintf(log_file, "]\n");
    fflush(log_file);
    pthread_mutex_unlock(&log_mutex);

    while(waited_ms < reservation_timeout_ms) 
    {
        pthread_mutex_lock(&paying_mutex);
        if(current_payments < max_concurrent_payments) 
        {
            current_payments++;
            pthread_mutex_unlock(&paying_mutex);

            sleep(1);

            pthread_mutex_lock(&log_mutex);
            now = time_ms();
            fprintf(log_file, "[%ld] Customer %d purchased product %d ( qty : %d ) | Stock : [ ",now, id, pid, qty);
            print_stock();
            fprintf(log_file, "]\n");
            fflush(log_file);
            pthread_mutex_unlock(&log_mutex);

            pthread_mutex_lock(&paying_mutex);
            current_payments--;
            pthread_cond_signal(&payment_cond);
            pthread_mutex_unlock(&paying_mutex);

            purchased = 1;
            break;
        }else 
        {
            pthread_mutex_unlock(&paying_mutex);

            pthread_mutex_lock(&log_mutex);
            now = time_ms();
            fprintf(log_file, "[%ld] Customer %d couldn 't purchase product %d ( qty : %d ) and had to wait ! ( maximum number of concurrent payments reached !)\n",now, id, pid, qty);
            fflush(log_file);
            pthread_mutex_unlock(&log_mutex);

            if(!retry_given[id]) 
            {
                retry_given[id] = 1;
                usleep(check_interval * 1000);
                waited_ms += check_interval;

                pthread_mutex_lock(&log_mutex);
                now = time_ms();
                fprintf(log_file, "[%ld] Customer %d ( automatically ) retried purchasing product %d ( qty : %d ) | Stock : [ ",
                        now, id, pid, qty);
                print_stock();
                fprintf(log_file, "] //( checked for available cashier slot before timeout expired .)\n");
                fflush(log_file);
                pthread_mutex_unlock(&log_mutex);
            } else {
                break; //sadece tek deneme icin
            }
        }
    }

    if(!purchased) 
    {
        pthread_mutex_lock(&stk_mutex[pid]);
        product_stock[pid] += qty;
        pthread_mutex_unlock(&stk_mutex[pid]);

        pthread_mutex_lock(&log_mutex);
        now = time_ms();
        fprintf(log_file, "[%ld] Customer %d could not purchase product %d ( qty : %d ) in time . Timeout is expired !!! Product %d ( qty : %d ) returned to the stock ! | Stock : [ ",now, id, pid, qty, pid, qty);
        print_stock();
        fprintf(log_file, "]\n");
        fprintf(log_file, "[%ld] Customer %d retry attempt failed - product already reserved ( or purchased ) by another customer | Stock : [ ",now, id);
        print_stock();
        fprintf(log_file, "] // no more retry for this thread\n");
        fflush(log_file);
        pthread_mutex_unlock(&log_mutex);
    }

    return NULL;
}

int main() 
{
    srand(time(NULL));

    if(load_input_config("input.txt") != 0) return 1;

    log_file = fopen("log.txt", "w");
    if(!log_file) 
    {
        perror("failed to open log.txt");
        return 1;
    }

    pthread_mutex_lock(&log_mutex);
    long now = time_ms();
    fprintf(log_file, "[%ld] Initial Stock : [ ", now);
    print_stock();
    fprintf(log_file, "]\n");
    fflush(log_file);
    pthread_mutex_unlock(&log_mutex);

    pthread_t thread_group[MAX_CUSTOMERS];
    int thread_index = 0;

    for(int i = 0; i < request_count; i++) 
    {
        int is_last = (i == request_count - 1);
        if(requests[i] == NULL || is_last) 
        {
            int start_idx = i - thread_index;
            int group_size = (is_last && requests[i] != NULL) ? thread_index + 1 : thread_index;

            for(int j = 0; j< group_size; j++) 
            {
                pthread_create(&thread_group[j], NULL, reg_handle, (void *)requests[start_idx + j]);
            }
            for(int j = 0; j < group_size; j++) 
            {
                pthread_join(thread_group[j], NULL);
            }
            thread_index = 0;
        } else {
            thread_index++;
        }
    }

    fclose(log_file);
    for(int i = 0; i < request_count; i++) //memory leak engellendi burada
    {
        if (requests[i] != NULL) 
        {
            free(requests[i]);
        }
    }

    return 0;
}

/* first try of my code wrong code!!
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define MAX_PRODUCTS 100
#define MAX_CUSTOMERS 100
#define MAX_REQUESTS 1000

typedef struct 
{
    int customer_id;
    int product_id;
    int quantity;
} ProductRequest;

int product_stock[MAX_PRODUCTS];                  //ürünlerin güncel stok miktarı
pthread_mutex_t stock_mutex[MAX_PRODUCTS];        //her ürün için ayrı mutex

int max_concurrent_payments;                      //aynı anda en fazla ödeme yapabilecek müşteri 
int current_payments = 0;                         //şu an ödeme yapan müşteri sayısı
pthread_mutex_t payment_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t payment_cond = PTHREAD_COND_INITIALIZER;

int reservation_timeout_ms;                       
FILE *log_file;                                   
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

ProductRequest* requests[MAX_REQUESTS];  //istekler burada tutulur
int request_count = 0;

int num_customers;
int num_products;

int parse_input_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("failed to open input.txt");
        return -1;
    }

    char line[256];
    int line_num = 0;

    while(fgets(line, sizeof(line), file)) 
        {
            if(line[0] == '\n' || line[0] == '\r' || strlen(line) <= 1) 
            {
                requests[request_count++] = NULL;
                continue;
            }

            //system params.
            if(line_num == 0) 
            {
                sscanf(line, "num_customers=%d", &num_customers);
            }else if(line_num == 1) 
            {
                sscanf(line, "num_products=%d", &num_products);
            } else if(line_num == 2) 
            {
                sscanf(line, "reservation_timeout_ms=%d", &reservation_timeout_ms);
            } else if(line_num == 3) 
            {
                sscanf(line, "max_concurrent_payments=%d", &max_concurrent_payments);
            } else if (line_num == 4) 
            {
                char *token = strtok(line + strlen("initial_stock="), ",");
                int i = 0;
                while(token != NULL && i < num_products) 
                {
                    product_stock[i] = atoi(token);
                    pthread_mutex_init(&stock_mutex[i], NULL);
                    token = strtok(NULL, ",");
                    i++;
                }
            }
            else
            {
                ProductRequest *req = malloc(sizeof(ProductRequest));
                if(!req) 
                {
                    perror("malloc failed");
                    fclose(file);
                    return -1;
                }
                sscanf(line, "%d,%d,%d", &req->customer_id, &req->product_id, &req->quantity);
                requests[request_count++] = req;
            }
            line_num++;
    }

    fclose(file);
    return 0;
}

void* handle_request(void *arg) {
    ProductRequest *req = (ProductRequest *)arg;
    int id = req->customer_id;
    int pid = req->product_id;
    int qty = req->quantity;

    time_t now;
    int reserved = 0;

    // Ürün rezervasyonu
    pthread_mutex_lock(&stock_mutex[pid]);
    if (product_stock[pid] >= qty) {
        product_stock[pid] -= qty;
        reserved = 1;
    }
    pthread_mutex_unlock(&stock_mutex[pid]);

    // Log rezervasyon
    pthread_mutex_lock(&log_mutex);
    now = time(NULL);
    if (reserved) {
        fprintf(log_file, "[%ld] Customer %d tried to add product %d ( qty : %d) to cart | Stock : [ ", now, id, pid, qty);
    } else {
        fprintf(log_file, "[%ld] Customer %d tried to add product %d ( qty : %d) but only %d units were available | Stock : [ ", now, id, pid, qty, product_stock[pid]);
    }
    for (int i = 0; i < num_products; i++) {
        fprintf(log_file, "product %d: %d", i, product_stock[i]);
        if (i < num_products - 1) fprintf(log_file, ", ");
    }
    fprintf(log_file, "] %s\n", reserved ? "// succeed" : "// failed");
    fflush(log_file);
    pthread_mutex_unlock(&log_mutex);

    if (!reserved) return NULL;

    srand(time(NULL) + id + pid);
    int will_purchase = rand() % 2;

    if (!will_purchase) {
        usleep(reservation_timeout_ms * 1000);
        pthread_mutex_lock(&stock_mutex[pid]);
        product_stock[pid] += qty;
        pthread_mutex_unlock(&stock_mutex[pid]);

        pthread_mutex_lock(&log_mutex);
        now = time(NULL);
        fprintf(log_file, "[%ld] Customer %d could not purchase product %d ( qty : %d) in time . Timeout is expired !!! Product %d ( qty : %d) returned to the stock ! | Stock : [ ",
                now, id, pid, qty, pid, qty);
        for (int i = 0; i < num_products; i++) {
            fprintf(log_file, "product %d: %d", i, product_stock[i]);
            if (i < num_products - 1) fprintf(log_file, ", ");
        }
        fprintf(log_file, "]\n");
        fflush(log_file);
        pthread_mutex_unlock(&log_mutex);
        return NULL;
    }

    int purchased = 0;
    int waited_ms = 0;
    int check_interval = 100;

    pthread_mutex_lock(&log_mutex);
    now = time(NULL);
    fprintf(log_file, "[%ld] Customer %d attempted to purchase product %d ( qty : %d ) | Stock : [ ", now, id, pid, qty);
    for (int i = 0; i < num_products; i++) {
        fprintf(log_file, "product %d: %d", i, product_stock[i]);
        if (i < num_products - 1) fprintf(log_file, ", ");
    }
    fprintf(log_file, "]\n");
    fflush(log_file);
    pthread_mutex_unlock(&log_mutex);

    while (waited_ms < reservation_timeout_ms) {
        pthread_mutex_lock(&payment_mutex);
        if (current_payments < max_concurrent_payments) {
            current_payments++;
            pthread_mutex_unlock(&payment_mutex);

            sleep(1);

            pthread_mutex_lock(&log_mutex);
            now = time(NULL);
            fprintf(log_file, "[%ld] Customer %d purchased product %d ( qty : %d ) | Stock : [ ", now, id, pid, qty);
            for (int i = 0; i < num_products; i++) {
                fprintf(log_file, "product %d: %d", i, product_stock[i]);
                if (i < num_products - 1) fprintf(log_file, ", ");
            }
            fprintf(log_file, "]\n");
            fflush(log_file);
            pthread_mutex_unlock(&log_mutex);

            pthread_mutex_lock(&payment_mutex);
            current_payments--;
            pthread_cond_signal(&payment_cond);
            pthread_mutex_unlock(&payment_mutex);

            purchased = 1;
            break;
        } else {
            pthread_mutex_unlock(&payment_mutex);

            pthread_mutex_lock(&log_mutex);
            now = time(NULL);
            fprintf(log_file, "[%ld] Customer %d couldn 't purchase product %d ( qty : %d ) and had to wait ! ( maximum number of concurrent payments reached !)\n",
                    now, id, pid, qty);
            fflush(log_file);
            pthread_mutex_unlock(&log_mutex);

            usleep(check_interval * 1000);
            waited_ms += check_interval;

            // retry log
            pthread_mutex_lock(&log_mutex);
            now = time(NULL);
            fprintf(log_file, "[%ld] Customer %d ( automatically ) retried purchasing product %d ( qty : %d ) | Stock : [ ",
                    now, id, pid, qty);
            for (int i = 0; i < num_products; i++) {
                fprintf(log_file, "product %d: %d", i, product_stock[i]);
                if (i < num_products - 1) fprintf(log_file, ", ");
            }
            fprintf(log_file, "] //( checked for available cashier slot before timeout expired .)\n");
            fflush(log_file);
            pthread_mutex_unlock(&log_mutex);
        }
    }

    if (!purchased) {
        pthread_mutex_lock(&stock_mutex[pid]);
        product_stock[pid] += qty;
        pthread_mutex_unlock(&stock_mutex[pid]);

        pthread_mutex_lock(&log_mutex);
        now = time(NULL);
        fprintf(log_file, "[%ld] Customer %d could not purchase product %d ( qty : %d ) in time . Timeout is expired !!! Product %d ( qty : %d ) returned to the stock ! | Stock : [ ",
                now, id, pid, qty, pid, qty);
        for (int i = 0; i < num_products; i++) {
            fprintf(log_file, "product %d: %d", i, product_stock[i]);
            if (i < num_products - 1) fprintf(log_file, ", ");
        }
        fprintf(log_file, "]\n");
        fflush(log_file);
        pthread_mutex_unlock(&log_mutex);
    }

    return NULL;
}


int main() {
    if (parse_input_file("input.txt") != 0) {
        return 1;
    }

    // Log dosyasını aç
    log_file = fopen("log.txt", "w");
    if (!log_file) {
        perror("Failed to open log.txt");
        return 1;
    }

    // Başlangıç stok durumunu logla
    pthread_mutex_lock(&log_mutex);
    time_t now = time(NULL);
    fprintf(log_file, "[%ld] Initial Stock : ", now);
    for (int i = 0; i < num_products; i++) {
        fprintf(log_file, "product %d: %d", i, product_stock[i]);
        if (i < num_products - 1) fprintf(log_file, " , ");
    }
    fprintf(log_file, "\n");
    fflush(log_file);
    pthread_mutex_unlock(&log_mutex);

    // Müşteri isteklerini grup grup işleme
    pthread_t thread_group[MAX_CUSTOMERS];
    int thread_index = 0;

    for (int i = 0; i < request_count; i++) {
        if (requests[i] == NULL || i == request_count - 1) {
            // Grup sonuna geldik (veya son eleman)
            int group_size = thread_index;

            // Tüm thread'leri başlat
            for (int j = 0; j < group_size; j++) {
                pthread_create(&thread_group[j], NULL, handle_request, (void *)requests[i - group_size + j]);
            }

            // Tüm thread'leri bekle
            for (int j = 0; j < group_size; j++) {
                pthread_join(thread_group[j], NULL);
            }

            thread_index = 0; // Yeni grup için sıfırla
        } else {
            thread_index++;
        }
    }

    fclose(log_file);
    return 0;
}


*/