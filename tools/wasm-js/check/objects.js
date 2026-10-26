// Run by tools/wasm-js/check-patches.sh compare on the unpatched and the patched engine: the outputs must match.
// Object literal cases whose results must not change; prints a description of each.
function describe(o) {
    var names = Object.getOwnPropertyNames(o), out = [];
    for (var k of Reflect.ownKeys(o)) {
        var d = Object.getOwnPropertyDescriptor(o, k);
        out.push(String(k) + (d.get || d.set ? "(accessor)" : "=" + (typeof d.value === "function" ? "fn:" + d.value.name : JSON.stringify(d.value))) +
                 (d.writable ? "w" : "") + (d.enumerable ? "e" : "") + (d.configurable ? "c" : ""));
    }
    return "{" + out.join(",") + "} proto=" + (Object.getPrototypeOf(o) === Object.prototype ? "Object" : String(Object.getPrototypeOf(o))) +
        " ext=" + Object.isExtensible(o) + " forin=" + (function () { var r = []; for (var k in o) r.push(k); return r.join("|"); })();
}
var log = [];
function t(name, f) { try { log.push(name + ": " + describe(f())); } catch (e) { log.push(name + ": threw " + e.name + ": " + e.message); } }
var a = 1, b = "two", c = { n: 3 };
t("plain", () => ({ x: 1, y: 2, z: 3 }));
t("shorthand", () => ({ a, b, c }));
t("mixed", () => ({ a, y: b + "!", z: [1, 2] }));
t("empty", () => ({}));
t("one", () => ({ only: null }));
t("duplicate", () => ({ x: 1, y: 2, x: 3 }));
t("proto", () => ({ __proto__: c, x: 1 }));
t("proto-null", () => ({ __proto__: null, x: 1 }));
t("proto-string-key", () => ({ "__proto__": c, y: 2 }));
t("computed", () => ({ ["k" + a]: 1, x: 2 }));
t("spread", () => ({ x: 1, ...c, y: 2 }));
t("method", () => ({ x: 1, m() { return 1; }, y: 2 }));
t("getter", () => ({ x: 1, get g() { return 2; } }));
t("numeric", () => ({ 2: "b", 1: "a", x: 0, "10": "j", "-1": "m" }));
t("big-index", () => ({ 4294967295: 1, 4294967294: 2, x: 0 }));
t("string-keys", () => ({ "hello world": 1, "": 2, "x y": 3 }));
t("anon-fn-names", () => ({ f: function () {}, g: () => 1, h: class {} }));
t("nested", () => ({ p: { x: 1, y: { z: 2 } }, q: [{ r: 1 }] }));
t("symbol-computed", () => ({ [Symbol.iterator]: 1, x: 2 }));
t("throws-midway", () => ({ x: 1, y: (() => { throw new Error("midway"); })(), z: 3 }));
var sixtyFour = "({" + Array.from({ length: 64 }, (_, i) => "p" + i + ": " + i).join(", ") + "})";
var sixtyFive = "({" + Array.from({ length: 65 }, (_, i) => "p" + i + ": " + i).join(", ") + "})";
t("64-fields", () => eval(sixtyFour));
t("65-fields", () => eval(sixtyFive));
// a setter and a getter on Object.prototype with the same names: a literal defines, so neither runs
var calls = 0;
Object.defineProperty(Object.prototype, "trap", { set(v) { calls++; }, get() { calls += 100; return "proto"; }, configurable: true });
t("define-not-set", () => ({ trap: 1, other: 2 }));
log.push("setter calls: " + calls);
delete Object.prototype.trap;
// the same keys from two places share a shape-dependent behavior: property order and later additions
t("later-add", () => { var o = { x: 1, y: 2 }; o.z = 3; delete o.x; o.x = 4; return o; });
t("freeze", () => Object.freeze({ x: 1, y: 2 }));
t("seal-then-add", () => { var o = Object.seal({ x: 1 }); o.y = 2; return o; });
// values that see variables changed by earlier values
t("order", () => { var i = 0; return { a: i++, b: i++, c: i * 10 }; });
// generators and async functions suspending inside a literal
function* gen() { var o = { x: yield 1, y: yield 2, z: 3 }; return o; }
var g = gen(); g.next(); g.next("ex"); var r = g.next("why");
t("generator", () => r.value);
async function asy() { return { p: await 1, q: await Promise.resolve(2) }; }
var done = asy().then(v => { log.push("async: " + describe(v)); });
// many objects from the same literal, mutated independently
t("independent", () => { var arr = []; for (var i = 0; i < 3; i++) arr.push({ i, sq: i * i }); arr[0].i = 99; arr[1].extra = 1; return { a0: arr[0], a1: arr[1], a2: arr[2] }; });
t("json", () => ({ s: JSON.stringify({ b: 1, a: [1, { c: 2 }], d: "x" }) }));
t("keys", () => ({ k: Object.keys({ z: 1, y: 2, x: 3 }).join(), e: JSON.stringify(Object.entries({ m: 1, n: 2 })) }));
t("loop-shapes", () => { var s = 0, v; for (var i = 0; i < 1000; i++) { v = { x: i, y: i * 2, z: 0 }; s += v.x + v.y; } return { s, v }; });
t("closure", () => { var mk = (x) => () => ({ x, y: x + 1 }); return mk(5)(); });
t("this-in-arrow", function () { return (() => ({ t: typeof this }))(); });
t("in-class", () => { class K { constructor() { this.o = { a: 1, b: 2 }; } } return new K().o; });
t("eval-literal", () => eval("({ ev: 1, al: 2 })"));
t("Function", () => new Function("return { fn: 1, ctor: 2 };")());
done.then(() => { for (var line of log) console.log(line); });
