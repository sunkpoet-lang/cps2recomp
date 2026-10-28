// SPDX-License-Identifier: GPL-3.0-only
// Part of cps2recomp (the user's fork).
//
// cps-tunel: carries a game's UDP netplay (UltraCombo's, for Killer Instinct) over an Iroh connection, direct or through
// a public relay, so two players need no open ports nor a VPN. The game knows nothing of it: it plays against
// 127.0.0.1 as on a LAN.
//
//   cps-tunel host [--game-port 7845]       prints "CODE <room code>"; each guest reaches the game at 127.0.0.1:7845
//                                            from a local port of its own (up to 8: a second player, spectators)
//   cps-tunel join <room code> [--listen N] prints "LISTEN <port>": the game joins 127.0.0.1:<port>
//   both: --parent PID (ends with that process), --relay-only (never direct, for tests)
//
// It reports on stdout, one event per line (the launcher reads them from its log): CODE, LISTEN, CONNECTED [n],
// PATH [n] direct|relay, PING [n] <ms>, TOOBIG [n] <count>, CLOSED [n], ERROR [n] <message>. On the host, a guest's
// events carry its number.

mod code;

use std::net::SocketAddr;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;
use std::time::Duration;

use anyhow::{bail, Context, Result};
use bytes::Bytes;
use iroh::endpoint::{presets, Connection, QuicTransportConfig};
use iroh::Endpoint;
use tokio::net::UdpSocket;
use tokio::time::timeout;

const ALPN: &[u8] = b"cps-tunel/udp/1";
const MAX_GUESTS: usize = 8;
const ROOM_FULL: u32 = 1; // (the code a host closes a ninth guest with)

#[derive(Default)]
struct Options {
    relay_only: bool,
    parent: Option<u32>,
}

#[tokio::main]
async fn main() {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let value = |name: &str| args.iter().position(|a| a == name).and_then(|i| args.get(i + 1)).cloned();
    let number = |name: &str, default: u16| match value(name) {
        Some(v) => v.parse::<u16>().map_err(|_| anyhow::anyhow!("{name} necesita un número de puerto")),
        None => Ok(default),
    };
    let options = Options {
        relay_only: args.iter().any(|a| a == "--relay-only"),
        parent: value("--parent").and_then(|p| p.parse().ok()),
    };
    if let Some(pid) = options.parent {
        watch_parent(pid);
    }
    let result = match args.first().map(String::as_str) {
        Some("host") => match number("--game-port", 7845) {
            Ok(port) => host(port, &options).await,
            Err(e) => Err(e),
        },
        Some("join") => {
            // (a code pasted with spaces may arrive as several words: they are one code)
            let words: Vec<&str> = args[1..].iter().take_while(|a| !a.starts_with("--")).map(String::as_str).collect();
            match number("--listen", 0) {
                Ok(listen) => join(&words.join(""), listen, &options).await,
                Err(e) => Err(e),
            }
        }
        _ => {
            println!("uso: cps-tunel host [--game-port 7845] [--parent PID] [--relay-only]");
            println!("     cps-tunel join <código> [--listen PUERTO] [--parent PID] [--relay-only]");
            std::process::exit(2);
        }
    };
    if let Err(e) = result {
        println!("ERROR {e:#}");
        std::process::exit(1);
    }
}

async fn endpoint(options: &Options, accept: bool) -> Result<Endpoint> {
    // a second of silence sends a keep-alive; eight mean the other side is gone (noticed within ten)
    let transport = QuicTransportConfig::builder()
        .keep_alive_interval(Duration::from_secs(1))
        .max_idle_timeout(Some(Duration::from_secs(8).try_into()?))
        .build();
    let mut builder = Endpoint::builder(presets::N0).transport_config(transport);
    if accept {
        builder = builder.alpns(vec![ALPN.to_vec()]);
    }
    if options.relay_only {
        builder = builder.clear_ip_transports();
    }
    Ok(builder.bind().await?)
}

async fn host(game_port: u16, options: &Options) -> Result<()> {
    let ep = endpoint(options, true).await?;
    timeout(Duration::from_secs(20), ep.online())
        .await
        .context("no se pudo llegar a internet (ningún relay respondió)")?;
    let relay = ep.addr().relay_urls().next().cloned().context("no hay relay para el código")?;
    println!("CODE {}", code::encode(&ep.id(), &relay));
    let guests = Arc::new(AtomicUsize::new(0));
    let mut next = 0u32;
    while let Some(incoming) = ep.accept().await {
        let conn = match incoming.await {
            Ok(conn) => conn,
            Err(e) => {
                println!("ERROR un invitado no pudo conectarse: {e}");
                continue;
            }
        };
        if guests.load(Ordering::SeqCst) >= MAX_GUESTS {
            conn.close(ROOM_FULL.into(), b"room full");
            continue;
        }
        next += 1;
        let n = next;
        guests.fetch_add(1, Ordering::SeqCst);
        let guests = guests.clone();
        tokio::spawn(async move {
            println!("CONNECTED {n}");
            // this guest's own local port: the game tells the guests apart by it
            let carried = async {
                let udp = UdpSocket::bind("127.0.0.1:0").await?;
                udp.connect(SocketAddr::from(([127, 0, 0, 1], game_port))).await?;
                pump(&conn, &udp, false, &format!(" {n}")).await;
                anyhow::Ok(())
            };
            if let Err(e) = carried.await {
                println!("ERROR {n} {e:#}");
            }
            conn.close(0u32.into(), b"bye");
            println!("CLOSED {n}");
            guests.fetch_sub(1, Ordering::SeqCst);
        });
    }
    Ok(())
}

async fn join(room: &str, listen: u16, options: &Options) -> Result<()> {
    let addr = code::decode(room)?;
    let ep = endpoint(options, false).await?;
    let udp = UdpSocket::bind(SocketAddr::from(([127, 0, 0, 1], listen))).await?;
    println!("LISTEN {}", udp.local_addr()?.port());
    let conn = timeout(Duration::from_secs(30), ep.connect(addr, ALPN))
        .await
        .context("el anfitrión no respondió")?
        .context("no se pudo conectar con el anfitrión")?;
    println!("CONNECTED");
    pump(&conn, &udp, true, "").await;
    let full = matches!(conn.close_reason(), Some(iroh::endpoint::ConnectionError::ApplicationClosed(ref c))
        if c.error_code == ROOM_FULL.into());
    ep.close().await;
    if full {
        bail!("la sala está llena");
    }
    println!("CLOSED");
    Ok(())
}

// Datagrams both ways until the connection ends. As guest the game's address is learnt from its first packet.
async fn pump(conn: &Connection, udp: &UdpSocket, guest: bool, tag: &str) {
    let mut game: Option<SocketAddr> = None;
    let mut buf = vec![0u8; 2048];
    let mut tick = tokio::time::interval(Duration::from_secs(2));
    let mut too_big = 0u64;
    loop {
        tokio::select! {
            r = udp.recv_from(&mut buf) => {
                let (n, from) = match r {
                    Ok(x) => x,
                    // (Windows: an earlier packet found nobody listening yet, e.g. the game still starting)
                    Err(e) if e.kind() == std::io::ErrorKind::ConnectionReset => continue,
                    Err(_) => break,
                };
                if guest {
                    game = Some(from);
                }
                if conn.max_datagram_size().is_none_or(|max| n > max) {
                    too_big += 1;
                    continue;
                }
                if conn.send_datagram(Bytes::copy_from_slice(&buf[..n])).is_err() {
                    break;
                }
            }
            d = conn.read_datagram() => {
                let Ok(d) = d else { break };
                let sent = match (guest, game) {
                    (false, _) => udp.send(&d).await,
                    (true, Some(to)) => udp.send_to(&d, to).await,
                    (true, None) => continue,     // the game has not spoken yet
                };
                if let Err(e) = sent {
                    if e.kind() != std::io::ErrorKind::ConnectionReset {
                        break;
                    }
                }
            }
            _ = tick.tick() => report(conn, tag, too_big),
        }
    }
}

fn report(conn: &Connection, tag: &str, too_big: u64) {
    let paths = conn.paths();
    if let Some(path) = paths.iter().find(|p| p.is_selected()) {
        println!("PATH{tag} {}", if path.is_relay() { "relay" } else { "direct" });
        println!("PING{tag} {}", path.rtt().as_millis());
    }
    if too_big > 0 {
        println!("TOOBIG{tag} {too_big}");
    }
}

// Ends the tunnel when the launcher that started it is gone, so no port is left taken.
fn watch_parent(pid: u32) {
    std::thread::spawn(move || {
        wait_for_exit(pid);
        println!("CLOSED");
        std::process::exit(0);
    });
}

#[cfg(windows)]
fn wait_for_exit(pid: u32) {
    use windows_sys::Win32::Foundation::CloseHandle;
    use windows_sys::Win32::System::Threading::{OpenProcess, WaitForSingleObject, INFINITE, PROCESS_SYNCHRONIZE};
    unsafe {
        let handle = OpenProcess(PROCESS_SYNCHRONIZE, 0, pid);
        if handle.is_null() {
            return; // (already gone)
        }
        WaitForSingleObject(handle, INFINITE);
        CloseHandle(handle);
    }
}

#[cfg(not(windows))]
fn wait_for_exit(pid: u32) {
    let proc = std::path::PathBuf::from(format!("/proc/{pid}"));
    loop {
        std::thread::sleep(Duration::from_secs(1));
        if std::path::Path::new("/proc").exists() && !proc.exists() {
            return;
        }
    }
}
