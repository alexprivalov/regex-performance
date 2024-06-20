FROM debian:12

ENV CXX_FLAGS="-O3 -march=native"
ENV CXXFLAGS="-O3 -march=native"
ENV CFLAGS="-O3 -march=native"
ENV C_FLAGS="-O3 -march=native"

# Install basic dependencies
RUN apt-get update -y && apt-get install -y \
      g++ git \
      cmake ragel python3 python3-pip python3-xlsxwriter \
      libboost-dev libpcap-dev libpcre2-dev \
      autoconf automake autopoint \
      gettext libtool git libboost-regex-dev libboost-regex1.74.0 libhyperscan5 libhyperscan-dev

# Install rustc
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
ENV PATH="/root/.cargo/bin:${PATH}"

# Setup project
WORKDIR /a
COPY . .
WORKDIR /a/build
RUN cmake ..
RUN make

# ENTRYPOINT [ "/a/build/src/regex_perf" ] 
# CMD [ "-f", "/a//3200.txt"]

