// Run by tools/wasm-js/check-patches.sh compare on the unpatched and the patched engine: the outputs must match.
// JSON.parse cases whose results must not change.
var out = [];
function show(v) {
    if (typeof v === "number") return Object.is(v, -0) ? "-0" : String(v) + (Number.isInteger(v) ? "i" : "");
    if (v === null || typeof v !== "object") return typeof v + ":" + String(v);
    if (Array.isArray(v)) return "[" + v.map(show).join(",") + "]";
    return "{" + Reflect.ownKeys(v).map(k => String(k) + "=" + show(v[k]) + (Object.getOwnPropertyDescriptor(v, k).enumerable ? "" : "!e")).join(",") + "}" + (Object.getPrototypeOf(v) === Object.prototype ? "" : "(proto?)");
}
function t(name, text, reviver) { try { out.push(name + ": " + show(JSON.parse(text, reviver))); } catch (e) { out.push(name + ": threw " + e.name + ": " + e.message); } }
var nums = ["0", "-0", "1", "-1", "9", "123456789", "-123456789", "999999999", "1000000000", "2147483647", "2147483648", "-2147483648", "-2147483649",
    "12345678901234567890", "0.1", "-0.0", "0.0", "1.0", "1e0", "1E2", "1e-2", "-1e+2", "0e5", "-0e-5", "1.7976931348623157e308", "1.7976931348623159e308", "1e400", "-1e400",
    "5e-324", "2e-324", "4.9406564584124654e-324", "2.2250738585072011e-308", "0.30000000000000004", "123.456e-7", "00", "01", "-", "1.", ".5", "1e", "+1", "--1", "1e+", "0x10", "Infinity", "NaN", "1_000",
    "9007199254740993", "-9007199254740993", "0.000001", "1e21", "123456789.0", "000000001"];
for (var n of nums) t("num " + n, n);
for (var n of nums) t("arr " + n, "[" + n + ", " + n + "]");
t("dup", '{"a":1,"b":2,"a":3}');
t("dup-nested", '{"a":{"x":1},"a":{"y":2}}');
t("proto", '{"__proto__":{"evil":1},"x":2}');
t("proto-dup", '{"__proto__":1,"__proto__":2}');
t("numeric-keys", '{"b":1,"2":2,"1":1,"-1":0,"01":5,"4294967295":9,"4294967294":8}');
t("empty", '{}');
t("empty-key", '{"":1,"":2}');
t("escapes", '{"k\\n\\u0041\\"":"v\\t\\u00e9\\ud83d\\ude00\\u0000"}');
t("unicode-raw", '{"ключ":"значение","😀":"ok"}');
t("nested", '{"a":[1,{"b":[2,[3,{"c":null}]]}],"d":true,"e":false}');
t("ws", ' \t\n\r{ \n"a" \r: \t[ 1 , 2 ] } \n');
t("bad-ws", '\u000b1');
t("trailing-comma", '{"a":1,}');
t("trailing-comma-arr", '[1,]');
t("single-quote", "{'a':1}");
t("unquoted", '{a:1}');
t("comment", '{"a":1 /* c */}');
t("control-in-string", '"a\u0001b"');
t("bad-escape", '"\\x41"');
t("bad-unicode", '"\\u12G4"');
t("truncated", '{"a":[1,2');
t("truncated-string", '"abc');
t("extra", '{"a":1} x');
t("bigdeep", "[".repeat(10000) + "]".repeat(10000));
t("lone-surrogate-escape", '"\\ud800"');
t("bad-utf8", '"ÿ"');
t("reviver", '{"a":1,"b":[2,3],"c":{"d":4}}', function (k, v) { return typeof v === "number" ? v * 10 : v; });
t("reviver-delete", '{"a":1,"b":2}', function (k, v) { return k === "a" ? undefined : v; });
t("reviver-this", '{"a":{"b":1}}', function (k, v) { if (k === "b") this.extra = 7; return v; });
t("reviver-source", '{"a":1.0,"b":[12345678901234567890, 0.1]}', function (k, v, ctx) { return ctx && ctx.source !== undefined ? "src:" + ctx.source : v; });
t("reviver-dup", '{"a":1,"a":2}', function (k, v, ctx) { return ctx && ctx.source !== undefined ? "src:" + ctx.source : v; });
// round trips of random structures
var seed = 777;
function rnd(n) { seed = (seed * 1103515245 + 12345) & 0x7fffffff; return seed % n; }
var alphabet = ["a", "b", "Z", "0", "\"", "\\", "\n", "\u0001", "é", "ÿ", "中", "\ud800", "😀", " ", "/"];
function rstr() { var s = "", n = rnd(6); for (var i = 0; i < n; i++) s += alphabet[rnd(alphabet.length)]; return s; }
function rval(d) {
    switch (d > 4 ? rnd(5) : rnd(7)) {
    case 0: return rnd(2000000000) - 1000000000;
    case 1: return (rnd(2000000) - 1000000) / (rnd(97) + 1) * (rnd(4) ? 1 : 1e-200);
    case 2: return rstr();
    case 3: return [true, false, null, -0, 0][rnd(5)];
    case 4: return rnd(2) ? 2 ** (rnd(1100) - 1074) : -(10 ** rnd(30));
    case 5: { var a = [], n = rnd(5); for (var i = 0; i < n; i++) a.push(rval(d + 1)); return a; }
    default: { var o = {}, n = rnd(6); for (var i = 0; i < n; i++) o[rnd(4) ? rstr() : String(rnd(20))] = rval(d + 1); return o; }
    }
}
var acc = [];
for (var k = 0; k < 4000; k++) { var v = rval(0); var s = JSON.stringify(v); var back = JSON.parse(s); acc.push(show(back)); acc.push(JSON.stringify(back, null, 1)); }
var all = acc.join("\u0000"), h = 0;
for (var i = 0; i < all.length; i++) h = (h * 31 + all.charCodeAt(i)) | 0;
out.push("random: " + acc.length + " " + all.length + " " + h);
for (var line of out) console.log(line);
