// Shared by the join and admin pages (discord-community-lobby): the service's address, the page's session (from the
// address's #session=... after Discord, kept for this tab only), calls to the service, and flags.
(function () {
  const api = () => window.FORKADE_API.replace(/\/$/, "");

  // (the session Discord's round trip left in the hash: kept in this tab, and taken out of the address bar)
  function session() {
    const said = new URLSearchParams(location.hash.slice(1)).get("session");
    if (said) {
      try {
        sessionStorage.setItem("forkade-session", said);
      } catch {}
      history.replaceState(null, "", location.pathname + location.search);
    }
    try {
      return sessionStorage.getItem("forkade-session") || "";
    } catch {
      return said || "";
    }
  }

  function signOut() {
    try {
      sessionStorage.removeItem("forkade-session");
    } catch {}
  }

  async function call(path, body) {
    const answer = await fetch(api() + path, { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify(body || {}) });
    let said = {};
    try {
      said = await answer.json();
    } catch {}
    return { ok: answer.ok, status: answer.status, body: said };
  }

  function signInUrl(kind) {
    return `${api()}/v1/auth/discord/start?for=${kind}`;
  }

  // A flag from its country code (regional indicator letters)
  function flag(code) {
    return /^[A-Z]{2}$/.test(code || "") ? String.fromCodePoint(...[...code].map((c) => 0x1f1e6 + c.charCodeAt(0) - 65)) : "";
  }

  // Every ISO 3166-1 alpha-2 country, named in Spanish by the browser; Latin America, Spain and Brazil first
  const kFirst = "MX AR BR CL CO PE VE EC UY PY BO CR PA GT HN SV NI DO PR CU ES US".split(" ");
  const kAll = ("AD AE AF AG AI AL AM AO AQ AR AS AT AU AW AX AZ BA BB BD BE BF BG BH BI BJ BL BM BN BO BQ BR BS BT BV BW BY BZ CA CC CD CF CG CH CI CK CL CM CN CO CR CU CV CW CX CY CZ " +
    "DE DJ DK DM DO DZ EC EE EG EH ER ES ET FI FJ FK FM FO FR GA GB GD GE GF GG GH GI GL GM GN GP GQ GR GS GT GU GW GY HK HM HN HR HT HU ID IE IL IM IN IO IQ IR IS IT JE JM JO JP " +
    "KE KG KH KI KM KN KP KR KW KY KZ LA LB LC LI LK LR LS LT LU LV LY MA MC MD ME MF MG MH MK ML MM MN MO MP MQ MR MS MT MU MV MW MX MY MZ NA NC NE NF NG NI NL NO NP NR NU NZ " +
    "OM PA PE PF PG PH PK PL PM PN PR PS PT PW PY QA RE RO RS RU RW SA SB SC SD SE SG SH SI SJ SK SL SM SN SO SR SS ST SV SX SY SZ TC TD TF TG TH TJ TK TL TM TN TO TR TT TV TW TZ " +
    "UA UG UM US UY UZ VA VC VE VG VI VN VU WF WS YE YT ZA ZM ZW").split(" ");
  function countries() {
    let names;
    try {
      names = new Intl.DisplayNames(["es"], { type: "region" });
    } catch {
      names = { of: (c) => c };
    }
    const named = (c) => ({ code: c, name: names.of(c) || c });
    const rest = kAll.filter((c) => !kFirst.includes(c)).map(named).sort((a, b) => a.name.localeCompare(b.name, "es"));
    return [...kFirst.map(named), ...rest];
  }

  // A flag as a picture: flags.png holds every code of kAll (sorted) at 24x18, 16 a row (flag-icons 7.6.0, MIT:
  // flags-LICENSE.txt). Windows draws no flag emoji, so the pages show these instead.
  const kSorted = kAll.slice().sort();
  function flagNode(code) {
    const node = document.createElement("span");
    const at = kSorted.indexOf(code || "");
    if (at < 0) {
      return node;
    }
    node.className = "flag";
    node.title = code;
    node.style.backgroundPosition = `${-(at % 16) * 24}px ${-Math.floor(at / 16) * 18}px`;
    return node;
  }

  window.ForkAde = { api, session, signOut, call, signInUrl, flag, flagNode, countries };
})();
