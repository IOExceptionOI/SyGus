#include "istool/sygus/parser/parser.h"
#include "istool/basic/config.h"
#include <string>
#include <iostream>
#include "glog/logging.h"
#include "istool/sygus/theory/basic/clia/clia_example_sampler.h"
#include "istool/basic/example_sampler.h"

// Construct all programs with size 'size' expanded from grammar rule 'rule'.
ProgramList constructPrograms(Rule* rule, int size, const std::vector<std::vector<ProgramList>>& program_storage);

/**
 * Enumerate programs in the grammar from small to large, and return the first program satisfying all examples
 * @param grammar a context-free grammar specifying the program space
 * @param examples a set of input-output examples
 * @param env an environment for running programs on a given input
 * @return a program in grammar satisfying all given example.
 */
PProgram synthesisFromExample(Grammar* grammar, const IOExampleList& examples, Env* env) {
    // Assign consecutive indices to non-terminals in the grammar.
    grammar->indexSymbol();

    // A verifier checking whether a program satisfying all examples
    auto verifier = [&](Program* candidate_program) -> bool {
        for (auto& [inp, oup]: examples) {
            if (!(env->run(candidate_program, inp) == oup)) return false;
        }
        return true;
    };

    // A storage for all enumerated programs
    // program_storage[i][s] stores the set of programs expanded from the i-th non-terminal whose size is s.
    std::vector<std::vector<ProgramList>> program_storage(grammar->symbol_list.size(), {{}});

    // TODO: (Task 1.2) Integrate observational equivalence to the enumeration below.
    // 从程序大小为1开始枚举，逐步增加程序大小
    for (int size = 1; size <=4; ++size) {
        // 记录当前枚举的程序大小
        LOG(INFO) << "Current size " << size;
        
        // 遍历语法中的所有非终结符
        for (auto* symbol: grammar->symbol_list) {
            // 为当前非终结符在program_storage中添加一个新的空列表，用于存储该大小下的程序
            program_storage[symbol->id].emplace_back();
            std::cout << "symbol: " << symbol->name << std::endl;
            // 遍历该非终结符的所有产生式规则
            for (auto* rule: symbol->rule_list) {
                // 使用当前规则和大小构造候选程序
                
                for (auto& candidate_program: constructPrograms(rule, size, program_storage)) {
                    // 将生成的候选程序存储到对应的位置
                    std::cout << "symbol :" << symbol->name << " rule: " << rule->toString() << " size :" << size << std::endl;
                    std::cout << "candidate_program: " << candidate_program->toString() << std::endl;
                    program_storage[symbol->id][size].push_back(candidate_program);
                    
                    // 如果当前符号是起始符号，并且候选程序通过了验证器的检查
                    if (symbol->id == grammar->start->id && verifier(candidate_program.get())) {
                        // 返回找到的满足条件的程序
                        return candidate_program;
                    }
                }
            }
        }
    }
}


PProgram cegis(Grammar* grammar, const IOExampleList& examples, Env* env, FiniteIOExampleSpace* example_space) {
}
   
int main(int argv, char** argc) {
    // 初始化任务路径
    std::string task_path;
    
    // 如果命令行参数为2，使用第一个参数作为任务路径
    if (argv == 2) {
        task_path = std::string(argc[1]);
    } 
    // 否则使用默认的测试文件路径
    else {
        task_path = config::KSourcePath + "/tests/plus.sl";
    }
    std::cout << "任务文件: " << task_path << std::endl;

    // 从文件中解析SyGuS规范
    auto spec = parser::getSyGuSSpecFromFile(task_path);
    if (spec->info_list.empty()) {
        std::cerr << "错误：规范信息列表为空" << std::endl;
        return 1;
    }
    std::cout << "目标函数: " << spec->info_list[0]->name << std::endl;
    
    // 将示例空间转换为有限IO示例空间
    auto* example_space = dynamic_cast<FiniteIOExampleSpace*>(spec->example_space.get());
    if (!example_space) {
        std::cerr << "错误：示例空间类型不匹配" << std::endl;
        return 1;
    }
    std::cout << "示例数量: " << example_space->example_space.size() << std::endl;
    
    // 初始化IO示例列表
    IOExampleList io_examples;
    
    // 遍历示例空间中的所有示例，将其转换为IO示例并添加到列表中
    for (auto& example: example_space->example_space) {
        auto io = example_space->getIOExample(example);
        io_examples.push_back(io);
        std::cout << "示例: ";
        for (auto& v : io.first) std::cout << v.toString() << " ";
        std::cout << "=> " << io.second.toString() << std::endl;
    }
    
    // 获取语法规则
    auto* grammar = spec->info_list[0]->grammar;
    std::cout << "语法规则:" << std::endl;
    for (auto* symbol : grammar->symbol_list) {
        std::cout << symbol->name << ": \n";
        for (auto* rule : symbol->rule_list) {
            std::cout << rule->toString() << "\n";
        }
        std::cout << std::endl;
    }

    // 创建并启动计时器
    auto* recorder = new TimeRecorder();
    recorder->start("synthesis");
    
    // 使用CEGIS算法进行程序综合
    auto res = cegis(grammar, io_examples, spec->env.get(), example_space);
    
    // 停止计时器
    recorder->end("synthesis");

    // 输出综合结果
    std::cout << "Result: " << res->toString() << std::endl;
    
    // 输出综合耗时
    std::cout << "Time Cost: " << recorder->query("synthesis") << " seconds";
}



// The following are auxiliary functions used in enumeration
namespace {
    void _getAllSizeScheme(int pos, int rem, const std::vector<std::vector<int>>& pool, std::vector<int>& tmp, std::vector<std::vector<int>>& res);

    // Get all possible schemes to distribute @size to all parameters.
    // @pool[i] is the list of possible sizes of the i-th parameter.
    std::vector<std::vector<int>> getAllSizeScheme(int size, const std::vector<std::vector<int>>& pool) {
        std::vector<int> tmp;
        std::vector<std::vector<int>> res;
        _getAllSizeScheme(0, size, pool, tmp, res);
        return res;
    }

    void _getAllSizeScheme(int pos, int rem, const std::vector<std::vector<int>>& pool, std::vector<int>& tmp, std::vector<std::vector<int>>& res) {
        // pos == pool.size() 表示所有参数的size都已经确定
        if (pos == pool.size()) {
            // rem == 0 说明各个参数的size和达到总size - 1
            if (rem == 0) res.push_back(tmp); return;
        }
        for (auto size: pool[pos]) {
            // size > rem 说明当前参数的size超过了剩余的size，剩下的都跳过
            if (size > rem) continue;
            tmp.push_back(size);
            // 当前pos参数的size选好了，处理pos + 1的参数, 剩余的size减去当前参数的size
            _getAllSizeScheme(pos + 1, rem - size, pool, tmp, res);
            // 因为是DFS, 当前pos参数为size的组合已经遍历完了，所以需要回溯
            tmp.pop_back();
        }
    }

    // Construct programs expanded from rule @rule from a pool of sub-programs
    /*
        pos 表示当前正在处理的sub_programs中的program的pos
        sub_programs是各个scheme中，各个param的program列表的集合
        rule 表示当前正在处理的rule
        sub_list 表示当前正在处理的program列表
        res 表示所有组合的program列表
    */
    void buildAllCombination(int pos, const std::vector<ProgramList>& sub_programs, Rule* rule, ProgramList& sub_list, ProgramList& res) {
        // 如果当前pos已经处理完了所有param, 则将当前program列表加入到res中
        if (pos == sub_programs.size()) {
            res.push_back(rule->buildProgram(sub_list)); return;
        }
        // 遍历当前pos的program列表
        for (auto& sub_program: sub_programs[pos]) {
            // 将当前program加入到sub_list中
            sub_list.push_back(sub_program);
            // 递归处理下一个pos
            buildAllCombination(pos + 1, sub_programs, rule, sub_list, res);
            // 因为是DFS, 所以当前pos的param对应的后续所有可能都找完了后回溯
            sub_list.pop_back();
        }
    }
}

ProgramList constructPrograms(Rule* rule, int size, const std::vector<std::vector<ProgramList>>& program_storage) {
    std::vector<std::vector<int>> size_pool;
    std::cout << "================================================" << std::endl;
    std::cout << "constructPrograms 开始" << std::endl;
    std::cout << "rule内容: " << std::endl;
    std::cout << rule->toString() << std::endl;
    
    int index = 0;
    std::cout << "para_list: " << std::endl;
    for (auto* nt: rule->param_list) {
        std::vector<int> size_list;
        
        
        std::cout << "--------------------------------" << std::endl;
        std::cout << "nt_"  << index++ << ": " << nt->name << std::endl;
        std::cout << "--------------------------------" << std::endl;

        for (int i = 0; i < size; ++i) {
            if (!program_storage[nt->id][i].empty()) {
                size_list.push_back(i);
                std::cout << "size_list.push_back: " << i << std::endl;
                std::cout << "program_storage[" << nt->id << "][" << i << "] 内容: " << std::endl;
                for (const auto& program : program_storage[nt->id][i]) {
                    std::cout << "  " << program->toString() << std::endl;
                }
            }
        }
        size_pool.push_back(size_list);
    }
    ProgramList res;

    // 获取所有可能的size组合
    for (const auto& scheme: getAllSizeScheme(size - 1, size_pool)) {
        // scheme 是参数的size总和为size -  1的组合, 每个pos对应当前rule的para_list中的一个param的size
        // eg. scheme = [1, 2, 3], 表示第一个param的size为1, 第二个param的size为2, 第三个param的size为3
        ProgramStorage sub_programs;
        for (int i = 0; i < scheme.size(); ++i) {
            // 根据scheme中的size, 从program_storage中获取对应的program
            // [rule->param_list[i]->id][scheme[i]] 表示从program_storage中获取对应非终结符param的size为scheme[i]的program
            sub_programs.push_back(program_storage[rule->param_list[i]->id][scheme[i]]);
        }
        ProgramList sub_list;
        // 例如当前rule为：+(Start,Start)，当前size为3
        // 遍历sub_programs中的所有program, 将它们组合成一个program
        // eg. 第一个param的size为1, 第二个param的size为1, size和为 size - 1 = 2
        // start 有 0 1 2 param0 param1 五种program
        // 组合后为：
        // +(0, 0) +(0, 1) +(0, 2) +(param0, 0) +(param0, 1) +(param0, 2) ...
        // 共5 * 5 = 25种program
        buildAllCombination(0, sub_programs, rule, sub_list, res);
    }
    return res;
}