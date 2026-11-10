// SPDX-License-Identifier: GPL-3.0-only
// (this fork, ForkAde's online-abuse-protection) A stand-in for the Workers runtime, for `node --test`: each Durable
// Object namespace makes one object per name, with an in-memory storage (get, put, delete, deleteAll, setAlarm), and
// a stub's fetch hands the object a Request, as the runtime does. Enough for the service's own logic; not Cloudflare.
import worker, { Limits, Lobby, Match, Room, Stats } from "../src/index.js";

class Storage {
  constructor() {
    this.map = new Map();
    this.alarm = null;
  }
  async get(key) {
    return this.map.has(key) ? structuredClone(this.map.get(key)) : undefined;
  }
  async put(key, value) {
    this.map.set(key, structuredClone(value));
  }
  async delete(key) {
    return this.map.delete(key);
  }
  async deleteAll() {
    this.map.clear();
  }
  async setAlarm(at) {
    this.alarm = at;
  }
  async getAlarm() {
    return this.alarm;
  }
}

class Namespace {
  constructor(make, env) {
    this.make = make;
    this.env = env;
    this.objects = new Map();
  }
  idFromName(name) {
    return name;
  }
  get(id) {
    if (!this.objects.has(id)) {
      this.objects.set(id, this.make({ storage: new Storage(), id }, this.env));
    }
    const object = this.objects.get(id);
    return { fetch: (url, init) => object.fetch(new Request(url, init)) };
  }
}

// A fresh service: its env, and `call(method, path, body, headers)` answering a Response
export function service(extra = {}) {
  const env = { ...extra };
  env.MATCHES = new Namespace((state, e) => new Match(state, e), env);
  env.LOBBIES = new Namespace((state, e) => new Lobby(state, e), env);
  env.ROOMS = new Namespace((state, e) => new Room(state, e), env);
  env.STATS = new Namespace((state, e) => new Stats(state, e), env);
  env.LIMITS_COUNTER = new Namespace((state, e) => new Limits(state, e), env);
  const waiting = [];
  const ctx = { waitUntil: (p) => waiting.push(p) };
  const call = async (method, path, body, headers = {}) => {
    const init = { method, headers: { "cf-connecting-ip": "203.0.113.7", ...headers } };
    if (headers.from) {
      init.headers["cf-connecting-ip"] = headers.from;
      delete init.headers.from;
    }
    if (body !== undefined) {
      if (body instanceof ReadableStream) {
        init.body = body;
        init.duplex = "half";
      } else {
        init.body = typeof body === "string" ? body : JSON.stringify(body);
      }
    }
    const answer = await worker.fetch(new Request(`https://online.test${path}`, init), env, ctx);
    await Promise.allSettled(waiting.splice(0));
    return answer;
  };
  return { env, call };
}
