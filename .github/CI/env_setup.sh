# This file should be "sourced" into your environment

export BUILD_TYPE=$1
export SHARED_TYPE=$2
export PROFILING=$3

echo "Loading environment"
source /apps/spacks/current/github_env/share/spack/setup-env.sh
spack env activate parsec

DEBUG=ON
if [ $BUILD_TYPE = "Release" ]; do
   DEBUG=OFF
fi

export BUILD_DIRECTORY=build
export INSTALL_DIRECTORY=install
read -d '' BUILD_CONFIG << EOF
        -G Ninja
        -DCMAKE_BUILD_TYPE=$BUILD_TYPE
        -DPARSEC_DEBUG_NOISIER=$DEBUG
        -DPARSEC_DEBUG_PARANOID=$DEBUG
        -DBUILD_SHARED_LIBS=$SHARED_TYPE
        -DPARSEC_PROF_TRACE=$PROFILING
        -DMPIEXEC_PREFLAGS='--bind-to;none;--oversubscribe'
        -DCMAKE_INSTALL_PREFIX=$INSTALL_DIRECTORY
EOF
export BUILD_CONFIG


