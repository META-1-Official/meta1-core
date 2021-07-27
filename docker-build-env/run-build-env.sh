# Initialize the build sub-directory that will be used in the Docker build environment
mkdir -p build

echo "cmake -DCMAKE_BUILD_TYPE=Release ../..; make -j1 witness_node cli_wallet" > build/build.sh
chmod a+x build/build.sh

# Run the Docker build environment
PROJECT_DIR=$(pwd)"/.."
docker run -it --rm \
        -v=$PROJECT_DIR:/meta1 \
        -u=$(id -u):$(id -g) \
        ubuntu-18.04:meta1-build-env /bin/bash
