// Run by tools/wasm-js/check-patches.sh compare on the unpatched and the patched engine: the outputs must match.
// Function calls whose results must not change: arguments, frames, closures, exceptions and backtraces, generators
// and async functions, calls from C into JavaScript and back. Recursion stays far below the stack limit (about 930
// calls natively with QuickJS-NG 0.17.0's 1 MB default), which a change to the call path may move.
var out = [];
function t(name, f) { try { out.push(name + ": " + String(f())); } catch (e) { out.push(name + ": threw " + e.name + ": " + e.message); } }
function stackOf(e) { return String(e.stack).split("\n").map(s => s.trim()).filter(Boolean).join(" | "); }
t("fewer-args", () => { function f(a, b, c) { return [a, b, c, arguments.length].join(); } return f(1); });
t("more-args", () => { function f(a) { return [a, arguments.length, arguments[2]].join(); } return f(1, 2, 3); });
t("mapped-arguments", () => { function f(a, b) { arguments[0] = 9; b = 8; return [a, arguments[1]].join(); } return f(1, 2); });
t("strict-arguments", () => { function f(a, b) { "use strict"; arguments[0] = 9; b = 8; return [a, arguments[1]].join(); } return f(1, 2); });
t("default-params", () => { function f(a, b = a + 1, c = b * 2) { return [a, b, c].join(); } return [f(1), f(1, 5), f(1, undefined, 0)].join(" "); });
t("rest-params", () => { function f(a, ...r) { return a + ":" + r.join("/") + ":" + r.length; } return [f(), f(1), f(1, 2, 3)].join(" "); });
t("param-assigned", () => { function f(a) { a = 5; return arguments[0]; } return f(1); });
t("length-and-name", () => { function f(a, b = 1, ...c) {} return f.length + ":" + f.name + ":" + ((x, y) => 0).length; });
t("closure-counter", () => { function mk() { var n = 0; return function () { return ++n; }; } var c = mk(); c(); c(); return c(); });
t("closure-loop-let", () => { var fs = []; for (let i = 0; i < 3; i++) fs.push(() => i); return fs.map(f => f()).join(); });
t("closure-loop-var", () => { var fs = []; for (var i = 0; i < 3; i++) fs.push(() => i); return fs.map(f => f()).join(); });
t("closure-param-captured", () => { function f(a) { var g = () => a; a = 2; return g(); } return f(1); });
t("closure-arguments", () => { function f() { return () => arguments[0]; } return f(7)(); });
t("closure-outlives-frame", () => { function mk(x) { var y = x * 2; return [() => x + y, (v) => { y = v; }]; } var p = mk(3); p[1](10); return p[0](); });
t("recursion", () => { function fib(k) { return k < 2 ? k : fib(k - 1) + fib(k - 2); } return fib(20); });
t("deep-recursion-overflow", () => { function r(k) { return r(k + 1) + 1; } return r(0); });
t("overflow-then-continue", () => { function r(k) { return r(k + 1) + 1; } try { r(0); } catch (e) {} function ok(k) { return k ? ok(k - 1) + 1 : 0; } return ok(200); });
t("mutual-recursion", () => { function even(n) { return n === 0 ? true : odd(n - 1); } function odd(n) { return n === 0 ? false : even(n - 1); } return even(200) + "," + odd(177); });
t("this-sloppy", function () { function f() { return this === undefined ? "undefined" : typeof this; } return f() + "," + f.call(5) + "," + f.call(null); });
t("this-strict", function () { function f() { "use strict"; return this === undefined ? "undefined" : typeof this; } return f() + "," + f.call(5) + "," + f.call(null); });
t("call-apply-bind", () => { function f(a, b) { return this.k + a + b; } var o = { k: 1 }; var g = f.bind(o, 10); return [f.call(o, 2, 3), f.apply(o, [4, 5]), g(6), g.length, g.name].join(); });
t("reflect", () => { function F(a) { this.a = a; } return Reflect.apply(Math.max, null, [1, 9, 3]) + "," + Reflect.construct(F, [4]).a + "," + JSON.stringify(Reflect.construct(F, [5], Object)); });
t("new-target", () => { function F() { return new.target === F; } class A { constructor() { this.t = new.target.name; } } class B extends A {} return [typeof new F(), F(), new B().t].join(); });
t("class-super", () => { class A { m(x) { return "A" + x; } static s() { return "S"; } } class B extends A { m(x) { return "B" + super.m(x); } } return new B().m(1) + B.s(); });
t("class-fields", () => { var n = 0; class A { x = ++n; y = this.x * 10; static z = 5; } var a = new A(), b = new A(); return [a.x, a.y, b.x, b.y, A.z].join(); });
t("class-call-without-new", () => { class A {} return A(); });
t("constructor-returns", () => { function F() { this.a = 1; return { b: 2 }; } function G() { this.a = 1; return 3; } return JSON.stringify(new F()) + JSON.stringify(new G()); });
t("getter-setter", () => { var log = []; var o = { get g() { log.push("g"); return 1; }, set s(v) { log.push("s" + v); } }; o.s = o.g + 1; return log.join(); });
t("proxy-apply", () => { var p = new Proxy(function (a) { return a * 2; }, { apply(t, th, args) { return t(...args) + 1; } }); return p(5); });
t("exception-through-js", () => { function a() { b(); } function b() { throw new Error("deep"); } try { a(); } catch (e) { return e.message + " | " + stackOf(e); } });
t("exception-through-c", () => { try { [1, 2, 3].map(function (x) { if (x === 2) throw new TypeError("in map " + x); return x; }); } catch (e) { return e.name + ": " + e.message + " | " + stackOf(e); } });
t("rethrow-in-catch", () => { function f() { try { throw 1; } catch (e) { throw e + 1; } } try { f(); } catch (e) { return e; } });
t("stack-from-callback", () => [1].map(function inner() { return stackOf(new Error("s")); })[0]);
t("stack-from-sort", () => { var s; [2, 1].sort(function cmp(a, b) { s = stackOf(new Error("x")); return a - b; }); return s; });
t("stack-from-getter", () => { var o = { get g() { return stackOf(new Error("g")); } }; return o.g; });
t("stack-from-toString", () => { var o = { toString() { return stackOf(new Error("t")); } }; return "" + o; });
t("stack-from-json", () => JSON.stringify({ a: { toJSON() { return stackOf(new Error("j")); } } }));
t("stack-native-error", () => { try { JSON.parse("{bad"); } catch (e) { return stackOf(e); } });
t("stack-of-native-call", () => { try { null.f(); } catch (e) { return stackOf(e); } });
t("finally-and-return", () => { var log = []; function f() { try { return "try"; } finally { log.push("finally"); } } return f() + " " + log.join(); });
t("generator", () => { function* g(a) { var x = yield a; var y = yield x + 1; return x + y; } var it = g(1); return [it.next().value, it.next(10).value, it.next(5).value, it.next().done].join(); });
t("generator-throw", () => { function* g() { try { yield 1; } catch (e) { yield "caught " + e; } } var it = g(); it.next(); return it.throw("boom").value; });
t("generator-calls", () => { function* g() { yield* [1, 2]; yield helper(3); } function helper(x) { return x * 10; } return [...g()].join(); });
t("generator-stack", () => { function* g() { yield stackOf(new Error("in generator")); } return g().next().value; });
t("spread-call", () => { function f(...a) { return a.length + ":" + a.join(); } return f(...[1, 2], 3, ...new Set([4])); });
t("optional-call", () => { var o = { f() { return 1; } }; return [o.f?.(), o.g?.(), o?.f()].join(); });
t("tail-recursion", () => { "use strict"; function loop(n, acc) { if (n === 0) return acc; return loop(n - 1, acc + n); } return loop(300, 0); });
t("eval-in-function", () => { function f(a) { return eval("a + 1"); } return f(1); });
t("arguments-callee", () => { function f(n) { return n <= 1 ? 1 : n * arguments.callee(n - 1); } return f(5); });
t("native-arity", () => [Math.max(), Math.max(1), Math.max(1, "2"), parseInt("12px"), String.fromCharCode(65, 66)].join());
t("native-this", () => [Array.prototype.join.call({ length: 2, 0: "a", 1: "b" }, "-"), Object.prototype.toString.call(null)].join(" "));
t("native-callbacks", () => [[3, 1, 2].map((x, i, a) => x * i + a.length).join(), [1, 2, 3, 4].filter(function (x) { return x % 2 === this.r; }, { r: 0 }).join(), [1, 2, 3].reduceRight((a, b) => a + "" + b)].join(" "));
var done = (async function () {
    var log = [];
    async function a(x) { log.push("a" + x); var y = await b(x); log.push("a-after" + y); return y + 1; }
    async function b(x) { log.push("b" + x); await null; return x * 2; }
    var r = await Promise.all([a(1), a(2)]);
    out.push("async: " + r.join() + " " + log.join());
    try { await (async function () { throw new RangeError("async throw"); })(); } catch (e) { out.push("async-throw: " + e.message + " | " + stackOf(e)); }
    var it = (async function* () { yield 1; yield await Promise.resolve(2); })();
    out.push("async-generator: " + (await it.next()).value + "," + (await it.next()).value + "," + (await it.next()).done);
})();
done.then(() => { for (var line of out) console.log(line); });
