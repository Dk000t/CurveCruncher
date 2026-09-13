# CurveCruncher

A CPU core-by-core stress test tool for Linux using y-cruncher.

## What it does

Tests physical CPU cores individually using y-cruncher to detect stability issues on specific cores. Ideal for tuning Curve Optimizer settings on Ryzen processors on Linux systems.

## Requirements

- Linux OS
- `y-cruncher` executable
- A y-cruncher configuration file
- GCC or Clang

> **Note:** Both the `y-cruncher` executable and your configuration file must be placed in the same directory as CurveCruncher.

## Usage

### Basic execution

```bash
./CurveCruncher -c <config_file>
```

### Available Options

- `-c <config_file>` : Specify your y-cruncher configuration file

### Example

```bash
# Using a configuration file named "config.cfg"
./CurveCruncher -c config.cfg
```

## Example configuration file

```text
{
    Action : "StressTest"
    StressTest : {
        AllocateLocally : true
        LogicalCores : [0]
        TotalMemory : 29944734720
        SecondsPerTest : 60
        SecondsTotal : 480
        StopOnError : true
        Tests : ["BKT" "BBP" "SFTv4" "SNT" "SVT" "FFTv4" "N63" "VT3"]
    }
}
```
