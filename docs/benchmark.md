# Benchmarks


## WatDiv Benchmark

[WatDiv](https://dsg.uwaterloo.ca/watdiv/) is a benchmark designed for evaluating the performance of RDF (Resource Description Framework) systems. It covers a variety of queries and data patterns to assess the efficiency of RDF stores.

### How to Use WatDiv Benchmark
1. *Data Generation*: Use the provided tool to generate WatDiv datasets or download the WatDiv datasets from the official website.

```
watdiv-gen -s 100 -f watdiv100.nt
```

2. *Load Data*: Load the generated WatDiv data into grove.

```
grovetool create watdiv100MDB
grovetool load watdiv100MDB watdiv100M.nt
```

3. *Run Queries*: WatDiv offers a collection of predefined query templates, conveniently accessible within the [watdiv benchmark] (https://github.com/dlutsky/grove/tree/master/benchmark/watdiv), already generated and ready for use.

```
grovetool query watdiv100MDB --input-file=/benchmark/watdiv/100M/S1.rq
```

### Results