FROM debian:11

ENV CXX_FLAGS="-O3 -march=native"
ENV CXXFLAGS="-O3 -march=native"
ENV CFLAGS="-O3 -march=native"
ENV C_FLAGS="-O3 -march=native"

# Install basic dependencies
RUN apt-get update -y && apt-get install -y \
      g++ \
      cmake ragel python3 \
      libboost-dev libpcap-dev \
      autoconf automake autopoint \
      gettext libtool git libboost-regex-dev libboost-regex1.74.0 libhyperscan5 libhyperscan-dev

# Install rustc
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
ENV PATH="/root/.cargo/bin:${PATH}"

# Install tini
ENV TINI_VERSION v0.18.0
ADD https://github.com/krallin/tini/releases/download/${TINI_VERSION}/tini /usr/local/sbin/tini
RUN chmod +x /usr/local/sbin/tini

# Setup project
WORKDIR /a
COPY . .
WORKDIR /a/build
RUN cmake ..
RUN make

ENTRYPOINT ["/usr/local/sbin/tini", "--"]
CMD ["./src/regex_perf", "-f", "../3200.txt"]
