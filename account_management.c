#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define MAX_ACCOUNTS 10
#define ACCOUNT_FILE "accounts.txt"
#define TRANSACTION_LOG_FILE "transaction_log.txt"

typedef struct {
    int id;
    double balance;
    pthread_mutex_t lock;          
} Account;

Account accounts[MAX_ACCOUNTS];
int account_count = 0;
pthread_mutex_t file_lock; 


const char *get_current_time() {
    static char buffer[20];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return buffer;
}


void load_accounts() {
    FILE *file = fopen(ACCOUNT_FILE, "r");
    if (file == NULL) {
        printf("No existing account file found. Starting fresh.\n");
        return;
    }

    while (fscanf(file, "%d %lf\n", &accounts[account_count].id, &accounts[account_count].balance) != EOF) {
        pthread_mutex_init(&accounts[account_count].lock, NULL);
        account_count++;
    }
    fclose(file);
}

void save_accounts() {
    pthread_mutex_lock(&file_lock);
    FILE *file = fopen(ACCOUNT_FILE, "w");
    if (file == NULL) {
        fprintf(stderr, "Unable to open file for saving accounts.\n");
        pthread_mutex_unlock(&file_lock);
        return;
    }

    for (int i = 0; i < account_count; i++) {
        fprintf(file, "%d %.2f\n", accounts[i].id, accounts[i].balance);
    }
    fclose(file);
    pthread_mutex_unlock(&file_lock);
}


void log_transaction(const char *operation, double amount, int from_account_id, int to_account_id, const char *status) {
    pthread_mutex_lock(&file_lock);
    FILE *log_file = fopen(TRANSACTION_LOG_FILE, "a");
    if (log_file == NULL) {
        fprintf(stderr, "Unable to open transaction log file.\n");
        pthread_mutex_unlock(&file_lock);
        return;
    }

    fprintf(log_file, "Time: %s, Operation: %s, Amount: %.2f, From Account ID: %d, To Account ID: %d, Status: %s\n",
            get_current_time(), operation, amount, from_account_id, to_account_id, status);
    fclose(log_file);
    pthread_mutex_unlock(&file_lock);
}

// Function to create an account
void create_account(double initial_balance) {
    if (account_count < MAX_ACCOUNTS) {
        pthread_mutex_lock(&accounts[account_count].lock);
        Account new_account;
        new_account.id = account_count;
        new_account.balance = initial_balance;
        pthread_mutex_init(&new_account.lock, NULL);
        accounts[account_count] = new_account;
        account_count++;
        pthread_mutex_unlock(&accounts[account_count - 1].lock);

        log_transaction("Create Account", initial_balance, new_account.id, -1, "Successful");

        printf("Account created: ID = %d, Balance = %.2f\n", new_account.id, new_account.balance);
        save_accounts(); // Save to file
    } else {
        printf("Maximum account limit reached!\n");
    }
}

// Function to deposit
void deposit(int account_id, double amount) {
    pthread_mutex_lock(&accounts[account_id].lock);
    accounts[account_id].balance += amount;
    pthread_mutex_unlock(&accounts[account_id].lock);

    log_transaction("Deposit", amount, account_id, -1, "Successful");

    printf("Deposited: %.2f to Account ID = %d\n", amount, account_id);
    save_accounts(); // Save to file
}

// Function to withdraw
void withdraw(int account_id, double amount) {
    pthread_mutex_lock(&accounts[account_id].lock);
    if (accounts[account_id].balance >= amount) {
        accounts[account_id].balance -= amount;
        pthread_mutex_unlock(&accounts[account_id].lock);

        log_transaction("Withdraw", amount, account_id, -1, "Successful");

        printf("Withdrawn: %.2f from Account ID = %d\n", amount, account_id);
    } else {
        pthread_mutex_unlock(&accounts[account_id].lock);
        log_transaction("Withdraw", amount, account_id, -1, "Failed: Insufficient balance");
        printf("Insufficient balance in Account ID = %d\n", account_id);
    }
    save_accounts(); // Save to file
}

// Function to transfer funds
void transfer_funds(int from_account_id, int to_account_id, double amount) {
    if (from_account_id < to_account_id) {
        pthread_mutex_lock(&accounts[from_account_id].lock);
        pthread_mutex_lock(&accounts[to_account_id].lock);
    } else {
        pthread_mutex_lock(&accounts[to_account_id].lock);
        pthread_mutex_lock(&accounts[from_account_id].lock);
    }

    // Start atomic transaction
    if (accounts[from_account_id].balance >= amount) {
        // Perform transaction
        accounts[from_account_id].balance -= amount;
        accounts[to_account_id].balance += amount;

        log_transaction("Transfer", amount, from_account_id, to_account_id, "Successful");

        printf("Transferred: %.2f from Account ID = %d to Account ID = %d\n", amount, from_account_id, to_account_id);
    } else {
        // Rollback
        log_transaction("Transfer", amount, from_account_id, to_account_id, "Failed: Insufficient balance");
        printf("Insufficient balance in Account ID = %d\n", from_account_id);
    }

    pthread_mutex_unlock(&accounts[to_account_id].lock);
    pthread_mutex_unlock(&accounts[from_account_id].lock);
    save_accounts(); // Save to file
}

// Function to display transaction history
void display_transaction_history() {
    char line[256];
    FILE *log_file = fopen(TRANSACTION_LOG_FILE, "r");
    if (log_file == NULL) {
        printf("No transaction log found.\n");
        return;
    }

    printf("Transaction History:\n");
    while (fgets(line, sizeof(line), log_file)) {
        printf("%s", line);
    }
    fclose(log_file);
}

// User interface
void user_interface() {
    int choice, account_id, to_account_id;
    double amount;

    while (1) {
        printf("\n1. Create Account\n");
        printf("2. Check Balance\n");
        printf("3. Deposit\n");
        printf("4. Withdraw\n");
        printf("5. Transfer Funds\n");
        printf("6. Display Transaction History\n");
        printf("7. Exit\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);

        switch (choice) {
            case 1:
                printf("Enter initial balance: ");
                scanf("%lf", &amount);
                create_account(amount);
                break;
            case 2:
                printf("Enter account ID: ");
                scanf("%d", &account_id);
                printf("Balance: %.2f\n", accounts[account_id].balance);
                break;
            case 3:
                printf("Enter account ID and amount to deposit: ");
                scanf("%d %lf", &account_id, &amount);
                deposit(account_id, amount);
                break;
            case 4:
                printf("Enter account ID and amount to withdraw: ");
                scanf("%d %lf", &account_id, &amount);
                withdraw(account_id, amount);
                break;
            case 5:
                printf("Enter from account ID, to account ID and amount to transfer: ");
                scanf("%d %d %lf", &account_id, &to_account_id, &amount);
                transfer_funds(account_id, to_account_id, amount);
                break;
            case 6:
                display_transaction_history();
                break;
            case 7:
                save_accounts(); // Save on exit
                exit(0);
            default:
                printf("Invalid choice! Please try again.\n");
        }
    }
}

// Main function
int main() {
    pthread_mutex_init(&file_lock, NULL); // Initialize the file mutex lock
    load_accounts(); // Load existing accounts from file
    user_interface();
    return 0;
}