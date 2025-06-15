FROM alpine:3.22.0

RUN apk update

RUN apk add python3 py3-pip
RUN apk add nodejs npm
RUN apk add clang lld
RUN apk add make g++

RUN pip3 install --break-system-packages imageio

WORKDIR /gloom

COPY package.json .

RUN npm install
RUN cd node_modules/sqlite3 && npm run rebuild

COPY static/ static/
COPY res/ res/
COPY scripts/ scripts/
COPY src/ src/

RUN npm run build

ENTRYPOINT [ "node", "./build/server.js" ]
