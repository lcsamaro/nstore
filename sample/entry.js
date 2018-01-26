var m = require("mithril");
var Conn = require('./sync');

var s = new Conn("ws://" + window.location.host + "/ws",
  function() {
    console.log('transacting schema');
    this.transact([
      [-1, ":db/ident", ":task/name", 0],
      [-1, ":db/cardinality", ":db.cardinality/one", 0],
      [-1, ":db/unique", ":db.unique/yes", 0],

      [-2, ":db/ident", ":task/done", 0],
      [-2, ":db/cardinality", ":db.cardinality/one", 0]
    ]);
  },
  function() {
    console.log("redraw");
    m.redraw();
  }
  );


var TaskList = {
  view: function() {
    let res = [];
    try {
      res = s.query('[ :find ?e ?task ?done '+
          ':where ' +
          '[?e ":task/name" ?task]' +
          '[?e ":task/done" ?done]' +
          ']');
    } catch (e) { return; }

    res.sort(function(a,b) { 
      if (a[1] < b[1]) {
        return -1;
      }
      if (b[1] < a[1]) {
        return 1;
      }
      return 0;
         })

    return m("ul", [
      res.map(function([e, t, d]) {
        return m("li" ,
          [
        m("label",{ onmousedown:
              function(c) {
                if (d == "false") s.transact([[e, ":task/done", "true", 0]]);
                else s.transact([[e, ":task/done", "false", 0]]);
              }
            },
            d == "true" ?  m("strike", t) : t
          ),
        m("button", { onclick:
            c => s.transact([[e, ":task/name", t, 1],
            [e, ":task/done", d, 1]]) }, "remove")
          ]
            );
      })]);
  }
}

var TaskCreation = {
  name: "",
  view() {
    let me = this;
    return m("input", {
      placeholder: "What needs to be done?",
      type: "text",
      value: me.name,
      oninput: m.withAttr("value", v => me.name = v),
      onkeydown(e) {
        if (e.keyCode === 13 && me.name !== "") {
          s.transact([
          [-1, ":task/name", me.name, 0],
          [-1, ":task/done", "false", 0]
          ]);
          me.name = "";
        }
      }
    });
  }
}

var App = {
  view: function() {
    return m("div", [ 
      m(TaskCreation),
      m(TaskList)
    ]);
  }
}

m.mount(document.body, App);

