/* This implements datascript/nstore syncing */
var d = require('datascript');

let retryms = 1000;
let basic_schema = {
  db_nil:               0,
  db_ident:             1,
  db_type:              2,
  db_cardinality:       3,
  db_unique:            4,
  db_type_value:        5,
  db_type_ref:          6,
  db_cardinality_one:   7,
  db_cardinality_many:  8,
  db_unique_no:         9,
  db_unique_yes:       10,
  db_initial_id:       11
};

/* Conn class */
function Conn(uri, onSync, onUpdate, onLost) {
  this.websocket = null;
  this.uri = uri;
  this.onSync = onSync;
  this.onUpdate = onUpdate;
  this.onLost = onLost;

  this.sync = true;
  this.curtx = -1;
  this.schema = {};
  this.db = d.empty_db(this.schema);
  this.idents   = null;
  this.ident_id = null;

  this.start();
}

Conn.prototype.transact = function(datoms) {
  for (let i = 0; i < datoms.length; i++) {
    // TODO handle ident_ids added here
    if (typeof datoms[i][1] === 'string') {
      //console.log(datoms[i][1] + " -> " + this.ident_id[datoms[i][1]]);
      datoms[i][1] = this.ident_id[datoms[i][1]];
    }

    if (datoms[i][1] == basic_schema.db_cardinality) {

    } else if (datoms[i][1] == basic_schema.db_unique) {

    } else if (datoms[i][1] == basic_schema.db_type) {

    }
  }
  //console.log('transact', datoms);
  this.doSend("transact " + JSON.stringify(datoms) + "\r\n"); 
}

Conn.prototype.query = function(q) {
  return d.q(q, this.db);
}

Conn.prototype.pull = function(q) {
}

Conn.prototype.tx = function() {
  return this.curtx;
}

Conn.prototype.start = function(websocketServerLocation) {
  let me = this;
  console.log('connecting');
  let websocket = new WebSocket(this.uri);
  websocket.onopen = function(e) {
    me.sync = true;
    me.curtx = -1;
    me.schema = {};
    me.db = d.empty_db(this.schema);
    me.idents   = null;
    me.ident_id = null;
    me.doSend('facts -1\r\n');
  }
  websocket.onmessage = e => me.onMessage(e);
  websocket.onerror = e => console.log('ERROR: ', e);
  websocket.onclose = function() {
    me.onLost();
    console.log("disconnected, retrying in " + retryms/1000 + "s...");
    setTimeout(function(){me.start(websocketServerLocation)}, retryms);
  };
  this.websocket = websocket;
}

Conn.prototype.doSend = function(message) {
  //console.log('REQ ' + message);
  this.websocket.send(message);
}

Conn.prototype.updateSchema = function(schema) {
  let datoms = d.datoms(this.db, ":eavt");
  let nfacts = [];
  for (var i = 0; i < datoms.length; i++) { // hack, so we maintain tx
    nfacts.push([ datoms[i].added ? ":db/add" : ":db/retract",
        datoms[i].e, datoms[i].a, datoms[i].v,
        datoms[i].tx]);
  }
  this.db = d.empty_db(schema);
  this.db = d.db_with(this.db, nfacts);

  //db = d.db_with(this.db, datoms); //nfacts);
  //db = d.init_db(datoms, schema); // this loses tx data
}

Conn.prototype.updateStore = function(facts) {
  /*console.log('ids', this.idents);
  console.log('ids', this.ident_id);*/
  console.time('buildstore');
  let schemaChanged = false;
  let newFacts = [];
  for (var i = 0; i < facts.length; i++) {
    let [e, a, v, t, r] = facts[i];
    if (a == basic_schema.db_ident) {
      if (r) {
        this.idents[e] = null; this.ident_id[v] = null;
      } else {
        this.idents[e] = v; this.ident_id[v] = e;
      }
    }

    if (this.idents[a])
      newFacts.push([r != 0 ? ":db/retract" : ":db/add", e, this.idents[a], v, t]);

    if (e == basic_schema.db_cardinality) continue;
    if (e == basic_schema.db_ident) continue;
    if (e == basic_schema.db_type) continue;
    if (e == basic_schema.db_unique) continue;

    if (a == basic_schema.db_ident) {
      schemaChanged = true;
      this.schema[e] = this.schema[e] || {};
      if (!r) this.schema[e].ident = v;
      else this.schema[e].ident = null;
    }

    if (a == basic_schema.db_cardinality) {
      schemaChanged = true;
      this.schema[e] = this.schema[e] || {};
      if (!r) this.schema[e].cardinality = v;
      else this.schema[e].cardinality = null;
    }

    if (a == basic_schema.db_unique) {
      schemaChanged = true;
      this.schema[e] = this.schema[e] || {};
      if (!r) this.schema[e].unique = v;
      else this.schema[e].unique = null;
    }

    if (a == basic_schema.db_type) {
      schemaChanged = true;
      if (v = basic_schema.db_type_ref) {
        this.schema[e] = this.schema[e] || {};
        if (!r) this.schema[e].type = v;
        else this.schema[e].type = null;
      }
    }
  }

  if (schemaChanged) {
    //console.log("new schema!");
    //console.log(this.idents);
    let dschema = {};
    let keys = Object.keys(this.schema);
    for (var i = 0; i < keys.length; i++) {
      let o = this.schema[keys[i]];
      if (!o.ident) continue;
      let s = {};
      if (o.cardinality) s[":db/cardinality"] = this.idents[o.cardinality];
      if (o.type) s[":db/valueType"] = this.idents[o.type];
      if (o.unique == basic_schema.db_unique_yes) s[":db/unique"] = ":db.unique/value";
      dschema[o.ident] = s;
    }
    console.log("new SCHEMA", dschema);
    this.updateSchema(dschema);
  }

  console.time('merge');
  this.db = d.db_with(this.db, newFacts);
  console.timeEnd('merge');
  console.timeEnd('buildstore');

  this.onUpdate();
}

Conn.prototype.onMessage = function(evt) {
  /*if (evt.data.length > 1024 * 1024) {
    console.log("new message " + evt.data.length / (1024*1024) + "MB of data");
  } else if (evt.data.length > 1024) {
    console.log("new message " + evt.data.length / 1024 + "KB of data");
  } else {
    console.log("new message " + evt.data.length + "B of data");
  }*/
  let data = JSON.parse(evt.data);
  //console.log(data);
  if (data !== null && typeof data === 'object' && 'tx' in data) this.curtx = data.tx;

  //console.log("cur tx: ", this.curtx);
  if (data.type == "response" && this.sync) {
    //console.log("received facts from tx: " + data.tx);
    //console.log(data.facts);
    let snapshot = data.facts;

    // derive idents
    this.idents = {};
    this.ident_id = {};
    for (var i = 0; i < snapshot.length; i++) {
      let [e, a, v, t] = snapshot[i];
      if (a == basic_schema.db_ident) {
        this.idents[e] = v;
        this.ident_id[v] = e;
      }
    }
    this.updateStore(snapshot);

    this.onSync();
    this.sync = false;
    return;
  }
  if (data && data.facts) this.updateStore(data.facts);
}

module.exports = function(a, b, c, d) { return new Conn(a, b, c, d) };

