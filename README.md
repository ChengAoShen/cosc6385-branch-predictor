# COSC6385 Assignment 1 — Pentium M Hybrid Branch Predictor

**ChengAo Shen — Student ID: 2437132**

Implementation of the Pentium M branch *direction* predictor (bimodal table +
tagged, set-associative global predictor) on top of the CBP-2 simulation
infrastructure. The loop predictor, PIR hashing and target prediction are not
implemented (not required by the assignment).

- Code: `src/my_predictor.h` → classes `pm_update` and `pm_predictor`
- Report: [`report/report.pdf`](report/report.pdf) (LaTeX source: `report/report.tex`)
- Results: `results/`
- GitHub: https://github.com/ChengAoShen/cosc6385-branch-predictor

## Build

Requirements: a C++ compiler (`g++` or `clang++`) and `make`.

```sh
make -C src            # builds src/predict (and src/predict_extra_credit)
```

## Run

```sh
./run.sh traces        # all 20 traces, prints per-trace MPKI and the average
```

`run.sh` is a bash version of the original `run` script (the original needs
`csh` and `dc`). If `csh` and `dc` are available, `./run traces` gives the same
output.

Single trace:

```sh
./src/predict traces/164.gzip/gzip.trace.bz2
```

Expected result of the final configuration: **average MPKI 5.495**
(always-taken 86.312, sample gshare 6.306).

## Configuration

All parameters are macros at the top of the `pm_predictor` section in
`src/my_predictor.h`. Each one can be overridden at compile time, for example:

```sh
g++ -O3 -DPM_WAYS=2 -DPM_ALLOC=0 -o src/predict src/predict.cc src/trace.cc
```

| Macro | Final value | Meaning |
|---|---|---|
| `PM_BIM_BITS` | 12 | bimodal table has 2^12 = 4096 2-bit counters |
| `PM_SET_BITS` | 9 | global table has 2^9 = 512 sets |
| `PM_WAYS` | 4 | ways per set (2048 entries in total) |
| `PM_TAG_BITS` | 10 | partial tag width |
| `PM_HIST_LEN` | 21 | global history register length |
| `PM_FOLD` | 1 | fold history bits above set+tag width into the hash |
| `PM_ALLOC` | 1 | allocate a global entry only when bimodal mispredicted (0 = always) |

## Files

```
src/my_predictor.h          predictor implementation (my_predictor.h.orig in the
                            course folder is the untouched original)
src/*.cc, *.h, Makefile     CBP-2 infrastructure (unchanged)
traces/                     20 CBP-2 traces
run, run_extra              original csh scripts
run.sh                      bash replacement for run
results/final_mpki.txt      ./run.sh traces output of the final configuration
results/per_trace.tsv       trace, always-taken, gshare, final MPKI
results/param_study.tsv     average MPKI of every configuration tested
report/report.pdf, .tex     report (PDF and LaTeX source)
```
