#include <vector>
#include <numeric>
#include <stdexcept>
#include <string>
#include "unity.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "soc/soc.h"

TEST_CASE("can use new and delete", "[cxx]")
{
    int* int_p = new int(10);
    delete int_p;
    int* int_array = new int[10];
    delete[] int_array;
}

class Base
{
public:
    virtual ~Base() {}
    virtual void foo() = 0;
};

class Derived : public Base
{
public:
    virtual void foo() { }
};

TEST_CASE("can call virtual functions", "[cxx]")
{
    Derived d;
    Base& b = static_cast<Base&>(d);
    b.foo();
}

TEST_CASE("can use std::vector", "[cxx]")
{
    std::vector<int> v(10, 1);
    v[0] = 42;
    TEST_ASSERT_EQUAL(51, std::accumulate(std::begin(v), std::end(v), 0));
}

/* Note: When first exception (in system) is thrown this test produces memory leaks report (~300 bytes):
   - 392 bytes (can vary) as libunwind allocates memory to keep stack frames info to handle exceptions.
     This info is kept until global destructors are called by __do_global_dtors_aux()
   - 8 bytes are allocated by __cxa_get_globals() to keep __cxa_eh_globals
   - 16 bytes are allocated by pthread_setspecific() which is called by __cxa_get_globals() to init TLS var for __cxa_eh_globals
   - 88 bytes are allocated by pthread_setspecific() to init internal lock
   - some more memory...
   */
#ifdef CONFIG_COMPILER_CXX_EXCEPTIONS

TEST_CASE("c++ exceptions work", "[cxx] [exceptions] [leaks=300]")
{
    int thrown_value;
    try {
        throw 20;
    } catch (int e) {
        thrown_value = e;
    }
    TEST_ASSERT_EQUAL(20, thrown_value);
    printf("OK?\n");
}

TEST_CASE("c++ bool exception", "[cxx] [exceptions] [leaks=300]")
{
    bool thrown_value = false;
    try {
        throw true;
    } catch (bool e) {
        thrown_value = e;
    }
    TEST_ASSERT_EQUAL(true, thrown_value);
    printf("OK?\n");
}

TEST_CASE("c++ void exception", "[cxx] [exceptions] [leaks=300]")
{
    void* thrown_value = 0;
    try {
        throw (void*) 47;
    } catch (void* e) {
        thrown_value = e;
    }
    TEST_ASSERT_EQUAL(47, thrown_value);
    printf("OK?\n");
}

TEST_CASE("c++ uint64_t exception", "[cxx] [exceptions] [leaks=300]")
{
    uint64_t thrown_value = 0;
    try {
        throw (uint64_t) 47;
    } catch (uint64_t e) {
        thrown_value = e;
    }
    TEST_ASSERT_EQUAL(47, thrown_value);
    printf("OK?\n");
}

TEST_CASE("c++ char exception", "[cxx] [exceptions] [leaks=300]")
{
    char thrown_value = '0';
    try {
        throw '/';
    } catch (char e) {
        thrown_value = e;
    }
    TEST_ASSERT_EQUAL('/', thrown_value);
    printf("OK?\n");
}

TEST_CASE("c++ wchar exception", "[cxx] [exceptions] [leaks=300]")
{
    wchar_t thrown_value = 0;
    try {
        throw (wchar_t) 47;
    } catch (wchar_t e) {
        thrown_value = e;
    }
    TEST_ASSERT_EQUAL((wchar_t) 47, thrown_value);
    printf("OK?\n");
}

TEST_CASE("c++ float exception", "[cxx] [exceptions] [leaks=300]")
{
    float thrown_value = 0;
    try {
        throw 23.5f;
    } catch (float e) {
        thrown_value = e;
    }
    TEST_ASSERT_EQUAL(23.5, thrown_value);
    printf("OK?\n");
}

TEST_CASE("c++ double exception", "[cxx] [exceptions] [leaks=300]")
{
    double thrown_value = 0;
    try {
        throw 23.5d;
    } catch (double e) {
        thrown_value = e;
    }
    TEST_ASSERT_EQUAL(23.5d, thrown_value);
    printf("OK?\n");
}

TEST_CASE("c++ const char* exception", "[cxx] [exceptions] [leaks=300]")
{
    const char *thrown_value = 0;
    try {
        throw "Hi :)";
    } catch (const char *e) {
        thrown_value = e;
    }
    TEST_ASSERT_EQUAL_STRING("Hi :)", thrown_value);
    printf("OK?\n");
}

struct NonExcTypeThrowee {
    int value;
public:
    NonExcTypeThrowee(int value) : value(value) { }
};

TEST_CASE("c++ any class exception", "[cxx] [exceptions] [leaks=300]")
{
    int thrown_value = 0;
    try {
        throw NonExcTypeThrowee(47);
    } catch (NonExcTypeThrowee &e) {
        thrown_value = e.value;
    }
    TEST_ASSERT_EQUAL(47, thrown_value);
    printf("OK?\n");
}

struct ExcTypeThrowee : public std::exception {
    int value;
public:
    ExcTypeThrowee(int value) : value(value) { }
};

TEST_CASE("c++ std::exception child", "[cxx] [exceptions] [leaks=300]")
{
    int thrown_value = 0;
    try {
        throw ExcTypeThrowee(47);
    } catch (ExcTypeThrowee &e) {
        thrown_value = e.value;
    }
    TEST_ASSERT_EQUAL(47, thrown_value);
    printf("OK?\n");
}

TEST_CASE("c++ exceptions emergency pool", "[cxx] [exceptions] [ignore] [leaks=300]")
{
    void **p, **pprev = NULL;
    int thrown_value = 0;
    // throw first exception to ensure that all initial allocations are made
    try
    {
        throw 33;
    }
    catch (int e)
    {
        thrown_value = e;
    }
    TEST_ASSERT_EQUAL(33, thrown_value);
    // consume all dynamic memory
    while ((p = (void **)malloc(sizeof(void *)))) {
        if (pprev) {
            *p = pprev;
        } else {
            *p = NULL;
        }
        pprev = p;
    }
    try
    {
        throw 20;
    }
    catch (int e)
    {
        thrown_value = e;
        printf("Got exception %d\n", thrown_value);
    }
#if CONFIG_COMPILER_CXX_EXCEPTIONS_EMG_POOL_SIZE > 0
    // free all memory
    while (pprev) {
        p = (void **)(*pprev);
        free(pprev);
        pprev = p;
    }
    TEST_ASSERT_EQUAL(20, thrown_value);
#else
    // if emergency pool is disabled we should never get here,
    // expect abort() due to lack of memory for new exception
    TEST_ASSERT_TRUE(0 == 1);
#endif
}


#define TIMEOUT 19

#define RECURSION 19

static esp_timer_handle_t crash_timer;

static uint32_t result = 0;

uint32_t calc_fac(uint32_t n) {
    if (n == 1 || n == 0) {
        return 1;
    } else {
        return n * calc_fac(n - 1);
    }
}

static void timer_cb(void *arg) {
    result = calc_fac(RECURSION);
}

// TODO: Not a unit test, refactor to integration test/system test, etc.
TEST_CASE("frequent interrupts don't interfere with c++ exceptions", "[cxx] [exceptions] [leaks=800]")
{// if exception workaround is disabled, this is almost guaranteed to fail
    const esp_timer_create_args_t timer_args {
        timer_cb,
        NULL,
        ESP_TIMER_TASK,
        "crash_timer"
    };

    TEST_ESP_OK(esp_timer_create(&timer_args, &crash_timer));
    TEST_ESP_OK(esp_timer_start_periodic(crash_timer, TIMEOUT));

    for (int i = 0; i < 500; i++) {
        bool thrown_value = false;
        try {
            throw true;
        } catch (bool e) {
            thrown_value = e;
        }

        if (thrown_value) {
            printf("ex thrown %d\n", i);
        } else {
            printf("ex not thrown\n");
            TEST_ASSERT(false);
        }
    }

    TEST_ESP_OK(esp_timer_stop(crash_timer));
    TEST_ESP_OK(esp_timer_delete(crash_timer));
}

#else // !CONFIG_COMPILER_CXX_EXCEPTIONS

TEST_CASE("std::out_of_range exception when -fno-exceptions", "[cxx][reset=abort,SW_CPU_RESET]")
{
    std::vector<int> v(10);
    v.at(20) = 42;
    TEST_FAIL_MESSAGE("Unreachable because we are aborted on the line above");
}

TEST_CASE("std::bad_alloc exception when -fno-exceptions", "[cxx][reset=abort,SW_CPU_RESET]")
{
    std::string s = std::string(2000000000, 'a');
    (void)s;
    TEST_FAIL_MESSAGE("Unreachable because we are aborted on the line above");
}

#endif

/* These test cases pull a lot of code from libstdc++ and are disabled for now
 */
#if 0
#include <iostream>
#include <functional>

TEST_CASE("can use iostreams", "[cxx]")
{
    std::cout << "hello world";
}

TEST_CASE("can call std::function and bind", "[cxx]")
{
    int outer = 1;
    std::function<int(int)> fn = [&outer](int x) -> int {
        return x + outer;
    };
    outer = 5;
    TEST_ASSERT_EQUAL(6, fn(1));

    auto bound = std::bind(fn, outer);
    outer = 10;
    TEST_ASSERT_EQUAL(15, bound());
}

#endif

/* Tests below are done in the compile time, don't actually get run. */
/* Check whether a enumerator flag can be used in C++ */


template<typename T> __attribute__((unused)) static void test_binary_operators()
{
    T flag1 = (T)0;
    T flag2 = (T)0;
    flag1 = ~flag1;
    flag1 = flag1 | flag2;
    flag1 = flag1 & flag2;
    flag1 = flag1 ^ flag2;
    flag1 = flag1 >> 2;
    flag1 = flag1 << 2;
    flag1 |= flag2;
    flag1 &= flag2;
    flag1 ^= flag2;
    flag1 >>= 2;
    flag1 <<= 2;
}

//Add more types here. If any flags cannot pass the build, use FLAG_ATTR in esp_attr.h
#include "hal/timer_types.h"
template void test_binary_operators<timer_intr_t>();




