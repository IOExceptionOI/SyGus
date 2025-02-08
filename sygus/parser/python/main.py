import sys
from sexp import sexp as sexpParser
import json
import pprint as pp

def stripComments(bmFile):
    # 初始化一个字符串，以'('开头，确保最终生成的S-expression格式正确
    noComments = '('
    
    # 逐行读取文件内容
    for line in bmFile:
        # 使用split方法去除每行中的注释部分（以';'开头的内容）
        # split(';', 1)表示以';'为分隔符，最多分割一次，取分割后的第一部分
        line = line.split(';', 1)[0]
        
        # 将处理后的行内容拼接到noComments字符串中
        noComments += line
    
    # 返回完整的S-expression字符串，确保以')'结尾
    return noComments + ')'

    # 示例：
    # 输入文件内容：
    # (define x 10) ; 定义变量x
    # (+ x 20) ; 计算x + 20
    # 输出：
    # (define x 10)(+ x 20)

def sexpFromString(value):
    return sexpParser.parseString(value, parseAll = True).asList()[0]

    # 示例：
    # 输入字符串：
    # "(define x 10)(+ x 20)"
    # 解析后的Python列表：
    # ['define', 'x', 10]
    # ['+', 'x', 20]

def sexpFromFile(benchmarkFileName):
    try:
        benchmarkFile = open(benchmarkFileName)
    except:
        print('File not found: %s' % benchmarkFileName)
        return None

    bm = stripComments(benchmarkFile)
    bmExpr = sexpFromString(bm)
    benchmarkFile.close()
    return bmExpr

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("python3 main.py $file_name $output_file")
        exit(0)
    file_name = sys.argv[1]
    output_file = sys.argv[2]
    result = sexpFromFile(file_name)

    with open(output_file, "w") as oup:
        json.dump(result, fp=oup, indent=4)