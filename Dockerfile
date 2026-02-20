FROM alpine:3.22.0

# update apk cache
RUN apk update

# install python3 (needed for build scripts)
RUN apk add python3 py3-pip
# install node and npm
# (node is for the server and npm is to build the client and server)
RUN apk add nodejs npm
# install clang and lld (needed to build wasm)
RUN apk add clang lld

# install imageio python package (needed for some scripts)
RUN pip3 install --break-system-packages imageio

WORKDIR /gloom

# copy package.json, install the dependencies
# and build the node-sqlite3 node module
COPY package.json .
RUN npm install

# copy over the project source
COPY static/ static/
COPY res/ res/
COPY scripts/ scripts/
COPY src/ src/
COPY cert.key .
COPY cert.pem .

# build the project
RUN npm run build

# launch the server
ENTRYPOINT [ "node", "./build/server.js" ]
