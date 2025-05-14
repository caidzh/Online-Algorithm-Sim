import os
import sys
import argparse
import itertools
import matplotlib.pyplot as plt
import src.data_downloader as data_downloader
from src.input_processor import InputProcessor
from src.utils import MyNamespace
from src.scheduler import OPT, FIFO, LIFO, LRU, LFU, Marking, SVM

# 安全算法映射
ALGORITHM_MAP = {
    'OPT': OPT,
    'FIFO': FIFO,
    'LIFO': LIFO,
    'LRU': LRU,
    'LFU': LFU,
    'Marking': Marking,
    'SVM': SVM
}

def read_config(config_file):
    """读取配置文件并支持多算法参数"""
    with open(config_file, 'r') as stream:
        try:
            import yaml
            config = yaml.safe_load(stream)
            
            # 处理多算法配置
            if 'algorithms' not in config:
                config['algorithms'] = [config.get('algorithm', 'default')]
            if 'cache_sizes' not in config:
                config['cache_sizes'] = [config.get('cache_size', -1)]
                
            return MyNamespace(**config)
        except yaml.YAMLError as exc:
            print(exc)
            exit(1)

def validate_algorithms(algorithms):
    """验证算法参数有效性"""
    invalid = [a for a in algorithms if a not in ALGORITHM_MAP]
    if invalid:
        print(f"错误：无效的算法参数 {invalid}")
        print("可用算法:", ", ".join(ALGORITHM_MAP.keys()))
        exit(1)

def generate_combinations(args):
    """生成算法与缓存大小的组合"""
    return list(itertools.product(args.algorithms, args.cache_sizes))

def main():
    # 参数解析
    parser = argparse.ArgumentParser(
        description="多算法页面置换模拟器",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument("--config", default="./config/config.yaml",
                      help="配置文件路径")
    parser.add_argument("--algorithms", nargs='+', default=[],
                      choices=ALGORITHM_MAP.keys(), help="指定多个算法")
    parser.add_argument("--cache_sizes", nargs='+', type=int, default=[],
                      help="指定多个缓存大小")
    args = parser.parse_args()

    # 合并配置与命令行参数
    config = read_config(args.config)
    final_algorithms = args.algorithms or config.algorithms
    final_cache_sizes = args.cache_sizes or config.cache_sizes

    # 参数验证
    validate_algorithms(final_algorithms)
    if any(size <= 0 for size in final_cache_sizes):
        print("错误：缓存大小必须为正整数")
        exit(1)

    # 生成所有组合
    combinations = generate_combinations(
        MyNamespace(algorithms=final_algorithms, cache_sizes=final_cache_sizes)
    )

    print("启用的算法组合:")
    for algo, size in combinations:
        print(f"- {algo} (缓存大小: {size})")

    # 下载数据
    traces_list = data_downloader.download_data(config)
    input_processor = InputProcessor()

    label = []
    value = []

    # 处理每个组合
    for algorithm, cache_size in combinations:
        print(f"\n正在运行 {algorithm} (缓存大小: {cache_size})")
        if (algorithm != "SVM"):
            label.append(f"{algorithm}_{cache_size}")

            output_file = os.path.join(
                config.output_dir, 
                f"{algorithm}_{cache_size}.csv"
            )

            # 初始化结果文件
            with open(output_file, 'w') as f:
                f.write("trace_file,total_requests,unique_pages,cache_misses\n")
            
            cache_miss_sum = 0

            # 处理每个trace文件
            for trace_file in traces_list:
                try:
                    print(f"处理 {os.path.basename(trace_file)}...", end=' ')
                    requests = input_processor.process_input(trace_file)
                    
                    # 初始化调度器
                    scheduler = ALGORITHM_MAP[algorithm](cache_size, 0)
                    result = scheduler.run(requests)

                    with open(output_file, 'a') as f:
                        f.write(f"{os.path.basename(trace_file)},"
                                f"{result.total_requests},"
                                f"{result.unique_pages},"
                                f"{result.cache_misses}\n")
                    cache_miss_sum += result.cache_misses
                    print("完成")
                    
                except Exception as e:
                    print(f"处理失败: {str(e)}")
                    continue
            
            value.append(cache_miss_sum)
        else:
            for prediv in range(1, 101, 10):
                label.append(f"{algorithm}_{cache_size}_{prediv}")

                output_file = os.path.join(
                    config.output_dir, 
                    f"{algorithm}_{cache_size}_{prediv}.csv"
                )

                # 初始化结果文件
                with open(output_file, 'w') as f:
                    f.write("trace_file,total_requests,unique_pages,cache_misses\n")

                cache_miss_sum = 0

                # 处理每个trace文件
                for trace_file in traces_list:
                    try:
                        print(f"处理 {os.path.basename(trace_file)}...", end=' ')
                        requests = input_processor.process_input(trace_file)
                        
                        # 初始化调度器
                        scheduler = ALGORITHM_MAP[algorithm](cache_size, prediv)
                        result = scheduler.run(requests)

                        with open(output_file, 'a') as f:
                            f.write(f"{os.path.basename(trace_file)},"
                                    f"{result.total_requests},"
                                    f"{result.unique_pages},"
                                    f"{result.cache_misses}\n")
                        cache_miss_sum += result.cache_misses
                        print("完成")
                        
                    except Exception as e:
                        print(f"处理失败: {str(e)}")
                        continue
                value.append(cache_miss_sum)
    plt.title("Algorithm-Miss")
    plt.xlabel('Algorithm')
    plt.ylabel('Miss')
    plt.bar(label, value, color='skyblue')
    plt.xticks(rotation=45, ha='right')
    plt.tight_layout()
    y_min = min(value) * 0.8
    y_max = max(value)
    plt.ylim(y_min, y_max)
    plt.savefig('result.png', dpi=300, bbox_inches='tight')
if __name__ == "__main__":
    main()