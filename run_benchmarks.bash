#!/bin/bash
# RRL Automated Benchmark Pipeline
#
#

set -e 

BENCHMARKS_DIR="benchmarks"
REPORTS_DIR="${BENCHMARKS_DIR}/reports"
BENCHMARKS_BUILD_DIR="build/benchmarks"

mkdir -p "${REPORTS_DIR}"


# --- Benchmark Execution Function --------------------------------
# Takes a benchmark configuration and handles isolated building 
# and execution.
# Arguments:
#   $1 - Target Base Name (e.g., swr_opencv_rungholt)
#   $2 - Configuration Name (e.g., scalar, simd)
#   $3 - CMake Configuration Flags string
run_benchmark_config() {
    local BASE_NAME="$1"
    local CONFIG_NAME="$2"
    local CMAKE_FLAGS="$3"
    
    # Ensure the source file exists in the BENCHMARKS_DIR
    if [[ ! -f "${BENCHMARKS_DIR}/${BASE_NAME}.cpp" && ! -f "${BENCHMARKS_DIR}/${BASE_NAME}.cc" ]]; then
        echo "Error: Source file for '${BASE_NAME}' not found in ${BENCHMARKS_DIR}/"
        exit 1
    fi
    
    local TARGET_NAME="bm_${BASE_NAME}"
    local BUILD_DIR="${BENCHMARKS_BUILD_DIR}/${CONFIG_NAME}"
    local OUT_JSON="${REPORTS_DIR}/${CONFIG_NAME}_results.json"

    echo ""
    echo "Building Benchmark Configuration ${CONFIG_NAME^^}"
    cmake -B "${BUILD_DIR}" \
        -D CMAKE_BUILD_TYPE=Release \
        -D RRL_ENABLE_NATIVE_ARCH=ON \
        -D RRL_BENCHMARKS=ON \
        ${CMAKE_FLAGS}
    cmake --build "${BUILD_DIR}" --config Release --target "${TARGET_NAME}" -j$(nproc)

    echo ""
    echo "Running Benchmark: ${CONFIG_NAME^^}"
    ./"${BUILD_DIR}/bin/${TARGET_NAME}" \
        --benchmark_out="${OUT_JSON}" \
        --benchmark_out_format=json
}


# --- Compare Benchmark Runs Function -----------------------------
# Takes two benchmark configuration names, locates their respective
# JSON reports, and uses the Google Benchmark python tool to compare them.
# Arguments:
#   $1 - Baseline Configuration Name (e.g., scalar)
#   $2 - Contender Configuration Name (e.g., simd)
compare_benchmark_runs() {
    local CONFIG_A="$1"
    local CONFIG_B="$2"
    
    local JSON_A="${REPORTS_DIR}/${CONFIG_A}_results.json"
    local JSON_B="${REPORTS_DIR}/${CONFIG_B}_results.json"

    echo ""
    echo " Comparing Results: ${CONFIG_A^^} vs ${CONFIG_B^^}"
    echo ""

    local VENV_DIR="${BENCHMARKS_BUILD_DIR}/.venv"
    local PYTHON_BIN="${VENV_DIR}/bin/python"
    local PIP_BIN="${VENV_DIR}/bin/pip"
    if [ ! -d "${VENV_DIR}" ]; then
        echo "Setting up Python virtual environment at ${VENV_DIR}..."
        python3 -m venv "${VENV_DIR}"
        
        echo "Installing Python dependencies (scipy)..."
        "${PIP_BIN}" install --upgrade pip -q
        "${PIP_BIN}" install scipy -q
    fi

    local GBENCH_REPO_DIR="${BENCHMARKS_BUILD_DIR}/google-benchmark"
    local COMPARE_SCRIPT="${GBENCH_REPO_DIR}/tools/compare.py"
    if [ ! -d "${GBENCH_REPO_DIR}" ]; then
        echo "Cloning Google Benchmark tools to common directory..."
        git clone --depth 1 https://github.com/google/benchmark.git "${GBENCH_REPO_DIR}" -q
    fi

    if [[ ! -f "${JSON_A}" || ! -f "${JSON_B}" ]]; then
        echo "Error: Could not find report files to compare."
        echo "Expected: ${JSON_A} and ${JSON_B}"
        exit 1
    fi

    "${PYTHON_BIN}" "${COMPARE_SCRIPT}" benchmarks "${JSON_A}" "${JSON_B}"
}



# --- Benchmark Configurations ------------------------------------
run_benchmark_config "swr_opencv_rungholt" "scalar" \
    "-D RRL_BUILD_SWR_SIMD_BACKEND=OFF \
     -D RRL_BUILD_WINDOW_OPENCV=ON"
run_benchmark_config "swr_opencv_rungholt" "simd" \
    "-D RRL_BUILD_SWR_SIMD_BACKEND=ON \
     -D RRL_BUILD_WINDOW_OPENCV=ON"



# --- Compare Results ---------------------------------------------
compare_benchmark_runs "scalar" "simd"


