# nstore

Facts store, loosely based on [Datomic](https://www.datomic.com/). Meant to be used in conjunction with [DataScript](https://github.com/tonsky/datascript).

This implements active facts retrieval for a given transaction id and publishing of new facts added to the store. Syncing of new facts is handled automatically. The idea is to use DataScript as the application state.

![Alt Text](https://github.com/lcsamaro/nstore/raw/master/sample/sample.gif)

## building the server
* install libsodium
* cd src
* make

## running the sample
* install openresty
* cd sample
* npm install
* npm run dev

* run nstore server: ./src/server --db filename

* sample is running at localhost:8080
