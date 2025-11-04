#!/bin/bash
# Download SOSD (Searching on Sorted Data) benchmark datasets
# These are the industry-standard datasets used in RMI and ALEX papers

set -e

SOSD_DIR="data/sosd"
mkdir -p "$SOSD_DIR"

echo "=========================================="
echo "Downloading SOSD Benchmark Datasets"
echo "=========================================="
echo ""

# SOSD dataset URLs (200M keys each)
# Source: https://github.com/learnedsystems/SOSD

DATASETS=(
    "books_200M_uint64:https://dataverse.harvard.edu/api/access/datafile/:persistentId?persistentId=doi:10.7910/DVN/JGVF9A/EATHF7"
    "fb_200M_uint64:https://dataverse.harvard.edu/api/access/datafile/:persistentId?persistentId=doi:10.7910/DVN/JGVF9A/SVN8PI"
    "osm_cellids_200M_uint64:https://dataverse.harvard.edu/api/access/datafile/:persistentId?persistentId=doi:10.7910/DVN/JGVF9A/TLVHXF"
    "wiki_ts_200M_uint64:https://dataverse.harvard.edu/api/access/datafile/:persistentId?persistentId=doi:10.7910/DVN/JGVF9A/K7VGLG"
)

download_dataset() {
    local name=$1
    local url=$2
    local output_file="$SOSD_DIR/${name}"

    if [ -f "$output_file" ]; then
        echo "✓ $name already exists, skipping..."
        return
    fi

    echo "Downloading $name..."
    echo "  URL: $url"
    echo "  Output: $output_file"

    # Download with curl
    curl -L -o "$output_file" "$url"

    if [ $? -eq 0 ]; then
        # Check file size
        size=$(du -h "$output_file" | cut -f1)
        echo "  ✓ Downloaded successfully (Size: $size)"
    else
        echo "  ✗ Download failed!"
        rm -f "$output_file"
        return 1
    fi
}

# Download all datasets
for dataset in "${DATASETS[@]}"; do
    IFS=':' read -r name url <<< "$dataset"
    echo ""
    download_dataset "$name" "$url"
    echo "----------------------------------------"
done

echo ""
echo "=========================================="
echo "Download Summary"
echo "=========================================="
echo ""

# List downloaded files
echo "Downloaded datasets in $SOSD_DIR:"
ls -lh "$SOSD_DIR"

echo ""
echo "Total disk usage:"
du -sh "$SOSD_DIR"

echo ""
echo "=========================================="
echo "SOSD datasets ready for benchmarking!"
echo "=========================================="
echo ""
echo "Note: These datasets contain 200M pre-sorted 64-bit keys each."
echo "They can be loaded directly into the indexes for benchmarking."
echo ""
echo "To use in benchmarks:"
echo "  1. Modify src/main.cpp to load SOSD binary files"
echo "  2. Use std::ifstream with binary mode to read uint64_t arrays"
echo "  3. Pass loaded keys to index->load() method"
