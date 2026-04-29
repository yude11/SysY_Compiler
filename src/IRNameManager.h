#pragma once

#include <unordered_map>
#include <string>
#include <iostream>

// IR生成过程中符号命名管理器
class IRNameManager {
public:
    // 具名符号：对应源码中的变量/函数/标号
    // 全局唯一，避免跨作用域冲突
    std::string AllocateNamed(const std::string& ident) {
        int count = named_counter[ident]++;
        std::string body = (count == 0) ? ident : ident + "_" + std::to_string(count);
        return "@" + body;
    }

    // 临时符号：对应编译器生成的值/标号
    // 可以在每个函数内独立计数（因为临时值不会跨函数引用）
    // 也可以全局计数，看个人喜好
    std::string AllocateTemp() {
        return "%" + std::to_string(temp_counter++);
    }

    // 带提示的临时符号（调试用）
    std::string AllocateTempHint(const std::string& hint) {
        int count = temp_hint_counters[hint]++;
        return "%" + ((count == 0) ? hint : hint + "_" + std::to_string(count));
    }

    // 标号符号：用于生成基本块的标签
    std::string AllocateLabel(const std::string& hint) {
        int count = label_counter[hint]++;
        return (count == 0) ? hint : hint + "_" + std::to_string(count);
    }

    int GetLabelCount(const std::string& hint) {
        return label_counter[hint];
    }

    // 每个函数开始时重置临时计数器
    // 具名计数器永不重置（保证全局唯一）
    void EnterFunction() {
        temp_counter = 0;
        temp_hint_counters.clear();
    }

    void ResetAll() {
        named_counter.clear();
        temp_counter = 0;
        temp_hint_counters.clear();
    }

private:
    std::unordered_map<std::string, int> named_counter;  // 具名符号全局唯一
    int temp_counter = 0;                                  // 临时符号计数
    std::unordered_map<std::string, int> temp_hint_counters;
    std::unordered_map<std::string, int> label_counter; // 标号计数
};