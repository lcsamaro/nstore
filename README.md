# nstore

Facts store and server, loosely based on [Datomic](https://www.datomic.com/).

## dependencies
* libsodium

## building
$ cd src
$ make

## usage
$ ./server --db master --port 5555

retrieve active facts

$ echo "f-1" | nc -C localhost 5555
```
{"type":"response","tx":1,"facts":[[1,1,":db/ident",0,false],[1,2,5,0,false],[1,3,7,0,false],[1,4,10,0,false],[2,1,":db/type",0,false],[2,2,6,0,false],[2,3,7,0,false],[2,4,9,0,false],[3,1,":db/cardinality",0,false],[3,2,6,0,false],[3,3,7,0,false],[3,4,9,0,false],[4,1,":db/unique",0,false],[4,2,6,0,false],[4,3,7,0,false],[4,4,9,0,false],[5,1,":db.type/value",0,false],[6,1,":db.type/ref",0,false],[7,1,":db.cardinality/one",0,false],[8,1,":db.cardinality/many",0,false],[9,1,":db.unique/no",0,false],[10,1,":db.unique/yes",0,false]]}
```

transact new facts

$ echo "t[[-1,777,777,0]]" | nc -C localhost 5555
```
{"type":"response","tx":1,"facts":[[11,777,777,1,false]],"new_ids":[[-1,11]]}
```

transact new facts

$ echo "t[[888,888,888,0]]" | nc -C localhost 5555
```
{"type":"response","tx":2,"facts":[[888,888,888,2,false]],"new_ids":[]}
```

retract previous fact

$ echo "t[[888,888,888,1]]" | nc -C localhost 5555
```
{"type":"response","tx":3,"facts":[[888,888,888,3,true]],"new_ids":[]}
```

retrieve facts at t = 0

$ echo "f0" | nc -C localhost 5555
```
{"type":"response","tx":0,"facts":[[1,1,":db/ident",0,false],[1,2,5,0,false],[1,3,7,0,false],[1,4,10,0,false],[2,1,":db/type",0,false],[2,2,6,0,false],[2,3,7,0,false],[2,4,9,0,false],[3,1,":db/cardinality",0,false],[3,2,6,0,false],[3,3,7,0,false],[3,4,9,0,false],[4,1,":db/unique",0,false],[4,2,6,0,false],[4,3,7,0,false],[4,4,9,0,false],[5,1,":db.type/value",0,false],[6,1,":db.type/ref",0,false],[7,1,":db.cardinality/one",0,false],[8,1,":db.cardinality/many",0,false],[9,1,":db.unique/no",0,false],[10,1,":db.unique/yes",0,false]]}
```

retrieve facts at t = 1

$ echo "f1" | nc -C localhost 5555
```
{"type":"response","tx":1,"facts":[[1,1,":db/ident",0,false],[1,2,5,0,false],[1,3,7,0,false],[1,4,10,0,false],[2,1,":db/type",0,false],[2,2,6,0,false],[2,3,7,0,false],[2,4,9,0,false],[3,1,":db/cardinality",0,false],[3,2,6,0,false],[3,3,7,0,false],[3,4,9,0,false],[4,1,":db/unique",0,false],[4,2,6,0,false],[4,3,7,0,false],[4,4,9,0,false],[5,1,":db.type/value",0,false],[6,1,":db.type/ref",0,false],[7,1,":db.cardinality/one",0,false],[8,1,":db.cardinality/many",0,false],[9,1,":db.unique/no",0,false],[10,1,":db.unique/yes",0,false],[11,777,777,1,false]]}
```

retrieve facts at t = 2

$ echo "f2" | nc -C localhost 5555
```
{"type":"response","tx":2,"facts":[[1,1,":db/ident",0,false],[1,2,5,0,false],[1,3,7,0,false],[1,4,10,0,false],[2,1,":db/type",0,false],[2,2,6,0,false],[2,3,7,0,false],[2,4,9,0,false],[3,1,":db/cardinality",0,false],[3,2,6,0,false],[3,3,7,0,false],[3,4,9,0,false],[4,1,":db/unique",0,false],[4,2,6,0,false],[4,3,7,0,false],[4,4,9,0,false],[5,1,":db.type/value",0,false],[6,1,":db.type/ref",0,false],[7,1,":db.cardinality/one",0,false],[8,1,":db.cardinality/many",0,false],[9,1,":db.unique/no",0,false],[10,1,":db.unique/yes",0,false],[11,777,777,1,false],[888,888,888,2,false]]}
```

retrieve facts at t = 3

$ echo "f3" | nc -C localhost 5555
```
{"type":"response","tx":3,"facts":[[1,1,":db/ident",0,false],[1,2,5,0,false],[1,3,7,0,false],[1,4,10,0,false],[2,1,":db/type",0,false],[2,2,6,0,false],[2,3,7,0,false],[2,4,9,0,false],[3,1,":db/cardinality",0,false],[3,2,6,0,false],[3,3,7,0,false],[3,4,9,0,false],[4,1,":db/unique",0,false],[4,2,6,0,false],[4,3,7,0,false],[4,4,9,0,false],[5,1,":db.type/value",0,false],[6,1,":db.type/ref",0,false],[7,1,":db.cardinality/one",0,false],[8,1,":db.cardinality/many",0,false],[9,1,":db.unique/no",0,false],[10,1,":db.unique/yes",0,false],[11,777,777,1,false]]}
```

