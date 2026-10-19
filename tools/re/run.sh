#!/bin/bash
# usage: run.sh Script.java args...
cd "$(dirname "$0")"
GHIDRA_HEADLESS_MAXMEM=5G ghidra_12.1.4_PUBLIC/support/analyzeHeadless proj sky -process SkyrimSE.exe -noanalysis -readOnly -scriptPath scripts -postScript "$@" > last.log 2>&1
grep -E "ERROR|Exception" last.log | head -5
