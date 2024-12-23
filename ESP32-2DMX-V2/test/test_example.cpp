#include <Arduino.h>
#include <unity.h>
//#include "../src/web/WebServer.h"


// 你要测试的函数
int add(int a, int b) {
    return a + b;
}

// 编写测试函数
void test_add() {
    TEST_ASSERT_EQUAL(4, add(2, 2));
    TEST_ASSERT_EQUAL(0, add(2, -2));
}

// 设置和拆解函数
void setUp() {
    // 在每个测试用例运行前调用
}

void tearDown() {
    // 在每个测试用例运行后调用
}

// 主函数，Unity 测试框架会从这里开始执行
void setup() {
    UNITY_BEGIN();    // 开始 Unity 测试
    RUN_TEST(test_add);  // 运行测试
    UNITY_END();      // 结束测试
}

void loop() {
    // 在 Unity 测试中，不需要在 loop 中做任何事情
}
