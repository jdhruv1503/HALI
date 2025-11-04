#!/bin/bash
# Convenience script to build and run experiments in Docker

set -e

echo "=========================================="
echo "HALI Docker Experiment Runner"
echo "=========================================="
echo ""

# Check if Docker is available
if ! command -v docker &> /dev/null; then
    echo "Error: Docker is not installed or not in PATH"
    echo "Please install Docker Desktop for Windows"
    exit 1
fi

# Build Docker image
echo "Building Docker image..."
docker-compose build

echo ""
echo "Docker image built successfully!"
echo ""

# Start container
echo "Starting container with fixed resources (8 CPUs, 8GB RAM)..."
docker-compose up -d

echo ""
echo "Container started: hali-research"
echo ""

# Wait a moment for container to be ready
sleep 2

# Show instructions
echo "=========================================="
echo "Docker Container Ready!"
echo "=========================================="
echo ""
echo "To enter the container:"
echo "  docker exec -it hali-research bash"
echo ""
echo "Inside the container, run:"
echo "  bash scripts/build.sh              # Compile the project"
echo "  bash scripts/download_sosd.sh      # Download SOSD datasets"
echo "  ./build/simulator 1000000 100000   # Run benchmarks"
echo "  python3 scripts/visualize_results.py  # Generate plots"
echo ""
echo "To stop the container:"
echo "  docker-compose down"
echo ""
echo "=========================================="
echo ""

# Option to enter container directly
read -p "Enter container now? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    docker exec -it hali-research bash
fi
