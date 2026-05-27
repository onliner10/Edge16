EDGE16_SUPPORTED_TARGETS="tx16s tx16smk3"
# Edge16 product support is limited to RadioMaster TX16S MK2 and TX16S MK3.

ensure_uv_build_env() {
    if [[ "${EDGE16_UV_ACTIVE:-}" == "1" || "${EDGE16_SKIP_UV:-}" == "1" ]]; then
        return 0
    fi

    if ! command -v uv >/dev/null 2>&1; then
        echo "Edge16 build scripts run through uv by default." >&2
        echo "Install uv, then rerun this command: https://docs.astral.sh/uv/getting-started/installation/" >&2
        exit 1
    fi

    local original_script_path="$1"
    local script_path="$1"
    shift || true

    if [[ "$script_path" != /* ]]; then
        if [[ "$script_path" == */* ]]; then
            script_path="$(cd "$(dirname "$script_path")" && pwd)/$(basename "$script_path")"
        else
            script_path="$(command -v "$script_path" || true)"
        fi
    fi

    if [[ -z "$script_path" || ! -f "$script_path" ]]; then
        echo "Unable to locate script for uv re-exec: $original_script_path" >&2
        exit 1
    fi

    local repo_root
    repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

    export EDGE16_UV_ACTIVE=1
    exec uv run --with-requirements "${repo_root}/requirements.txt" bash "$script_path" "$@"
}

get_target_build_options() {
    local target_name=$1

    case " ${EDGE16_SUPPORTED_TARGETS} " in
        *" ${target_name} "*)
            ;;
        *)
            echo "Unsupported Edge16 target: $target_name"
            return 1
            ;;
    esac

    case $target_name in
        tx16s)
            BUILD_OPTIONS+="-DPCB=X10 -DPCBREV=TX16S"
            ;;
        tx16smk3)
            BUILD_OPTIONS+="-DPCB=TX16SMK3"
            ;;
        *)
            echo "Unknown target: $target_name"
            return 1
            ;;
    esac
}

# Determine parallel job limit based on environment
determine_max_jobs() {
  if [[ -n ${CMAKE_BUILD_PARALLEL_LEVEL} ]]; then
    MAX_JOBS=${CMAKE_BUILD_PARALLEL_LEVEL}
  elif [ -n "$GITHUB_ACTIONS" ]; then
    # Limit jobs in GitHub Actions to n-1 to avoid resource contention
    if [ "$(uname)" = "Darwin" ]; then
      MAX_JOBS=2  # macOS runners have 3 cores
    else
      MAX_JOBS=3  # Linux and Windows runners have 4 cores
    fi
  else
    MAX_JOBS=""  # Let CMake/build system decide
  fi
  export MAX_JOBS
}

# Helper function to run cmake build with appropriate parallelism
cmake_build_parallel() {
  local args=()
  local native_flags=""
  
  # Separate cmake args from native flags (anything after --)
  for arg in "$@"; do
    if [[ "$arg" == "--" ]]; then
      # Capture everything after -- as native flags
      shift
      native_flags="-- $*"
      break
    fi
    args+=("$arg")
    shift
  done
  
  if [[ -n ${MAX_JOBS} ]]; then
    cmake --build "${args[@]}" --parallel ${MAX_JOBS} ${native_flags}
  else
    cmake --build "${args[@]}" --parallel ${native_flags}
  fi
}
