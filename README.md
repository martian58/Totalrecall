# TotalRecall

TotalRecall is a tool that extracts and displays data from the Recall feature in Windows 11.


This repository includes implementations in both PowerShell and C, in addition to the original Python script:

- **PowerShell**
- **C**
- **Python**: The original script by Alexander Hagenah (@xaitax).

## Requirements

- **Windows 11**
- **GCC** 
- **SQLite3 library for C**

## Compilation

```bash
gcc -o TotalRecall TotalRecall.c -lsqlite3

or 

make
```
