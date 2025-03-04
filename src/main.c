/**
 * @file main.c
 * @brief Entry point of the system
 */
#include "main.h"
#include "memory.h"
pid_t child_pids[MAX_PROCESSES];
pid_t foreground_pid = NO_FOREGROUND_PID;
/**
 * @brief pid of the suspended process
 */
pid_t suspended_pid = INVALID_PID;
int num_child_pids = 0;
int job_id = 0;
bool cpu_enabled = false;
bool memory_enabled = false;
bool battery_enabled = false;
bool avg_load_enabled = false;
bool cpu_temp_enabled = false;
bool cpu_speed_enabled = false;
bool processes_enabled = false;
bool sys_calls_enabled = false;
bool disk_io_enabled = false;
bool network_enabled = false;
char config_file_path[PATH_MAX];
int sleep_time;
/**
 * @brief pid of the monitor process
 */
pid_t monitor_pid = NO_MONITOR_PID;

#ifndef TESTING

/**
 * @brief Main entry point of the program.
 */
int main(int argc, char* argv[])
{
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        char parent_dir[PATH_MAX];
        strncpy(parent_dir, cwd, sizeof(parent_dir));
        parent_dir[sizeof(parent_dir) - 1] = '\0';
        char* parent = dirname(parent_dir);
        explore_and_read_config_files(parent, ".json");
    }
    else
    {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }
    printf("config file path: %s\n", config_file_path);
    printf(getenv("PWD"));
    show_config();
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    signal(SIGINT, handle_signal);
    signal(SIGTSTP, handle_signal);
    signal(SIGQUIT, handle_signal);

    if (argc == 2)
    {
        // Batch mode
        FILE* file = fopen(argv[1], "r");
        if (file == NULL)
        {
            perror("fopen");
            exit(EXIT_FAILURE);
        }
        char command[1024];
        while (fgets(command, sizeof(command), file) != NULL)
        {
            execute_command(command);
        }
        fclose(file);
    }
    else
    {
        // Interactive mode

        while (1)
        {
            print_prompt();
            char command[MAX_COMMAND_LENGTH];
            if (fgets(command, sizeof(command), stdin) == NULL)
            {
                break; // Exit on EOF
            }
            execute_command(command);
        }
    }
    return 0;
}
#endif

void print_prompt()
{
    char hostname[HOST_NAME_MAX];
    char cwd[PATH_MAX];
    struct passwd* pw = getpwuid(getuid());

    gethostname(hostname, sizeof(hostname));
    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }

    // ANSI escape codes for colors
    const char* user_color = USER_COLOR;
    const char* pwd_color = PWD_COLOR;
    const char* reset_color = BASE_COLOR;

    printf("%s%s@%s:%s%s%s$ ", user_color, pw->pw_name, hostname, pwd_color, cwd, reset_color);
}

void execute_cd(char* args[])
{
    if (args[1] == NULL || strcmp(args[1], "~") == 0)
    {
        // Change to home directory
        const char* home = getenv("HOME");
        if (home == NULL)
        {
            home = getpwuid(getuid())->pw_dir;
        }
        if (chdir(home) != 0)
        {
            perror("cd");
        }
    }
    else if (strcmp(args[1], "-") == 0)
    {
        // Change to previous directory
        const char* oldpwd = getenv("OLDPWD");
        if (oldpwd != NULL)
        {
            printf("%s\n", oldpwd);
            if (chdir(oldpwd) != 0)
            {
                perror("cd");
            }
        }
        else
        {
            fprintf(stderr, "cd: OLDPWD not set\n");
        }
    }
    else
    {
        // Change to specified directory
        if (chdir(args[1]) != 0)
        {
            perror("cd");
        }
    }
    // Update PWD and OLDPWD
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }
    setenv("OLDPWD", getenv("PWD"), OVERWRITE);
    setenv("PWD", cwd, OVERWRITE);
}

void execute_clr()
{
    printf("\033[H\033[J");
}

void execute_echo(char* args[], char* output_file, int background)
{
    if (background)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            if (output_file != NULL)
            {
                int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, FILE_PERMISSIONS);
                if (fd < 0)
                {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            for (int j = 1; args[j] != NULL; j++)
            {
                if (args[j][0] == '$')
                {
                    printf("%s ", getenv(args[j] + 1));
                }
                else
                {
                    printf("%s ", args[j]);
                }
            }
            printf("\n");
            exit(EXIT_SUCCESS);
        }
        else if (pid > 0)
        {
            add_child_pid(pid);
            printf("[%d] %d\n", num_child_pids, pid);
        }
        else
        {
            perror("fork");
        }
    }
    else
    {
        int saved_stdout = dup(STDOUT_FILENO);
        if (output_file != NULL)
        {
            int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, FILE_PERMISSIONS);
            if (fd < 0)
            {
                perror("open");
                return;
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        for (int j = 1; args[j] != NULL; j++)
        {
            if (args[j][0] == '$')
            {
                printf("%s ", getenv(args[j] + 1));
            }
            else
            {
                printf("%s ", args[j]);
            }
        }
        printf("\n");
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }
}

void execute_quit()
{
    kill_all_child_processes();
    exit(EXIT_SUCCESS);
}

void execute_fg()
{
    if (suspended_pid > 0)
    {
        pid_t pid = suspended_pid;
        suspended_pid = SUSPENDED_PID;
        foreground_pid = pid;
        kill(pid, SIGCONT);
        int status;
        waitpid(pid, &status, WUNTRACED);
        if (WIFSTOPPED(status))
        {
            printf("\n[%d] %d suspended\n", num_child_pids, pid);
            suspended_pid = pid;
        }
        foreground_pid = NO_FOREGROUND_PID;
    }
    else
    {
        printf("No suspended process to resume\n");
    }
}

void execute_external_command(char* args[], char* input_file, char* output_file, int background)
{
    if (!command_exists(args[0]))
    {
        fprintf(stderr, "Executable not found or not executable: %s\n", args[0]);
        return;
    }

    pid_t pid = fork();
    if (pid == 0)
    {
        // Restore default signal handling in the child process
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);

        if (input_file != NULL)
        {
            int fd = open(input_file, O_RDONLY);
            if (fd < 0)
            {
                perror("open");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        if (output_file != NULL)
        {
            int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, FILE_PERMISSIONS);
            if (fd < 0)
            {
                perror("open");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        execvp(args[0], args);
        perror("execvp");
        exit(EXIT_FAILURE);
    }
    else if (pid > 0)
    {
        if (background)
        {
            add_child_pid(pid);
            printf("[%d] %d\n", num_child_pids, pid);
        }
        else
        {
            foreground_pid = pid;
            add_child_pid(pid);
            int status;
            waitpid(pid, &status, WUNTRACED); // Wait for the process to change state
            if (WIFSTOPPED(status))
            {
                printf("\n[%d] %d suspended\n", num_child_pids, pid);
                suspended_pid = pid;
            }
            foreground_pid = NO_FOREGROUND_PID;
        }
    }
    else
    {
        perror("fork");
    }
}

void execute_command(char* command)
{
    char* args[MAX_COMMAND_LENGTH];
    char* token;
    int i = 0;
    int background = 0;
    int num_pipes = 0;
    char* commands[MAX_COMMAND_LENGTH];
    char* input_file = NULL;
    char* output_file = NULL;

    token = strtok(command, "|");
    while (token != NULL)
    {
        commands[num_pipes++] = token;
        token = strtok(NULL, "|");
    }
    commands[num_pipes] = NULL;

    if (num_pipes == 1)
    {
        token = strtok(commands[0], " \n");
        while (token != NULL)
        {
            if (strcmp(token, "<") == 0)
            {
                token = strtok(NULL, " \n");
                if (token != NULL)
                {
                    input_file = token;
                }
            }
            else if (strcmp(token, ">") == 0)
            {
                token = strtok(NULL, " \n");
                if (token != NULL)
                {
                    output_file = token;
                }
            }
            else
            {
                args[i++] = token;
            }
            token = strtok(NULL, " \n");
        }
        args[i] = NULL;

        if (i > 0 && strcmp(args[i - 1], "&") == 0)
        {
            background = 1;
            args[i - 1] = NULL; // Remove '&' from arguments
        }

        if (args[0] == NULL)
        {
            return; // Empty command
        }

        if (strcmp(args[0], "cd") == 0)
        {
            execute_cd(args);
        }
        else if (strcmp(args[0], "clr") == 0)
        {
            execute_clr();
        }
        else if (strcmp(args[0], "echo") == 0)
        {
            execute_echo(args, output_file, background);
        }
        else if (strcmp(args[0], "quit") == 0)
        {
            execute_quit();
        }
        else if (strcmp(args[0], "fg") == 0)
        {
            execute_fg();
        }
        else if (strcmp(args[0], "start_monitor") == 0)
        {
            start_monitor();
        }
        else if (strcmp(args[0], "stop_monitor") == 0)
        {
            stop_monitor();
        }
        else if (strcmp(args[0], "status_monitor") == 0)
        {
            status_monitor();
        }
        else if (strcmp(args[0], "update_interval") == 0 && args[1])
        {
            update_interval(args[1]);
        }
        else if (strcmp(args[0], "add_metric") == 0 && args[1])
        {
            add_metric(args[1]);
        }
        else if (strcmp(args[0], "remove_metric") == 0 && args[1])
        {
            remove_metric(args[1]);
        }
        else if (strcmp(args[0], "show_config") == 0)
        {
            show_config();
        }
        else
        {
            execute_external_command(args, input_file, output_file, background);
        }
    }
    else
    {
        // Handle pipes
        execute_piped_commands(commands, num_pipes);
    }
}

int command_exists(char* command)
{
    if (access(command, X_OK) == 0)
    {
        return 1;
    }

    char* path = getenv("PATH");
    if (path == NULL)
    {
        return 0;
    }

    char* path_dup = strdup(path);
    char* dir = strtok(path_dup, ":");
    while (dir != NULL)
    {
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, command);
        if (access(full_path, X_OK) == 0)
        {
            free(path_dup);
            return 1;
        }
        dir = strtok(NULL, ":");
    }
    free(path_dup);
    return 0;
}

void add_child_pid(pid_t pid)
{
    if (num_child_pids < MAX_PROCESSES)
    {
        child_pids[num_child_pids++] = pid;
    }
    else
    {
        fprintf(stderr, "Max number of child processes reached\n");
    }
}

void kill_all_child_processes()
{
    for (int i = 0; i < num_child_pids; i++)
    {
        kill(child_pids[i], SIGKILL);
    }
    num_child_pids = 0;
    if (monitor_pid > 0)
    {
        stop_monitor();
    }
}

void handle_signal(int signal)
{
    if (foreground_pid > 0)
    {
        kill(foreground_pid, signal);
    }
    if (signal == SIGTSTP)
    {
        if (foreground_pid > 0)
        {
            suspended_pid = foreground_pid;
            foreground_pid = NO_FOREGROUND_PID;
        }
    }
}

void execute_piped_commands(char* commands[], int num_pipes)
{
    int pipefds[2 * (num_pipes - 1)];
    for (int i = 0; i < num_pipes - 1; i++)
    {
        if (pipe(pipefds + i * 2) < 0)
        {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    int j = 0;
    for (int i = 0; i < num_pipes; i++)
    {
        char* cmd = commands[i];
        char* cmd_args[100];
        int k = 0;
        char* token = strtok(cmd, " \n");
        char* input_file = NULL;
        char* output_file = NULL;

        while (token != NULL)
        {
            if (strcmp(token, "<") == 0)
            {
                token = strtok(NULL, " \n");
                if (token != NULL)
                {
                    input_file = token;
                }
            }
            else if (strcmp(token, ">") == 0)
            {
                token = strtok(NULL, " \n");
                if (token != NULL)
                {
                    output_file = token;
                }
            }
            else
            {
                cmd_args[k++] = token;
            }
            token = strtok(NULL, " \n");
        }
        cmd_args[k] = NULL;

        pid_t pid = fork();
        if (pid == 0)
        {
            // Child process
            if (i < num_pipes - 1)
            {
                if (dup2(pipefds[j + 1], STDOUT_FILENO) < 0)
                {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }
            if (j != 0)
            {
                if (dup2(pipefds[j - 2], STDIN_FILENO) < 0)
                {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }

            if (input_file != NULL && i == 0)
            {
                int fd = open(input_file, O_RDONLY);
                if (fd < 0)
                {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            if (output_file != NULL && i == num_pipes - 1)
            {
                int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0)
                {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            for (int l = 0; l < 2 * (num_pipes - 1); l++)
            {
                close(pipefds[l]);
            }

            if (execvp(cmd_args[0], cmd_args) < 0)
            {
                perror("execvp");
                exit(EXIT_FAILURE);
            }
        }
        else if (pid < 0)
        {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        j += 2;
    }

    for (int l = 0; l < 2 * (num_pipes - 1); l++)
    {
        close(pipefds[l]);
    }

    for (int i = 0; i < num_pipes; i++)
    {
        wait(NULL);
    }
}

void start_monitor()
{
    if (monitor_pid > 0)
    {
        printf("Monitor already running\n");
        return;
    }
    monitor_pid = fork();
    if (monitor_pid == 0)
    {
        printf("config file path: %s", config_file_path);
        execl("shell/metricas/metrics_executable", "metrics_executable", config_file_path, (char*)NULL);
        perror("execl");
        exit(EXIT_FAILURE);
    }
    else if (monitor_pid < 0)
    {
        perror("fork");
    }
    else
    {
        printf("Monitor started with PID %d\n", monitor_pid);
        printf("Configuration: ");
        show_config();
    }
}

void stop_monitor()
{
    if (monitor_pid > 0)
    {
        kill(monitor_pid, SIGTERM);
        waitpid(monitor_pid, NULL, 0);
        printf("Stopped monitoring program with PID %d\n", monitor_pid);
        monitor_pid = NO_MONITOR_PID;
    }
    else
    {
        printf("No monitoring program is running\n");
    }
}

void status_monitor()
{
    if (monitor_pid > 0)
    {
        printf("Monitoring program is running with PID %d\n", monitor_pid);
    }
    else
    {
        printf("No monitoring program is running\n");
    }
}

void read_config()
{
    printf("config file path: %s\n", config_file_path);
    if (config_file_path == NULL)
    {
        printf("Environment variable CONFIG_FILE_PATH is not set.\n");
        fprintf(stderr, "Environment variable CONFIG_FILE_PATH is not set.\n");
        exit(EXIT_FAILURE);
    }
    FILE* file = fopen(config_file_path, "r");
    if (file == NULL)
    {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* data = malloc_(length + 1);
    if (data == NULL)
    {
        perror("malloc");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    size_t read_size = fread(data, 1, length, file);
    if (read_size != length)
    {
        perror("fread");
        free(data);
        fclose(file);
        exit(EXIT_FAILURE);
    }
    data[length] = '\0'; // Null-terminate the string

    fclose(file);
    cJSON* json = cJSON_Parse(data);
    if (!json)
    {
        fprintf(stderr, "Error parsing JSON: %s\n", cJSON_GetErrorPtr());
        free(data);
        return;
    }

    cJSON* interval = cJSON_GetObjectItem(json, "sampling_interval");
    if (cJSON_IsNumber(interval))
    {
        sleep_time = interval->valueint;
    }
    cpu_enabled = false;
    memory_enabled = false;
    battery_enabled = false;
    avg_load_enabled = false;
    cpu_temp_enabled = false;
    cpu_speed_enabled = false;
    processes_enabled = false;
    sys_calls_enabled = false;
    disk_io_enabled = false;
    network_enabled = false;
    cJSON* metrics = cJSON_GetObjectItem(json, "metrics");
    if (cJSON_IsArray(metrics))
    {
        cJSON* metric;
        cJSON_ArrayForEach(metric, metrics)
        {
            if (cJSON_IsString(metric))
            {
                if (strcmp(metric->valuestring, "cpu") == 0)
                {
                    cpu_enabled = true;
                }
                else if (strcmp(metric->valuestring, "memory") == 0)
                {
                    memory_enabled = true;
                }
                else if (strcmp(metric->valuestring, "battery") == 0)
                {
                    battery_enabled = true;
                }
                else if (strcmp(metric->valuestring, "avg_load") == 0)
                {
                    avg_load_enabled = true;
                }
                else if (strcmp(metric->valuestring, "cpu_temp") == 0)
                {
                    cpu_temp_enabled = true;
                }
                else if (strcmp(metric->valuestring, "cpu_speed") == 0)
                {
                    cpu_speed_enabled = true;
                }
                else if (strcmp(metric->valuestring, "processes") == 0)
                {
                    processes_enabled = true;
                }
                else if (strcmp(metric->valuestring, "sys_calls") == 0)
                {
                    sys_calls_enabled = true;
                }
                else if (strcmp(metric->valuestring, "disk") == 0)
                {
                    disk_io_enabled = true;
                }
                else if (strcmp(metric->valuestring, "network") == 0)
                {
                    network_enabled = true;
                }
            }
        }
    }
    cJSON_Delete(json);
    free_(data);
}

void update_interval(const char* interval)
{
    FILE* file = fopen(config_file_path, "r+");
    if (file == NULL)
    {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* data = malloc_(length + 1);
    if (data == NULL)
    {
        perror("malloc");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    size_t read_size = fread(data, 1, length, file);
    if (read_size != length)
    {
        perror("fread");
        free_(data);
        fclose(file);
        exit(EXIT_FAILURE);
    }
    data[length] = '\0'; // Null-terminate the string

    cJSON* json = cJSON_Parse(data);
    if (!json)
    {
        fprintf(stderr, "Error parsing JSON: %s\n", cJSON_GetErrorPtr());
        free_(data);
        fclose(file);
        return;
    }

    cJSON* interval_item = cJSON_GetObjectItem(json, "sampling_interval");
    if (interval_item)
    {
        cJSON_SetNumberValue(interval_item, atoi(interval));
    }
    else
    {
        cJSON_AddNumberToObject(json, "sampling_interval", atoi(interval));
    }

    char* updated_data = cJSON_Print(json);
    if (freopen(config_file_path, "w", file) == NULL)
    {
        perror("freopen");
        cJSON_Delete(json);
        free_(data);
        free(updated_data);
        fclose(file);
        exit(EXIT_FAILURE);
    }
    fwrite(updated_data, 1, strlen(updated_data), file);

    cJSON_Delete(json);
    free_(data);
    free(updated_data);
    fclose(file);
    show_config();
}

void add_metric(const char* metric)
{
    FILE* file = fopen(config_file_path, "r+");
    if (file == NULL)
    {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* data = malloc_(length + 1);
    if (data == NULL)
    {
        perror("malloc");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    size_t read_size = fread(data, 1, length, file);
    if (read_size != length)
    {
        perror("fread");
        free_(data);
        fclose(file);
        exit(EXIT_FAILURE);
    }
    data[length] = '\0'; // Null-terminate the string

    cJSON* json = cJSON_Parse(data);
    if (!json)
    {
        fprintf(stderr, "Error parsing JSON: %s\n", cJSON_GetErrorPtr());
        free_(data);
        fclose(file);
        return;
    }

    cJSON* metrics = cJSON_GetObjectItem(json, "metrics");
    if (!metrics)
    {
        metrics = cJSON_AddArrayToObject(json, "metrics");
    }

    cJSON* metric_item;
    cJSON_ArrayForEach(metric_item, metrics)
    {
        if (cJSON_IsString(metric_item) && strcmp(metric_item->valuestring, metric) == 0)
        {
            printf("Metric already exists: %s\n", metric);
            cJSON_Delete(json);
            free(data);
            fclose(file);
            return;
        }
    }

    cJSON_AddItemToArray(metrics, cJSON_CreateString(metric));

    char* updated_data = cJSON_Print(json);
    if (freopen(config_file_path, "w", file) == NULL)
    {
        perror("freopen");
        cJSON_Delete(json);
        free_(data);
        free(updated_data);
        fclose(file);
        return;
    }
    fwrite(updated_data, 1, strlen(updated_data), file);

    cJSON_Delete(json);
    free_(data);
    free(updated_data);
    fclose(file);
    printf("Updated config: \n");
    show_config();
}

void show_config()
{
    read_config();
    cJSON* root = cJSON_CreateObject();
    // Add sampling interval
    cJSON_AddNumberToObject(root, "sampling_interval", sleep_time);
    // Add metrics array
    cJSON* metrics = cJSON_CreateArray();
    if (cpu_enabled)
        cJSON_AddItemToArray(metrics, cJSON_CreateString("cpu"));
    if (memory_enabled)
        cJSON_AddItemToArray(metrics, cJSON_CreateString("memory"));
    if (battery_enabled)
        cJSON_AddItemToArray(metrics, cJSON_CreateString("battery"));
    if (avg_load_enabled)
        cJSON_AddItemToArray(metrics, cJSON_CreateString("avg_load"));
    if (cpu_temp_enabled)
        cJSON_AddItemToArray(metrics, cJSON_CreateString("cpu_temp"));
    if (cpu_speed_enabled)
        cJSON_AddItemToArray(metrics, cJSON_CreateString("cpu_speed"));
    if (processes_enabled)
        cJSON_AddItemToArray(metrics, cJSON_CreateString("processes"));
    if (sys_calls_enabled)
        cJSON_AddItemToArray(metrics, cJSON_CreateString("sys_calls"));
    if (disk_io_enabled)
        cJSON_AddItemToArray(metrics, cJSON_CreateString("disk"));
    if (network_enabled)
        cJSON_AddItemToArray(metrics, cJSON_CreateString("network"));
    cJSON_AddItemToObject(root, "metrics", metrics);

    // Print JSON object
    char* json_string = cJSON_Print(root);

    // if(json_string!=NULL)
    printf("%s\n", json_string);

    // Cleanup
    cJSON_Delete(root);
    free(json_string);
}

void remove_metric(const char* metric)
{
    FILE* file = fopen(config_file_path, "r+");
    if (file == NULL)
    {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    // Read the file content
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* data = malloc_(length + 1);
    if (data == NULL)
    {
        perror("malloc");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    fread(data, 1, length, file);
    data[length] = '\0';

    // Parse JSON
    cJSON* json = cJSON_Parse(data);
    if (json == NULL)
    {
        perror("cJSON_Parse");
        free_(data);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // Remove metric
    cJSON* metrics = cJSON_GetObjectItem(json, "metrics");
    if (metrics == NULL)
    {
        cJSON_Delete(json);
        free_(data);
        fclose(file);
        return;
    }

    cJSON* item = NULL;
    int index = 0;
    cJSON_ArrayForEach(item, metrics)
    {
        if (cJSON_IsString(item) && strcmp(item->valuestring, metric) == 0)
        {
            cJSON_DeleteItemFromArray(metrics, index);
            break;
        }
        index++;
    }

    // Write updated JSON back to file
    char* updated_data = cJSON_Print(json);
    if (updated_data == NULL)
    {
        perror("cJSON_Print");
        cJSON_Delete(json);
        free_(data);
        fclose(file);
        exit(EXIT_FAILURE);
    }
    fseek(file, 0, SEEK_SET);
    fwrite(updated_data, 1, strlen(updated_data), file);
    ftruncate(fileno(file), strlen(updated_data));

    // Clean up
    cJSON_Delete(json);
    free_(data);
    free(updated_data);
    fclose(file);

    printf("Updated config: \n");
    show_config();
}

void explore_and_read_config_files(const char* directory, const char* extension)
{
    struct dirent* entry;
    DIR* dp = opendir(directory);
    // printf("Exploring directory: %s\n", directory);
    if (dp == NULL)
    {
        perror("opendir");
        return;
    }

    while ((entry = readdir(dp)))
    {
        if (entry->d_type == DT_DIR)
        {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
            {
                char path[PATH_MAX];
                snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);
                explore_and_read_config_files(path, extension);
            }
        }
        else if (entry->d_type == DT_REG)
        {
            const char* ext = strrchr(entry->d_name, '.');
            if (ext && strcmp(ext, extension) == 0)
            {
                char file_path[PATH_MAX];
                snprintf(file_path, sizeof(file_path), "%s/%s", directory, entry->d_name);
                printf("Configuration file found: %s\n", file_path);

                FILE* file = fopen(file_path, "r");
                if (file == NULL)
                {
                    perror("fopen");
                    continue;
                }

                printf("Contenido de %s:\n", file_path);
                char line[LINE_MAX];
                while (fgets(line, sizeof(line), file))
                {
                    printf("%s", line);
                }
                fclose(file);

                if (strcmp(entry->d_name, "config.json") == 0)
                {
                    printf("FOUND JSON FILE\n");
                    snprintf(config_file_path, sizeof(config_file_path), "%s/%s", directory, entry->d_name);
                }
            }
        }
    }

    closedir(dp);
}