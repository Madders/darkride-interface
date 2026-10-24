//! The library from a terminal, for tests: commands as JSON lines on stdin, events as JSON lines on stdout, the
//! SDK's log on stderr (RUST_LOG, default warn). The same commands and events the viewer's panels will use.
//!   spike <homeserver> <data dir>
use std::{
    io::{BufRead, Write},
    sync::Arc,
};

use darkride_chat_core::{Chat, Config};

fn main() -> anyhow::Result<()> {
    tracing_subscriber::fmt()
        .with_env_filter(tracing_subscriber::EnvFilter::try_from_default_env().unwrap_or_else(|_| "warn".into()))
        .with_writer(std::io::stderr)
        .init();
    let mut args = std::env::args().skip(1);
    let (Some(homeserver), Some(data_dir)) = (args.next(), args.next()) else {
        anyhow::bail!("spike <homeserver> <data dir>");
    };
    let config = Config { homeserver, data_dir: data_dir.into(), device_name: "Darkride viewer (spike)".into() };
    let chat = Chat::start(
        config,
        Arc::new(|event| {
            let mut out = std::io::stdout().lock();
            let _ = writeln!(out, "{event}");
            let _ = out.flush();
        }),
    )?;
    for line in std::io::stdin().lock().lines() {
        let line = line?;
        if !line.trim().is_empty() {
            chat.command(&line);
        }
    }
    // the input has ended (`echo {...} | spike`): what it asked for finishes before the chat stops
    chat.wait_idle(std::time::Duration::from_secs(60));
    Ok(())
}
