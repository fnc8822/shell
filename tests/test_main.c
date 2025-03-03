#include "main.h"
#include "unity.h"

// Function declarations
void test_execute_cd_home(void);
void test_execute_cd_invalid(void);
void test_execute_clr(void);
void test_execute_echo(void);
void test_change_interval(void);

void test_remove_metrics()
{
    remove_metric("cpu");
    TEST_ASSERT_FALSE(cpu_enabled);
}
void test_add_metrics()
{
    add_metric("cpu");
    TEST_ASSERT_TRUE(cpu_enabled);
}
void test_change_interval()
{
    update_interval("1");
    read_config();
    TEST_ASSERT_EQUAL(1, sleep_time);
    update_interval("5");
    read_config();
    TEST_ASSERT_EQUAL(5, sleep_time);
}

int main()
{

    UNITY_BEGIN();
    RUN_TEST(test_remove_metrics);
    RUN_TEST(test_add_metrics);
    RUN_TEST(test_change_interval);
    RUN_TEST(test_execute_clr);
    return UNITY_END();
}

void tearDown()
{
}

void setUp()
{
}
void test_execute_cd_home()
{
    char* args[] = {"cd", "-", NULL};
    execute_cd(args);
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)))
    {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }
    TEST_ASSERT_EQUAL_STRING(getenv("HOME"), cwd);
}

void test_execute_cd_invalid()
{
    char* args[] = {"cd", "/invalid_directory", NULL};
    execute_cd(args);
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }
    TEST_ASSERT_NOT_EQUAL("/invalid_directory", cwd);
}

void test_execute_clr()
{
    // Redirect stdout to a buffer
    char buffer[1024];
    FILE* fp = freopen("/dev/null", "w", stdout);
    if (fp == NULL)
    {
        perror("freopen");
        exit(EXIT_FAILURE);
    }
    setbuf(stdout, buffer);
    execute_clr();
    // Check if the buffer contains the clear screen ANSI escape code
    TEST_ASSERT_EQUAL_STRING("\033[H\033[J", buffer);
}
