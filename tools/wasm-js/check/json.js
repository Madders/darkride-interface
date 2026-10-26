// Run by tools/wasm-js/check-patches.sh compare on the unpatched and the patched engine: the outputs must match.
// JSON.stringify cases whose results must not change; prints each result (or error) and side-effect logs.
var out = [];
function t(name, f) { try { var r = f(); out.push(name + ": " + (typeof r === "string" ? JSON.stringify(r) : String(r))); } catch (e) { out.push(name + ": threw " + e.name + ": " + e.message); } }
var S = JSON.stringify;
var ctl = ""; for (var c = 0; c < 0x80; c++) ctl += String.fromCharCode(c);
var latin = ""; for (var c = 0x80; c < 0x100; c++) latin += String.fromCharCode(c);
t("control", () => S(ctl));
t("latin1", () => S(latin));
t("wide", () => S("héllo wörld    中文 😀 end"));
t("lone-surrogates", () => S(["\ud800", "\udfff", "a\ud800b", "\udc00\ud800", "\ud83d"]));
t("quotes", () => S('a"b\\c/d\'e'));
t("rope", () => { var s = ""; for (var i = 0; i < 50; i++) s += "part" + i + "\n"; return S({ s: s, t: s + s }); });
t("slices", () => { var big = "0123456789\"abcdef\\".repeat(20); return S([big.substring(5, 40), big.slice(100), big.substr(3, 7)]); });
t("numbers", () => S([0, -0, 1, -1, 1.5, -1.5, 1e21, 1e-7, 123456789012345680000, 5e-324, 1.7976931348623157e308, NaN, Infinity, -Infinity, 0.1 + 0.2, 2 ** 31, -(2 ** 31), 2 ** 53 + 2]));
t("ints", () => S({ a: 2147483647, b: -2147483648, c: 42 }));
t("literals", () => S([true, false, null, undefined, function () {}, Symbol("x")]));
t("object-omits", () => S({ u: undefined, f: function () {}, s: Symbol("x"), n: null, ok: 1 }));
t("nested", () => S({ a: [1, [2, [3, { b: [] }]]], c: {}, d: [{}] }));
t("sparse", () => { var a = [1, , 3]; a[6] = 7; return S(a); });
t("array-extra-props", () => { var a = [1, 2]; a.x = 5; return S(a); });
t("numeric-keys", () => S({ b: 1, 2: "two", a: 3, 1: "one", "10": "ten", "-1": "m", "01": "z" }));
t("symbol-keys", () => S({ [Symbol("k")]: 1, v: 2 }));
t("non-enumerable", () => { var o = { a: 1 }; Object.defineProperty(o, "h", { value: 2, enumerable: false }); return S(o); });
t("inherited", () => S(Object.create({ inherited: 1 }, { own: { value: 2, enumerable: true } })));
t("boxed", () => S([new Number(3), new String("str"), new Boolean(false), Object(1n) === 0 ? 0 : 1]));
t("date", () => S({ d: new Date(0) }));
t("toJSON", () => S({ a: { toJSON(k) { return "A(" + k + ")"; } }, b: [{ toJSON(k) { return "B(" + typeof k + ":" + k + ")"; } }] }));
t("toJSON-proto", () => { function P() {} P.prototype.toJSON = function (k) { return { key: k, type: "P" }; }; return S({ x: new P(), y: [new P()] }); });
t("toJSON-undefined", () => S({ a: { toJSON() { return undefined; } }, b: [{ toJSON() { return undefined; } }] }));
t("toJSON-top", () => S({ toJSON(k) { return "top:[" + k + "]"; } }));
t("bigint", () => S({ n: 1n }));
t("bigint-toJSON", () => { BigInt.prototype.toJSON = function (k) { return "big:" + this + ":" + k; }; var r = S({ n: 10n, m: [20n] }); delete BigInt.prototype.toJSON; return r; });
t("replacer-fn", () => { var log = []; var r = S({ a: 1, b: [2, { c: 3 }], d: "x" }, function (k, v) { log.push(typeof k + ":" + k + ":" + (this === undefined ? "u" : Array.isArray(this) ? "arr" : typeof this)); return typeof v === "number" ? v * 10 : v; }); return r + " | " + log.join(","); });
t("replacer-array", () => S({ a: 1, b: 2, c: { a: 3, z: 4 }, 1: "one" }, ["a", "c", 1, new String("b"), "a", {}, null]));
t("replacer-array-missing", () => S({ a: 1 }, ["a", "zz", "toString"]));
t("space-number", () => S({ a: [1, { b: 2 }], c: {} , d: [] }, null, 2));
t("space-big", () => S({ a: [1] }, null, 20));
t("space-string", () => S({ a: [1, { b: 2 }] }, null, "--tab--long-gap--"));
t("space-boxed", () => S({ a: 1 }, null, new Number(3)) + S({ a: 1 }, null, new String("..")));
t("cycle", () => { var o = { a: 1 }; o.self = o; return S(o); });
t("cycle-array", () => { var a = [1]; a.push([a]); return S(a); });
t("shared-not-cycle", () => { var s = { v: 1 }; return S({ a: s, b: s, c: [s, s] }); });
t("getter-order", () => { var log = []; var o = { get a() { log.push("a"); return 1; }, b: 2, get c() { log.push("c"); return 3; } }; return S(o) + " " + log.join(""); });
t("getter-deletes-later", () => { var o = { get a() { delete this.b; return 1; }, b: 2, c: 3 }; return S(o); });
t("getter-adds", () => { var o = { get a() { this.z = 9; return 1; }, b: 2 }; return S(o); });
t("getter-makes-nonenum", () => { var o = { get a() { Object.defineProperty(this, "b", { enumerable: false }); return 1; }, b: 2 }; return S(o); });
t("toJSON-mutates-parent", () => { var p = { a: { toJSON() { delete p.b; p.c = 1; return "A"; } }, b: 2 }; return S(p); });
t("array-shrinks", () => { var a = [1, { toJSON() { a.length = 1; return "X"; } }, 3, 4]; return S(a); });
t("array-grows", () => { var a = [{ toJSON() { a.push(99); return "X"; } }, 2]; return S(a); });
t("proxy-object", () => { var log = []; var p = new Proxy({ a: 1, b: { c: 2 } }, { ownKeys(t) { log.push("ownKeys"); return Reflect.ownKeys(t); }, getOwnPropertyDescriptor(t, k) { log.push("gopd:" + String(k)); return Reflect.getOwnPropertyDescriptor(t, k); }, get(t, k) { log.push("get:" + String(k)); return Reflect.get(t, k); } }); return S(p) + " " + log.join(","); });
t("proxy-array", () => { var log = []; var p = new Proxy([1, 2], { get(t, k) { log.push("get:" + String(k)); return Reflect.get(t, k); } }); return S(p) + " " + log.join(","); });
t("rawJSON", () => typeof JSON.rawJSON === "function" ? S({ r: JSON.rawJSON("1e1000"), s: [JSON.rawJSON('"x"')] }) : "none");
t("deep", () => { var o = {}; for (var i = 0; i < 1000; i++) o = { o: o }; return S(o).length; });
t("too-deep", () => { var o = []; for (var i = 0; i < 1000000; i++) o = [o]; return S(o).length; });
t("top-primitives", () => [S(1), S("s"), S(null), S(undefined), S(true), S(Symbol()), S(function () {})].join("|"));
t("class-instance", () => { class K { constructor() { this.x = 1; this.y = [2]; } get z() { return 3; } } return S(new K()); });
t("map-set-error", () => S([new Map([[1, 2]]), new Set([1]), new Error("e"), /re/g, new Uint8Array([1, 2])]));
t("unicode-keys", () => S({ "ключ": 1, "\u0000k": 2, "q\"k": 3, "\ud800": 4, "😀": 5 }));
// random structures
var seed = 12345;
function rnd(n) { seed = (seed * 1103515245 + 12345) & 0x7fffffff; return seed % n; }
var alphabet = ["a", "b", "Z", "0", "\"", "\\", "\n", "\t", "\u0001", "\u001f", "é", "ÿ", "Ā", "中", "\ud800", "\udc00", "😀", " ", "/", " "];
function rstr() { var s = "", n = rnd(8); for (var i = 0; i < n; i++) s += alphabet[rnd(alphabet.length)]; return s; }
function rval(d) {
    switch (d > 4 ? rnd(6) : rnd(9)) {
    case 0: return rnd(2000) - 1000;
    case 1: return (rnd(2000000) - 1000000) / (rnd(97) + 1) * (rnd(3) ? 1 : 1e-12);
    case 2: return rstr();
    case 3: return [true, false, null, undefined, NaN, -0][rnd(6)];
    case 4: return rnd(2) ? 2 ** (rnd(80) - 10) : -(2 ** rnd(60));
    case 5: return rstr() + rstr();
    case 6: { var a = [], n = rnd(5); for (var i = 0; i < n; i++) a.push(rval(d + 1)); return a; }
    default: { var o = {}, n = rnd(6); for (var i = 0; i < n; i++) o[rnd(4) ? rstr() : String(rnd(20))] = rval(d + 1); return o; }
    }
}
var all = [];
for (var k = 0; k < 3000; k++) { var v = rval(0); all.push(S(v), S(v, null, rnd(4)), S(v, (key, x) => typeof x === "string" ? x.length : x)); }
t("random", () => all.length + " " + all.join("").length + " " + (function (s) { var h = 0; for (var i = 0; i < s.length; i++) h = (h * 31 + s.charCodeAt(i)) | 0; return h; })(all.join("\u0000")));
for (var line of out) console.log(line);
