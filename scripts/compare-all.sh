#!/bin/bash
cd "$(dirname "$0")/.."

total_failure=0

compare_group() {
    local requirement="$1"
    local tolerance="$2"
    local threshold="$3"
    shift 3
    local files=("$@")
    
    local expected="expected/$requirement"
    local output="screenshots/$requirement"
    local errors="errors/$requirement"
    
    mkdir -p "$errors"
    
    local success=0
    
    echo ""
    echo "Comparing $requirement output:"
    
    for file in "${files[@]}"; do
        echo "Testing $file ..."
        ./scripts/imgcmp "$expected/$file" "$output/$file" -o "$errors/$file" -t "$tolerance" -e "$threshold"
        if [ $? -eq 0 ]; then
            ((success++))
        fi
    done
    
    local total=${#files[@]}
    echo ""
    echo "Matches: $success/$total"
    if [ "$success" -eq "$total" ]; then
        echo "SUCCESS: All outputs are correct"
    else
        local failure=$((total - success))
        if [ "$failure" -eq 1 ]; then
            echo "FAILURE: $failure output is incorrect"
        else
            echo "FAILURE: $failure outputs are incorrect"
        fi
        ((total_failure += failure))
    fi
}

if [ "$#" -eq 0 ] || [[ " $@ " =~ " shader-test " ]]; then
    files=("test-0.png" "test-1.png" "test-2.png" "test-3.png" "test-4.png" "test-5.png" "test-6.png" "test-7.png" "test-8.png" "test-9.png")
    compare_group "shader-test" 0.01 0 "${files[@]}"
fi

if [ "$#" -eq 0 ] || [[ " $@ " =~ " mesh-test " ]]; then
    files=("default-0.png" "default-1.png" "default-2.png" "default-3.png" "monkey-0.png" "monkey-1.png" "monkey-2.png" "monkey-3.png")
    compare_group "mesh-test" 0.01 0 "${files[@]}"
fi

if [ "$#" -eq 0 ] || [[ " $@ " =~ " transform-test " ]]; then
    files=("test-0.png")
    compare_group "transform-test" 0.01 0 "${files[@]}"
fi

if [ "$#" -eq 0 ] || [[ " $@ " =~ " pipeline-test " ]]; then
    files=("fc-0.png" "fc-1.png" "fc-2.png" "fc-3.png" "dt-0.png" "dt-1.png" "dt-2.png" "b-0.png" "b-1.png" "b-2.png" "b-3.png" "b-4.png" "cm-0.png" "dm-0.png")
    compare_group "pipeline-test" 0.01 64 "${files[@]}"
fi

if [ "$#" -eq 0 ] || [[ " $@ " =~ " texture-test " ]]; then
    files=("test-0.png")
    compare_group "texture-test" 0.01 0 "${files[@]}"
fi

if [ "$#" -eq 0 ] || [[ " $@ " =~ " sampler-test " ]]; then
    files=("test-0.png" "test-1.png" "test-2.png" "test-3.png" "test-4.png" "test-5.png" "test-6.png" "test-7.png")
    compare_group "sampler-test" 0.01 0 "${files[@]}"
fi

if [ "$#" -eq 0 ] || [[ " $@ " =~ " material-test " ]]; then
    files=("test-0.png" "test-1.png")
    compare_group "material-test" 0.02 64 "${files[@]}"
fi

if [ "$#" -eq 0 ] || [[ " $@ " =~ " entity-test " ]]; then
    files=("test-0.png" "test-1.png")
    compare_group "entity-test" 0.04 64 "${files[@]}"
fi

if [ "$#" -eq 0 ] || [[ " $@ " =~ " renderer-test " ]]; then
    files=("test-0.png" "test-1.png")
    compare_group "renderer-test" 0.04 64 "${files[@]}"
fi

if [ "$#" -eq 0 ] || [[ " $@ " =~ " sky-test " ]]; then
    files=("test-0.png" "test-1.png")
    compare_group "sky-test" 0.04 64 "${files[@]}"
fi

if [ "$#" -eq 0 ] || [[ " $@ " =~ " postprocess-test " ]]; then
    files=("test-0.png" "test-1.png" "test-2.png" "test-3.png")
    compare_group "postprocess-test" 0.04 64 "${files[@]}"
fi

echo ""
echo "Overall Results"
if [ "$total_failure" -eq 0 ]; then
    echo "SUCCESS: All outputs are correct"
else
    if [ "$total_failure" -eq 1 ]; then
        echo "FAILURE: $total_failure output is incorrect"
    else
        echo "FAILURE: $total_failure outputs are incorrect"
    fi
fi
