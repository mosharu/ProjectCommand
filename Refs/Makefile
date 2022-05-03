INSTALL_DIR = ${HOME}/local

# https://www.boost.org/doc/libs/1_79_0/more/getting_started/unix-variants.html
download-boost:
	wget https://boostorg.jfrog.io/artifactory/main/release/1.79.0/source/boost_1_79_0.tar.gz
	tar -xzf boost_1_79_0.tar.gz

pre-boost:
	cd boost_1_79_0 && ./bootstrap.sh --prefix=${INSTALL_DIR} --with-libraries=chrono,filesystem,json,program_options,regex,test

build-boost:
	mkdir -p ${PWD}/boost-build
	cd boost_1_79_0 && ./b2 --prefix=${INSTALL_DIR} --build-dir=${PWD}/boost-build install

rebuild-boost:
	mkdir -p ${PWD}/boost-build
	cd boost_1_79_0 && ./b2 -a --prefix=${INSTALL_DIR} --build-dir=${PWD}/boost-build install
