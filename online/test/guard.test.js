// SPDX-License-Identifier: GPL-3.0-only
// (this fork, ForkAde's online-abuse-protection 2.1) The request guard: oversize, malformed JSON, wrong methods, and
// honest requests as before. `node --test online/test/`
import assert from "node:assert/strict";
import test from "node:test";
import { service } from "./fake.js";

const host = { game: "ssf2t", build: "b1", version: "0.1", addresses: ["198.51.100.4:6000"], listed: true, name: "MrBelette", protocol: 3 };

test("a 5 MB body is refused before it is read", async () => {
  const { call } = service();
  const big = "x".repeat(5 * 1024 * 1024);
  const answer = await call("POST", "/v1/room", big);
  assert.equal(answer.status, 413);
});

test("a body larger than it says is refused while read", async () => {
  const { call } = service();
  const big = JSON.stringify({ secret: "s".repeat(32), name: "x".repeat(20000) });
  // (no content-length the guard can trust: a stream)
  const stream = new ReadableStream({ start(c) { c.enqueue(new TextEncoder().encode(big)); c.close(); } });
  const answer = await call("POST", "/v1/room", stream, {});
  assert.equal(answer.status, 413);
});

test('"{" gets 400 on every JSON route', async () => {
  const { call } = service();
  for (const path of ["/v1/host", "/v1/room", "/v1/room/leave", "/v1/room/decline", "/v1/join/K7QM2X"]) {
    const answer = await call("POST", path, "{");
    assert.equal(answer.status, 400, path);
  }
});

test("JSON that is not an object gets 400", async () => {
  const { call } = service();
  for (const body of ["[]", "3", '"x"', "null"]) {
    assert.equal((await call("POST", "/v1/host", body)).status, 400, body);
  }
});

test("the objects' own routes answer broken JSON with 400 (the lobby's /seen, the stats' /waiting)", async () => {
  const { env } = service();
  const lobby = env.LOBBIES.get("all");
  assert.equal((await lobby.fetch("https://lobby/seen", { method: "POST", body: "{" })).status, 400);
  const stats = env.STATS.get("all");
  assert.equal((await stats.fetch("https://stats/waiting", { method: "POST", body: "{" })).status, 400);
});

test("a wrong method or an unknown route is refused", async () => {
  const { call } = service();
  assert.equal((await call("PUT", "/v1/host", "{}")).status, 405);
  assert.equal((await call("GET", "/v1/join/K7QM2X")).status, 404);
  assert.equal((await call("POST", "/v1/nothing", "{}")).status, 404);
});

test("honest play as before: host, list, look up, join, poll", async () => {
  const { call } = service();
  const hosted = await (await call("POST", "/v1/host", host)).json();
  assert.match(hosted.code, /^[2-9A-HJ-NP-Z]{6}$/);
  const listed = await (await call("GET", "/v1/lobby?protocol=3")).json();
  assert.equal(listed.matches.length, 1);
  assert.equal(listed.matches[0].name, "MrBelette");
  const info = await (await call("GET", `/v1/code/${hosted.code}`)).json();
  assert.equal(info.game, "ssf2t");
  const waiting = await (await call("GET", `/v1/host/${hosted.code}?secret=${hosted.secret}`)).json();
  assert.equal(waiting.joined, false);
  const joined = await call("POST", `/v1/join/${hosted.code}`, { game: "ssf2t", build: "b1", version: "0.1", addresses: ["192.0.2.9:7000"] });
  assert.equal(joined.status, 200);
  assert.deepEqual((await joined.json()).addresses, ["198.51.100.4:6000"]);
  const told = await (await call("GET", `/v1/host/${hosted.code}?secret=${hosted.secret}`)).json();
  assert.deepEqual(told, { joined: true, addresses: ["192.0.2.9:7000"] });
});

test("honest waiting room as before: check in, see each other, leave", async () => {
  const { call } = service();
  const a = { secret: "a".repeat(32), name: "Daniel", games: ["ssf2t"], protocol: 3, version: "0.1" };
  const b = { secret: "b".repeat(32), name: "Fabs", games: ["kof97"], protocol: 3, version: "0.1" };
  await call("POST", "/v1/room", a);
  const seen = await (await call("POST", "/v1/room", b)).json();
  assert.deepEqual(seen.players.map((p) => p.name), ["Daniel"]);
  assert.equal((await call("POST", "/v1/room/leave", { secret: a.secret })).status, 200);
  const after = await (await call("POST", "/v1/room", b)).json();
  assert.equal(after.players.length, 0);
});

test("a browser at the top is sent to the project", async () => {
  const { call } = service();
  const answer = await call("GET", "/");
  assert.equal(answer.status, 302);
});
